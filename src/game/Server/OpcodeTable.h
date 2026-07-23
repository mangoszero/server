/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * multiple client versions.
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
