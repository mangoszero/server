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

#include "IpcThread.h"
#include "IpcServerHandler.h"
#include "IpcClientHandler.h"
#include "IpcSocket.h"
#include "Log/Log.h"

#include <cstdio>
#include <memory>

// ===========================================================================
// IpcThread  (server side)
// ===========================================================================

IpcThread::IpcThread(const char* host,
                     uint16 port,
                     const std::string& secret,
                     BoundedQueue<IpcMessage>* inbound,
                     IpcServerLink* link)
    : m_host(host ? host : "127.0.0.1"),
      m_port(port),
      m_secret(secret),
      m_inbound(inbound),
      m_link(link),
      m_stop(false)
{
    if (m_link)
    {
        m_link->AddRef();
    }
}

IpcThread::~IpcThread()
{
    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
}

void IpcThread::run()
{
    IpcSocket::GlobalInit();

    IpcSocket listener;
    if (!listener.Listen(m_host, m_port))
    {
        sLog.outError("IpcThread: listen failed on %s:%u",
                      m_host.c_str(), m_port);
        IpcSocket::GlobalShutdown();
        return;
    }

    sLog.outString("IpcThread: listening on %s:%u", m_host.c_str(), m_port);

    while (!m_stop.load(std::memory_order_acquire))
    {
        IpcSocket peer;
        const int r = listener.AcceptOnce(200, peer);
        if (r == 0)
        {
            continue; // timeout: re-check stop
        }
        if (r < 0)
        {
            if (m_stop.load(std::memory_order_acquire))
            {
                break;
            }
            continue;
        }

        // SINGLE-OWNER GUARD: only one child connection is served at a time. A
        // second concurrent local connection is hostile - refuse it without
        // touching the live handler.
        bool expected = false;
        if (!m_link->handlerActive.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel, std::memory_order_acquire))
        {
            sLog.outError("IpcThread: a child connection is already active"
                          " - refusing additional connection");
            peer.Close();
            continue;
        }

        const uint32 runId = m_link->runId.load(std::memory_order_acquire);
        const uint8  wa    = m_link->writeAuthority.load(std::memory_order_acquire);

        sLog.outString("IpcThread: child connected, awaiting handshake");

        auto handler = std::make_shared<IpcServerHandler>(
            std::move(peer), m_inbound, m_secret, m_link, runId, wa);

        // Runs until the connection closes or Stop() is requested; clears the
        // link (and the single-owner guard) on exit.
        handler->ReceiveLoop(m_stop);
    }

    listener.Close();
    IpcSocket::GlobalShutdown();
    sLog.outString("IpcThread: server loop exited");
}

void IpcThread::Stop()
{
    m_stop.store(true, std::memory_order_release);
}

// ===========================================================================
// IpcClientThread  (client side)
// ===========================================================================

IpcClientThread::IpcClientThread(const char* host,
                                 uint16 port,
                                 const std::string& secret,
                                 BoundedQueue<IpcMessage>* inbound,
                                 IpcClientLink* link)
    : m_host(host ? host : "127.0.0.1"),
      m_port(port),
      m_secret(secret),
      m_inbound(inbound),
      m_link(link),
      m_stop(false),
      m_ready(false)
{
    if (m_link)
    {
        m_link->AddRef();
    }
}

IpcClientThread::~IpcClientThread()
{
    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
}

void IpcClientThread::run()
{
    IpcSocket::GlobalInit();

    IpcSocket sock;
    if (!sock.Connect(m_host, m_port))
    {
        fprintf(stderr, "IpcClientThread: connect failed to %s:%u\n",
                m_host.c_str(), m_port);
        IpcSocket::GlobalShutdown();
        return;
    }

    fprintf(stdout, "IpcClientThread: connected to %s:%u\n",
            m_host.c_str(), m_port);
    fflush(stdout);

    auto handler = std::make_shared<IpcClientHandler>(
        std::move(sock), m_inbound, m_secret, m_link);

    if (handler->SendHello() == -1)
    {
        fprintf(stderr, "IpcClientThread: SendHello failed\n");
        IpcSocket::GlobalShutdown();
        return;
    }

    m_ready.store(true, std::memory_order_release);

    handler->ReceiveLoop(m_stop);

    IpcSocket::GlobalShutdown();
    fprintf(stdout, "IpcClientThread: client loop exited\n");
    fflush(stdout);
}

void IpcClientThread::Stop()
{
    m_stop.store(true, std::memory_order_release);
}
