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

#ifndef MANGOS_PROTO_ICLIENTLINK_H
#define MANGOS_PROTO_ICLIENTLINK_H

#include "WorldPacket.h"

#include <string>

namespace proto
{
    /**
     * @brief Everything the world is allowed to do to a live connection.
     *
     * The other half of the seam. IWorldGateway is what a connection may ask of
     * the world; this is what the world may ask of a connection, and the two
     * together are the complete contract between the layers -- deliberately small,
     * and deliberately abstract in both directions.
     *
     * Holding this rather than a concrete socket buys three things:
     *
     *   - The world can be exercised with a fake link, so session logic becomes
     *     testable without opening a port.
     *   - Lifetime stops being a hazard. A WorldSession used to keep a raw
     *     WorldSocket* and had to be kept in step with the socket's own teardown
     *     by hand; a shared_ptr to this interface simply cannot dangle.
     *   - Nothing about framing, ciphers or byte buffers leaks upward. The world
     *     hands over a WorldPacket and is done.
     *
     * Every method is safe to call from any thread, and safe to call after the
     * peer has gone -- a send on a dead link is a no-op, not a crash. That is what
     * lets a world thread finish ticking a session whose socket died mid-tick.
     */
    class IClientLink
    {
        public:

            virtual ~IClientLink() {}

            /// Encode, encrypt and queue a packet for this client.
            virtual void SendPacket(const WorldPacket& packet) = 0;

            /// Ask the transport to tear the connection down.
            virtual void Close() = 0;

            /// Printable remote address, for logging, bans and IP locking.
            virtual const std::string& GetRemoteAddress() const = 0;

            /// True once the connection is dead and sends are being discarded.
            virtual bool IsClosed() const = 0;
    };
}

#endif
