/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/// \addtogroup mangosd
/// @{
/// \file

#ifndef MANGOS_H_SERVICE
#define MANGOS_H_SERVICE

/**
 * @brief One auxiliary thread of the daemon, with a uniform lifecycle.
 *
 * The freeze watchdog, the console reader, the remote-access listener and SOAP each used to
 * carry their own bespoke start and their own bespoke shutdown dance, hand-wired into
 * Master::Run() in an order that existed only as the sequence those statements happened to
 * appear in. Nobody had written that order down, and nothing enforced it — so the teardown
 * was correct by coincidence rather than by construction, and every new background thread
 * added another private ritual to remember.
 *
 * Behind this interface they all start the same way, are asked to stop the same way, and are
 * joined in exactly the reverse of the order they were started.
 *
 * The stop *signal* is not part of this interface: it is global (World::StopNow /
 * World::IsStopped), and every service loop already watches it. RequestStop() exists only
 * for the services that park in a blocking call and therefore need a nudge before they can
 * notice a flag that was set while they were asleep.
 */
class IService
{
    public:

        virtual ~IService() = default;

        /// Name of this service, for the shutdown log.
        virtual const char* Name() const = 0;

        /// Bring the service up. Called exactly once.
        virtual void Start() = 0;

        /**
         * @brief Wake a service that is parked in a blocking call.
         *
         * The world has already been asked to stop by the time this runs; a service whose
         * loop simply polls World::IsStopped() needs nothing here and can take the default.
         * Only a service blocked inside a syscall it cannot otherwise leave (the console
         * reader, sitting in fgets()) has to override this.
         */
        virtual void RequestStop() {}

        /// Block until the service's thread has exited. Called once, after RequestStop().
        virtual void Join() = 0;
};

#endif
/// @}
