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

#ifndef MANGOS_H_GDB_SERVER
#define MANGOS_H_GDB_SERVER

#include "Common.h"
#include "GdbRsp.h"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

/**
 * GdbServer — facade and cooperative stop controller that bridges the
 * network listener thread (GdbServerThread, in src/mangosd) and the world
 * thread, where all protocol/state work happens.
 *
 * Data flow:
 *   - The network thread pushes received RSP bytes (FeedRsp) and plain-text
 *     monitor lines (SubmitMonitorLine) into thread-safe queues, and
 *     registers a per-connection output writer (AttachRsp / AttachMonitor).
 *   - The world thread drains those queues once per tick (OnWorldUpdate),
 *     feeds bytes to the RSP engine, runs monitor verbs, and flushes replies
 *     back through the registered writer.
 *   - A GDB interrupt (0x03) raises an async-stop request; OnWorldUpdate
 *     then enters a cooperative stop loop that pauses the 50 ms world tick
 *     and pumps packets until the debugger resumes. The loop always honours
 *     World::IsStopped() so a paused-in-debugger server can still shut down.
 *
 * Consistency is "world-tick-consistent", NOT globally race-free: the
 * network, database and map-update threads keep running during a stop.
 */
class GdbServer
{
    public:
        typedef void (*RspWriter)(void* ctx, const uint8* data, uint32 len);
        typedef void (*MonWriter)(void* ctx, const char* text);

        static GdbServer& Instance();

        /// Read config, wire the RSP output sink, run the monitor self-test.
        /// Called once at startup regardless of whether the listener starts.
        void Init();

        bool IsEnabled() const { return m_enabled; }

        // --- network thread side -------------------------------------------

        /// Register the active RSP connection's output writer. Resets the
        /// protocol session for the freshly attached debugger.
        void AttachRsp(void* ctx, RspWriter writer);
        /// Drop the active RSP connection (called from the socket's close).
        void DetachRsp(void* ctx);
        /// Queue inbound RSP bytes; scans for the 0x03 interrupt sentinel.
        void FeedRsp(const uint8* data, uint32 len);

        /// Submit one plain-text monitor line ("mangos ...") for execution on
        /// the world thread; the reply is delivered via @p writer.
        void SubmitMonitorLine(void* ctx, MonWriter writer, const char* line);

        // --- world thread side ---------------------------------------------

        /// Drain queues, service protocol traffic, and handle async stops.
        /// Invoked once per tick from World::Update.
        void OnWorldUpdate();

        /// Enter the cooperative stop from a game-level breakpoint site (on
        /// the world thread). No-op unless enabled AND a debugger is attached
        /// — otherwise there would be no one to resume the server. Captures
        /// the live register context so the debugger can backtrace the real
        /// call stack, then pumps packets until the debugger resumes.
        void EnterBreak(const char* reason);

        /// True when an RSP debugger is currently attached (used by the
        /// breakpoint hot path to skip work when nobody can act on a stop).
        bool DebuggerAttached() const { return m_rspWriter != nullptr && !m_peerClosed.load(); }

    private:
        GdbServer() = default;
        GdbServer(const GdbServer&) = delete;
        GdbServer& operator=(const GdbServer&) = delete;

        static void RspSinkThunk(uint8 b); // appends to m_outBuf (world thread)

        void DrainMonitorRequests();
        void DrainAndServiceRsp();
        void FlushRspOut();
        void EnterStop(const char* reason);
        bool CaptureContext(GdbRsp::RegSnapshot& out);

        struct MonitorReq
        {
            void* ctx = nullptr;
            MonWriter writer = nullptr;
            std::string line;
        };

        bool m_enabled = false;
        bool m_allowMemWrite = false;

        // Active RSP writer (guarded so a concurrent socket close cannot race
        // an in-progress flush).
        std::mutex m_writerLock;
        void* m_rspCtx = nullptr;
        RspWriter m_rspWriter = nullptr;
        std::atomic<bool> m_peerClosed{false};
        std::atomic<bool> m_interrupt{false};
        // Set by AttachRsp (network thread); consumed by OnWorldUpdate so all
        // RSP engine state mutation happens on the world thread.
        std::atomic<bool> m_resetPending{false};

        // Inbound RSP byte chunks (network -> world).
        std::mutex m_inLock;
        std::deque<std::vector<uint8>> m_inbound;

        // Plain-text monitor requests (network -> world).
        std::mutex m_monLock;
        std::deque<MonitorReq> m_monitor;

        // Outbound RSP scratch (world thread only).
        std::vector<uint8> m_outBuf;

        // Register context captured at the current stop (world thread only).
        GdbRsp::RegSnapshot m_capturedRegs{};
};

#define sGdbServer GdbServer::Instance()

#endif
/// @}
