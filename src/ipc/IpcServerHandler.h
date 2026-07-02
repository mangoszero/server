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

#include <ace/Svc_Handler.h>
#include <ace/SOCK_Stream.h>
#include <ace/SOCK_Acceptor.h>
#include <ace/Acceptor.h>
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>
#include <ace/Message_Block.h>

#include "Common.h"
#include "Utilities/ByteBuffer.h"
#include "IpcMessage.h"
#include "BoundedQueue.h"
#include "IpcLink.h"

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
 * @brief Server-side IPC socket handler.
 *
 * Modelled on WorldSocket. Registered with the reactor by IpcThread's
 * ACE_Acceptor. On handle_input, received bytes are appended to a
 * reassembly ByteBuffer and looped through IpcMessage::Decode; complete
 * frames are pushed into the shared inbound BoundedQueue.
 *
 * The handshake sequence (server side):
 *   recv IPC_HELLO  -> verify proto + secret -> send IPC_HELLO_ACK
 *   recv IPC_READY  -> mark live, log "AH service READY"
 *
 * This is a 1-connection server; only one IpcServerHandler is ever active.
 */
class IpcServerHandler : public ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_NULL_SYNCH>
{
    public:
        typedef ACE_Thread_Mutex LockType;

        // --- static injection used by IpcThread before the acceptor runs ---

        /**
         * @brief Provide the shared inbound queue, secret and link before any
         *        connection is accepted. Called once by IpcThread::run on the
         *        reactor thread before the acceptor fires open().
         *
         * @param inbound Inbound queue shared with the IpcServer facade.
         * @param secret  Shared secret validated against IPC_HELLO.
         * @param link    Coupling object (outbound queue + liveness + handler
         *                slot) shared with the facade. May be null in legacy
         *                paths but is always set by IpcThread.
         */
        static void SetPendingContext(BoundedQueue<IpcMessage>* inbound,
                                      const std::string& secret,
                                      IpcServerLink* link);

        static void SetPendingRunId(uint32 runId);

        /// Set the write-authority bit sent in IPC_HELLO_ACK (supervisor
        /// thread, before SpawnChild). SP-2: the worker only becomes the
        /// auction write-authority when this is true.
        static void SetPendingWriteAuthority(bool on);

        // --- ACE framework callbacks ---

        IpcServerHandler();
        virtual ~IpcServerHandler();

        /// Called by the acceptor when a new connection is accepted.
        int open(void* acceptor = 0) override;

        /// Called when the close(u_long) hook fires.
        int close(u_long flags = 0) override;

        /// Called by the reactor when data is available to read.
        int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE) override;

        /// Called by the reactor when the socket is ready to write.
        int handle_output(ACE_HANDLE = ACE_INVALID_HANDLE) override;

        /// Called on connection close or error.
        int handle_close(
                ACE_HANDLE = ACE_INVALID_HANDLE,
                ACE_Reactor_Mask = ACE_Event_Handler::ALL_EVENTS_MASK) override;

        // --- Public interface used by IpcServer facade ---

        /**
         * @brief Encode and queue @p msg for send; schedule WRITE_MASK.
         * @return 0 on success, -1 on failure.
         */
        int SendFrame(const IpcMessage& msg);

        /// True once the handshake is complete.
        bool IsLive() const;

        /// True once handle_close has been called.
        bool IsClosing() const;

    private:
        // Output buffer and lock (mirrors WorldSocket pattern).
        LockType                    m_outLock;
        ACE_Message_Block*          m_outBuffer;
        static const size_t         k_outBufSize = 65536;

        // Reassembly buffer for incoming raw bytes.
        ByteBuffer                  m_recvBuf;

        // Handshake state.
        IpcServerHandshakeState     m_state;

        // Shared secret expected in IPC_HELLO.
        std::string                 m_secret;

        uint32                      m_runId;  ///< Per-spawn run-id from handshake.
        uint8                       m_writeAuthority;  ///< Snapshot at ctor.

        // Inbound queue shared with IpcServer facade.
        BoundedQueue<IpcMessage>*   m_inbound;

        // Coupling object shared with the facade (outbound queue + liveness +
        // reactor-thread handler slot). Owned via reference count.
        IpcServerLink*              m_link;

        // Closing flag. Only touched on the reactor thread (under m_outLock for
        // the close transition); the facade never reads it.
        bool                        m_closing;

        // True only for the handler that won the single-owner test-and-set in
        // open(). A handler refused because another is already active never
        // sets this, so its handle_close() does NOT clear the owner's
        // handlerActive flag or its handler slot. Reactor-thread only.
        bool                        m_isOwner;

        // --- static context set before first accept (reactor thread only) ---
        static BoundedQueue<IpcMessage>*  s_pendingInbound;
        static std::string                s_pendingSecret;
        static IpcServerLink*             s_pendingLink;
        static std::atomic<uint32>        s_pendingRunId;
        static std::atomic<uint8>         s_pendingWriteAuthority;

        // Helpers.
        int ProcessFrame(const IpcMessage& msg);
        int FlushOutBuffer();
        void CompactRecvBuf();

        /**
         * @brief PF2-C: reject an oversize-for-op frame at header parse.
         *
         * Precondition: at least 8 header bytes are buffered at m_recvBuf's
         * current read position. Peeks the opcode + declared body length
         * WITHOUT consuming any bytes and, for a KNOWN opcode, returns true
         * when the declared length exceeds that opcode's allowed body size -
         * so the caller can close the connection before the body is buffered/
         * reassembled. Unknown opcodes and in-range lengths return false (the
         * normal Decode()/ProcessFrame() path handles them).
         *
         * @return true if the frame must be rejected (close the connection).
         */
        bool RejectOversizeForOp();
};

typedef ACE_Acceptor<IpcServerHandler, ACE_SOCK_ACCEPTOR> IpcAcceptor;

#endif // AH_IPC_SERVER_HANDLER_H
