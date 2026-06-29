/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/// \addtogroup mangosd
/// @{
/// \file

#include "GdbServer.h"

#include "Config.h"
#include "GdbMonitor.h"
#include "GdbRsp.h"
#include "Log.h"
#include "World.h"

#include <chrono>
#include <thread>

#if defined(_WIN32)
#  include <windows.h>
#elif defined(__x86_64__)
#  include <ucontext.h>
#endif

GdbServer& GdbServer::Instance()
{
    static GdbServer s_instance;
    return s_instance;
}

void GdbServer::Init()
{
    m_enabled = sConfig.GetBoolDefault("GdbServer.Enable", false);
    m_allowMemWrite = sConfig.GetBoolDefault("GdbServer.AllowMemWrite", false);

    GdbRsp::SetSink(&GdbServer::RspSinkThunk);
    GdbRsp::SetAllowMemWrite(m_allowMemWrite);

    // Always validate the monitor surface, even when the listener is off, so
    // a broken port is caught at boot rather than on first connect.
    GdbMon::GdbMonitorSelfTest();

    if (m_enabled)
    {
        sLog.outString("GdbServer: monitor surface ready (mem-write %s)",
            m_allowMemWrite ? "ENABLED" : "disabled");
    }
}

// -----------------------------------------------------------------------------
// RSP output sink (world thread only)
// -----------------------------------------------------------------------------

void GdbServer::RspSinkThunk(uint8 b)
{
    Instance().m_outBuf.push_back(b);
}

void GdbServer::FlushRspOut()
{
    if (m_outBuf.empty())
    {
        return;
    }
    std::lock_guard<std::mutex> guard(m_writerLock);
    if (m_rspWriter != nullptr && !m_peerClosed.load())
    {
        m_rspWriter(m_rspCtx, m_outBuf.data(), static_cast<uint32>(m_outBuf.size()));
    }
    m_outBuf.clear();
}

// -----------------------------------------------------------------------------
// Network-thread entry points
// -----------------------------------------------------------------------------

void GdbServer::AttachRsp(void* ctx, RspWriter writer)
{
    {
        std::lock_guard<std::mutex> guard(m_writerLock);
        m_rspCtx = ctx;
        m_rspWriter = writer;
    }
    m_peerClosed.store(false);
    m_interrupt.store(false);

    {
        std::lock_guard<std::mutex> guard(m_inLock);
        m_inbound.clear();
    }

    // Defer the engine reset to the world thread — the RSP parser state must
    // only ever be mutated there.
    m_resetPending.store(true);
    sLog.outString("GdbServer: debugger attached");
}

void GdbServer::DetachRsp(void* ctx)
{
    std::lock_guard<std::mutex> guard(m_writerLock);
    if (m_rspCtx == ctx)
    {
        m_rspWriter = nullptr;
        m_rspCtx = nullptr;
        m_peerClosed.store(true);
        sLog.outString("GdbServer: debugger detached");
    }
}

void GdbServer::FeedRsp(const uint8* data, uint32 len)
{
    if (data == nullptr || len == 0)
    {
        return;
    }
    // A GDB interrupt is a bare 0x03 byte outside packet framing.
    for (uint32 i = 0; i < len; ++i)
    {
        if (data[i] == 0x03)
        {
            m_interrupt.store(true);
        }
    }
    std::lock_guard<std::mutex> guard(m_inLock);
    m_inbound.emplace_back(data, data + len);
}

void GdbServer::SubmitMonitorLine(void* ctx, MonWriter writer, const char* line)
{
    if (line == nullptr || writer == nullptr)
    {
        return;
    }
    MonitorReq req;
    req.ctx = ctx;
    req.writer = writer;
    req.line = line;
    std::lock_guard<std::mutex> guard(m_monLock);
    m_monitor.push_back(std::move(req));
}

// -----------------------------------------------------------------------------
// World-thread service
// -----------------------------------------------------------------------------

void GdbServer::DrainMonitorRequests()
{
    for (;;)
    {
        MonitorReq req;
        {
            std::lock_guard<std::mutex> guard(m_monLock);
            if (m_monitor.empty())
            {
                break;
            }
            req = std::move(m_monitor.front());
            m_monitor.pop_front();
        }

        char buf[4096];
        GdbMon::MonitorWriter w(buf, sizeof(buf));
        if (!GdbMon::GdbMonitorDispatch(req.line.c_str(), static_cast<uint32>(req.line.size()), w))
        {
            req.writer(req.ctx, "error: not a 'mangos' command (try 'mangos help')\n");
        }
        else
        {
            req.writer(req.ctx, w.Data());
        }
    }
}

void GdbServer::DrainAndServiceRsp()
{
    for (;;)
    {
        std::vector<uint8> chunk;
        {
            std::lock_guard<std::mutex> guard(m_inLock);
            if (m_inbound.empty())
            {
                break;
            }
            chunk = std::move(m_inbound.front());
            m_inbound.pop_front();
        }
        for (uint8 b : chunk)
        {
            GdbRsp::ReceiveByte(b);
        }
    }
    FlushRspOut();
}

bool GdbServer::CaptureContext(GdbRsp::RegSnapshot& out)
{
    out = GdbRsp::RegSnapshot{};
#if defined(_WIN32) && (defined(_M_X64) || defined(__x86_64__))
    CONTEXT ctx;
    RtlCaptureContext(&ctx);
    out.rax = ctx.Rax; out.rbx = ctx.Rbx; out.rcx = ctx.Rcx; out.rdx = ctx.Rdx;
    out.rsi = ctx.Rsi; out.rdi = ctx.Rdi; out.rbp = ctx.Rbp; out.rsp = ctx.Rsp;
    out.r8 = ctx.R8; out.r9 = ctx.R9; out.r10 = ctx.R10; out.r11 = ctx.R11;
    out.r12 = ctx.R12; out.r13 = ctx.R13; out.r14 = ctx.R14; out.r15 = ctx.R15;
    out.rip = ctx.Rip; out.rflags = ctx.EFlags;
    out.cs = ctx.SegCs; out.ss = ctx.SegSs; out.ds = ctx.SegDs;
    out.es = ctx.SegEs; out.fs = ctx.SegFs; out.gs = ctx.SegGs;
    return true;
#elif defined(__x86_64__)
    ucontext_t uc;
    if (getcontext(&uc) != 0)
    {
        return false;
    }
    const greg_t* g = uc.uc_mcontext.gregs;
    out.rax = g[REG_RAX]; out.rbx = g[REG_RBX]; out.rcx = g[REG_RCX]; out.rdx = g[REG_RDX];
    out.rsi = g[REG_RSI]; out.rdi = g[REG_RDI]; out.rbp = g[REG_RBP]; out.rsp = g[REG_RSP];
    out.r8 = g[REG_R8]; out.r9 = g[REG_R9]; out.r10 = g[REG_R10]; out.r11 = g[REG_R11];
    out.r12 = g[REG_R12]; out.r13 = g[REG_R13]; out.r14 = g[REG_R14]; out.r15 = g[REG_R15];
    out.rip = g[REG_RIP]; out.rflags = static_cast<uint64>(g[REG_EFL]);
    // CSGSFS packs cs (bits 0-15), gs (16-31), fs (32-47).
    const uint64 csgsfs = static_cast<uint64>(g[REG_CSGSFS]);
    out.cs = static_cast<uint32>(csgsfs & 0xFFFF);
    out.gs = static_cast<uint32>((csgsfs >> 16) & 0xFFFF);
    out.fs = static_cast<uint32>((csgsfs >> 32) & 0xFFFF);
    return true;
#else
    return false; // non-x86_64: registers stay zeroed (memory + monitor still work)
#endif
}

void GdbServer::EnterStop(const char* reason)
{
    // Pause the world tick: this function runs inside the world thread (either
    // from OnWorldUpdate or inline at a breakpoint), so the 50 ms heartbeat is
    // blocked for as long as we stay here. All game-state reads issued by the
    // debugger therefore see a quiescent world.
    sLog.outString("GdbServer: target stopped (%s)", reason);

    // Capture the live register context so the debugger can backtrace the real
    // call stack (it reads stack memory through the 'm' packets).
    if (CaptureContext(m_capturedRegs))
    {
        GdbRsp::PublishRegisters(&m_capturedRegs);
    }

    // Discard any stale resume left by a 'c'/'s' issued while the target was
    // running, so this stop actually waits for a fresh resume command.
    (void)GdbRsp::TakeResume();

    GdbRsp::SendStopReply();
    FlushRspOut();

    bool resumed = false;
    while (!resumed)
    {
        // Top priority: never hold the world hostage past a shutdown request.
        if (World::IsStopped() || m_peerClosed.load())
        {
            break;
        }

        DrainAndServiceRsp();

        switch (GdbRsp::TakeResume())
        {
            case GdbRsp::ResumeAction::Continue:
            case GdbRsp::ResumeAction::Step:
            case GdbRsp::ResumeAction::Detached:
            case GdbRsp::ResumeAction::Killed:
                resumed = true;
                break;
            case GdbRsp::ResumeAction::None:
            default:
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                break;
        }
    }

    GdbRsp::PublishRegisters(nullptr);
    sLog.outString("GdbServer: target resumed");
}

void GdbServer::EnterBreak(const char* reason)
{
    // Only break when a debugger is attached — otherwise no one could resume
    // the server and the world would hang.
    if (!m_enabled || !DebuggerAttached())
    {
        return;
    }
    EnterStop(reason);
}

void GdbServer::OnWorldUpdate()
{
    if (!m_enabled)
    {
        return;
    }

    if (m_resetPending.exchange(false))
    {
        GdbRsp::ResetSession();
        m_outBuf.clear();
    }

    DrainMonitorRequests();
    DrainAndServiceRsp();

    // PollAsyncStop: a pending GDB interrupt enters the cooperative stop.
    if (m_interrupt.exchange(false))
    {
        EnterStop("debugger interrupt");
    }
}
/// @}
