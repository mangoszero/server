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
#include "Log/Log.h"
#include <cstdio>

// ===========================================================================
// IpcServer
// ===========================================================================

IpcServer::IpcServer()
    : m_inbound(IPC_INBOUND_QUEUE_CAP),
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

    m_thread = new IpcThread(host, port, secret, &m_inbound);
    m_aceThread = new ACE_Based::Thread(m_thread);
    // ACE_Based::Thread::start() is called in its constructor when given a Runnable.
    // The constructor calls start() internally — see Threading.h.
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
    // m_thread is deleted by ACE_Based reference counting when decReference() reaches 0.
    m_thread = nullptr;
}

bool IpcServer::SendFrame(const IpcMessage& msg)
{
    if (!m_thread)
    {
        return false;
    }

    ACE_Reactor* r = m_thread->GetReactor();
    if (!r)
    {
        return false;
    }

    // The handler is registered with the reactor; find it by iterating reactor's
    // registered handlers.  Since this is a 1-connection server, we reach it via
    // the static pending context: IpcServerHandler stores no back-pointer to IpcServer.
    // A clean pattern: IpcThread can expose the last-accepted handler.
    // For now, reach it via the reactor's notification mechanism: post a custom
    // event_handler notification. However, the cleanest and WorldSocket-compatible
    // approach is: have IpcThread track the live handler pointer set in open().
    //
    // *** IpcThread exposes GetHandler() via the IpcServerHandler static registration;
    // we call SendFrame directly on the handler. ***
    //
    // Because IpcServerHandler::open() is called on the reactor thread, the handler
    // pointer is set there. IpcThread does not expose it because ACE_Acceptor owns it.
    // The correct way: have IpcServerHandler register itself with IpcThread via callback
    // or a shared pointer slot.
    //
    // For Task 3, we use the simplest safe approach: IpcServerHandler sets a static
    // "live handler" pointer after open() and clears it in handle_close().
    // SendFrame on the handler is thread-safe (m_outLock guards the buffer).

    IpcServerHandler* h = IpcServerHandler::GetLiveHandler();
    if (!h || !h->IsLive())
    {
        return false;
    }

    return h->SendFrame(msg) == 0;
}

bool IpcServer::PopInbound(IpcMessage& out)
{
    return m_inbound.pop(out);
}

bool IpcServer::Connected() const
{
    IpcServerHandler* h = IpcServerHandler::GetLiveHandler();
    return h && h->IsLive();
}

// ===========================================================================
// IpcClient
// ===========================================================================

IpcClient::IpcClient()
    : m_inbound(IPC_INBOUND_QUEUE_CAP),
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

    m_thread = new IpcClientThread(host, port, secret, &m_inbound);
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
}

bool IpcClient::SendFrame(const IpcMessage& msg)
{
    if (!m_thread)
    {
        return false;
    }

    IpcClientHandler* h = m_thread->GetHandler();
    if (!h || !h->IsLive())
    {
        return false;
    }

    return h->SendFrame(msg) == 0;
}

bool IpcClient::PopInbound(IpcMessage& out)
{
    return m_inbound.pop(out);
}

bool IpcClient::Connected() const
{
    if (!m_thread)
    {
        return false;
    }

    IpcClientHandler* h = m_thread->GetHandler();
    return h && h->IsLive();
}
