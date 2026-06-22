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

#ifndef AH_IPC_THREAD_H
#define AH_IPC_THREAD_H

#include "Threading/Threading.h"
#include "IpcServerHandler.h"
#include "IpcClientHandler.h"
#include "BoundedQueue.h"
#include "IpcMessage.h"

#include <ace/Reactor.h>
#include <ace/TP_Reactor.h>

#include <string>

/**
 * @brief Reactor thread that owns the IPC acceptor (server side).
 *
 * Modelled on SqlDelayThread (ACE_Based::Runnable) and WorldSocketMgr.
 *
 * run() creates an ACE_TP_Reactor + ACE_Reactor, opens an
 * ACE_Acceptor<IpcServerHandler, ACE_SOCK_ACCEPTOR> on
 * 127.0.0.1:<port>, then calls run_reactor_event_loop() — mirroring
 * WorldSocketMgr::StartNetwork lines ~152-164.
 *
 * Stop() signals end_reactor_event_loop() so run() returns.
 */
class IpcThread : public ACE_Based::Runnable
{
    public:
        /**
         * @param host    Bind address (e.g. "127.0.0.1").
         * @param port    TCP port to listen on.
         * @param secret  Shared secret validated against IPC_HELLO.
         * @param inbound Shared inbound queue populated by IpcServerHandler.
         */
        IpcThread(const char* host,
                  uint16 port,
                  const std::string& secret,
                  BoundedQueue<IpcMessage>* inbound);

        ~IpcThread() override;

        /// ACE_Based::Runnable interface — runs the reactor event loop.
        void run() override;

        /// Signal the reactor to stop.
        void Stop();

        /// Access the reactor (used by IpcServer to retrieve the handler).
        ACE_Reactor* GetReactor() const { return m_reactor; }

    private:
        std::string               m_host;
        uint16                    m_port;
        std::string               m_secret;
        BoundedQueue<IpcMessage>* m_inbound;

        ACE_Reactor*  m_reactor;
        IpcAcceptor*  m_acceptor;

        volatile bool m_running;
};

/**
 * @brief Reactor thread that owns the IPC connector (client side).
 *
 * Symmetric to IpcThread but uses ACE_Connector instead of ACE_Acceptor.
 * On start, injects context into IpcClientHandler, then connects and
 * runs the reactor event loop.
 */
class IpcClientThread : public ACE_Based::Runnable
{
    public:
        /**
         * @param host    Server address to connect to.
         * @param port    Server TCP port.
         * @param secret  Shared secret sent in IPC_HELLO.
         * @param inbound Shared inbound queue populated by IpcClientHandler.
         */
        IpcClientThread(const char* host,
                        uint16 port,
                        const std::string& secret,
                        BoundedQueue<IpcMessage>* inbound);

        ~IpcClientThread() override;

        /// ACE_Based::Runnable interface.
        void run() override;

        /// Signal the reactor to stop.
        void Stop();

        /// Return the handler after connection (may be null until connected).
        IpcClientHandler* GetHandler() const { return m_handler; }

        /// True after run() establishes the connection and reactor is live.
        bool IsReady() const { return m_ready; }

    private:
        std::string               m_host;
        uint16                    m_port;
        std::string               m_secret;
        BoundedQueue<IpcMessage>* m_inbound;

        ACE_Reactor*       m_reactor;
        IpcConnector*      m_connector;
        IpcClientHandler*  m_handler;

        volatile bool m_running;
        volatile bool m_ready;
};

#endif // AH_IPC_THREAD_H
