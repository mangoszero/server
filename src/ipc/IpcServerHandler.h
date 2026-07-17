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

#ifndef AH_IPC_SERVER_HANDLER_H
#define AH_IPC_SERVER_HANDLER_H

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
 * @brief Handshake state for the server side of the IPC connection.
 */
enum IpcServerHandshakeState
{
    IPC_SRV_WAIT_HELLO  = 0,    ///< Waiting for IPC_HELLO from child
    IPC_SRV_WAIT_READY  = 1,    ///< Waiting for IPC_READY from child
    IPC_SRV_LIVE        = 2,    ///< Handshake complete; channel is live
    IPC_SRV_CLOSING     = 3,    ///< Shutting down
};

/**
 * @brief Server-side IPC connection handler.
 *
 * Owns the accepted socket and runs a receive loop on the server thread: bytes
 * are reassembled into IpcMessage frames and dispatched through the handshake
 * state machine, then application frames are pushed onto the shared inbound
 * queue (or the reliable lane). SendFrame() writes directly to the socket under
 * a mutex, so it is safe to call from the world thread via the facade.
 *
 * Handshake (server side):
 *   recv IPC_HELLO -> verify proto + secret -> send IPC_HELLO_ACK
 *   recv IPC_READY -> mark live
 *
 * This is a 1-connection server; the owning thread guarantees only one handler
 * is active at a time (single-owner guard on the link).
 */
class IpcServerHandler : public std::enable_shared_from_this<IpcServerHandler>
{
    public:
        IpcServerHandler(IpcSocket&& sock,
                         BoundedQueue<IpcMessage>* inbound,
                         const std::string& secret,
                         IpcServerLink* link,
                         uint32 runId,
                         uint8 writeAuthority);
        ~IpcServerHandler();

        /// Run the blocking receive loop until the peer closes, an error occurs,
        /// or @p stop is set. Clears the link on exit.
        void ReceiveLoop(std::atomic<bool>& stop);

        /**
         * @brief Encode and send @p msg on the socket. Thread-safe.
         * @return 0 on success, -1 on failure.
         */
        int SendFrame(const IpcMessage& msg);

        bool IsLive() const { return m_state == IPC_SRV_LIVE; }
        bool IsClosing() const { return m_closing.load(std::memory_order_acquire); }

    private:
        IpcSocket                   m_sock;
        std::mutex                  m_sendMtx;

        ByteBuffer                  m_recvBuf;
        IpcServerHandshakeState     m_state;
        std::string                 m_secret;
        uint32                      m_runId;
        uint8                       m_writeAuthority;

        BoundedQueue<IpcMessage>*   m_inbound;
        IpcServerLink*              m_link;

        std::atomic<bool>           m_closing;

        int  ProcessFrame(const IpcMessage& msg);
        void CompactRecvBuf();
        bool RejectOversizeForOp();
        void OnClose();
};

#endif // AH_IPC_SERVER_HANDLER_H
