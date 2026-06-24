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

/**
 * @file WorkerSupervisor.cpp
 * @brief Spawn, heartbeat, restart, orphan-guard, and graceful shutdown for
 *        the ah-service child process.
 */

#include "WorkerSupervisor.h"
#include "Log.h"
#include "IpcOpcodes.h"

#include <ace/OS_NS_time.h>

#include <ctime>
#include <cstdio>

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

WorkerSupervisor::WorkerSupervisor(const std::string& name,
                                   const std::string& exePath,
                                   uint16             port,
                                   const std::string& secret,
                                   uint32             botGuid,
                                   const std::string& cfgPath)
    : m_name(name)
    , m_exePath(exePath)
    , m_port(port)
    , m_secret(secret)
    , m_botGuid(botGuid)
    , m_cfgPath(cfgPath)
    , m_pid(ACE_INVALID_PID)
    , m_lastHeartbeatSent(0)
    , m_lastHeartbeatAck(0)
    , m_backoffSec(1)
    , m_nextRetryAt(0)
    , m_failCount(0)
    , m_started(false)
    , m_childExited(true)
    , m_runId(0)
    , m_appDropped(0)
#ifdef _WIN32
    , m_jobObject(NULL)
#endif
{
}

WorkerSupervisor::~WorkerSupervisor()
{
#ifdef _WIN32
    if (m_jobObject != NULL)
    {
        CloseHandle(m_jobObject);
        m_jobObject = NULL;
    }
#endif
}

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------

bool WorkerSupervisor::Start()
{
    // Loud warning (but do NOT refuse to start) when the shared secret is
    // empty or the well-known default: the loopback IPC channel would accept
    // any local process that knows the trivial secret.
    if (m_secret.empty() || m_secret == "changeme")
    {
        sLog.outError("[WorkerSupervisor:%s] SECURITY: AH service is using an"
                      " INSECURE DEFAULT secret (%s) - set a strong unique"
                      " secret in the config; any local process can otherwise"
                      " impersonate the AH worker on 127.0.0.1:%u",
                      m_name.c_str(),
                      m_secret.empty() ? "empty" : "\"changeme\"",
                      static_cast<unsigned>(m_port));
    }

    // Bind the IPC acceptor FIRST so it is listening before the child connects.
    if (!m_ipc.Start("127.0.0.1", m_port, m_secret))
    {
        sLog.outError("[WorkerSupervisor:%s] IpcServer::Start failed on port %u",
                      m_name.c_str(), m_port);
        return false;
    }

    sLog.outString("[WorkerSupervisor:%s] IPC acceptor bound on 127.0.0.1:%u",
                   m_name.c_str(), m_port);

    if (!SpawnChild())
    {
        m_ipc.Stop();
        return false;
    }

    m_started           = true;
    // Treat start as the first ack to give the child time to connect.
    m_lastHeartbeatAck  = time(nullptr);
    m_lastHeartbeatSent = time(nullptr);
    return true;
}

// ---------------------------------------------------------------------------
// SpawnChild
// ---------------------------------------------------------------------------

bool WorkerSupervisor::SpawnChild()
{
    // Build command line.
    // ah-service --port <p> --secret <s> --botguid <g>
    ACE_Process_Options opts;

    // Buffer large enough for a typical path + args.
    char cmdBuf[2048];
    snprintf(cmdBuf, sizeof(cmdBuf),
             "\"%s\" --port %u --secret \"%s\" --botguid %u --config \"%s\"",
             m_exePath.c_str(),
             static_cast<unsigned>(m_port),
             m_secret.c_str(),
             static_cast<unsigned>(m_botGuid),
             m_cfgPath.c_str());

    // Build a SEPARATE redacted command line for logging: the real secret must
    // never reach the log. Only cmdBuf (with the real secret) goes to ACE.
    char cmdLog[2048];
    snprintf(cmdLog, sizeof(cmdLog),
             "\"%s\" --port %u --secret \"***\" --botguid %u --config \"%s\"",
             m_exePath.c_str(),
             static_cast<unsigned>(m_port),
             static_cast<unsigned>(m_botGuid),
             m_cfgPath.c_str());

    if (opts.command_line("%s", cmdBuf) != 0)
    {
        sLog.outError("[WorkerSupervisor:%s]"
                      " ACE_Process_Options::command_line failed",
                      m_name.c_str());
        return false;
    }

#ifdef _WIN32
    // CREATE_NEW_CONSOLE: child gets its own console window.
    // The child will manage show/hide in Task 5.
    opts.creation_flags(CREATE_NEW_CONSOLE);
#endif

    // Assign a new per-spawn run-id (monotonically increasing; 0 is never used).
    ++m_runId;
    m_ipc.SetRunId(m_runId);
    sLog.outString("[WorkerSupervisor:%s] assigned run-id %u",
                   m_name.c_str(), static_cast<unsigned>(m_runId));

    // Spawn via the singleton ACE_Process_Manager, passing our ACE_Process
    // object so we can retrieve the process HANDLE for the Job Object.
    pid_t pid = ACE_Process_Manager::instance()->spawn(&m_process, opts);

    if (pid == ACE_INVALID_PID)
    {
        sLog.outError("[WorkerSupervisor:%s] ACE_Process_Manager::spawn"
                      " failed (cmd: %s)",
                      m_name.c_str(), cmdLog);
        return false;
    }

    m_pid         = pid;
    m_childExited = false;

    // Defensive: start the new child with an empty staged buffer so a frame
    // that slipped in from the prior run-id can never be applied as if it
    // came from this child. Exit detection already clears on the way down;
    // this also covers the very first Start() and any race on respawn.
    ClearStagedFrames();

    sLog.outString("[WorkerSupervisor:%s] child spawned (pid=%u, cmd: %s)",
                   m_name.c_str(), static_cast<unsigned>(m_pid), cmdLog);

#ifdef _WIN32
    // ------------------------------------------------------------------
    // Orphan guard: Job Object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
    // If mangosd crashes, the OS closes the job handle and kills the child.
    // ------------------------------------------------------------------

    // Close any previous job object first (restart path).
    if (m_jobObject != NULL)
    {
        CloseHandle(m_jobObject);
        m_jobObject = NULL;
    }

    m_jobObject = CreateJobObjectA(NULL, NULL);
    if (m_jobObject == NULL)
    {
        sLog.outError("[WorkerSupervisor:%s] CreateJobObject failed"
                      " (err=%lu) - orphan guard disabled",
                      m_name.c_str(), GetLastError());
    }
    else
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
        memset(&jeli, 0, sizeof(jeli));
        jeli.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

        if (!SetInformationJobObject(m_jobObject,
                                     JobObjectExtendedLimitInformation,
                                     &jeli,
                                     sizeof(jeli)))
        {
            sLog.outError("[WorkerSupervisor:%s] SetInformationJobObject"
                          " failed (err=%lu) - orphan guard disabled",
                          m_name.c_str(), GetLastError());
            CloseHandle(m_jobObject);
            m_jobObject = NULL;
        }
        else
        {
            // ACE_Process::gethandle() returns the Windows HANDLE for child.
            HANDLE hChild = m_process.gethandle();
            if (hChild == INVALID_HANDLE_VALUE || hChild == NULL)
            {
                sLog.outError("[WorkerSupervisor:%s] ACE_Process::gethandle()"
                              " returned invalid - orphan guard disabled",
                              m_name.c_str());
                CloseHandle(m_jobObject);
                m_jobObject = NULL;
            }
            else if (!AssignProcessToJobObject(m_jobObject, hChild))
            {
                sLog.outError("[WorkerSupervisor:%s] AssignProcessToJobObject"
                              " failed (err=%lu) - orphan guard disabled",
                              m_name.c_str(), GetLastError());
                CloseHandle(m_jobObject);
                m_jobObject = NULL;
            }
            else
            {
                sLog.outString("[WorkerSupervisor:%s] orphan guard"
                               " (Job Object) armed",
                               m_name.c_str());
            }
        }
    }
#else
    // Linux orphan guard: implemented CHILD-SIDE. The ah-service installs
    // prctl(PR_SET_PDEATHSIG, SIGTERM) at startup via
    // Console_InstallParentDeathGuard(), so the kernel signals the child when
    // this (the parent) dies. Nothing to arm on the supervisor side here.
#endif

    return true;
}

// ---------------------------------------------------------------------------
// ServiceActive
// ---------------------------------------------------------------------------

bool WorkerSupervisor::ServiceActive() const
{
    if (!m_started || m_childExited || !m_ipc.Connected())
    {
        return false;
    }
    const time_t age = time(nullptr) - m_lastHeartbeatAck;
    return age <= static_cast<time_t>(WS_HEARTBEAT_TIMEOUT_SEC);
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void WorkerSupervisor::Tick(uint32 gametime)
{
    if (!m_started)
    {
        return;
    }

    const time_t now = time(nullptr);

    // Drain inbound queue every tick; protocol frames handled here,
    // app frames staged in m_pendingFrames for World::HandleAhInbound.
    DrainInboundProtocol();

    // Check if child has exited by polling ACE_Process_Manager.
    if (!m_childExited && m_pid != ACE_INVALID_PID)
    {
        // running() returns 1 if alive, 0 if exited.
        if (!m_process.running())
        {
            sLog.outError("[WorkerSupervisor:%s] child process (pid=%u)"
                          " exited unexpectedly",
                          m_name.c_str(), static_cast<unsigned>(m_pid));
            m_childExited = true;
            m_pid         = ACE_INVALID_PID;
            // Discard the dead child's staged intents: a reconnecting child
            // must never replay the previous child's stale, half-applied
            // batch (which would over-post against the resumed in-process bot).
            ClearStagedFrames();
        }
    }

    // Check heartbeat timeout if child appears alive via IPC.
    if (!m_childExited && m_ipc.Connected())
    {
        const time_t ackAge = now - m_lastHeartbeatAck;
        if (ackAge > static_cast<time_t>(WS_HEARTBEAT_TIMEOUT_SEC))
        {
            sLog.outError("[WorkerSupervisor:%s] heartbeat timeout"
                          " (no ACK for %u s) - marking child dead",
                          m_name.c_str(), static_cast<unsigned>(ackAge));
            m_childExited = true;
            if (m_pid != ACE_INVALID_PID)
            {
                ACE_Process_Manager::instance()->terminate(m_pid);
                m_pid = ACE_INVALID_PID;
            }
            // Discard the dead child's staged intents (see process-exit path).
            ClearStagedFrames();
        }
    }

    // If child is gone, schedule restart with backoff.
    if (m_childExited)
    {
        if (now >= m_nextRetryAt)
        {
            ++m_failCount;
            sLog.outString("[WorkerSupervisor:%s] restarting child"
                           " (attempt #%u, backoff was %u s)",
                           m_name.c_str(),
                           static_cast<unsigned>(m_failCount),
                           static_cast<unsigned>(m_backoffSec));

            if (SpawnChild())
            {
                m_lastHeartbeatAck  = now;
                m_lastHeartbeatSent = now;
                // Reset backoff on success (child at least spawned).
                m_backoffSec = 1;
            }
            else
            {
                // Spawn failed; double the backoff for the next attempt.
                uint32 nextBackoff = m_backoffSec * 2;
                if (nextBackoff > WS_MAX_BACKOFF_SEC)
                {
                    nextBackoff = WS_MAX_BACKOFF_SEC;
                }
                m_backoffSec  = nextBackoff;
                m_nextRetryAt = now + static_cast<time_t>(m_backoffSec);
                sLog.outError("[WorkerSupervisor:%s] spawn failed;"
                              " next retry in %u s",
                              m_name.c_str(),
                              static_cast<unsigned>(m_backoffSec));
            }
        }
        return;
    }

    // Send heartbeat on interval.
    const time_t hbInterval = static_cast<time_t>(WS_HEARTBEAT_INTERVAL_SEC);
    if (now - m_lastHeartbeatSent >= hbInterval)
    {
        if (m_ipc.Connected())
        {
            // IPC_HEARTBEAT
            IpcMessage hb;
            hb.op = IPC_HEARTBEAT;
            m_ipc.SendFrame(hb);

            // IPC_GAMETIME (uint32 LE body)
            IpcMessage gt;
            gt.op = IPC_GAMETIME;
            gt.body << gametime;
            m_ipc.SendFrame(gt);

            m_lastHeartbeatSent = now;
            DETAIL_LOG("[WorkerSupervisor:%s] heartbeat sent (gametime=%u)",
                       m_name.c_str(), gametime);
        }
    }
}

// ---------------------------------------------------------------------------
// ClearStagedFrames (private)
// ---------------------------------------------------------------------------

void WorkerSupervisor::ClearStagedFrames()
{
    // Drop any application frames staged but not yet handed to World::Update.
    // Called on child-exit detection and before each (re)spawn so a fresh /
    // reconnecting child can never apply the previous child's stale batch.
    // swap-with-empty releases capacity too; this runs only on rare
    // exit/restart events so the realloc cost is irrelevant.
    if (!m_pendingFrames.empty())
    {
        std::vector<IpcMessage>().swap(m_pendingFrames);
    }

    // Also purge the IPC server's INBOUND queue. Clearing only m_pendingFrames
    // is not enough: the reactor thread may have already enqueued frames from
    // the dead child into the inbound BoundedQueue that DrainInboundProtocol()
    // has not popped yet. Left in place they would survive the restart and be
    // applied under the NEXT child. The clear is thread-safe (the reactor
    // thread produces, this world thread consumes; the queue's own mutex
    // serialises the drain). Combined with the per-spawn run-id this closes the
    // stale-frame-across-restart hole.
    const size_t purged = m_ipc.ClearInbound();
    if (purged != 0)
    {
        sLog.outString("[WorkerSupervisor:%s] purged %u stale inbound frame(s)"
                       " on child death/respawn",
                       m_name.c_str(), static_cast<unsigned>(purged));
    }
}

// ---------------------------------------------------------------------------
// DrainInboundProtocol (private)
// ---------------------------------------------------------------------------

void WorkerSupervisor::DrainInboundProtocol()
{
    // Bound application frames staged PER CALL. The reactor thread refills the
    // inbound BoundedQueue while this loop pops, so without a cap a flooding
    // child could feed m_pendingFrames unbounded. Protocol frames are always
    // consumed inline and do NOT count toward this budget.
    uint32 appBudget = WS_DRAIN_APP_PER_CALL;

    IpcMessage msg;
    while (m_ipc.PopInbound(msg))
    {
        switch (msg.op)
        {
            case IPC_HEARTBEAT_ACK:
            {
                m_lastHeartbeatAck = time(nullptr);
                DETAIL_LOG("[WorkerSupervisor:%s] IPC_HEARTBEAT_ACK received",
                           m_name.c_str());
                break;
            }
            case IPC_SHUTDOWN_ACK:
            {
                // Handled in Shutdown(); log if it arrives during Tick.
                sLog.outString("[WorkerSupervisor:%s] IPC_SHUTDOWN_ACK"
                               " received (unexpected in Tick)",
                               m_name.c_str());
                break;
            }
            // IPC_READY is consumed at the handshake layer (IpcServerHandler)
            // and never reaches this drain; no case is needed here.
            default:
            {
                // Application / consumer frame: stage for World::HandleAhInbound.
                // Stop staging once this call's budget is spent so a
                // concurrently-refilling reactor cannot feed us unbounded.
                if (appBudget == 0)
                {
                    ++m_appDropped;
                    break;
                }

                // HARD CAP on the staged buffer across ticks: drop-newest
                // (matching the inbound BoundedQueue policy) rather than grow
                // past the cap. The bound is therefore PRODUCER-enforced here;
                // the public DrainInbound() never has to abort to keep it.
                if (m_pendingFrames.size() >= IPC_INBOUND_QUEUE_CAP)
                {
                    ++m_appDropped;
                    break;
                }

                m_pendingFrames.push_back(msg);
                --appBudget;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// DrainInbound (public) -- called from World::Update after Tick()
// ---------------------------------------------------------------------------

void WorkerSupervisor::DrainInbound(std::vector<IpcMessage>& out,
                                    size_t maxPerTick)
{
    // The cap on m_pendingFrames is enforced UPSTREAM in
    // DrainInboundProtocol() (drop-newest at IPC_INBOUND_QUEUE_CAP), so this
    // can no longer be exceeded. Never abort here: if the invariant were ever
    // violated, clamp the buffer and emit a rate-limited warning instead of
    // taking down a live realm (the old MANGOS_ASSERT was the crash vector).
    if (m_pendingFrames.size() > IPC_INBOUND_QUEUE_CAP)
    {
        static time_t s_lastClampWarn = 0;
        const time_t now = time(nullptr);
        if (now - s_lastClampWarn >= 60)
        {
            sLog.outError("[WorkerSupervisor:%s] pending-frame buffer over cap"
                          " (%u > %u) - clamping (should be unreachable)",
                          m_name.c_str(),
                          static_cast<unsigned>(m_pendingFrames.size()),
                          static_cast<unsigned>(IPC_INBOUND_QUEUE_CAP));
            s_lastClampWarn = now;
        }
        m_pendingFrames.resize(IPC_INBOUND_QUEUE_CAP);
    }

    if (m_pendingFrames.empty())
    {
        return;
    }

    size_t avail = m_pendingFrames.size();
    size_t take  = (avail < maxPerTick) ? avail : maxPerTick;

    // Move the first 'take' elements into out, then erase them.
    out.insert(out.end(),
               m_pendingFrames.begin(),
               m_pendingFrames.begin() + static_cast<ptrdiff_t>(take));
    m_pendingFrames.erase(m_pendingFrames.begin(),
                          m_pendingFrames.begin() + static_cast<ptrdiff_t>(take));
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void WorkerSupervisor::Shutdown()
{
    if (!m_started)
    {
        return;
    }

    sLog.outString("[WorkerSupervisor:%s] shutdown initiated", m_name.c_str());

    // 1. Try a graceful IPC_SHUTDOWN if the channel is up.
    if (m_ipc.Connected())
    {
        IpcMessage sd;
        sd.op = IPC_SHUTDOWN;
        m_ipc.SendFrame(sd);

        sLog.outString("[WorkerSupervisor:%s] IPC_SHUTDOWN sent;"
                       " waiting up to %u s for ACK",
                       m_name.c_str(), WS_SHUTDOWN_GRACE_SEC);

        const time_t grace    = static_cast<time_t>(WS_SHUTDOWN_GRACE_SEC);
        const time_t deadline = time(nullptr) + grace;
        bool gotAck = false;

        while (time(nullptr) < deadline)
        {
            IpcMessage msg;
            while (m_ipc.PopInbound(msg))
            {
                if (msg.op == IPC_SHUTDOWN_ACK)
                {
                    gotAck = true;
                    break;
                }
            }
            if (gotAck)
            {
                break;
            }
            ACE_OS::sleep(ACE_Time_Value(0, 50 * 1000)); // 50 ms
        }

        if (gotAck)
        {
            sLog.outString("[WorkerSupervisor:%s] IPC_SHUTDOWN_ACK received"
                           " - child exiting cleanly",
                           m_name.c_str());
        }
        else
        {
            sLog.outError("[WorkerSupervisor:%s] shutdown ACK timeout"
                          " - hard-killing child (pid=%u)",
                          m_name.c_str(), static_cast<unsigned>(m_pid));
        }
    }

    // 2. Hard-kill if child is still alive.
    if (m_pid != ACE_INVALID_PID)
    {
        ACE_Process_Manager::instance()->terminate(m_pid);
        m_pid = ACE_INVALID_PID;
    }

    // 3. Stop the IPC server (closes acceptor + reactor thread).
    m_ipc.Stop();

#ifdef _WIN32
    // 4. Close the Job Object handle (child already dead; frees the
    //    kernel object).
    if (m_jobObject != NULL)
    {
        CloseHandle(m_jobObject);
        m_jobObject = NULL;
    }
#endif

    m_started     = false;
    m_childExited = true;

    sLog.outString("[WorkerSupervisor:%s] shutdown complete", m_name.c_str());
}
