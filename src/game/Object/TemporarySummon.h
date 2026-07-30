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

#ifndef MANGOSSERVER_TEMPSUMMON_H
#define MANGOSSERVER_TEMPSUMMON_H

#include "Creature.h"
#include "ObjectLookup.h"

class TemporarySummon : public Creature
{
    public:
        explicit TemporarySummon(ObjectGuid summoner = ObjectGuid());
        virtual ~TemporarySummon() {};

        void Update(uint32 update_diff, uint32 time) override;
        void Summon(TempSpawnType type, uint32 lifetime);
        void  UnSummon();
        void SaveToDB() override;
        void RemoveFromWorld() override;
        ObjectGuid const& GetSummonerGuid() const { return m_summoner ; }
        Unit* GetSummoner() const { return ObjectLookup::GetUnit(*this, m_summoner); }
    private:
        TempSpawnType m_type;
        uint32 m_timer;
        uint32 m_lifetime;
        ObjectGuid m_summoner;
};

class TemporarySummonWaypoint : public TemporarySummon
{
    public:
        explicit TemporarySummonWaypoint(ObjectGuid summoner, uint32 waypoint_id, int32 path_id, uint32 pathOrigin);

        uint32 GetWaypointId() const { return m_waypoint_id; }
        int32 GetPathId() const { return m_path_id; }
        uint32 GetPathOrigin() const { return m_pathOrigin; }

    private:
        uint32 m_waypoint_id;
        int32 m_path_id;
        uint32 m_pathOrigin;
};

#endif
