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
#include "IpcClientHandler.h"
#include "Log/Log.h"

#include <ace/TP_Reactor.h>
#include <ace/INET_Addr.h>
#include <ace/OS_NS_unistd.h>

#include <cstdio>

// ===========================================================================
// IpcThread  (server / acceptor side)
// ===========================================================================

IpcThread::IpcThread(const char* host,
                     uint16 port,
                     const std::string& secret,
                     BoundedQueue<IpcMessage>* inbound)
    : m_host(host ? host : "127.0.0.1"),
      m_port(port),
      m_secret(secret),
      m_inbound(inbound),
      m_reactor(nullptr),
      m_acceptor(nullptr),
      m_running(true)
{
}

IpcThread::~IpcThread()
{
    if (m_acceptor)
    {
        delete m_acceptor;
        m_acceptor = nullptr;
    }
    if (m_reactor)
    {
        delete m_reactor;
        m_reactor = nullptr;
    }
}

void IpcThread::run()
{
    sLog.outString("IpcThread: starting server reactor on %s:%u",
                   m_host.c_str(), m_port);

    // Build a TP reactor (same as WorldSocketMgr::StartNetwork).
    ACE_TP_Reactor* impl = new ACE_TP_Reactor();
    impl->max_notify_iterations(128);
    m_reactor = new ACE_Reactor(impl, 1 /*delete_implementation*/);

    // Inject context before the acceptor fires open() on a new handler.
    IpcServerHandler::SetPendingContext(m_inbound, m_secret);

    m_acceptor = new IpcAcceptor();

    ACE_INET_Addr addr(m_port, m_host.c_str());
    if (m_acceptor->open(addr, m_reactor, ACE_NONBLOCK) == -1)
    {
        sLog.outError("IpcThread: acceptor->open() failed on %s:%u — %s",
                      m_host.c_str(), m_port, ACE_OS::strerror(errno));
        return;
    }

    sLog.outString("IpcThread: listening on %s:%u", m_host.c_str(), m_port);
    m_reactor->run_reactor_event_loop();
    sLog.outString("IpcThread: reactor loop exited");
}

void IpcThread::Stop()
{
    m_running = false;
    if (m_reactor)
    {
        m_reactor->end_reactor_event_loop();
    }
}

// ===========================================================================
// IpcClientThread  (client / connector side)
// ===========================================================================

IpcClientThread::IpcClientThread(const char* host,
                                  uint16 port,
                                  const std::string& secret,
                                  BoundedQueue<IpcMessage>* inbound)
    : m_host(host ? host : "127.0.0.1"),
      m_port(port),
      m_secret(secret),
      m_inbound(inbound),
      m_reactor(nullptr),
      m_connector(nullptr),
      m_handler(nullptr),
      m_running(true),
      m_ready(false)
{
}

IpcClientThread::~IpcClientThread()
{
    if (m_connector)
    {
        delete m_connector;
        m_connector = nullptr;
    }
    if (m_reactor)
    {
        delete m_reactor;
        m_reactor = nullptr;
    }
}

void IpcClientThread::run()
{
    fprintf(stdout, "IpcClientThread: connecting to %s:%u\n",
            m_host.c_str(), m_port);
    fflush(stdout);

    ACE_TP_Reactor* impl = new ACE_TP_Reactor();
    impl->max_notify_iterations(128);
    m_reactor = new ACE_Reactor(impl, 1);

    // Inject context before open() fires on the handler.
    IpcClientHandler::SetPendingContext(m_inbound, m_secret);

    m_connector = new IpcConnector();
    m_connector->reactor(m_reactor);

    // Allocate the handler.  ACE_Connector will call open() on success.
    ACE_NEW_NORETURN(m_handler, IpcClientHandler());
    if (!m_handler)
    {
        fprintf(stderr, "IpcClientThread: OOM allocating IpcClientHandler\n");
        return;
    }

    ACE_INET_Addr addr(m_port, m_host.c_str());
    if (m_connector->connect(m_handler, addr) == -1)
    {
        fprintf(stderr, "IpcClientThread: connect() failed: %s\n",
                ACE_OS::strerror(errno));
        return;
    }

    m_ready = true;
    fprintf(stdout, "IpcClientThread: connected; starting client reactor\n");
    fflush(stdout);

    m_reactor->run_reactor_event_loop();
    fprintf(stdout, "IpcClientThread: client reactor loop exited\n");
    fflush(stdout);
}

void IpcClientThread::Stop()
{
    m_running = false;
    if (m_reactor)
    {
        m_reactor->end_reactor_event_loop();
    }
}
