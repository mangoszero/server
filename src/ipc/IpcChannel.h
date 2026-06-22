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

#ifndef AH_IPC_CHANNEL_H
#define AH_IPC_CHANNEL_H

#include "Common.h"
#include "IpcMessage.h"
#include "IpcThread.h"
#include "IpcServerHandler.h"
#include "IpcClientHandler.h"
#include "IpcLink.h"
#include "BoundedQueue.h"
#include "Threading/Threading.h"

#include <string>
#include <memory>

/**
 * @brief Capacity of the inbound frame queue on each side.
 */
static const size_t IPC_INBOUND_QUEUE_CAP = 256;

// ---------------------------------------------------------------------------
// IpcServer  —  mangosd-side facade
// ---------------------------------------------------------------------------

/**
 * @brief mangosd-side IPC facade.
 *
 * Owns an IpcThread (acceptor + server reactor) and a BoundedQueue for
 * frames received from the child. The child connects and completes the
 * handshake autonomously on the reactor thread; IpcServer exposes a
 * poll-style interface to the game server loop.
 *
 * Lifetime:
 *   IpcServer srv;
 *   srv.Start("127.0.0.1", 7878, "mysecret");
 *   // ... game loop ...
 *   if (srv.PopInbound(msg)) { ... }
 *   srv.Stop();
 */
class IpcServer
{
    public:
        IpcServer();
        ~IpcServer();

        /**
         * @brief Start the server: bind, listen, spawn reactor thread.
         * @return true on success.
         */
        bool Start(const char* host, uint16 port, const std::string& secret);

        /// Stop the reactor thread and close the acceptor.
        void Stop();

        /**
         * @brief Send a frame to the connected child.
         * @return true on success, false if not connected or error.
         */
        bool SendFrame(const IpcMessage& msg);

        /**
         * @brief Pop one inbound frame (non-blocking).
         * @return true if a frame was available.
         */
        bool PopInbound(IpcMessage& out);

        /**
         * @brief True once the handshake has completed (IPC_READY received).
         *
         * Reads a std::atomic<bool> published by the reactor thread. No handler
         * pointer is dereferenced off the reactor thread.
         */
        bool Connected() const;

    private:
        BoundedQueue<IpcMessage>    m_inbound;
        IpcServerLink*              m_link;       ///< Shared with the reactor thread (refcounted).
        IpcThread*                  m_thread;
        ACE_Based::Thread*          m_aceThread;

        // Non-copyable.
        IpcServer(const IpcServer&);
        IpcServer& operator=(const IpcServer&);
};

// ---------------------------------------------------------------------------
// IpcClient  —  child-process-side facade
// ---------------------------------------------------------------------------

/**
 * @brief Child-side IPC facade.
 *
 * Owns an IpcClientThread (connector + client reactor) and a BoundedQueue.
 * Connect() spawns the connector thread which immediately sends IPC_HELLO
 * and waits for IPC_HELLO_ACK → IPC_READY exchange on the reactor thread.
 *
 * Lifetime:
 *   IpcClient cli;
 *   cli.Connect("127.0.0.1", 7878, "mysecret");
 *   // poll Connected() until true
 *   cli.SendFrame(echoMsg);
 *   cli.Stop();
 */
class IpcClient
{
    public:
        IpcClient();
        ~IpcClient();

        /**
         * @brief Connect to the server and start the client reactor thread.
         * @return true if the connection attempt was initiated.
         */
        bool Connect(const char* host, uint16 port, const std::string& secret);

        /// Stop the client reactor thread.
        void Stop();

        /**
         * @brief Send a frame to the server.
         * @return true on success.
         */
        bool SendFrame(const IpcMessage& msg);

        /**
         * @brief Pop one inbound frame (non-blocking).
         * @return true if a frame was available.
         */
        bool PopInbound(IpcMessage& out);

        /// True once the handshake has completed.
        bool Connected() const;

    private:
        BoundedQueue<IpcMessage>    m_inbound;
        IpcClientLink*              m_link;       ///< Shared with the reactor thread (refcounted).
        IpcClientThread*            m_thread;
        ACE_Based::Thread*          m_aceThread;

        // Non-copyable.
        IpcClient(const IpcClient&);
        IpcClient& operator=(const IpcClient&);
};

#endif // AH_IPC_CHANNEL_H
