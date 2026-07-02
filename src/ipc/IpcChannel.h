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

/**
 * @brief Byte budget for the server-side inbound frame queue.
 *
 * Belt-and-suspenders bound on TOTAL queued inbound bytes, in addition to the
 * IPC_INBOUND_QUEUE_CAP frame count. The per-opcode size validation on the
 * inbound path already makes every accepted frame tiny (the largest, a debug
 * ECHO reply, is capped at 256 bytes), so the legitimate working set is well
 * under 256 x 256 = 64 KiB; this 256 KiB cap sits comfortably above that yet
 * forecloses the old "256 x ~1 MiB" payload-flood exposure. Drop-newest when
 * either the count cap or this byte cap is hit.
 */
static const size_t IPC_INBOUND_BYTE_CAP = 256u * 1024u;

// ---------------------------------------------------------------------------
// IpcServer  -  mangosd-side facade
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
         * @brief [SP-2] Pop one frame from the UNBOUNDED reliable lane
         *        (mutation-class frames, decision 10). Drain this to exhaustion
         *        BEFORE PopInbound each pass so a browse flood on the bounded
         *        queue can never starve a value-bearing frame.
         * @return true if a reliable frame was available.
         */
        bool PopReliable(IpcMessage& out);

        /**
         * @brief [SP-2] Discard every frame in the reliable lane; returns the
         *        count discarded. Called by the supervisor purge on child
         *        death/respawn alongside ClearInbound (stale frames are also
         *        dropped by the generation gate; this is belt-and-suspenders).
         */
        size_t ClearReliable();

        /**
         * @brief Discard every frame currently in the inbound queue.
         *
         * Called by the supervisor on child death / before respawn so frames
         * the dead child enqueued cannot survive the restart and be applied
         * under the NEXT child. Thread-safe: the reactor thread produces and
         * the world thread (which calls this) consumes; the drain is
         * serialised by the inbound queue's own internal mutex.
         *
         * @return Number of frames discarded.
         */
        size_t ClearInbound() { return m_inbound.clear(); }

        /**
         * @brief Approximate number of frames currently in the inbound queue.
         */
        size_t InboundSize() const { return m_inbound.size(); }

        /**
         * @brief Cumulative count of frames dropped due to inbound overflow.
         */
        size_t InboundDropped() const { return m_inbound.dropped(); }

        /**
         * @brief True once the handshake has completed (IPC_READY received).
         *
         * Reads a std::atomic<bool> published by the reactor thread. No handler
         * pointer is dereferenced off the reactor thread.
         */
        bool Connected() const;

        /**
         * @brief Set the per-spawn run-id sent in IPC_HELLO_ACK.
         *
         * Called by WorkerSupervisor on the supervisor thread before
         * SpawnChild(). Uses an atomic store so the value is visible to
         * the reactor thread when the handler ctor runs.
         *
         * @param runId New run-id (supervisor increments per spawn).
         */
        void SetRunId(uint32 runId);

        /**
         * @brief Set the SP-2 write-authority bit sent in IPC_HELLO_ACK.
         * Supervisor thread, before SpawnChild(); atomic store.
         */
        void SetWriteAuthority(bool on);

    private:
        BoundedQueue<IpcMessage>    m_inbound;
        IpcServerLink*              m_link;       ///< Shared link (refcounted).
        IpcThread*                  m_thread;
        ACE_Based::Thread*          m_aceThread;

        // Non-copyable.
        IpcServer(const IpcServer&);
        IpcServer& operator=(const IpcServer&);
};

// ---------------------------------------------------------------------------
// IpcClient  -  child-process-side facade
// ---------------------------------------------------------------------------

/**
 * @brief Child-side IPC facade.
 *
 * Owns an IpcClientThread (connector + client reactor) and a BoundedQueue.
 * Connect() spawns the connector thread which immediately sends IPC_HELLO
 * and waits for IPC_HELLO_ACK -> IPC_READY exchange on the reactor thread.
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

        /**
         * @brief [SP-2] Pop one frame from the UNBOUNDED reliable lane
         *        (mutation-class frames, decision 10). Drain to exhaustion
         *        BEFORE PopInbound each pass.
         * @return true if a reliable frame was available.
         */
        bool PopReliable(IpcMessage& out);

        /// True once the handshake has completed.
        bool Connected() const;

        /**
         * @brief Per-spawn run-id received in IPC_HELLO_ACK.
         *
         * Returns 0 until the handshake completes.
         */
        uint32 RunId() const;

        /**
         * @brief [SP-2] Write-authority bit received in IPC_HELLO_ACK.
         * Returns false until the handshake completes (and for a legacy ACK).
         */
        bool WriteAuthority() const;

    private:
        BoundedQueue<IpcMessage>    m_inbound;
        IpcClientLink*              m_link;       ///< Shared link (refcounted).
        IpcClientThread*            m_thread;
        ACE_Based::Thread*          m_aceThread;

        // Non-copyable.
        IpcClient(const IpcClient&);
        IpcClient& operator=(const IpcClient&);
};

#endif // AH_IPC_CHANNEL_H
