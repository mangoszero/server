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
 */

#include "IpcChannel.h"
#include "IpcOutboundNotifier.h"
#include "Log/Log.h"
#include <cstdio>

// ===========================================================================
// Concurrency model
// ---------------------------------------------------------------------------
// The facade (IpcServer / IpcClient) is used by the CALLER thread (mangosd's
// world thread, or the child's service loop). The socket handler runs on a
// separate REACTOR thread. To stay race-free and free of use-after-free:
//
//   - All handler-pointer access and all peer().send() / socket I/O happen on
//     the reactor thread, never on the caller thread.
//   - SendFrame() (caller thread) enqueues the frame into a thread-safe
//     outbound queue and wakes the reactor via ACE_Reactor::notify(). The
//     reactor-thread notifier (IpcOutboundNotifier::handle_exception) drains
//     the queue and performs the actual send through the live handler.
//   - Liveness is a std::atomic<bool> published by the reactor thread on READY
//     and cleared on handle_close(); Connected() just reads the atomic.
//
// The IpcLink object couples the two threads. It is reference-counted and
// outlives both the handler and the reactor thread, so the notifier can always
// touch it safely.
// ===========================================================================

// ===========================================================================
// IpcServer
// ===========================================================================

IpcServer::IpcServer()
    : m_inbound(IPC_INBOUND_QUEUE_CAP, IPC_INBOUND_BYTE_CAP),
      m_link(nullptr),
      m_thread(nullptr),
      m_aceThread(nullptr)
{
}

IpcServer::~IpcServer()
{
    Stop();
}

bool IpcServer::Start(const char* host, uint16 port, const std::string& secret)
{
    if (m_thread)
    {
        sLog.outError("IpcServer::Start: already started");
        return false;
    }

    // Create the coupling object (one ref held by the facade); the thread takes
    // its own ref in its constructor.
    m_link = new IpcServerLink();
    m_link->AddRef();

    m_thread = new IpcThread(host, port, secret, &m_inbound, m_link);
    m_aceThread = new ACE_Based::Thread(m_thread);
    // ACE_Based::Thread::start() is called in its constructor when given a
    // Runnable (see Threading.h).
    return true;
}

void IpcServer::Stop()
{
    if (m_thread)
    {
        m_thread->Stop();
    }
    if (m_aceThread)
    {
        m_aceThread->wait();
        delete m_aceThread;
        m_aceThread = nullptr;
    }
    // m_thread is deleted by ACE_Based reference counting when decReference()
    // reaches 0.
    m_thread = nullptr;

    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
}

bool IpcServer::SendFrame(const IpcMessage& msg)
{
    if (!m_link)
    {
        return false;
    }

    // Only enqueue while the channel is live; otherwise the frame would sit in
    // the queue with no handler to drain it.
    if (!m_link->live.load(std::memory_order_acquire))
    {
        return false;
    }

    // Enqueue on the caller thread (thread-safe queue), then poke the reactor.
    if (!m_link->outbound.push(msg))
    {
        sLog.outError("IpcServer::SendFrame: outbound queue full"
                      " - frame 0x%04X dropped", msg.op);
        return false;
    }

    // Poke the reactor under m_notifyMtx so the notify can never race teardown
    // destroying the reactor/notifier. Teardown nulls reactor/notifier under
    // this same mutex AFTER joining the reactor thread, so here reactor is
    // either still valid (and notify is safe) or already null (we skip).
    // ACE_Reactor::notify() is documented thread-safe; the notifier's
    // handle_exception() runs on the reactor thread and drains the queue.
    std::lock_guard<std::mutex> guard(m_link->m_notifyMtx);

    ACE_Reactor* r = m_link->reactor.load(std::memory_order_acquire);
    if (!r)
    {
        return false;
    }

    if (r->notify(m_link->notifier) == -1)
    {
        sLog.outError("IpcServer::SendFrame: reactor notify failed");
        return false;
    }

    return true;
}

bool IpcServer::PopInbound(IpcMessage& out)
{
    return m_inbound.pop(out);
}

bool IpcServer::PopReliable(IpcMessage& out)
{
    // The reliable lane lives on the shared, reference-counted link so it
    // survives child reconnects (the reactor pushes, the world thread drains).
    return m_link && m_link->PopReliable(out);
}

size_t IpcServer::ClearReliable()
{
    return m_link ? m_link->ClearReliable() : 0u;
}

bool IpcServer::Connected() const
{
    return m_link && m_link->live.load(std::memory_order_acquire);
}

void IpcServer::SetRunId(uint32 runId)
{
    IpcServerHandler::SetPendingRunId(runId);
}

void IpcServer::SetWriteAuthority(bool on)
{
    IpcServerHandler::SetPendingWriteAuthority(on);
}

// ===========================================================================
// IpcClient
// ===========================================================================

IpcClient::IpcClient()
    : m_inbound(IPC_INBOUND_QUEUE_CAP),
      m_link(nullptr),
      m_thread(nullptr),
      m_aceThread(nullptr)
{
}

IpcClient::~IpcClient()
{
    Stop();
}

bool IpcClient::Connect(const char* host, uint16 port, const std::string& secret)
{
    if (m_thread)
    {
        fprintf(stderr, "IpcClient::Connect: already connected\n");
        return false;
    }

    m_link = new IpcClientLink();
    m_link->AddRef();

    m_thread = new IpcClientThread(host, port, secret,
                                   &m_inbound, m_link);
    m_aceThread = new ACE_Based::Thread(m_thread);
    return true;
}

void IpcClient::Stop()
{
    if (m_thread)
    {
        m_thread->Stop();
    }
    if (m_aceThread)
    {
        m_aceThread->wait();
        delete m_aceThread;
        m_aceThread = nullptr;
    }
    m_thread = nullptr;

    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
}

bool IpcClient::SendFrame(const IpcMessage& msg)
{
    if (!m_link)
    {
        return false;
    }

    if (!m_link->live.load(std::memory_order_acquire))
    {
        return false;
    }

    if (!m_link->outbound.push(msg))
    {
        fprintf(stderr, "IpcClient::SendFrame: outbound queue full"
                        " - frame 0x%04X dropped\n",
                        static_cast<unsigned>(msg.op));
        return false;
    }

    // See IpcServer::SendFrame: notify under m_notifyMtx so it cannot race
    // teardown destroying the reactor/notifier.
    std::lock_guard<std::mutex> guard(m_link->m_notifyMtx);

    ACE_Reactor* r = m_link->reactor.load(std::memory_order_acquire);
    if (!r)
    {
        return false;
    }

    if (r->notify(m_link->notifier) == -1)
    {
        fprintf(stderr, "IpcClient::SendFrame: reactor notify failed\n");
        return false;
    }

    return true;
}

bool IpcClient::PopInbound(IpcMessage& out)
{
    return m_inbound.pop(out);
}

bool IpcClient::PopReliable(IpcMessage& out)
{
    return m_link && m_link->PopReliable(out);
}

bool IpcClient::Connected() const
{
    return m_link && m_link->live.load(std::memory_order_acquire);
}

uint32 IpcClient::RunId() const
{
    if (!m_link)
    {
        return 0;
    }
    return m_link->runId.load(std::memory_order_acquire);
}

bool IpcClient::WriteAuthority() const
{
    if (!m_link)
    {
        return false;
    }
    return m_link->writeAuthority.load(std::memory_order_acquire) != 0u;
}
