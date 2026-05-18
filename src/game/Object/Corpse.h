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

/**
 * @file Corpse.h
 * @brief Corpse (player death) class definition.
 *
 * This file defines the Corpse class which represents the remains of a dead player.
 * Corpses can be in different states (bones, resurrectable for PvE/PvP) and have
 * specific mechanics around resurrection, loot, and decay.
 *
 * Key functionality includes:
 * - Corpse creation on player death
 * - Corpse persistence and database storage
 * - Corpse decay and expiration
 * - Resurrection mechanics and timers
 * - Loot management for corpse contents
 * - PvP corpse handling and resurrection rules
 * - Corpse visibility and ghosting
 * - Bones vs. resurrectable corpse states
 *
 * @see Corpse for the main corpse implementation
 * @see Player for player death handling
 * @see ObjectAccessor for corpse registry
 */

#ifndef MANGOSSERVER_CORPSE_H
#define MANGOSSERVER_CORPSE_H

#include "Common.h"
#include "Object.h"
#include "Database/DatabaseEnv.h"
#include "GridDefines.h"
#include "LootMgr.h"

/// @brief Corpse type enumeration.
///
/// Indicates what state the corpse is in and under what resurrection rules.
enum CorpseType
{
    CORPSE_BONES             = 0,
    CORPSE_RESURRECTABLE_PVE = 1,
    CORPSE_RESURRECTABLE_PVP = 2
};
#define MAX_CORPSE_TYPE        3

// Value equal to client resurrection dialog show radius (in game units)
#define CORPSE_RECLAIM_RADIUS 39

/// @brief Corpse display and behavior flags.
///
/// Controls how the corpse appears and what actions are available.
enum CorpseFlags
{
    CORPSE_FLAG_NONE        = 0x00,
    CORPSE_FLAG_BONES       = 0x01,
    CORPSE_FLAG_UNK1        = 0x02,
    CORPSE_FLAG_UNK2        = 0x04,
    CORPSE_FLAG_HIDE_HELM   = 0x08,
    CORPSE_FLAG_HIDE_CLOAK  = 0x10,
    CORPSE_FLAG_LOOTABLE    = 0x20
};

/// @brief Player corpse class.
///
/// Represents a player's remains after death. Corpses can be resurrected and
/// contain the player's items for recovery or looting.
class Corpse : public WorldObject
{
    public:
        explicit Corpse(CorpseType type = CORPSE_BONES);
        ~Corpse();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool Create(uint32 guidlow);
        bool Create(uint32 guidlow, Player* owner);

        void SaveToDB();
        bool LoadFromDB(uint32 guid, Field* fields);

        void DeleteBonesFromWorld();
        void DeleteFromDB();

        ObjectGuid const& GetOwnerGuid() const { return GetGuidValue(CORPSE_FIELD_OWNER); }

        time_t const& GetGhostTime() const { return m_time; }
        void ResetGhostTime() { m_time = time(NULL); }
        CorpseType GetType() const { return m_type; }

        bool IsHostileTo(Unit const* unit) const override;
        bool IsFriendlyTo(Unit const* unit) const override;

        bool IsControlledByPlayer() const override { return true; }

        GridPair const& GetGrid() const { return m_grid; }
        void SetGrid(GridPair const& grid) { m_grid = grid; }

        bool IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const override;

        Loot loot;                                          // remove insignia ONLY at BG
        Player* lootRecipient;
        bool lootForBody;

        GridReference<Corpse>& GetGridRef() { return m_gridRef; }

        bool IsExpired(time_t t) const;
    private:
        GridReference<Corpse> m_gridRef;

        CorpseType m_type;
        time_t m_time;
        GridPair m_grid;                                    // gride for corpse position for fast search
};
#endif
