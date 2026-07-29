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

#ifndef MANGOS_H_SERVICE
#define MANGOS_H_SERVICE

#include <thread>
#include <string>

/**
 * @brief A background activity that runs alongside the world loop.
 *
 * The console reader, remote administration, SOAP and the freeze watchdog were
 * four separate classes with four different shapes -- one ACE_Task_Base, one
 * reactor handler, one raw std::thread, and one more ACE task -- each started
 * and stopped by hand from main(), in an order that had to be remembered rather
 * than expressed. This is the one shape they all share.
 *
 * The contract is deliberately three calls, because shutdown needs all three
 * separated: RequestStop() must be able to run for every service before any of
 * them blocks in Join(), or stopping N services takes the sum of their timeouts
 * instead of the longest.
 */
class IService
{
    public:

        virtual ~IService() {}

        /// Human-readable name, used only for start-up and shutdown logging.
        virtual const char* Name() const = 0;

        /// Begin work. Called once, from the main thread, before the world loop.
        virtual void Start() = 0;

        /**
         * @brief Ask the service to wind down. Must not block.
         *
         * Called for every service before any Join(), and may arrive on a
         * different thread than Start(). Implementations set a flag, close a
         * socket, or wake a condition variable -- nothing that waits.
         */
        virtual void RequestStop() {}

        /// Wait for the service to finish. May block; called after RequestStop().
        virtual void Join() = 0;
};

#endif
