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

/** \file
 \ingroup mangosd
 */

#include "Common.h"
#include "WorldSocket.h"
#include "WorldSocketMgr.h"
#include "World.h"
#include "WorldThread.h"
#include "Timer.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include "Database/DatabaseEnv.h"

#include <chrono>
#include <thread>

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#define WORLD_SLEEP_CONST 50

#ifdef WIN32
#include "ServiceWin32.h"
extern int m_ServiceStatus;
#include <windows.h>
#endif

#ifdef _WIN32
/// Update the console title with current player/connection counts, only if changed.
static void UpdateConsoleTitle(uint32 players, uint32 connections)
{
    static std::string s_lastTitle;
    char title[128];
    snprintf(title, sizeof(title), "MaNGOS Zero (%u Players - %u Connections)", players, connections);
    std::string newTitle(title);
    if (s_lastTitle != newTitle)
    {
        s_lastTitle = newTitle;
        SetConsoleTitleA(title);
    }
}
#endif

/**
 * Initializes the world thread listener with the configured host and port.
 */
WorldThread::WorldThread(uint16 port, const char* host) : listen_addr(port, host)
{
}

/**
 * Starts the world socket network listener and activates the world thread.
 */
int WorldThread::open(void* unused)
{
    if (sWorldSocketMgr->StartNetwork(listen_addr) == -1)
    {
        sLog.outError("Failed to start network");
        Log::WaitBeforeContinueIfNeed();
        World::StopNow(ERROR_EXIT_CODE);
        return -1;
    }

    activate();
    return 0;
}

/// Heartbeat for the World
int WorldThread::svc()
{
    uint32 realCurrTime = 0;
    uint32 realPrevTime = getMSTime();
    sLog.outString("World Updater Thread started (%dms min update interval)", WORLD_SLEEP_CONST);

    ///- While we have not World::m_stopEvent, update the world
    while (!World::IsStopped())
    {
        ++World::m_worldLoopCounter;
        realCurrTime = getMSTime();

        uint32 diff = getMSTimeDiff(realPrevTime, realCurrTime);

        sWorld.Update(diff);
        realPrevTime = realCurrTime;

#ifdef _WIN32
        static uint32 titleUpdateCounter = 0;
        if ((++titleUpdateCounter) >= 60) // ~3 seconds at WORLD_SLEEP_CONST=50ms
        {
            titleUpdateCounter = 0;
            UpdateConsoleTitle(sWorld.GetActiveSessionCount(), WorldSocket::GetOpenConnectionCount());
        }
#endif

        uint32 executionTimeDiff = getMSTimeDiff(realCurrTime, getMSTime());

        if (executionTimeDiff > 1000)
        {
            sLog.outError("WorldThread::svc: sWorld.Update(diff=%u) took %u ms (loopCounter=%u, sessions=%u)",
                          diff, executionTimeDiff, World::m_worldLoopCounter.value(), sWorld.GetActiveSessionCount());
        }

        // we know exactly how long it took to update the world, if the update took less than WORLD_SLEEP_CONST, sleep for WORLD_SLEEP_CONST - world update time
        if (executionTimeDiff < WORLD_SLEEP_CONST)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(WORLD_SLEEP_CONST - executionTimeDiff));
        }
#ifdef _WIN32
        if (m_ServiceStatus == 0) // service stopped
        {
            World::StopNow(SHUTDOWN_EXIT_CODE);
        }

        while (m_ServiceStatus == 2) // service paused
        {
            Sleep(1000);
        }
#endif
    }
    sLog.outString("[shutdown] world loop stopped; entering world-thread shutdown tail");
    sLog.outString("[shutdown] KickAll: saving + kicking players...");
    sWorld.KickAll();                                       // save and kick all players
    sLog.outString("[shutdown] KickAll done");
    sLog.outString("[shutdown] final UpdateSessions...");
    sWorld.UpdateSessions(1);                               // real players unload required UpdateSessions call
    sLog.outString("[shutdown] final UpdateSessions done");
    sLog.outString("[shutdown] StopNetwork: ending reactor + joining network threads...");
    sWorldSocketMgr->StopNetwork();
    sLog.outString("[shutdown] StopNetwork done");
    sLog.outString("[shutdown] UnloadAll: unloading maps + MapUpdater teardown...");
    sMapMgr.UnloadAll();                                    // unload all grids (including locked in memory)
    sLog.outString("[shutdown] UnloadAll returned; world thread exiting");

    sLog.outString("World Updater Thread stopped");
    return 0;
}
