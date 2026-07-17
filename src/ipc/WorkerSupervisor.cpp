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

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>
#include <thread>
#include <vector>

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
    , m_pid(IpcProcess::INVALID_PID)
    , m_lastHeartbeatSent(0)
    , m_lastHeartbeatAck(0)
    , m_connectAnchor(0)
    , m_everConnected(false)
    , m_backoffSec(1)
    , m_nextRetryAt(0)
    , m_failCount(0)
    , m_started(false)
    , m_childExited(true)
    // OPEN-1: start UNHEALTHY so the in-process bot keeps running until the
    // child proves itself healthy in a heartbeat-ack (err toward a bot
    // running, never toward a silent stall).
    , m_childHealthy(false)
    , m_runId(0)
    , m_writeAuthority(false)
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
    // No valid bot owner GUID -> do not spawn, do not enter the restart loop.
    // (Forged system owner resolves to a non-zero sentinel; 0 means a
    // misconfigured AuctionHouseBot.CharacterName.) Caller falls back to the
    // in-process bot.
    if (m_botGuid == 0)
    {
        sLog.outError("[WorkerSupervisor:%s] bot GUID is 0 (unresolved AuctionHouseBot.CharacterName);"
                      " not starting the service worker.", m_name.c_str());
        return false;
    }

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
    // Build the argument list.
    // ah-service --port <p> --botguid <g> --config <c>
    //
    // C4: the shared secret is passed OUT-OF-BAND via the AH_SERVICE_SECRET
    // environment variable and is NOT placed on argv, so it cannot be read from
    // /proc/<pid>/cmdline (Linux) or the Win32 process command line by any local
    // account. The child reads the env var first and only falls back to a
    // manual-testing --secret when the env var is absent.
    std::vector<std::string> args;
    args.push_back("--port");
    args.push_back(std::to_string(static_cast<unsigned>(m_port)));
    args.push_back("--botguid");
    args.push_back(std::to_string(static_cast<unsigned>(m_botGuid)));
    args.push_back("--config");
    args.push_back(m_cfgPath);

    // For logging: the argument list carries NO secret, so it is safe to log.
    std::string cmdLog = m_exePath;
    for (const std::string& a : args)
    {
        cmdLog += ' ';
        cmdLog += a;
    }

    // Assign a new per-spawn run-id (monotonically increasing; 0 is never used).
    // Arm the run-id + SP-2 write-authority for the IPC_HELLO_ACK before the
    // child can connect.
    ++m_runId;
    m_ipc.SetRunId(m_runId);
    m_ipc.SetWriteAuthority(m_writeAuthority);
    sLog.outString("[WorkerSupervisor:%s] assigned run-id %u",
                   m_name.c_str(), static_cast<unsigned>(m_runId));

    // Spawn the child (Windows: CREATE_NEW_CONSOLE + a HANDLE for the Job
    // Object; secret injected into the child environment as AH_SERVICE_SECRET).
    if (!m_process.Spawn(m_exePath, args, "AH_SERVICE_SECRET", m_secret))
    {
        sLog.outError("[WorkerSupervisor:%s] spawn failed (cmd: %s)",
                      m_name.c_str(), cmdLog.c_str());
        return false;
    }

    m_pid         = m_process.Pid();
    m_childExited = false;

    // C2: arm the connect deadline from this spawn, and reset the
    // ever-connected flag so the deadline applies to the fresh child until it
    // completes the handshake.
    m_connectAnchor = time(nullptr);
    m_everConnected = false;

    // Defensive: start the new child with an empty staged buffer so a frame
    // that slipped in from the prior run-id can never be applied as if it
    // came from this child. Exit detection already clears on the way down;
    // this also covers the very first Start() and any race on respawn.
    ClearStagedFrames();

    sLog.outString("[WorkerSupervisor:%s] child spawned (pid=%u, cmd: %s)",
                   m_name.c_str(), static_cast<unsigned>(m_pid), cmdLog.c_str());

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
            HANDLE hChild = m_process.Handle();
            if (hChild == INVALID_HANDLE_VALUE || hChild == NULL)
            {
                sLog.outError("[WorkerSupervisor:%s] child process handle"
                              " invalid - orphan guard disabled",
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
    // prctl(PR_SET_PDEATHSIG, SIGUSR1) at startup via
    // Console_InstallParentDeathGuard() (OPEN-2: a distinct signal so SIGTERM
    // keeps its default-terminate disposition), so the kernel signals the
    // child when this (the parent) dies. Nothing to arm on the supervisor side
    // here. The supervisor's hard-kill uses kill(pid, SIGKILL), which is
    // uncatchable, so it always works regardless.
#endif

    return true;
}

// ---------------------------------------------------------------------------
// ReapChild (private)
// ---------------------------------------------------------------------------

void WorkerSupervisor::ReapChild()
{
    const uint32 pid = m_pid;

    // Release the child's OS handle (Windows) / reap the zombie (Linux) so the
    // kernel frees the entry before the next SpawnChild(). Only ever called
    // after the child is known dead/terminated, so the status is available.
    m_process.Reap();
    m_pid = IpcProcess::INVALID_PID;

    if (pid != IpcProcess::INVALID_PID)
    {
        DETAIL_LOG("[WorkerSupervisor:%s] reaped child pid=%u",
                   m_name.c_str(), static_cast<unsigned>(pid));
    }
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
    // OPEN-1: require RUNTIME health, not just transport. An unhealthy child
    // keeps heartbeating but stops emitting; if we returned true here the
    // in-process bot would stay suppressed and no bot would run (silent
    // stall). When the child recovers it reports healthy again and the
    // in-process bot stands back down - no child restart involved.
    if (!m_childHealthy)
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

    // Check if the child has exited.
    if (!m_childExited && m_pid != IpcProcess::INVALID_PID)
    {
        if (!m_process.Running())
        {
            sLog.outError("[WorkerSupervisor:%s] child process (pid=%u)"
                          " exited unexpectedly",
                          m_name.c_str(), static_cast<unsigned>(m_pid));
            // C5: reap the dead child's handle / process-table entry so a
            // flapping child never leaks.
            m_childExited = true;
            ReapChild();
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
            if (m_pid != IpcProcess::INVALID_PID)
            {
                m_process.Terminate();
                ReapChild();
            }
            // Discard the dead child's staged intents (see process-exit path).
            ClearStagedFrames();
        }
    }

    // C2: connect-deadline check. The heartbeat-timeout path above only fires
    // while IPC is CONNECTED, so a child that is alive but never completes the
    // handshake (or disconnected and is not reconnecting) would otherwise keep
    // the service inactive forever without a restart. Track connectivity and
    // restart once the deadline since the anchor elapses without a live
    // channel. The anchor is the spawn time, or "now" at each
    // CONNECTED -> disconnected transition (re-armed below).
    if (!m_childExited)
    {
        const bool connected = m_ipc.Connected();
        if (connected)
        {
            // First time we see a live channel for this spawn: latch it.
            m_everConnected = true;
        }
        else
        {
            // Not (currently) connected. If we WERE connected and just lost
            // the channel, re-arm the deadline anchor to now so a stuck
            // reconnect is bounded by the same generous window rather than the
            // original spawn time.
            if (m_everConnected)
            {
                m_connectAnchor = now;
                m_everConnected = false;
                // OPEN-1: the channel just dropped; the child must re-prove
                // health on reconnect before it suppresses the in-process bot.
                m_childHealthy = false;
            }

            const time_t connectAge = now - m_connectAnchor;
            if (connectAge > static_cast<time_t>(WS_CONNECT_DEADLINE_SEC))
            {
                sLog.outError("[WorkerSupervisor:%s] child (pid=%u) not"
                              " connected within %u s of spawn/disconnect"
                              " - terminating + restarting",
                              m_name.c_str(),
                              static_cast<unsigned>(m_pid),
                              static_cast<unsigned>(WS_CONNECT_DEADLINE_SEC));
                m_childExited = true;
                if (m_pid != IpcProcess::INVALID_PID)
                {
                    m_process.Terminate();
                    ReapChild(); // reap the terminated child before the next spawn
                }
                ClearStagedFrames();
            }
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
    // OPEN-1: a (re)connecting child must RE-PROVE its operational health
    // before it can suppress the in-process bot. This runs on every child
    // death/respawn path (process-exit, heartbeat-timeout, connect-deadline,
    // and each SpawnChild()), so clearing the flag here covers them all.
    m_childHealthy = false;

    // Drop any application frames staged but not yet handed to World::Update.
    // Called on child-exit detection and before each (re)spawn so a fresh /
    // reconnecting child can never apply the previous child's stale batch.
    // swap-with-empty releases capacity too; this runs only on rare
    // exit/restart events so the realloc cost is irrelevant.
    if (!m_pendingFrames.empty())
    {
        std::vector<IpcMessage>().swap(m_pendingFrames);
    }

    // [SP-2 decision 10 / Finding 1] The reliable lane has its OWN staging
    // container now (m_pendingReliableFrames), separate from m_pendingFrames,
    // and must be purged here too: DrainInboundProtocol() may already have
    // moved reliable frames out of the reactor's internal unbounded queue
    // (purged below via m_ipc.ClearReliable()) into m_pendingReliableFrames
    // before World::Update ever called DrainInbound() to consume them. Left
    // in place, those staged-but-unconsumed frames would survive the restart
    // and be applied under the NEXT child.
    if (!m_pendingReliableFrames.empty())
    {
        std::vector<IpcMessage>().swap(m_pendingReliableFrames);
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

    // [SP-2 decision 10] Same reasoning for the unbounded reliable lane: the
    // dead child's mutation frames must not survive into the next child. The
    // per-spawn run-id gate in DrainInboundProtocol() would drop them anyway,
    // but purging here keeps the lane from carrying stale frames across restart.
    const size_t purgedReliable = m_ipc.ClearReliable();
    if (purgedReliable != 0)
    {
        sLog.outString("[WorkerSupervisor:%s] purged %u stale reliable frame(s)"
                       " on child death/respawn",
                       m_name.c_str(), static_cast<unsigned>(purgedReliable));
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
    uint32 appBudget    = WS_DRAIN_APP_PER_CALL;
    uint32 browseBudget = WS_DRAIN_BROWSE_PER_CALL;

    // [SP-2 decision 10] Drain the UNBOUNDED reliable lane to EXHAUSTION FIRST,
    // before the bounded queue, so a browse flood on the bounded queue can never
    // starve a mutation-class frame. Reliable frames received here are all
    // application/consumer frames (IPC_PLAYER_RESULT, IPC_RESOLVE_APPLY,
    // IPC_INTENT_SELL) destined for World::HandleAhInbound, so they are staged
    // directly into their OWN unbounded container (m_pendingReliableFrames) -
    // bypassing the app/browse drop budgets AND the IPC_INBOUND_QUEUE_CAP
    // staging cap entirely: they carry money/item value and must NOT be
    // dropped. [Finding 1] Keeping them out of m_pendingFrames means
    // DrainInbound()'s over-cap clamp on the bounded lane can never truncate a
    // reliable frame, however many are staged in one drain interval.
    IpcMessage rmsg;
    while (m_ipc.PopReliable(rmsg))
    {
        // PF2-B: same generation/run-id gate as the bounded path below - drop a
        // frame stamped by a PRIOR child's connection so it can never be applied
        // under the current child.
        if (rmsg.generation != m_runId)
        {
            DETAIL_LOG("[WorkerSupervisor:%s] dropping stale-run reliable frame"
                       " 0x%04X (gen=%u, current=%u)",
                       m_name.c_str(),
                       static_cast<unsigned>(rmsg.op),
                       static_cast<unsigned>(rmsg.generation),
                       static_cast<unsigned>(m_runId));
            continue;
        }
        m_pendingReliableFrames.push_back(rmsg);
    }

    IpcMessage msg;
    while (m_ipc.PopInbound(msg))
    {
        // PF2-B: generation/run-id gate. Every inbound frame was stamped by
        // the receiving connection's IpcServerHandler with that connection's
        // run-id (msg.generation). Drop any frame whose stamp does not match
        // our CURRENT run-id: m_runId increments on every spawn, so a frame
        // produced by a PRIOR child's connection - even one that a dying
        // child's reactor decoded and pushed AFTER ClearInbound() purged the
        // queue - is dropped here before it can be staged or applied under the
        // current child. This is belt-and-suspenders WITH ClearInbound(): the
        // purge handles timing, this stamp handles a frame that slips past it.
        if (msg.generation != m_runId)
        {
            DETAIL_LOG("[WorkerSupervisor:%s] dropping stale-run inbound frame"
                       " 0x%04X (gen=%u, current=%u)",
                       m_name.c_str(),
                       static_cast<unsigned>(msg.op),
                       static_cast<unsigned>(msg.generation),
                       static_cast<unsigned>(m_runId));
            continue;
        }

        switch (msg.op)
        {
            case IPC_HEARTBEAT_ACK:
            {
                m_lastHeartbeatAck = time(nullptr);
                // OPEN-1: the ack body carries a 1-byte operational-health
                // flag (1 == healthy). The frame already passed the exact-size
                // B1 validation (IpcExpectedBodySize == 1) before reaching this
                // drain, so the byte is guaranteed present; read it defensively
                // regardless and treat a missing/zero flag as unhealthy.
                uint8 healthy = 0;
                if (msg.body.size() >= 1)
                {
                    msg.body >> healthy;
                }
                m_childHealthy = (healthy != 0);
                DETAIL_LOG("[WorkerSupervisor:%s] IPC_HEARTBEAT_ACK received"
                           " (childHealthy=%u)",
                           m_name.c_str(),
                           static_cast<unsigned>(m_childHealthy ? 1u : 0u));
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
                // I6: browse replies have their own sub-budget so they cannot
                // starve intent results (which share appBudget).
                const bool isBrowse = (msg.op == IPC_BROWSE_RESULT);
                uint32& budget = isBrowse ? browseBudget : appBudget;
                if (budget == 0)
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
                --budget;
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
    // [Finding 1] The IPC_INBOUND_QUEUE_CAP clamp below applies ONLY to the
    // bounded/browse lane (m_pendingFrames). Reliable (mutation-class) frames
    // live in the fully separate, UNBOUNDED m_pendingReliableFrames container
    // (staged directly by DrainInboundProtocol(), bypassing this cap), so a
    // reliable frame can never be silently dropped here regardless of how
    // many are staged in one drain interval - the never-drop guarantee from
    // decision 10 is therefore structural, not just accidental headroom
    // versus the worker's RESOLVE_WINDOW.
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

    if (m_pendingReliableFrames.empty() && m_pendingFrames.empty())
    {
        return;
    }

    size_t remaining = maxPerTick;

    // Drain the reliable lane first, to exhaustion (or the per-tick budget),
    // so mutation-class frames are handed to World::HandleAhInbound ahead of
    // bounded/browse frames and are never starved nor clamped away.
    if (!m_pendingReliableFrames.empty() && remaining > 0)
    {
        size_t availReliable = m_pendingReliableFrames.size();
        size_t takeReliable  = (availReliable < remaining) ? availReliable : remaining;

        out.insert(out.end(),
                   m_pendingReliableFrames.begin(),
                   m_pendingReliableFrames.begin() + static_cast<ptrdiff_t>(takeReliable));
        m_pendingReliableFrames.erase(m_pendingReliableFrames.begin(),
                                     m_pendingReliableFrames.begin() + static_cast<ptrdiff_t>(takeReliable));
        remaining -= takeReliable;
    }

    if (!m_pendingFrames.empty() && remaining > 0)
    {
        size_t avail = m_pendingFrames.size();
        size_t take  = (avail < remaining) ? avail : remaining;

        // Move the first 'take' elements into out, then erase them.
        out.insert(out.end(),
                   m_pendingFrames.begin(),
                   m_pendingFrames.begin() + static_cast<ptrdiff_t>(take));
        m_pendingFrames.erase(m_pendingFrames.begin(),
                              m_pendingFrames.begin() + static_cast<ptrdiff_t>(take));
    }
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
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (gotAck)
        {
            sLog.outString("[WorkerSupervisor:%s] IPC_SHUTDOWN_ACK received"
                           " - child exiting cleanly",
                           m_name.c_str());

            // C5: post-ACK grace. The child ACKs and THEN tears down + exits;
            // give it a brief moment to exit on its own rather than ACK-then-
            // immediately-terminate (which would needlessly hard-kill a child
            // that was about to exit cleanly). Bounded by the remaining time in
            // the existing grace window, so the overall shutdown bound is
            // unchanged. We reap the pid as soon as it exits.
            while (time(nullptr) < deadline && m_pid != IpcProcess::INVALID_PID)
            {
                if (!m_process.Running())
                {
                    ReapChild();
                    sLog.outString("[WorkerSupervisor:%s] child exited cleanly"
                                   " after ACK",
                                   m_name.c_str());
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        else
        {
            sLog.outError("[WorkerSupervisor:%s] shutdown ACK timeout"
                          " - hard-killing child (pid=%u)",
                          m_name.c_str(), static_cast<unsigned>(m_pid));
        }
    }

    // 2. Hard-kill if child is still alive (no ACK, or it did not exit within
    //    the post-ACK grace), then reap so we never leak a handle/zombie.
    if (m_pid != IpcProcess::INVALID_PID)
    {
        m_process.Terminate();
        ReapChild();
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
