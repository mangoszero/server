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

#ifndef AH_IPC_CLIENT_HANDLER_H
#define AH_IPC_CLIENT_HANDLER_H

#include "Common.h"
#include "Utilities/ByteBuffer.h"
#include "IpcMessage.h"
#include "BoundedQueue.h"
#include "IpcLink.h"
#include "IpcSocket.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

/**
 * @brief Handshake state for the client (child) side of the IPC connection.
 */
enum IpcClientHandshakeState
{
    IPC_CLI_WAIT_CONNECT    = 0,    ///< Not yet connected
    IPC_CLI_WAIT_HELLO_ACK  = 1,    ///< Sent IPC_HELLO; waiting for ACK
    IPC_CLI_WAIT_SEND_READY = 2,    ///< Got IPC_HELLO_ACK; sending IPC_READY
    IPC_CLI_LIVE            = 3,    ///< Handshake complete
    IPC_CLI_CLOSING         = 4,    ///< Shutting down
};

/**
 * @brief Client-side IPC connection handler (child / ah-service side).
 *
 * Owns the connected socket. SendHello() begins the handshake; ReceiveLoop()
 * reassembles frames and dispatches them until close. Symmetric to
 * IpcServerHandler.
 *
 * Handshake (client side):
 *   SendHello()        -> send IPC_HELLO { proto, pid, secret }
 *   recv IPC_HELLO_ACK -> send IPC_READY (channel now live)
 */
class IpcClientHandler : public std::enable_shared_from_this<IpcClientHandler>
{
    public:
        IpcClientHandler(IpcSocket&& sock,
                         BoundedQueue<IpcMessage>* inbound,
                         const std::string& secret,
                         IpcClientLink* link);
        ~IpcClientHandler();

        /// Send IPC_HELLO to begin the handshake. Call before ReceiveLoop.
        int SendHello();

        /// Run the blocking receive loop until close / stop. Clears the link.
        void ReceiveLoop(std::atomic<bool>& stop);

        /**
         * @brief Encode and send @p msg on the socket. Thread-safe.
         * @return 0 on success, -1 on failure.
         */
        int SendFrame(const IpcMessage& msg);

        /// True iff a frame with this opcode + body length is acceptable to
        /// stage (per-opcode size rule). Shared with tests.
        static bool InboundFrameAcceptable(uint16 op, uint32 bodyLen);

        bool IsLive() const { return m_state == IPC_CLI_LIVE; }
        bool IsClosing() const { return m_closing.load(std::memory_order_acquire); }
        IpcClientHandshakeState GetState() const { return m_state; }

    private:
        IpcSocket                   m_sock;
        std::mutex                  m_sendMtx;

        ByteBuffer                  m_recvBuf;
        IpcClientHandshakeState     m_state;
        std::string                 m_secret;
        BoundedQueue<IpcMessage>*   m_inbound;
        IpcClientLink*              m_link;

        std::atomic<bool>           m_closing;

        int  ProcessFrame(const IpcMessage& msg);
        void CompactRecvBuf();
        void OnClose();
};

#endif // AH_IPC_CLIENT_HANDLER_H
