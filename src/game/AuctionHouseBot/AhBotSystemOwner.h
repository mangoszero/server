#ifndef MANGOS_AHBOT_SYSTEM_OWNER_H
#define MANGOS_AHBOT_SYSTEM_OWNER_H

#include "Common.h"      // uint32
#include "ObjectGuid.h"  // ObjectGuid, HIGHGUID_PLAYER

// Forged "system" owner for the out-of-process AH bot: a reserved player
// low-GUID under this fixed name with NO characters row. See
// Memory/Future_Work/AH_SubProcess/2026-06-29-ah-bot-forged-system-owner-design.md
#define AHBOT_SYSTEM_OWNER_NAME "AuctionHouse"

// Top of the uint32 player-GUID range. The allocator's overflow guard
// (ObjectGuid.cpp: m_nextGuid >= GetMaxCounter - 1 == 0xFFFFFFFE) shuts the
// server down before this value could ever be assigned, so it cannot collide
// with a real character.
static uint32 const AHBOT_SYSTEM_OWNER_GUID = 0xFFFFFFFE;

/// True if @p guid is the AH bot's forged system owner.
inline bool IsAhBotSystemOwnerGuid(ObjectGuid guid)
{
    return guid.IsPlayer() && guid.GetCounter() == AHBOT_SYSTEM_OWNER_GUID;
}

/// Defensive: if @p candidate is the reserved system GUID, step past it.
inline uint32 SkipAhBotSystemOwnerGuid(uint32 candidate)
{
    if (candidate == AHBOT_SYSTEM_OWNER_GUID)
    {
        return candidate + 1;
    }
    return candidate;
}

#endif // MANGOS_AHBOT_SYSTEM_OWNER_H
