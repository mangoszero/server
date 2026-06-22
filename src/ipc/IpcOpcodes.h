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
    IPC_HELLO_ACK       = 0x0002,   ///< mangosd -> service: greeting acknowledged
    IPC_READY           = 0x0003,   ///< Service -> mangosd: ready to process work
    IPC_HEARTBEAT       = 0x0004,   ///< Either direction: keepalive ping
    IPC_HEARTBEAT_ACK   = 0x0005,   ///< Either direction: keepalive pong
    IPC_GAMETIME        = 0x0006,   ///< mangosd -> service: current server game time
    IPC_CONSOLE         = 0x0007,   ///< Either direction: console/log text
    IPC_SHUTDOWN        = 0x0008,   ///< Either direction: graceful shutdown request
    IPC_SHUTDOWN_ACK    = 0x0009,   ///< Either direction: shutdown acknowledged
    IPC_ECHO            = 0x000A,   ///< Debug echo request
    IPC_ECHO_REPLY      = 0x000B,   ///< Debug echo reply

    // 0x1000+ reserved for AH consumer (Milestone 2)
    IPC_AH_RESERVED_MIN = 0x1000,
};

#endif // AH_IPC_OPCODES_H
