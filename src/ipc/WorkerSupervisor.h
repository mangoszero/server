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

#include <ace/Process.h>
#include <ace/Process_Manager.h>

#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Heartbeat interval: send IPC_HEARTBEAT every N seconds.
 */
static const uint32 WS_HEARTBEAT_INTERVAL_SEC  = 15;

/**
 * @brief Declare the child dead if no IPC_HEARTBEAT_ACK arrives within K seconds.
 */
static const uint32 WS_HEARTBEAT_TIMEOUT_SEC   = 45;

/**
 * @brief Maximum exponential backoff (seconds).
 */
static const uint32 WS_MAX_BACKOFF_SEC         = 60;

/**
 * @brief Grace period for IPC_SHUTDOWN_ACK before hard-kill.
 */
static const uint32 WS_SHUTDOWN_GRACE_SEC      = 10;

/**
 * @brief Supervises a single child-process worker.
 *
 * Lifecycle:
 *   1. Start()  — binds IpcServer (acceptor up) THEN spawns the child.
 *                 On Windows, attaches a Job Object so the child dies
 *                 if mangosd terminates unexpectedly.
 *   2. Tick()   — called from the world loop (World::Update).
 *                 Sends IPC_HEARTBEAT + IPC_GAMETIME periodically.
 *                 Detects dead/disconnected child and restarts with
 *                 exponential backoff (1, 2, 4, … 60 s).
 *   3. Shutdown() — sends IPC_SHUTDOWN, waits up to WS_SHUTDOWN_GRACE_SEC
 *                   for IPC_SHUTDOWN_ACK, then hard-kills the child and
 *                   stops the IPC server.
 *
 * Thread-safety note: Start()/Shutdown() are called on the main thread;
 * Tick() is called on the world thread.  IpcServer::SendFrame and
 * IpcServer::PopInbound are internally thread-safe (BoundedQueue +
 * IpcServerLink mutex).  The m_pid / m_childExited members are only
 * written from the world thread (Tick) and read from the main thread
 * (Shutdown) — the brief data-race on m_childExited is benign
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
         */
        WorkerSupervisor(const std::string& name,
                         const std::string& exePath,
                         uint16             port,
                         const std::string& secret,
                         uint32             botGuid);

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

        /// Access the underlying IPC channel (Task 6 inbound drain).
        IpcServer& Channel() { return m_ipc; }

    private:
        /// Spawn the child process (called from Start() and on restart).
        bool SpawnChild();

        /// Drain inbound queue, handling IPC_HEARTBEAT_ACK and other frames.
        void DrainInbound();

        std::string  m_name;
        std::string  m_exePath;
        uint16       m_port;
        std::string  m_secret;
        uint32       m_botGuid;

        IpcServer    m_ipc;

        /// ACE process handle for the spawned child.
        ACE_Process  m_process;
        pid_t        m_pid;

        /// Wall-clock timestamps (seconds since epoch) for heartbeat logic.
        time_t       m_lastHeartbeatSent;
        time_t       m_lastHeartbeatAck;

        /// Restart backoff state.
        uint32       m_backoffSec;      ///< Current backoff delay.
        time_t       m_nextRetryAt;     ///< When to attempt next restart.
        uint32       m_failCount;       ///< Consecutive failure count.

        bool         m_started;         ///< true once Start() succeeded.
        bool         m_childExited;     ///< true when we know the child is gone.

#ifdef _WIN32
        HANDLE       m_jobObject;       ///< Job Object for orphan guard.
#endif

        // Non-copyable.
        WorkerSupervisor(const WorkerSupervisor&);
        WorkerSupervisor& operator=(const WorkerSupervisor&);
};

#endif // AH_WORKER_SUPERVISOR_H
