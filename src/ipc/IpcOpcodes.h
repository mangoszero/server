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
 */

#ifndef AH_IPC_OPCODES_H
#define AH_IPC_OPCODES_H

#include "Common.h"

/**
 * @brief IPC opcode enumeration for the AH subprocess wire protocol.
 *
 * Milestone 1 opcodes occupy 0x0001-0x0FFF.
 * 0x1000+ is reserved for AH consumer messages (Milestone 2).
 */
enum IpcOpcode : uint16
{
    IPC_HELLO           = 0x0001,   ///< Service -> mangosd: initial greeting
    IPC_HELLO_ACK       = 0x0002,   ///< mangosd -> service: greeting ack
    IPC_READY           = 0x0003,   ///< Service -> mangosd: ready
    IPC_HEARTBEAT       = 0x0004,   ///< Either direction: keepalive ping
    IPC_HEARTBEAT_ACK   = 0x0005,   ///< Either direction: keepalive pong
    IPC_GAMETIME        = 0x0006,   ///< mangosd -> service: server game time
    IPC_CONSOLE         = 0x0007,   ///< Either direction: console/log text
    IPC_SHUTDOWN        = 0x0008,   ///< Either direction: graceful shutdown
    IPC_SHUTDOWN_ACK    = 0x0009,   ///< Either direction: shutdown acknowledged
    IPC_ECHO            = 0x000A,   ///< Debug echo request
    IPC_ECHO_REPLY      = 0x000B,   ///< Debug echo reply

    // 0x1000+ reserved for AH consumer (Milestone 2)
    IPC_AH_RESERVED_MIN  = 0x1000,  ///< Boundary sentinel (not a real opcode)

    IPC_INTENT_SELL      = 0x1001,  ///< ah-service -> mangosd: place sell listing
    IPC_INTENT_BID       = 0x1002,  ///< ah-service -> mangosd: place bid
    IPC_INTENT_BUYOUT    = 0x1003,  ///< ah-service -> mangosd: instant buyout
    IPC_INTENT_RESULT    = 0x1010,  ///< mangosd -> ah-service: intent outcome
    IPC_QUEUE_FULL       = 0x1011,  ///< mangosd -> ah-service: AH queue full
    IPC_GMCMD            = 0x1020,  ///< mangosd -> ah-service: GM command relay
    IPC_GMCMD_RESULT     = 0x1021,  ///< ah-service -> mangosd: GM command result
    IPC_BROWSE_QUERY     = 0x1030,  ///< mangosd -> ah-service: AH read/browse request
    IPC_BROWSE_RESULT    = 0x1031,  ///< ah-service -> mangosd: AH read/browse reply

    // --- SP-2 player-mutation + resolve frames (write-authority worker) ---
    IPC_PLAYER_SELL           = 0x1040,  ///< mangosd -> worker: player listing
    IPC_PLAYER_BID            = 0x1041,  ///< mangosd -> worker: player bid
    IPC_PLAYER_BUYOUT         = 0x1042,  ///< mangosd -> worker: player buyout
    IPC_PLAYER_CANCEL         = 0x1043,  ///< mangosd -> worker: cancel PREPARE
    IPC_PLAYER_RESULT         = 0x1044,  ///< worker -> mangosd: mutation outcome + facts
    IPC_RESOLVE_APPLY         = 0x1045,  ///< worker -> mangosd: worker-initiated resolution
    IPC_RESOLVE_ACK           = 0x1046,  ///< mangosd -> worker: APPLIED|FAILED|DUPLICATE
    IPC_PLAYER_CANCEL_CONFIRM = 0x1047,  ///< mangosd -> worker: cancel commit
    IPC_PLAYER_CANCEL_ABORT   = 0x1048,  ///< mangosd -> worker: cancel abort/unlock
};

#endif // AH_IPC_OPCODES_H
