/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include <memory>
#include "Master.h"

#include "AntiFreezeService.h"
#include "CliService.h"
#include "RASession.h"
#include "AhService.h"

#include "Config/Config.h"
#include "Console/ConsoleUI.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "MangosdTest.h"
#include "MapManager.h"
#include "Server/WorldNetwork.h"
#include "SystemConfig.h"
#include "Timer.h"
#include "World.h"

#ifdef ENABLE_SOAP
#include "SOAP/SoapService.h"
#include <thread>
#endif

#ifdef _WIN32
#include "ServiceWin32.h"
extern int m_ServiceStatus;
#else
#include "PosixDaemon.h"
#endif

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

/// Shortest interval between two world ticks, in milliseconds.
#ifndef WORLD_SLEEP_CONST
#define WORLD_SLEEP_CONST 50
#endif

extern uint32 realmID;

namespace
{
    /**
     * @brief Open one database and verify its schema version.
     *
     * @param db          The database to open.
     * @param label       Name used in log messages.
     * @param infoKey     Config key holding the connection string.
     * @param countKey    Config key holding the extra connection count.
     * @param versionKind Which schema this database is expected to carry.
     * @return false if the database could not be opened or is the wrong version.
     */
    bool OpenDatabase(Database& db, const char* label, const char* infoKey,
                      const char* countKey, DatabaseTypes versionKind)
    {
        const std::string info = sConfig.GetStringDefault(infoKey, "");
        if (info.empty())
        {
            sLog.outError("%s database not specified in the configuration file", label);
            return false;
        }

        const int connections = sConfig.GetIntDefault(countKey, 1);
        sLog.outString("%s database total connections: %i", label, connections + 1);

        if (!db.Initialize(info.c_str(), connections))
        {
            sLog.outError("Cannot connect to the %s database", label);
            return false;
        }

        return db.CheckDatabaseVersion(versionKind);
    }

#ifdef _WIN32
    /**
     * @brief Show the live player and connection counts in the window title.
     *
     * The title belongs to the OS window, not to the console frame, so it stays
     * readable while the server is minimised or buried in the taskbar -- which
     * the in-frame status bar is not. Rewritten only when the text actually
     * changes, because SetConsoleTitleA is a cross-process call.
     *
     * @param players     Currently active sessions.
     * @param connections Currently open socket connections.
     */
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
}

Master::Master()
{
}

Master::~Master()
{
}

bool Master::StartDatabases()
{
    // Opened in order, and unwound in reverse on failure. The predecessor
    // repeated the whole unwind at each of eight failure points, which is how a
    // missed HaltDelayThread() hides: every new early return has to remember the
    // full list of everything opened so far.
    if (!OpenDatabase(WorldDatabase, "World", "WorldDatabaseInfo",
                      "WorldDatabaseConnections", DATABASE_WORLD))
    {
        WorldDatabase.HaltDelayThread();
        return false;
    }

    if (!OpenDatabase(CharacterDatabase, "Character", "CharacterDatabaseInfo",
                      "CharacterDatabaseConnections", DATABASE_CHARACTER))
    {
        CharacterDatabase.HaltDelayThread();
        WorldDatabase.HaltDelayThread();
        return false;
    }

    if (!OpenDatabase(LoginDatabase, "Login", "LoginDatabaseInfo",
                      "LoginDatabaseConnections", DATABASE_REALMD))
    {
        LoginDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        WorldDatabase.HaltDelayThread();
        return false;
    }

    realmID = sConfig.GetIntDefault("RealmID", 0);
    if (!realmID)
    {
        sLog.outError("Realm ID not defined in the configuration file");
        StopDatabases();
        return false;
    }

    sLog.outString("Realm running as realm ID %u", realmID);
    return true;
}

void Master::StopDatabases()
{
    LoginDatabase.HaltDelayThread();
    CharacterDatabase.HaltDelayThread();
    WorldDatabase.HaltDelayThread();
}

void Master::ClearOnlineAccounts()
{
    // Reset the online flags for this realm only: another realm sharing the same
    // login database may be up, and its sessions are none of our business.
    LoginDatabase.PExecute(
        "UPDATE `account` SET `active_realm_id` = 0 WHERE `active_realm_id` = '%u'",
        realmID);

    CharacterDatabase.Execute("UPDATE `characters` SET `online` = 0 WHERE `online` <> 0");

    CharacterDatabase.Execute("DELETE FROM `character_battleground_data`");

    CharacterDatabase.Execute(
        "UPDATE `groups` SET `leaderGuid` = (SELECT `memberGuid` FROM `group_member` "
        "WHERE `group_member`.`groupId` = `groups`.`groupId` ORDER BY `assistant` DESC LIMIT 1) "
        "WHERE `leaderGuid` NOT IN (SELECT `memberGuid` FROM `group_member` "
        "WHERE `group_member`.`groupId` = `groups`.`groupId`)");
}

void Master::StartServices()
{
    // Remote administration, over the same networking engine the world uses.
    if (sConfig.GetBoolDefault("Ra.Enable", false))
    {
        m_services.push_back(std::unique_ptr<IService>(new RaService(
            uint16(sConfig.GetIntDefault("Ra.Port", 3443)),
            sConfig.GetStringDefault("Ra.IP", "0.0.0.0"))));
    }

#ifdef ENABLE_SOAP
    if (sConfig.GetBoolDefault("SOAP.Enabled", false))
    {
        m_services.push_back(std::unique_ptr<IService>(new SoapService(
            sConfig.GetStringDefault("SOAP.IP", "127.0.0.1"),
            uint16(sConfig.GetIntDefault("SOAP.Port", 7878)))));
    }
#else
    if (sConfig.GetBoolDefault("SOAP.Enabled", false))
    {
        sLog.outError("SOAP is enabled in the configuration but was not compiled in; ignoring.");
    }
#endif

    // The out-of-process auction house. mangos_zero only, default-off.
    if (sConfig.GetBoolDefault("AH.Service.Enabled", false))
    {
        m_services.push_back(std::unique_ptr<IService>(new AhServiceService()));
    }

    // Watchdog. Disabled unless MaxCoreStuckTime is set.
    m_services.push_back(std::unique_ptr<IService>(new AntiFreezeService(
        1000 * uint32(sConfig.GetIntDefault("MaxCoreStuckTime", 0)))));

    // Console last, so its prompt lands after every other start-up line.
#ifdef _WIN32
    const bool consoleWanted = sConfig.GetBoolDefault("Console.Enable", true)
                            && m_ServiceStatus == -1;   // no console in service mode
#else
    const bool consoleWanted = sConfig.GetBoolDefault("Console.Enable", true);
#endif
    if (consoleWanted)
    {
        m_services.push_back(std::unique_ptr<IService>(
            new CliService(sConfig.GetBoolDefault("Console.BeepAtStart", true))));
    }

    for (auto& service : m_services)
    {
        service->Start();
    }
}

void Master::StopServices()
{
    // Ask everything to wind down first, then wait. Doing this in one pass per
    // service would serialise the timeouts: the total would be the sum of every
    // service's shutdown rather than the longest one.
    for (auto& service : m_services)
    {
        service->RequestStop();
    }

    for (auto itr = m_services.rbegin(); itr != m_services.rend(); ++itr)
    {
        sLog.outString("[shutdown] stopping %s", (*itr)->Name());
        (*itr)->Join();
    }

    m_services.clear();
}

void Master::PublishConsoleStatus(uint32 diff)
{
#ifdef _WIN32
    // Before the early-out below: the window title is owned by the OS rather
    // than by the console frame, so it is kept current even when the
    // full-screen UI is switched off and there is no status bar to publish to.
    UpdateConsoleTitle(sWorld.GetActiveSessionCount(),
                       sWorldNetwork.GetOpenConnectionCount());
#endif

    MaNGOS::Console::ConsoleUI& ui = MaNGOS::Console::ConsoleUI::Instance();
    if (!ui.Active())
    {
        return;
    }

    const uint32 uptime = sWorld.GetUptime();
    char buf[64];

    snprintf(buf, sizeof(buf), "%u / %u", sWorld.GetActiveSessionCount(),
             sWorld.GetPlayerAmountLimit());
    ui.SetStatus(0, "Players", buf);

    ui.SetStatus(1, "Queue", std::to_string(sWorld.GetQueuedSessionCount()),
                 sWorld.GetQueuedSessionCount() ? MaNGOS::Console::STYLE_WARN
                                                : MaNGOS::Console::STYLE_NORMAL);

    snprintf(buf, sizeof(buf), "%u ms", diff);
    ui.SetStatus(2, "Diff", buf, diff > WORLD_SLEEP_CONST ? MaNGOS::Console::STYLE_WARN
                                                          : MaNGOS::Console::STYLE_SUCCESS);

    snprintf(buf, sizeof(buf), "%ud %02u:%02u:%02u", uptime / 86400,
             (uptime / 3600) % 24, (uptime / 60) % 60, uptime % 60);
    ui.SetStatus(3, "Uptime", buf);
}

void Master::WorldLoop()
{
    sLog.outString("World updater started (%dms minimum update interval)",
                   WORLD_SLEEP_CONST);

    uint32 previous = getMSTime();
    uint32 lastStatus = 0;

    while (!World::IsStopped())
    {
        // Read by the freeze watchdog to tell a busy server from a wedged one.
        ++World::m_worldLoopCounter;

        const uint32 current = getMSTime();
        sWorld.Update(getMSTimeDiff(previous, current));
        previous = current;

        const uint32 spent = getMSTimeDiff(current, getMSTime());

        if (getMSTimeDiff(lastStatus, current) >= 1000)
        {
            lastStatus = current;
            PublishConsoleStatus(spent);
        }

        if (spent < WORLD_SLEEP_CONST)
        {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(WORLD_SLEEP_CONST - spent));
        }

#ifdef _WIN32
        if (m_ServiceStatus == 0)               // service stopped
        {
            World::StopNow(SHUTDOWN_EXIT_CODE);
        }
        while (m_ServiceStatus == 2)            // service paused
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
#endif
    }

    sLog.outString("World updater stopped.");
}

void Master::ShutdownWorld()
{
    // Strict order, and it matters: players must be saved before their sessions
    // are drained, sessions must be drained before the listener goes away, and
    // the listener must be gone before the maps they live on are unloaded.
    sLog.outString("[shutdown] saving and kicking all players");
    sWorld.KickAll();

    sLog.outString("[shutdown] draining remaining sessions");
    sWorld.UpdateSessions(1);

    sLog.outString("[shutdown] stopping the world listener");
    sWorldNetwork.Stop();

    sLog.outString("[shutdown] unloading maps");
    sMapMgr.UnloadAll();
}

int Master::Run(std::string const& testMode)
{
    if (!StartDatabases())
    {
        return 1;
    }

    if (!testMode.empty())
    {
        int const rc = RunMangosdTest(testMode);
        sLog.outString("mangosd test '%s' exit %d", testMode.c_str(), rc);
        sLog.Flush();
        fflush(stdout);
        _exit(rc);
    }

    ClearOnlineAccounts();

    sWorld.SetInitialWorldSettings();

#ifndef _WIN32
    detachDaemon();
#endif

    // Publish this realm's flags and the client builds it accepts.
    const uint8 recommendedOrNew =
        sWorld.getConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW)
            ? REALM_FLAG_NEW_PLAYERS : REALM_FLAG_RECOMMENDED;
    const uint8 realmStatus =
        sWorld.getConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW_ENABLED)
            ? recommendedOrNew : uint8(REALM_FLAG_NONE);

    std::string builds = AcceptableClientBuildsListStr();
    LoginDatabase.escape_string(builds);
    LoginDatabase.DirectPExecute(
        "UPDATE `realmlist` SET `realmflags` = %u, `population` = 0, "
        "`realmbuilds` = '%s' WHERE `id` = '%u'",
        realmStatus, builds.c_str(), realmID);

    // Async transactions are forbidden during start-up; enable them only now
    // that the world is fully loaded.
    WorldDatabase.ThreadStart();
    CharacterDatabase.AllowAsyncTransactions();
    WorldDatabase.AllowAsyncTransactions();
    LoginDatabase.AllowAsyncTransactions();

    if (!sWorldNetwork.Start(uint16(sWorld.getConfig(CONFIG_UINT32_PORT_WORLD)),
                             sConfig.GetStringDefault("BindIP", "0.0.0.0")))
    {
        StopDatabases();
        return 1;
    }

    StartServices();

    WorldLoop();

    ShutdownWorld();
    StopServices();

    ClearOnlineAccounts();
    StopDatabases();

    WorldDatabase.ThreadEnd();

    sLog.outString("Halting process...");
    return World::GetExitCode();
}
