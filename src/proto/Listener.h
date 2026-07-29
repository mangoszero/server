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

#ifndef MANGOS_PROTO_LISTENER_H
#define MANGOS_PROTO_LISTENER_H

#include "IWorldGateway.h"

#include "net/Server.hpp"

#include <cstdint>
#include <string>

namespace proto
{
    /**
     * @brief Accepts world connections and mints one ClientConnection per client.
     *
     * This is all that survives of WorldSocketMgr. Accepting, the thread pool,
     * socket options, output buffering, backpressure and teardown all moved into
     * the shared networking engine, so what is left is the two things that were
     * ever specific to the world server: which port to listen on, and what to make
     * of a connection once it arrives.
     *
     * It is deliberately not a singleton. The old manager was global because the
     * ACE reactor plumbing had no other way to be reached; nothing needs that now,
     * and an owned object makes the shutdown order explicit.
     */
    class Listener
    {
        public:

            explicit Listener(IWorldGateway& gateway);
            ~Listener();

            Listener(const Listener&) = delete;
            Listener& operator=(const Listener&) = delete;

            /**
             * @brief Bind and start accepting.
             *
             * @param port   TCP port to listen on.
             * @param bindIp Interface to bind, or empty for every interface.
             * @return false if the port could not be bound.
             */
            bool Start(uint16_t port, const std::string& bindIp = std::string());

            /// Stop accepting and tear down every live connection.
            void Stop();

        private:

            IWorldGateway& m_gateway;
            net::Server    m_server;
            bool           m_running;
    };
}

#endif
