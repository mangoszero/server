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

#ifndef MANGOS_H_OPCODETABLE
#define MANGOS_H_OPCODETABLE

#include "Opcodes.h"
#include "WorldSession.h"

extern void InitializeOpcodes();

enum SessionStatus
{
    STATUS_AUTHED = 0,
    STATUS_LOGGEDIN,
    STATUS_TRANSFER,
    STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT,
    STATUS_NEVER,
    STATUS_UNHANDLED
};

enum PacketProcessing
{
    PROCESS_INPLACE = 0,
    PROCESS_THREADUNSAFE,
    PROCESS_THREADSAFE
};

struct OpcodeHandler
{
    char const* name;
    SessionStatus status;
    PacketProcessing packetProcessing;
    void (WorldSession::*handler)(WorldPacket& recvPacket);
};

extern OpcodeHandler opcodeTable[NUM_MSG_TYPES];

inline const char* LookupOpcodeName(uint16 id)
{
    if (id >= NUM_MSG_TYPES)
    {
        return "Received unknown opcode, it's more than max!";
    }

    return opcodeTable[id].name ? opcodeTable[id].name : "UNKNOWN";
}

#endif
