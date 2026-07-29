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

#ifndef MANGOS_H_MASTER
#define MANGOS_H_MASTER

#include "Service.h"

#include "Platform/Define.h"

#include <memory>
#include <vector>

/**
 * @brief Owns the server's lifetime: databases, the world loop, and services.
 *
 * The world loop runs on the calling thread rather than a spawned one. That is
 * the one structural change worth pointing at: previously main() started a
 * WorldThread and then blocked waiting for it, so there were two threads where
 * one would do, and the shutdown tail (kick players, drain sessions, stop the
 * listener, unload maps) lived inside that thread's body where nothing else
 * could sequence against it. Running the loop here makes the order plain --
 * everything after Run() returns happens strictly after the last world tick.
 *
 * Services (console, remote administration, SOAP, freeze watchdog) are started
 * in registration order and stopped in reverse, with every RequestStop() issued
 * before the first Join().
 */
class Master
{
    public:

        Master();
        ~Master();

        Master(const Master&) = delete;
        Master& operator=(const Master&) = delete;

        /**
         * @brief Bring the server up, run the world, and shut it down again.
         *
         * @return The process exit code.
         */
        int Run();

    private:

        bool StartDatabases();
        void StopDatabases();
        void ClearOnlineAccounts();

        void StartServices();
        void StopServices();

        /// The world heartbeat. Returns when World::IsStopped() becomes true.
        void WorldLoop();

        /// Push the once-a-second figures into the full-screen console's status
        /// region. A no-op when that console is not running.
        void PublishConsoleStatus(uint32 diff);

        /// Everything that must happen after the final world tick.
        void ShutdownWorld();

        std::vector<std::unique_ptr<IService>> m_services;
};

#endif
