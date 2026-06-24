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

#include <ace/Svc_Handler.h>
#include <ace/SOCK_Stream.h>
#include <ace/SOCK_Connector.h>
#include <ace/Connector.h>
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>
#include <ace/Message_Block.h>

#include "Common.h"
#include "Utilities/ByteBuffer.h"
#include "IpcMessage.h"
#include "BoundedQueue.h"
#include "IpcLink.h"

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
 * @brief Client-side IPC socket handler.
 *
 * Modelled on WorldSocket / IpcServerHandler. Used by ACE_Connector on the
 * child (ah-service) side. On open(), immediately sends IPC_HELLO to begin
 * the handshake.
 *
 * Handshake sequence (client side):
 *   open()           -> send IPC_HELLO { proto, pid, secret }
 *   recv IPC_HELLO_ACK -> send IPC_READY
 *   (channel is now live)
 */
class IpcClientHandler : public ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_NULL_SYNCH>
{
    public:
        typedef ACE_Thread_Mutex LockType;

        // --- static injection before connector fires ---

        /**
         * @brief Set shared secret, inbound queue and link before Connect() is
         *        called. Invoked on the reactor thread before open() fires.
         */
        static void SetPendingContext(BoundedQueue<IpcMessage>* inbound,
                                      const std::string& secret,
                                      IpcClientLink* link);

        // --- ACE framework callbacks ---

        IpcClientHandler();
        virtual ~IpcClientHandler();

        /// Called by ACE_Connector when the connection is established.
        int open(void* connector = 0) override;

        int close(u_long flags = 0) override;

        int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE) override;
        int handle_output(ACE_HANDLE = ACE_INVALID_HANDLE) override;
        int handle_close(
                ACE_HANDLE = ACE_INVALID_HANDLE,
                ACE_Reactor_Mask = ACE_Event_Handler::ALL_EVENTS_MASK) override;

        // --- Public interface used by IpcClient facade ---

        /**
         * @brief Encode and queue @p msg for send.
         * @return 0 on success, -1 on failure.
         */
        int SendFrame(const IpcMessage& msg);

        /// True once the handshake is complete.
        bool IsLive() const;

        /// True once handle_close has been called.
        bool IsClosing() const;

        /// Expose handshake state for the IpcClient facade polling loop.
        IpcClientHandshakeState GetState() const { return m_state; }

    private:
        LockType                    m_outLock;
        ACE_Message_Block*          m_outBuffer;
        static const size_t         k_outBufSize = 65536;

        ByteBuffer                  m_recvBuf;

        IpcClientHandshakeState     m_state;

        std::string                 m_secret;
        BoundedQueue<IpcMessage>*   m_inbound;
        IpcClientLink*              m_link;
        bool                        m_closing;

        // Static context (reactor thread only).
        static BoundedQueue<IpcMessage>*  s_pendingInbound;
        static std::string                s_pendingSecret;
        static IpcClientLink*             s_pendingLink;

        int SendHello();
        int ProcessFrame(const IpcMessage& msg);
        int FlushOutBuffer();
        void CompactRecvBuf();
};

typedef ACE_Connector<IpcClientHandler, ACE_SOCK_CONNECTOR> IpcConnector;

#endif // AH_IPC_CLIENT_HANDLER_H
