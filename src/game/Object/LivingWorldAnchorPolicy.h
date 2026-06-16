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

// Wave 1 high-confidence LivingWorld anchor categories (bitmask).
enum LivingWorldAnchorCategory
{
    LW_ANCHOR_NONE                 = 0x0,
    LW_ANCHOR_WORLD_BOSS_OR_LEADER = 0x1, // creature rank == WORLDBOSS, continents only
    LW_ANCHOR_FLIGHT_MASTER        = 0x2, // npcflag & FLIGHTMASTER, continents only
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

#endif
