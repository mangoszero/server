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
                     BoundedQueue<IpcMessage>* inbound,
                     IpcServerLink* link)
    : m_host(host ? host : "127.0.0.1"),
      m_port(port),
      m_secret(secret),
      m_inbound(inbound),
      m_link(link),
      m_reactor(nullptr),
      m_acceptor(nullptr),
      m_notifier(nullptr),
      m_running(true)
{
    if (m_link)
    {
        m_link->AddRef();
    }
}

IpcThread::~IpcThread()
{
    if (m_acceptor)
    {
        delete m_acceptor;
        m_acceptor = nullptr;
    }

    // Teardown TOCTOU guard. This destructor runs on the CALLER thread, and only
    // AFTER IpcServer::Stop() has joined the reactor thread (m_aceThread->wait()
    // returns before ~Thread -> decReference() -> this dtor). With the reactor
    // thread gone, take m_notifyMtx and null reactor/notifier on the link, THEN
    // release the mutex and destroy the objects. A concurrent SendFrame() either
    // (a) ran its notify() entirely before us under the mutex (reactor still
    // alive then), or (b) blocks on the mutex and afterwards sees reactor == null
    // and skips. notify() can therefore never touch a freed reactor/notifier.
    // The lock is released before delete, and is NEVER taken across the join, so
    // there is no deadlock with the reactor-thread drain path.
    if (m_link)
    {
        std::lock_guard<std::mutex> guard(m_link->m_notifyMtx);
        m_link->reactor.store(nullptr, std::memory_order_release);
        m_link->notifier = nullptr;
    }

    // The notifier is purge_pending_notifications()'d and removed from the
    // reactor in run() before the reactor is destroyed; safe to delete here.
    if (m_notifier)
    {
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_reactor)
    {
        delete m_reactor;
        m_reactor = nullptr;
    }
    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
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

    // Create the outbound notifier and bind the reactor onto the link BEFORE
    // any accept fires. The notifier owns the send path: the facade enqueues a
    // frame and calls m_reactor->notify(m_notifier), and handle_exception()
    // drains the queue on this (the reactor) thread.
    if (m_link)
    {
        m_notifier = new IpcServerNotifier(m_link);
        m_notifier->reactor(m_reactor);
        m_link->notifier = m_notifier;
        // Publishing the reactor pointer last (release) makes Connected()/
        // SendFrame() on the facade a no-op until the notifier is visible.
        m_link->reactor.store(m_reactor, std::memory_order_release);
    }

    // Inject context before the acceptor fires open() on a new handler.
    IpcServerHandler::SetPendingContext(m_inbound, m_secret, m_link);

    m_acceptor = new IpcAcceptor();

    ACE_INET_Addr addr(m_port, m_host.c_str());
    if (m_acceptor->open(addr, m_reactor, ACE_NONBLOCK) == -1)
    {
        sLog.outError("IpcThread: acceptor->open() failed on %s:%u - %s",
                      m_host.c_str(), m_port, ACE_OS::strerror(errno));
        if (m_link)
        {
            m_link->reactor.store(nullptr, std::memory_order_release);
        }
        return;
    }

    sLog.outString("IpcThread: listening on %s:%u", m_host.c_str(), m_port);
    m_reactor->run_reactor_event_loop();
    sLog.outString("IpcThread: reactor loop exited");

    // Reactor loop has ended. Stop accepting facade notifications and flush any
    // still-queued notifications so the reactor never calls the notifier after
    // this thread proceeds to teardown.
    if (m_link)
    {
        m_link->reactor.store(nullptr, std::memory_order_release);
        m_link->live.store(false, std::memory_order_release);
    }
    if (m_notifier)
    {
        m_reactor->purge_pending_notifications(m_notifier);
    }
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
                                  BoundedQueue<IpcMessage>* inbound,
                                  IpcClientLink* link)
    : m_host(host ? host : "127.0.0.1"),
      m_port(port),
      m_secret(secret),
      m_inbound(inbound),
      m_link(link),
      m_reactor(nullptr),
      m_connector(nullptr),
      m_handler(nullptr),
      m_notifier(nullptr),
      m_running(true),
      m_ready(false)
{
    if (m_link)
    {
        m_link->AddRef();
    }
}

IpcClientThread::~IpcClientThread()
{
    if (m_connector)
    {
        delete m_connector;
        m_connector = nullptr;
    }

    // Teardown TOCTOU guard - symmetric to ~IpcThread. Runs on the caller thread
    // after IpcClient::Stop() has joined the reactor thread. Null reactor/notifier
    // under m_notifyMtx, release, THEN destroy the objects. See ~IpcThread for the
    // full rationale and the no-deadlock argument.
    if (m_link)
    {
        std::lock_guard<std::mutex> guard(m_link->m_notifyMtx);
        m_link->reactor.store(nullptr, std::memory_order_release);
        m_link->notifier = nullptr;
    }

    if (m_notifier)
    {
        delete m_notifier;
        m_notifier = nullptr;
    }
    if (m_reactor)
    {
        delete m_reactor;
        m_reactor = nullptr;
    }
    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
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

    // Create the outbound notifier and bind the reactor onto the link before
    // open() fires. See IpcThread::run for the contract.
    if (m_link)
    {
        m_notifier = new IpcClientNotifier(m_link);
        m_notifier->reactor(m_reactor);
        m_link->notifier = m_notifier;
        m_link->reactor.store(m_reactor, std::memory_order_release);
    }

    // Inject context before open() fires on the handler.
    IpcClientHandler::SetPendingContext(m_inbound, m_secret, m_link);

    m_connector = new IpcConnector();
    m_connector->reactor(m_reactor);

    // Allocate the handler.  ACE_Connector will call open() on success.
    ACE_NEW_NORETURN(m_handler, IpcClientHandler());
    if (!m_handler)
    {
        fprintf(stderr, "IpcClientThread: OOM allocating IpcClientHandler\n");
        if (m_link)
        {
            m_link->reactor.store(nullptr, std::memory_order_release);
        }
        return;
    }

    ACE_INET_Addr addr(m_port, m_host.c_str());
    if (m_connector->connect(m_handler, addr) == -1)
    {
        fprintf(stderr, "IpcClientThread: connect() failed: %s\n",
                ACE_OS::strerror(errno));

        // OWNERSHIP: do NOT remove_reference() here - it would be a double-free.
        // On EVERY -1 return, ACE_Connector::connect_i() has already called
        // sh->close(CLOSE_DURING_NEW_CONNECTION) on the handler. close() is
        // virtual and IpcClientHandler overrides it to call remove_reference(),
        // which (creation refcount 1 -> 0, no register_handler ref since open()
        // never ran) deletes the handler inside that close() call. m_handler is
        // therefore already dangling on this path; just null it so nothing else
        // touches the freed object. (~IpcClientThread never deletes m_handler.)
        m_handler = nullptr;

        if (m_link)
        {
            m_link->reactor.store(nullptr, std::memory_order_release);
        }
        return;
    }

    m_ready = true;
    fprintf(stdout, "IpcClientThread: connected; starting client reactor\n");
    fflush(stdout);

    m_reactor->run_reactor_event_loop();
    fprintf(stdout, "IpcClientThread: client reactor loop exited\n");
    fflush(stdout);

    // Reactor loop has ended; stop facade notifications and flush pending ones.
    if (m_link)
    {
        m_link->reactor.store(nullptr, std::memory_order_release);
        m_link->live.store(false, std::memory_order_release);
    }
    if (m_notifier)
    {
        m_reactor->purge_pending_notifications(m_notifier);
    }
}

void IpcClientThread::Stop()
{
    m_running = false;
    if (m_reactor)
    {
        m_reactor->end_reactor_event_loop();
    }
}
