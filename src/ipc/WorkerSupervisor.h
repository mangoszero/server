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

#ifndef AH_WORKER_SUPERVISOR_H
#define AH_WORKER_SUPERVISOR_H

#include "Common.h"
#include "IpcChannel.h"
#include "IpcMessage.h"

#include <ace/Process.h>
#include <ace/Process_Manager.h>

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Heartbeat interval: send IPC_HEARTBEAT every N seconds.
 */
static constexpr uint32 WS_HEARTBEAT_INTERVAL_SEC  = 15;

/**
 * @brief Declare the child dead if no IPC_HEARTBEAT_ACK arrives within
 *        K seconds.
 */
static constexpr uint32 WS_HEARTBEAT_TIMEOUT_SEC   = 45;

/**
 * @brief Maximum exponential backoff (seconds).
 */
static constexpr uint32 WS_MAX_BACKOFF_SEC         = 60;

/**
 * @brief Connect deadline: terminate + restart a child that is spawned but
 *        never completes the IPC handshake (or disconnects and never
 *        reconnects) within this many seconds.
 *
 * The heartbeat-timeout liveness check only fires while IPC is CONNECTED, so
 * a child whose process stays alive but never connects (e.g. its DB/pool
 * setup wedged, or it crashed AFTER the parent observed the process but
 * BEFORE READY) would otherwise leave the service inactive forever without a
 * restart.  This deadline is generous: it must exceed the child's pre-READY
 * setup time (config load + DB connect + item-pool build, a few seconds), so
 * 60 s never false-triggers during the legitimate spawn->init->connect
 * window.  The anchor is the spawn time, or the last disconnect time once the
 * child has connected at least once.
 */
static constexpr uint32 WS_CONNECT_DEADLINE_SEC    = 60;

/**
 * @brief Grace period for IPC_SHUTDOWN_ACK before hard-kill.
 */
static constexpr uint32 WS_SHUTDOWN_GRACE_SEC      = 3;

/**
 * @brief Maximum application frames DrainInboundProtocol() moves into
 *        m_pendingFrames in a single call.
 *
 * The reactor thread refills the inbound BoundedQueue concurrently while
 * the supervisor thread pops it, so an unbounded pop loop could run far
 * past IPC_INBOUND_QUEUE_CAP and grow m_pendingFrames without limit. This
 * per-call cap, combined with the hard cap on m_pendingFrames itself,
 * bounds the staged-frame buffer across ticks. Protocol frames
 * (HEARTBEAT_ACK / SHUTDOWN_ACK) are always consumed inline and do not
 * count toward this cap.
 */
static constexpr uint32 WS_DRAIN_APP_PER_CALL      = 256;

/**
 * @brief Maximum browse result frames DrainInboundProtocol() moves into
 *        m_pendingFrames in a single call (I6: browse replies do not starve
 *        intent results).
 */
static constexpr uint32 WS_DRAIN_BROWSE_PER_CALL = 128;

/**
 * @brief Supervises a single child-process worker.
 *
 * Lifecycle:
 *   1. Start()  - binds IpcServer (acceptor up) THEN spawns the child.
 *                 On Windows, attaches a Job Object so the child dies
 *                 if mangosd terminates unexpectedly.
 *   2. Tick()   - called from the world loop (World::Update).
 *                 Sends IPC_HEARTBEAT + IPC_GAMETIME periodically.
 *                 Detects dead/disconnected child and restarts with
 *                 exponential backoff (1, 2, 4, ... 60 s).
 *   3. Shutdown() - sends IPC_SHUTDOWN, waits up to WS_SHUTDOWN_GRACE_SEC
 *                   for IPC_SHUTDOWN_ACK, then hard-kills the child and
 *                   stops the IPC server.
 *
 * Thread-safety note: Start()/Shutdown() are called on the main thread;
 * Tick() is called on the world thread.  IpcServer::SendFrame and
 * IpcServer::PopInbound are internally thread-safe (BoundedQueue +
 * IpcServerLink mutex).  The m_pid / m_childExited members are only
 * written from the world thread (Tick) and read from the main thread
 * (Shutdown) - the brief data-race on m_childExited is benign
 * (boolean, single-writer, Shutdown fence provided by the world thread
 * stopping before Shutdown() is called in the cascade).
 */
class WorkerSupervisor
{
    public:
        /**
         * @param name     Human-readable name used in log messages.
         * @param exePath  Path to the worker executable.
         * @param port     TCP port for the loopback IPC channel.
         * @param secret   Shared secret for the handshake.
         * @param botGuid  AH bot GUID passed as --botguid to the child.
         * @param cfgPath  Path to the ah-service config file (--config).
         */
        WorkerSupervisor(const std::string& name,
                         const std::string& exePath,
                         uint16             port,
                         const std::string& secret,
                         uint32             botGuid,
                         const std::string& cfgPath);

        ~WorkerSupervisor();

        /**
         * @brief Bind the IPC acceptor then spawn the child process.
         * @return true on success; caller should log + discard on false.
         */
        bool Start();

        /**
         * @brief Heartbeat, liveness check, and restart logic.
         *
         * Must be called approximately once per world-tick from World::Update.
         * Uses real wall-clock time internally; gametime is forwarded to the
         * child via IPC_GAMETIME alongside each heartbeat.
         *
         * @param gametime Current server game time (from World::GetGameTime()).
         */
        void Tick(uint32 gametime);

        /**
         * @brief Graceful shutdown: drain, IPC_SHUTDOWN, wait, then hard-kill.
         *
         * Called from the mangosd main thread during the shutdown cascade,
         * BEFORE delete worldThread.
         */
        void Shutdown();

        /// Access the underlying IPC channel.
        IpcServer& Channel() { return m_ipc; }

        /**
         * @brief Drain up to @p maxPerTick application frames into @p out.
         *
         * Called from World::Update after Tick(). Protocol frames
         * (IPC_HEARTBEAT_ACK, IPC_READY, IPC_SHUTDOWN_ACK) are handled
         * inside Tick() and never appear in @p out. Unknown / consumer
         * frames are buffered per-tick and handed back here for
         * World::HandleAhInbound to dispatch.
         *
         * [Finding 1] Reliable (mutation-class) frames staged in
         * m_pendingReliableFrames are drained first, to exhaustion (or
         * @p maxPerTick), and are NEVER subject to the IPC_INBOUND_QUEUE_CAP
         * clamp: only the bounded/browse lane (m_pendingFrames) can lose
         * frames under a flood.
         *
         * @param out        Destination; frames are appended (not cleared).
         * @param maxPerTick Maximum frames to move; bounds per-tick work.
         */
        void DrainInbound(std::vector<IpcMessage>& out, size_t maxPerTick);

        /**
         * @brief Returns true when the AH service child is connected and healthy.
         *
         * Evaluates: m_started && !m_childExited && IPC channel connected
         * (handshake + IPC_READY complete) && last heartbeat-ack is fresh
         * (within WS_HEARTBEAT_TIMEOUT_SEC seconds) && the child reported
         * itself operationally healthy in its latest heartbeat-ack
         * (m_childHealthy; OPEN-1). The last term closes the runtime-stall
         * hole: a child whose snapshot/brain degrades keeps heartbeating but
         * stops emitting, so without this it would suppress the in-process bot
         * and NO bot would run.
         *
         * Callers on the world thread may use this to stand down the in-process
         * AH bot while the out-of-process service is active.  No locking is
         * needed: all members read here are written only on the world thread
         * (Tick()), and World::Update calls Tick() before consulting this.
         *
         * @return true if the service is active; false during startup, backoff,
         *         crash, or heartbeat-timeout.
         */
        bool ServiceActive() const;

        /**
         * @brief Cumulative inbound frames dropped due to queue overflow.
         *
         * A rising value indicates the reactor is producing faster than
         * the world thread is consuming; World::Update will log a warning.
         *
         * Folds together two producer-enforced drop sources: the inbound
         * BoundedQueue overflow (reactor thread, drop-newest) and the
         * m_pendingFrames hard-cap overflow (supervisor thread, drop-newest;
         * see @ref AppDropped). Either source signals back-pressure.
         */
        size_t InboundDropped() const
        {
            return m_ipc.InboundDropped() + m_appDropped;
        }

        /**
         * @brief Cumulative application frames dropped at the m_pendingFrames
         *        hard cap.
         *
         * The pending-frame buffer is producer-bounded at
         * IPC_INBOUND_QUEUE_CAP (drop-newest, matching the inbound queue
         * policy). Dropped intents are safe: they are idempotent and
         * re-validated, and the child re-emits them on its cadence.
         */
        size_t AppDropped() const { return m_appDropped; }

    private:
        /// Spawn the child process (called from Start() and on restart).
        bool SpawnChild();

        /**
         * @brief Reap the dead child's process-table entry / OS handle.
         *
         * The same ACE_Process is reused across restarts, so a flapping child
         * would otherwise leak zombies (Linux) or HANDLEs (Windows). This
         * waits (non-blocking) on the recorded pid via ACE_Process_Manager so
         * the kernel releases the entry before the next SpawnChild(). Safe to
         * call when there is nothing to reap. Clears @p pid to ACE_INVALID_PID.
         *
         * @param pid The child pid to reap (by value; caller clears its copy).
         */
        void ReapChild(pid_t pid);

        /**
         * @brief Discard all staged (not-yet-applied) application frames.
         *
         * Called on child-exit detection (process exit / heartbeat timeout)
         * and before each (re)spawn. A reconnecting child must never replay
         * the dead child's stale, partially-applied batch, which would
         * over-post against the resumed in-process AuctionHouseBot.
         */
        void ClearStagedFrames();

        /**
         * @brief Drain the IPC inbound queue into m_pendingFrames /
         *        m_pendingReliableFrames.
         *
         * Handles protocol frames (IPC_HEARTBEAT_ACK, IPC_READY,
         * IPC_SHUTDOWN_ACK) immediately. Reliable (mutation-class) frames are
         * staged into the unbounded m_pendingReliableFrames; all other
         * (bounded/browse) frames are staged into the capped m_pendingFrames.
         * Both are handed to the public DrainInbound() for
         * World::HandleAhInbound.
         */
        void DrainInboundProtocol();

        std::string  m_name;
        std::string  m_exePath;
        uint16       m_port;
        std::string  m_secret;
        uint32       m_botGuid;
        std::string  m_cfgPath;
        uint32       m_runId;       ///< Per-spawn run-id; incremented on every spawn.

        IpcServer    m_ipc;

        /// ACE process handle for the spawned child.
        ACE_Process  m_process;
        pid_t        m_pid;

        /// Wall-clock timestamps (seconds since epoch) for heartbeat logic.
        time_t       m_lastHeartbeatSent;
        time_t       m_lastHeartbeatAck;

        /// Connect-deadline tracking (C2).
        /// Anchor for the connect deadline: set on each spawn and re-armed to
        /// "now" each time the child transitions CONNECTED -> disconnected.
        time_t       m_connectAnchor;
        /// True once the IPC channel has reported Connected() for this spawn;
        /// reset to false on every (re)spawn.
        bool         m_everConnected;

        /// Restart backoff state.
        uint32       m_backoffSec;      ///< Current backoff delay.
        time_t       m_nextRetryAt;     ///< When to attempt next restart.
        uint32       m_failCount;       ///< Consecutive failure count.

        bool         m_started;         ///< true once Start() succeeded.
        bool         m_childExited;     ///< true when the child is gone.

        /// OPEN-1: child-reported RUNTIME operational health, taken from the
        /// 1-byte IPC_HEARTBEAT_ACK body. Initialised false and reset to false
        /// on every child death/disconnect, so a freshly-(re)connected child
        /// must re-prove health before it can suppress the in-process bot. A
        /// child whose snapshot/brain degrades at runtime keeps heartbeating
        /// but reports unhealthy here; ServiceActive() then returns false and
        /// the in-process bot resumes WITHOUT restarting the still-connected
        /// child (it stands back down once the child reports healthy again).
        bool         m_childHealthy;

        /// BOUNDED/browse application frames buffered by DrainInboundProtocol
        /// each tick. Producer-enforced HARD CAP of IPC_INBOUND_QUEUE_CAP
        /// across ticks: DrainInboundProtocol() drops newest frames
        /// (incrementing m_appDropped) rather than growing past the cap, so
        /// the public DrainInbound() never has to abort to enforce the
        /// bound. Holds ONLY non-reliable frames; reliable (mutation-class)
        /// frames are staged separately in m_pendingReliableFrames and are
        /// never subject to this cap (Finding 1 / decision 10).
        std::vector<IpcMessage> m_pendingFrames;

        /// [SP-2 decision 10 / Finding 1] UNBOUNDED staging container for
        /// reliable (mutation-class) frames popped via IpcServer::PopReliable
        /// (IPC_PLAYER_RESULT / IPC_RESOLVE_APPLY / IPC_INTENT_SELL / ...).
        /// Kept SEPARATE from m_pendingFrames so the IPC_INBOUND_QUEUE_CAP
        /// drop-newest clamp applied to that container in DrainInbound() can
        /// never truncate a reliable frame, no matter how many are staged in
        /// one drain interval. DrainInbound() drains this container to
        /// exhaustion (or the per-tick budget) BEFORE touching
        /// m_pendingFrames, preserving the "reliable frames never starve
        /// behind a browse flood" ordering the bounded path already had.
        std::vector<IpcMessage> m_pendingReliableFrames;

        /// Application frames dropped at the m_pendingFrames hard cap.
        size_t                  m_appDropped;

#ifdef _WIN32
        HANDLE       m_jobObject;       ///< Job Object for orphan guard.
#endif

        // Non-copyable.
        WorkerSupervisor(const WorkerSupervisor&);
        WorkerSupervisor& operator=(const WorkerSupervisor&);
};

#endif // AH_WORKER_SUPERVISOR_H
