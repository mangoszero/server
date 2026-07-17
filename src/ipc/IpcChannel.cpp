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
#include "IpcServerHandler.h"
#include "IpcClientHandler.h"
#include "Log/Log.h"

#include <cstdio>
#include <memory>

// ===========================================================================
// Concurrency model
// ---------------------------------------------------------------------------
// The facade (IpcServer / IpcClient) is used by the CALLER thread (mangosd's
// world thread, or the child's service loop). The socket handler runs its own
// receiver thread. To stay race-free:
//
//   - Liveness / run-id / write-authority are std::atomics on the shared,
//     reference-counted IpcLink, published by the receiver thread.
//   - SendFrame() copies the live handler shared_ptr from the link under its
//     mutex and sends outside the lock; the copied reference keeps the handler
//     alive for the duration of the send even if the receiver thread closes
//     concurrently, and the handler serialises the socket write internally.
//
// The IpcLink outlives both the handler and the driver thread, so the reliable
// lane and atomics can always be touched safely.
// ===========================================================================

// ===========================================================================
// IpcServer
// ===========================================================================

IpcServer::IpcServer()
    : m_inbound(IPC_INBOUND_QUEUE_CAP, IPC_INBOUND_BYTE_CAP),
      m_link(nullptr),
      m_thread(nullptr),
      m_worker(nullptr)
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

    // Create the coupling object (one ref for the facade; the driver thread
    // takes its own ref in its constructor).
    m_link = new IpcServerLink();
    m_link->AddRef();

    m_thread = new IpcThread(host, port, secret, &m_inbound, m_link);
    m_worker = new MaNGOS::Thread(m_thread); // starts the thread immediately
    return true;
}

void IpcServer::Stop()
{
    if (m_thread)
    {
        m_thread->Stop();
    }
    if (m_worker)
    {
        m_worker->wait();
        delete m_worker;   // joins + drops the last reference, reclaiming m_thread
        m_worker = nullptr;
    }
    m_thread = nullptr;

    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
}

bool IpcServer::SendFrame(const IpcMessage& msg)
{
    if (!m_link || !m_link->live.load(std::memory_order_acquire))
    {
        return false;
    }

    std::shared_ptr<IpcServerHandler> h = m_link->GetSendTarget();
    if (!h)
    {
        return false;
    }

    return h->SendFrame(msg) == 0;
}

bool IpcServer::PopInbound(IpcMessage& out)
{
    return m_inbound.pop(out);
}

bool IpcServer::PopReliable(IpcMessage& out)
{
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
    if (m_link)
    {
        m_link->runId.store(runId, std::memory_order_release);
    }
}

void IpcServer::SetWriteAuthority(bool on)
{
    if (m_link)
    {
        m_link->writeAuthority.store(on ? 1u : 0u, std::memory_order_release);
    }
}

// ===========================================================================
// IpcClient
// ===========================================================================

IpcClient::IpcClient()
    : m_inbound(IPC_INBOUND_QUEUE_CAP),
      m_link(nullptr),
      m_thread(nullptr),
      m_worker(nullptr)
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

    m_thread = new IpcClientThread(host, port, secret, &m_inbound, m_link);
    m_worker = new MaNGOS::Thread(m_thread);
    return true;
}

void IpcClient::Stop()
{
    if (m_thread)
    {
        m_thread->Stop();
    }
    if (m_worker)
    {
        m_worker->wait();
        delete m_worker;
        m_worker = nullptr;
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
    if (!m_link || !m_link->live.load(std::memory_order_acquire))
    {
        return false;
    }

    std::shared_ptr<IpcClientHandler> h = m_link->GetSendTarget();
    if (!h)
    {
        return false;
    }

    return h->SendFrame(msg) == 0;
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
    return m_link ? m_link->runId.load(std::memory_order_acquire) : 0u;
}

bool IpcClient::WriteAuthority() const
{
    return m_link && m_link->writeAuthority.load(std::memory_order_acquire) != 0u;
}
