/*
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

#ifndef MANGOS_LIVING_WORLD_ANCHOR_POLICY_H
#define MANGOS_LIVING_WORLD_ANCHOR_POLICY_H

#include "Creature.h"             // CreatureInfo, CREATURE_ELITE_WORLDBOSS, CREATURE_ELITE_NORMAL
#include "Unit.h"                 // UNIT_NPC_FLAG_FLIGHTMASTER, UNIT_NPC_FLAG_GOSSIP
#include "Server/DBCStructure.h"  // MapEntry
#include "Server/DBCStores.h"     // sFactionTemplateStore, FactionTemplateEntry
#include "Server/DBCEnums.h"      // FactionMasks

// Wave 1 high-confidence LivingWorld anchor categories (bitmask).
enum LivingWorldAnchorCategory
{
    LW_ANCHOR_NONE                 = 0x0,
    LW_ANCHOR_WORLD_BOSS_OR_LEADER = 0x1, // creature rank == WORLDBOSS, continents only
    LW_ANCHOR_FLIGHT_MASTER        = 0x2, // npcflag & FLIGHTMASTER, continents only
    LW_ANCHOR_SETTLEMENT_DEFENDER  = 0x4, // Wave 2: defender-faction continent waypoint patrollers
};

// Pure core: classify by raw template values + continent flag. Continent-gated so
// raid/dungeon/battleground spawns can never qualify. constexpr so it is compile-time testable.
constexpr uint32 LivingWorldAnchorCategoriesFor(uint32 rank, uint32 npcFlags, bool isContinent)
{
    if (!isContinent)
    {
        return LW_ANCHOR_NONE;
    }

    uint32 cats = LW_ANCHOR_NONE;
    if (rank == CREATURE_ELITE_WORLDBOSS)
    {
        cats |= LW_ANCHOR_WORLD_BOSS_OR_LEADER;
    }
    if (npcFlags & UNIT_NPC_FLAG_FLIGHTMASTER)
    {
        cats |= LW_ANCHOR_FLIGHT_MASTER;
    }
    return cats;
}

// Compile-time tests (the pilot's static_assert idiom).
static_assert(LivingWorldAnchorCategoriesFor(CREATURE_ELITE_WORLDBOSS, 0, true) == LW_ANCHOR_WORLD_BOSS_OR_LEADER,
    "continent world boss must match the world-boss/leader category");
static_assert(LivingWorldAnchorCategoriesFor(CREATURE_ELITE_NORMAL, UNIT_NPC_FLAG_FLIGHTMASTER, true) == LW_ANCHOR_FLIGHT_MASTER,
    "continent flight master must match the flight-master category");
static_assert(LivingWorldAnchorCategoriesFor(CREATURE_ELITE_NORMAL, UNIT_NPC_FLAG_FLIGHTMASTER | UNIT_NPC_FLAG_GOSSIP, true) & LW_ANCHOR_FLIGHT_MASTER,
    "multi-role flight master (also gossip) must still match via the FLIGHTMASTER bit");
static_assert(LivingWorldAnchorCategoriesFor(CREATURE_ELITE_WORLDBOSS, UNIT_NPC_FLAG_FLIGHTMASTER, true)
    == (LW_ANCHOR_WORLD_BOSS_OR_LEADER | LW_ANCHOR_FLIGHT_MASTER),
    "a continent creature matching both signals must report both categories");
static_assert(LivingWorldAnchorCategoriesFor(CREATURE_ELITE_WORLDBOSS, UNIT_NPC_FLAG_FLIGHTMASTER, false) == LW_ANCHOR_NONE,
    "non-continent (raid/dungeon/BG) spawns must never qualify, even at WORLDBOSS rank");
static_assert(LivingWorldAnchorCategoriesFor(CREATURE_ELITE_NORMAL, 0, true) == LW_ANCHOR_NONE,
    "ordinary continent creatures must not qualify");

// Runtime helpers used at the two active-object decision points.
inline uint32 GetLivingWorldAnchorCategories(CreatureInfo const* cInfo, MapEntry const* mapEntry)
{
    if (!cInfo || !mapEntry)
    {
        return LW_ANCHOR_NONE;
    }
    return LivingWorldAnchorCategoriesFor(cInfo->Rank, cInfo->NpcFlags, mapEntry->IsContinent());
}

inline bool IsLivingWorldAnchor(CreatureInfo const* cInfo, MapEntry const* mapEntry, uint32 enabledMask)
{
    return (GetLivingWorldAnchorCategories(cInfo, mapEntry) & enabledMask) != 0;
}

// --- pure cores (compile-time testable) ---

// Player-aligned, friendly to a player side, hostile to monsters.
constexpr uint32 LW_PLAYER_SIDES = FACTION_MASK_PLAYER | FACTION_MASK_ALLIANCE | FACTION_MASK_HORDE;

constexpr bool IsLivingWorldDefenderFactionMasks(uint32 ourMask, uint32 friendlyMask, uint32 hostileMask)
{
    return (ourMask     & LW_PLAYER_SIDES)    != 0
        && (friendlyMask & LW_PLAYER_SIDES)   != 0
        && (hostileMask  & FACTION_MASK_MONSTER) != 0;
}

// Any NpcFlags service bit >= 0x80 (vendor/banker/innkeeper/auctioneer/flightmaster/
// spirit-healer...). Gossip(0x1)/questgiver(0x2)/trainer(0x10) are < 0x80 and survive.
constexpr bool IsLivingWorldServiceNpc(uint32 npcFlags)
{
    return (npcFlags & ~uint32(0x7F)) != 0;
}

constexpr uint32 LivingWorldDefenderCategoryFor(uint32 ourMask, uint32 friendlyMask, uint32 hostileMask,
                                                uint32 npcFlags, bool isWaypointMover, bool isContinent)
{
    if (!isContinent || !isWaypointMover)
    {
        return LW_ANCHOR_NONE;
    }
    if (IsLivingWorldServiceNpc(npcFlags))
    {
        return LW_ANCHOR_NONE;
    }
    if (!IsLivingWorldDefenderFactionMasks(ourMask, friendlyMask, hostileMask))
    {
        return LW_ANCHOR_NONE;
    }
    return LW_ANCHOR_SETTLEMENT_DEFENDER;
}

// --- compile-time tests (representative Ironforge-style masks) ---
// Ironforge guard faction: our=ALLIANCE, friendly=PLAYER|ALLIANCE, hostile=HORDE|MONSTER.
static_assert(LivingWorldDefenderCategoryFor(FACTION_MASK_ALLIANCE,
    FACTION_MASK_PLAYER | FACTION_MASK_ALLIANCE, FACTION_MASK_HORDE | FACTION_MASK_MONSTER,
    0, true, true) == LW_ANCHOR_SETTLEMENT_DEFENDER,
    "continent waypoint guard-faction NPC must match the settlement-defender category");
static_assert(LivingWorldDefenderCategoryFor(FACTION_MASK_MONSTER, 0,
    FACTION_MASK_PLAYER | FACTION_MASK_ALLIANCE | FACTION_MASK_HORDE,
    0, true, true) == LW_ANCHOR_NONE,
    "a pure-monster (hostile wildlife) faction must never be a defender");
static_assert(LivingWorldDefenderCategoryFor(FACTION_MASK_ALLIANCE,
    FACTION_MASK_PLAYER | FACTION_MASK_ALLIANCE, FACTION_MASK_HORDE | FACTION_MASK_MONSTER,
    0x80 /*VENDOR*/, true, true) == LW_ANCHOR_NONE,
    "a vendor (service NpcFlag >= 0x80) must be excluded even on a guard faction");
static_assert(LivingWorldDefenderCategoryFor(FACTION_MASK_ALLIANCE,
    FACTION_MASK_PLAYER | FACTION_MASK_ALLIANCE, FACTION_MASK_HORDE | FACTION_MASK_MONSTER,
    0, false /*idle/random*/, true) == LW_ANCHOR_NONE,
    "a stationary (non-waypoint) guard-faction NPC is not a settlement defender");
static_assert(LivingWorldDefenderCategoryFor(FACTION_MASK_ALLIANCE,
    FACTION_MASK_PLAYER | FACTION_MASK_ALLIANCE, FACTION_MASK_HORDE | FACTION_MASK_MONSTER,
    0, true, false /*instance*/) == LW_ANCHOR_NONE,
    "non-continent (raid/dungeon/BG) spawns must never qualify");
static_assert(IsLivingWorldServiceNpc(0x1 | 0x2) == false,
    "gossip/questgiver-only NPCs are not service NPCs");

// --- thin runtime resolver (does the DBC lookup; not constexpr) ---
inline uint32 GetLivingWorldDefenderCategory(CreatureInfo const* cInfo, MapEntry const* mapEntry, bool isWaypointMover)
{
    if (!cInfo || !mapEntry || !mapEntry->IsContinent() || !isWaypointMover)
    {
        return LW_ANCHOR_NONE;
    }
    if (IsLivingWorldServiceNpc(cInfo->NpcFlags))
    {
        return LW_ANCHOR_NONE;
    }
    if (FactionTemplateEntry const* ft = sFactionTemplateStore.LookupEntry(cInfo->FactionAlliance))
    {
        if (IsLivingWorldDefenderFactionMasks(ft->FactionGroup, ft->FriendGroup, ft->EnemyGroup))
        {
            return LW_ANCHOR_SETTLEMENT_DEFENDER;
        }
    }
    if (cInfo->FactionHorde != cInfo->FactionAlliance)
    {
        if (FactionTemplateEntry const* ft = sFactionTemplateStore.LookupEntry(cInfo->FactionHorde))
        {
            if (IsLivingWorldDefenderFactionMasks(ft->FactionGroup, ft->FriendGroup, ft->EnemyGroup))
            {
                return LW_ANCHOR_SETTLEMENT_DEFENDER;
            }
        }
    }
    return LW_ANCHOR_NONE;
}

#endif
