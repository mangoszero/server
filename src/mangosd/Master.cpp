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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/// \addtogroup mangosd
/// @{
/// \file

#include "Master.h"

#include "AuctionHouseBot.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "MangosdTest.h"
#include "MapManager.h"
#include "MassMailMgr.h"
#include "ObjectAccessor.h"
#include "ProgressBar.h"
#include "ScriptMgr.h"
#include "SystemConfig.h"
#include "Timer.h"
#include "Util.h"
#include "World.h"
#include "WorldNetwork.h"
#include "WorkerSupervisor.h"

#ifdef ENABLE_SOAP
#include "SOAP/SoapThread.h"
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include "ServiceWin32.h"
extern int m_ServiceStatus;
#else
#include "PosixDaemon.h"     // detachDaemon()
#include <unistd.h>          // STDIN_FILENO
#include <sys/select.h>      // select(), fd_set
#endif

extern DatabaseType WorldDatabase;
extern DatabaseType CharacterDatabase;
extern DatabaseType LoginDatabase;
extern uint32 realmID;

/// Shortest interval between two world ticks.
constexpr std::chrono::milliseconds WORLD_SLEEP_CONST{50};

/// How often the console reader looks for a pending line while idle. Fast enough that a
/// typed command feels instant, slow enough that an idle server is not making syscalls.
constexpr std::chrono::milliseconds CLI_POLL_INTERVAL{100};

namespace
{
    /// Forward a fully-built progress-bar redraw to the off-thread console writer
    /// (verbatim: no prefix, colour or newline), so the bar shares one serialised stdout
    /// with the log lines and cannot tear against them.
    void BarConsoleSink(char const* bytes, size_t len)
    {
        sLog.ConsoleEmitRaw(std::string(bytes, len));
    }

#ifdef _WIN32
    /// Update the console title with current player/connection counts, only if changed.
    void UpdateConsoleTitle(uint32 players, uint32 connections)
    {
        static std::string s_lastTitle;
        char title[128];
        snprintf(title, sizeof(title), "%s (%u Players - %u Connections)",
                 MANGOS_PACKAGENAME, players, connections);
        std::string newTitle(title);
        if (s_lastTitle != newTitle)
        {
            s_lastTitle = newTitle;
            SetConsoleTitleA(title);
        }
    }
#endif

    /**
     * @brief RAII handle for one opened database.
     *
     * Opening the three databases is a ladder, and every rung can fail. Done by hand, each
     * failure has to remember to halt the delay threads of everything opened so far — which
     * is where the repeated (and easy to get wrong) HaltDelayThread() cascades came from.
     * Here, unwinding is automatic, and Release() is called only once every database is up.
     */
    class DatabaseGuard
    {
        public:

            explicit DatabaseGuard(DatabaseType& db) : m_db(&db) {}

            ~DatabaseGuard()
            {
                if (m_db)
                {
                    m_db->HaltDelayThread();
                }
            }

            DatabaseGuard(const DatabaseGuard&) = delete;
            DatabaseGuard& operator=(const DatabaseGuard&) = delete;

            /// The database is now owned by the caller; stop tracking it.
            void Release() { m_db = nullptr; }

        private:

            DatabaseType* m_db;
    };

    /**
     * @brief Open one database and verify its schema version.
     */
    bool OpenDatabase(DatabaseType& db, const char* infoKey, const char* connKey,
                      const char* label, DatabaseTypes versionCheck)
    {
        const std::string dbstring = sConfig.GetStringDefault(infoKey, "");
        if (dbstring.empty())
        {
            sLog.outError("%s not specified in configuration file", label);
            return false;
        }

        const int nConnections = sConfig.GetIntDefault(connKey, 1);
        sLog.outString("%s total connections: %i", label, nConnections + 1);

        if (!db.Initialize(dbstring.c_str(), nConnections))
        {
            sLog.outError("Can not connect to %s %s", label, dbstring.c_str());
            return false;
        }

        return db.CheckDatabaseVersion(versionCheck);
    }
}

// -- Databases ---------------------------------------------------------------------

bool Master::StartDatabases()
{
    if (!OpenDatabase(WorldDatabase, "WorldDatabaseInfo", "WorldDatabaseConnections",
                      "World Database", DATABASE_WORLD))
    {
        return false;
    }
    DatabaseGuard worldGuard(WorldDatabase);

    if (!OpenDatabase(CharacterDatabase, "CharacterDatabaseInfo", "CharacterDatabaseConnections",
                      "Character Database", DATABASE_CHARACTER))
    {
        return false;
    }
    DatabaseGuard characterGuard(CharacterDatabase);

    if (!OpenDatabase(LoginDatabase, "LoginDatabaseInfo", "LoginDatabaseConnections",
                      "Login Database", DATABASE_REALMD))
    {
        return false;
    }
    DatabaseGuard loginGuard(LoginDatabase);

    sLog.outString();

    ///- Get the realm Id from the configuration file
    realmID = sConfig.GetIntDefault("RealmID", 0);
    if (!realmID)
    {
        sLog.outError("Realm ID not defined in configuration file");
        return false;
    }

    sLog.outString("Realm running as realm ID %d", realmID);
    sLog.outString();

    ///- Clean the database before starting
    ClearOnlineAccounts();

    sWorld.LoadDBVersion();
    sLog.outString("Using World DB: %s", sWorld.GetDBVersion());
    sLog.outString();

    // Everything is up: the databases are ours to keep, so the guards must not unwind.
    loginGuard.Release();
    characterGuard.Release();
    worldGuard.Release();

    return true;
}

void Master::StopDatabases()
{
    sLog.outString("[shutdown] halting DB delay threads (Login/Character/World)...");

    // Reverse of the open order in StartDatabases(), which is also the order the
    // DatabaseGuards unwind in on a failed startup. The halts are independent, so this
    // costs nothing — but having one teardown order instead of two means the shutdown
    // path and the startup-failure path can't drift apart later.
    LoginDatabase.HaltDelayThread();
    CharacterDatabase.HaltDelayThread();
    WorldDatabase.HaltDelayThread();

    sLog.outString("[shutdown] DB delay threads halted");
}

void Master::ClearOnlineAccounts()
{
    // Cleanup online status for characters hosted at current realm
    /// \todo Only accounts with characters logged on *this* realm should have online
    /// status reset. Move the online column from 'account' to 'realmcharacters'?
    LoginDatabase.PExecute("UPDATE `account` SET `active_realm_id` = 0, `os` = ''  WHERE `active_realm_id` = '%u'", realmID);

    CharacterDatabase.Execute("UPDATE `characters` SET `online` = 0 WHERE `online`<>0");

    // Battleground instance ids reset at server restart
    CharacterDatabase.Execute("UPDATE `character_battleground_data` SET `instance_id` = 0");
}

// -- World loop --------------------------------------------------------------------

/**
 * @brief The world heartbeat.
 *
 * Runs on the caller's thread (main), so there is no world thread to spawn or join any
 * more. Ticks the world at most every WORLD_SLEEP_CONST ms, then unwinds the world in
 * order: kick the players, flush their sessions, stop the network, unload the maps.
 */
void Master::WorldLoop()
{
    uint32 realPrevTime = getMSTime();

    sLog.outString("World Updater started (%lldms min update interval)",
                   static_cast<long long>(WORLD_SLEEP_CONST.count()));

    while (!World::IsStopped())
    {
        ++World::m_worldLoopCounter;

        const uint32 realCurrTime = getMSTime();
        const uint32 diff = getMSTimeDiff(realPrevTime, realCurrTime);

        sWorld.Update(diff);
        realPrevTime = realCurrTime;

#ifdef _WIN32
        static uint32 titleUpdateCounter = 0;
        if ((++titleUpdateCounter) >= 60)                   // ~3 seconds at a 50ms tick
        {
            titleUpdateCounter = 0;
            UpdateConsoleTitle(sWorld.GetActiveSessionCount(),
                sWorldNetwork.GetOpenConnectionCount());
        }
#endif

        const uint32 executionTimeDiff = getMSTimeDiff(realCurrTime, getMSTime());

        if (executionTimeDiff > 1000)
        {
            sLog.outError("Master::WorldLoop: sWorld.Update(diff=%u) took %u ms (loopCounter=%u, sessions=%u)",
                          diff, executionTimeDiff, World::m_worldLoopCounter.load(),
                          sWorld.GetActiveSessionCount());
        }

        // Sleep off whatever is left of this tick's budget.
        const std::chrono::milliseconds executionTime{executionTimeDiff};
        if (executionTime < WORLD_SLEEP_CONST)
        {
            std::this_thread::sleep_for(WORLD_SLEEP_CONST - executionTime);
        }

#ifdef _WIN32
        if (m_ServiceStatus == 0)                           // service stopped
        {
            World::StopNow(SHUTDOWN_EXIT_CODE);
        }

        while (m_ServiceStatus == 2)                        // service paused
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
#endif
    }

    sLog.outString("[shutdown] world loop stopped; entering world shutdown tail");

    sLog.outString("[shutdown] KickAll: saving + kicking players...");
    sWorld.KickAll();                                       // save and kick all players
    sLog.outString("[shutdown] KickAll done");

    sLog.outString("[shutdown] final UpdateSessions...");
    sWorld.UpdateSessions(1);                               // real players unload requires this call
    sLog.outString("[shutdown] final UpdateSessions done");

    sLog.outString("[shutdown] StopNetwork: closing listener + joining network threads...");
    sWorldNetwork.Stop();
    sLog.outString("[shutdown] StopNetwork done");

    sLog.outString("[shutdown] UnloadAll: unloading maps + MapUpdater teardown...");
    sMapMgr.UnloadAll();                                    // unload all grids (including locked ones)
    sLog.outString("[shutdown] UnloadAll returned");

    sLog.outString("World Updater stopped");
}

// -- Services ----------------------------------------------------------------------

namespace
{
    /**
     * @brief Watchdog: abort the process if the world loop stops advancing.
     *
     * A frozen world is worse than a dead one — clients hang, nothing saves — so this
     * deliberately crashes the process rather than letting it sit there.
     */
    class FreezeDetectorService : public IService
    {
        public:

            explicit FreezeDetectorService(uint32 maxStuckMs) : m_maxStuckMs(maxStuckMs) {}

            const char* Name() const override { return "freeze detector"; }

            void Start() override
            {
                m_thread = std::thread([this] { Run(); });
            }

            void Join() override
            {
                if (m_thread.joinable())
                {
                    m_thread.join();
                }
            }

        private:

            void Run()
            {
                sLog.outString("AntiFreeze thread started (%u seconds max stuck time)",
                               m_maxStuckMs / 1000);

                uint32 lastLoops  = 0;
                uint32 lastChange = 0;

                while (!World::IsStopped())
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));

                    const uint32 curtime = getMSTime();
                    const uint32 loops   = World::m_worldLoopCounter.load();

                    if (loops != lastLoops)                 // normal progress
                    {
                        lastLoops  = loops;
                        lastChange = curtime;
                    }
                    else if (getMSTimeDiff(lastChange, curtime) > m_maxStuckMs)
                    {
                        sLog.outError("World Thread hangs, kicking out server!");

                        // Deliberately die, so the freeze leaves a core/minidump pinned at
                        // the point it was detected. A volatile null store would be
                        // undefined behaviour: the optimiser is entitled to delete it
                        // outright, silently turning the watchdog into a no-op in Release.
                        // abort() is defined, dumps core on POSIX, and still reaches
                        // WheatyExceptionReport on Windows via _CALL_REPORTFAULT.
                        sLog.Flush();                       // get the diagnosis to disk first
                        std::abort();
                    }
                }

                sLog.outString("AntiFreeze thread stopped");
            }

            uint32      m_maxStuckMs;
            std::thread m_thread;
    };

    /// Print the interactive prompt through the console writer, so it shares the single
    /// serialised stdout with log lines and progress bars and cannot overtake them.
    void CliPrompt(void* /*callbackArg*/ = nullptr, bool /*status*/ = true)
    {
        sLog.ConsoleEmitRaw("mangos>");
    }

#if PLATFORM != PLATFORM_WINDOWS
    /// Non-blocking check for pending console input.
    int kb_hit_return()
    {
        struct timeval tv;
        fd_set fds;
        tv.tv_sec  = 0;
        tv.tv_usec = 0;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv);
        return FD_ISSET(STDIN_FILENO, &fds);
    }
#endif

    /**
     * @brief Console reader: turns stdin lines into CLI commands queued to the world thread.
     *
     * The only service that has to override RequestStop(): on Windows it parks inside
     * fgets(), which no flag can interrupt, so it has to be woken by hand.
     */
    class ConsoleService : public IService
    {
        public:

            explicit ConsoleService(bool beep) : m_beep(beep) {}

            const char* Name() const override { return "console"; }

            void Start() override
            {
                m_thread = std::thread([this] { Run(); });
            }

            void RequestStop() override
            {
#ifdef _WIN32
                if (!m_thread.joinable())
                {
                    return;
                }

                // The reader is parked in fgets(); feed it a synthetic Return so it wakes,
                // sees that the world has stopped, and leaves. (On POSIX it polls instead,
                // so it notices the flag on its own and needs nothing here.)
                INPUT_RECORD record;
                record.EventType                        = KEY_EVENT;
                record.Event.KeyEvent.bKeyDown          = TRUE;
                record.Event.KeyEvent.dwControlKeyState = 0;
                record.Event.KeyEvent.uChar.AsciiChar   = '\r';
                record.Event.KeyEvent.wVirtualKeyCode   = VK_RETURN;
                record.Event.KeyEvent.wRepeatCount      = 1;
                record.Event.KeyEvent.wVirtualScanCode  = 0x1c;

                DWORD written = 0;
                WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &record, 1, &written);
#endif
            }

            void Join() override
            {
                if (m_thread.joinable())
                {
                    m_thread.join();
                }
            }

        private:

            void Run()
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                if (m_beep)
                {
                    sLog.ConsoleEmitRaw("\a");      // \a = Alert, via the single-owner stdout
                }

                CliPrompt();

                char buffer[256];

                while (!World::IsStopped())
                {
#if PLATFORM != PLATFORM_WINDOWS
                    // Poll rather than block, so shutdown does not have to interrupt a
                    // parked read. The interval is what bounds the cost: this used to sleep
                    // 100 *micro*seconds despite claiming to cap the console at ~10
                    // commands/second, so an idle server sat here spinning through ~20k
                    // syscalls/second for its entire uptime.
                    while (!kb_hit_return() && !World::IsStopped())
                    {
                        std::this_thread::sleep_for(CLI_POLL_INTERVAL);
                    }

                    if (World::IsStopped())
                    {
                        break;
                    }
#endif
                    char* command_str = fgets(buffer, sizeof(buffer), stdin);
                    if (!command_str)
                    {
                        if (feof(stdin))
                        {
                            World::StopNow(SHUTDOWN_EXIT_CODE);
                        }
                        continue;
                    }

                    for (int x = 0; command_str[x]; ++x)
                    {
                        if (command_str[x] == '\r' || command_str[x] == '\n')
                        {
                            command_str[x] = 0;
                            break;
                        }
                    }

                    if (!*command_str)
                    {
                        CliPrompt();
                        continue;
                    }

                    std::string command;
                    if (!consoleToUtf8(command_str, command))   // console encoding to utf8
                    {
                        CliPrompt();
                        continue;
                    }

                    sWorld.QueueCliCommand(new CliCommandHolder(0, SEC_CONSOLE, nullptr,
                                                                command.c_str(),
                                                                &utf8print, &CliPrompt));
                }
            }

            bool        m_beep;
            std::thread m_thread;
    };

    /// The remote-access (telnet) listener. The socket, and its accept loop, belong to
    /// RaServer; this only gives it the same lifecycle as everything else.
    class RaService : public IService
    {
        public:

            RaService(uint16 port, const std::string& bindIp) : m_port(port), m_bindIp(bindIp) {}

            const char* Name() const override { return "remote access"; }

            void Start() override { m_server.Start(m_port, m_bindIp); }

            // The listener is not a thread we join; closing it *is* the join.
            void Join() override { m_server.Stop(); }

        private:

            uint16      m_port;
            std::string m_bindIp;
            RaServer    m_server;
    };

#ifdef ENABLE_SOAP
    /// The SOAP listener. Its loop polls World::IsStopped() on a 3-second accept timeout,
    /// so it needs no wake-up — it leaves on its own within one timeout.
    class SoapService : public IService
    {
        public:

            SoapService(const std::string& ip, uint16 port) : m_ip(ip), m_port(port) {}

            const char* Name() const override { return "SOAP"; }

            void Start() override
            {
                m_thread = std::thread(SoapThread, m_ip, m_port);
            }

            void Join() override
            {
                if (m_thread.joinable())
                {
                    m_thread.join();
                }
            }

        private:

            std::string m_ip;
            uint16      m_port;
            std::thread m_thread;
    };
#endif

    /**
     * @brief The auction-house worker subprocess (optional, default-off).
     *
     * Not a thread: the supervisor owns a child process and its IPC link. It gets the
     * same lifecycle as everything else so its Shutdown() is ordered by the same rule —
     * joined after the world loop has stopped, so no tick can race it.
     */
    class AhServiceService : public IService
    {
        public:

            AhServiceService() : m_supervisor(nullptr) {}

            const char* Name() const override { return "AH service"; }

            void Start() override
            {
                // SP-1 coordinator authority: the worker is the configured AH read
                // authority from here on. Set BEFORE Start() so that even if Start()
                // fails (missing exe / port taken / bad bot GUID) and the supervisor is
                // torn down below, the read handlers still send "AH unavailable" rather
                // than silently reverting to in-process reads.
                sWorld.SetAhServiceConfigured(true);

                m_supervisor = new WorkerSupervisor(
                    "ah-service",
                    sConfig.GetStringDefault("AH.Service.Path", "service-workers/ah-service/ah-service"),
                    uint16(sConfig.GetIntDefault("AH.Service.Port", 5760)),
                    sConfig.GetStringDefault("AH.Service.Secret", "changeme"),
                    sAuctionBotConfig.GetAHBotId(),
                    sConfig.GetStringDefault("AH.Service.Config", "ah-service.conf"));

                // SP-2: arm the write-authority bit for the IPC handshake BEFORE Start() --
                // IPC_HELLO_ACK carries {runId, writeAuthority} to the worker (spec
                // decision 7: the worker never reads it from its own conf). Applied on
                // every child respawn.
                m_supervisor->SetWriteAuthority(sWorld.IsAhWriteAuthority());

                if (!m_supervisor->Start())
                {
                    sLog.outError("AH service failed to start; falling back to in-process bot");
                    delete m_supervisor;
                    m_supervisor = nullptr;
                }

                // Published before the world loop starts, so the first tick already sees it.
                sWorld.SetAhSupervisor(m_supervisor);
            }

            void Join() override
            {
                if (!m_supervisor)
                {
                    return;
                }

                // Unpublish before destroying: the world loop has stopped, but nothing else
                // should be able to observe a dangling supervisor.
                sWorld.SetAhSupervisor(nullptr);
                m_supervisor->Shutdown();
                delete m_supervisor;
                m_supervisor = nullptr;
            }

        private:

            WorkerSupervisor* m_supervisor;
    };
}

void Master::StartService(std::unique_ptr<IService> service)
{
    service->Start();
    m_services.push_back(std::move(service));
}

void Master::StopServices()
{
    // Ask everyone to stop first, then join. Doing it in one pass instead would mean each
    // service is only *told* to stop once the one before it has fully exited, serialising
    // shutdowns that could have overlapped.
    for (const std::unique_ptr<IService>& service : m_services)
    {
        service->RequestStop();
    }

    // Join in reverse order of start, so a service may rely on anything started before it
    // still being alive while it winds down.
    while (!m_services.empty())
    {
        const std::unique_ptr<IService>& service = m_services.back();

        sLog.outString("[shutdown] joining %s...", service->Name());
        service->Join();
        sLog.outString("[shutdown] %s joined", service->Name());

        m_services.pop_back();
    }
}

// -- Run ---------------------------------------------------------------------------

int Master::Run(const std::string& testMode)
{
    // Register this thread with the MySQL client library BEFORE it issues its first query.
    //
    // The main thread is a MySQL client thread like any other: it runs every synchronous
    // query of the startup load — the schema checks in StartDatabases(), then the thousands
    // that SetInitialWorldSettings() fires over the next few minutes. mysql_thread_init()
    // has to precede all of them. It used to be called after the whole load had finished,
    // which is far too late to mean anything; it survived only because mysql_init()
    // initialises the calling thread implicitly, making the explicit call a no-op that
    // looked load-bearing.
    //
    // (No matching ThreadEnd() here on purpose: this thread lives for the whole process, and
    // its thread-local block is released by mysql_library_end() when the DatabaseType globals
    // are destroyed. The threads that genuinely come and go — the SqlDelayThreads — pair
    // their own init/end around run(), which is where it matters.)
    WorldDatabase.ThreadStart();

    ///- Start the databases
    if (!StartDatabases())
    {
        return 1;
    }

    ///- Run an in-process self-test instead of the world, if one was asked for
    if (!testMode.empty())
    {
        const int rc = RunMangosdTest(testMode);
        sLog.outString("mangosd test '%s' exit %d", testMode.c_str(), rc);
        sLog.Flush();
        fflush(stdout);
        _exit(rc);
    }

    // Move console output off the world/map-update threads. Started only after the
    // fallible init above, so an early-return error path never leaves a writer thread
    // running into stdio teardown — but before SetInitialWorldSettings(), whose spawn
    // burst is exactly the hot console path this exists to cover.
    sLog.StartConsoleThread();

    // The writer now owns stdout, so route progress-bar redraws through it too: the bars
    // were previously raw printf from the loading thread and could tear against it. Must
    // follow StartConsoleThread, since ConsoleEmitRaw falls back to a synchronous write
    // whenever the writer is not running.
    BarGoLink::SetConsoleSink(&BarConsoleSink);

    ///- Set Realm to Offline, in case a crash happened. Only used once.
    LoginDatabase.DirectPExecute("UPDATE `realmlist` SET `realmflags` = `realmflags` | %u WHERE `id` = '%u'",
                                 REALM_FLAG_OFFLINE, realmID);

    ///- Initialize the World
    sWorld.SetInitialWorldSettings();

    // A stop can already have been asked for: signals are hooked before Run(), and the load
    // above takes minutes on a cold cache, so Ctrl-C during it is both likely and, until
    // now, useless — nothing looked at the flag until the world loop was reached, by which
    // point the whole load had been paid for anyway. Bail out here instead of standing up a
    // listener and a thread fleet purely to tear them all down on the next line.
    if (World::IsStopped())
    {
        sLog.outString("[shutdown] stop requested during startup; skipping world run");
        StopDatabases();
        return World::GetExitCode();
    }

#ifndef _WIN32
    detachDaemon();
#endif

    // Set the realm flags from configuration, and mark the realm online.
    const uint8 recommendedornew = sWorld.getConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW)
                                 ? REALM_FLAG_NEW_PLAYERS : REALM_FLAG_RECOMMENDED;
    const uint8 realmstatus = sWorld.getConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW_ENABLED)
                            ? recommendedornew : uint8(REALM_FLAG_NONE);

    std::string builds = AcceptableClientBuildsListStr();
    LoginDatabase.escape_string(builds);
    LoginDatabase.DirectPExecute("UPDATE `realmlist` SET `realmflags` = %u, `population` = 0, `realmbuilds` = '%s' WHERE `id` = '%u'",
                                 realmstatus, builds.c_str(), realmID);

    // The server is up: async DB requests are allowed from here (they are forbidden
    // during startup, which is why this is not done any earlier).
    CharacterDatabase.AllowAsyncTransactions();
    WorldDatabase.AllowAsyncTransactions();
    LoginDatabase.AllowAsyncTransactions();

    ///- Start the world listener
    const std::string bindIp = sConfig.GetStringDefault("BindIP", "0.0.0.0");
    const uint16 worldPort = uint16(sWorld.getConfig(CONFIG_UINT32_PORT_WORLD));

    if (!sWorldNetwork.Start(worldPort, bindIp))
    {
        sLog.outError("Failed to start network");
        World::StopNow(ERROR_EXIT_CODE);
        StopDatabases();
        return 1;
    }

    ///- Start the remote access listener
    if (sConfig.GetBoolDefault("Ra.Enable", false))
    {
        StartService(std::unique_ptr<IService>(
            new RaService(uint16(sConfig.GetIntDefault("Ra.Port", 3443)),
                          sConfig.GetStringDefault("Ra.IP", "0.0.0.0"))));
    }

    ///- Start the SOAP listener
    if (sConfig.GetBoolDefault("SOAP.Enabled", false))
    {
#ifdef ENABLE_SOAP
        StartService(std::unique_ptr<IService>(
            new SoapService(sConfig.GetStringDefault("SOAP.IP", "127.0.0.1"),
                            uint16(sConfig.GetIntDefault("SOAP.Port", 7878)))));
#else
        sLog.outError("SOAP is enabled but wasn't included during compilation, not activating it.");
#endif
    }

    ///- Start the freeze detector
    const uint32 stuckTime = uint32(sConfig.GetIntDefault("MaxCoreStuckTime", 0));
    sLog.outString("AntiFreeze: MaxCoreStuckTime = %u, watchdog %s", stuckTime,
                   stuckTime > 0 ? "ARMED" : "DISABLED");
    if (stuckTime)
    {
        StartService(std::unique_ptr<IService>(new FreezeDetectorService(1000 * stuckTime)));
    }

    ///- Start the AH subprocess worker (optional, default-off)
    if (sConfig.GetBoolDefault("AH.Service.Enabled", false))
    {
        StartService(std::unique_ptr<IService>(new AhServiceService()));
    }

    ///- Start the console
#ifdef _WIN32
    const bool consoleEnabled = sConfig.GetBoolDefault("Console.Enable", true) &&
                                (m_ServiceStatus == -1);    // no console when run as a service
#else
    const bool consoleEnabled = sConfig.GetBoolDefault("Console.Enable", true);
#endif
    if (consoleEnabled)
    {
        StartService(std::unique_ptr<IService>(
            new ConsoleService(sConfig.GetBoolDefault("BeepAtStart", true))));
    }

    ///- Run the world. Returns once World::StopNow() has been called and the world has
    ///  finished unwinding.
    WorldLoop();

    sLog.outString("[shutdown] world loop returned; joining auxiliary threads");

    ///- Wind the services down in the reverse of the order they were started.
    StopServices();

    sLog.outString("Halting process...");

    ///- Set the realm offline again
    LoginDatabase.DirectPExecute("UPDATE `realmlist` SET `realmflags` = `realmflags` | %u WHERE `id` = '%u'",
                                 REALM_FLAG_OFFLINE, realmID);

    ///- Clean the account database before leaving
    ClearOnlineAccounts();

    // Send any still-queued mass mails before the DB connections go down.
    sMassMailMgr.Update(true);

    StopDatabases();

    // Unload the script library before it is unloaded automatically: ~ScriptMgr() runs
    // too late, being allocated with static storage.
    sLog.outString("[shutdown] unloading script library...");
    sScriptMgr.UnloadScriptLibrary();
    sLog.outString("[shutdown] script library unloaded");

    return World::GetExitCode();
}
/// @}
