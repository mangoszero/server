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

#ifndef MANGOS_H_MASTER
#define MANGOS_H_MASTER

#include "Common.h"
#include "RASession.h"
#include "Service.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Brings the world daemon up, runs it, and takes it back down.
 *
 * Owns everything with a lifetime: the databases, the listening sockets, and the
 * auxiliary threads (freeze detector, console, remote access, SOAP, AH service).
 * The world heartbeat itself runs on the caller's thread — Run() only returns once
 * the world has stopped and everything else has been joined.
 *
 * This replaces the WorldThread / CliThread / AntiFreezeThread task objects and the
 * hand-wired teardown that used to reap them.
 */
class Master
{
    public:

        Master() = default;

        Master(const Master&) = delete;
        Master& operator=(const Master&) = delete;

        /**
         * @brief Run the server to completion.
         * @param testMode when non-empty, run that -t self-test against the freshly
         *        opened databases and exit instead of starting the world.
         * @return The process exit code.
         */
        int Run(const std::string& testMode);

    private:

        /// Open the three databases and check their schema versions.
        bool StartDatabases();

        /// Flush and close the databases (in the reverse order they were opened).
        void StopDatabases();

        /// Reset the online flags left behind by an unclean shutdown.
        void ClearOnlineAccounts();

        /// The world heartbeat. Runs on the calling thread and returns once stopped.
        void WorldLoop();

        /**
         * @brief Start an auxiliary service and take ownership of it.
         *
         * Order matters, and now it is the only thing that has to: services are joined in
         * exactly the reverse of the order they were started here, so a service may safely
         * depend on anything started before it.
         */
        void StartService(std::unique_ptr<IService> service);

        /// Ask every service to stop, then join them in reverse order of start.
        void StopServices();

        /// The auxiliary threads: freeze watchdog, console, remote access, SOAP, AH service.
        std::vector<std::unique_ptr<IService>> m_services;
};

#endif
/// @}
