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

#include "Utilities/Errors.h"
#include "CorpseManager.h"

#include "CellImpl.h"
#include "Corpse.h"
#include "GridNotifiers.h"
#include "Log.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "World.h"

#include <ctime>
#include <forward_list>


CorpseManager::~CorpseManager()
{
    // The owner index is the one that owns the corpse objects themselves.
    m_byOwner.WithExclusive([](MaNGOS::ConcurrentRegistry<ObjectGuid, Corpse>::MapType& map)
    {
        for (auto& itr : map)
        {
            itr.second->RemoveFromWorld();
            delete itr.second;
        }
        map.clear();
    });
}

// --- lookup -----------------------------------------------------------------

Corpse* CorpseManager::Find(ObjectGuid corpseGuid) const
{
    return m_corpses.Find(corpseGuid);
}

Corpse* CorpseManager::FindInMap(ObjectGuid corpseGuid, uint32 mapId) const
{
    Corpse* corpse = m_corpses.Find(corpseGuid);
    if (!corpse || corpse->GetMapId() != mapId)
    {
        return nullptr;
    }
    return corpse;
}

Corpse* CorpseManager::FindForPlayer(ObjectGuid playerGuid) const
{
    Corpse* corpse = m_byOwner.Find(playerGuid);
    if (!corpse)
    {
        return nullptr;
    }

    MANGOS_ASSERT(corpse->GetType() != CORPSE_BONES);
    return corpse;
}

// --- registry ---------------------------------------------------------------

void CorpseManager::AddObject(Corpse* corpse)
{
    m_corpses.Insert(corpse->GetObjectGuid(), corpse);
}

void CorpseManager::RemoveObject(Corpse* corpse)
{
    m_corpses.Remove(corpse->GetObjectGuid());
}

// --- lifecycle --------------------------------------------------------------

void CorpseManager::RecordCell(Corpse* corpse)
{
    const CellPair cellPair =
        MaNGOS::ComputeCellPair(corpse->Where().X(), corpse->Where().Y());
    const uint32 cellId =
        (cellPair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cellPair.x_coord;

    sObjectMgr.AddCorpseCellData(corpse->GetMapId(), cellId,
                                 corpse->GetOwnerGuid().GetCounter(),
                                 corpse->GetInstanceId());
}

void CorpseManager::ForgetCell(Corpse* corpse)
{
    const CellPair cellPair =
        MaNGOS::ComputeCellPair(corpse->Where().X(), corpse->Where().Y());
    const uint32 cellId =
        (cellPair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cellPair.x_coord;

    sObjectMgr.DeleteCorpseCellData(corpse->GetMapId(), cellId,
                                    corpse->GetObjectGuid().GetCounter());
}

void CorpseManager::Add(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    m_byOwner.Insert(corpse->GetOwnerGuid(), corpse);
    RecordCell(corpse);
}

void CorpseManager::Remove(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    // Only touch the cell data if we were actually tracking this corpse, which
    // is what the old code's find-then-bail guard was for.
    if (!m_byOwner.Find(corpse->GetOwnerGuid()))
    {
        return;
    }

    ForgetCell(corpse);
    corpse->RemoveFromWorld();
    m_byOwner.Remove(corpse->GetOwnerGuid());
}

void CorpseManager::AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map)
{
    m_byOwner.ForEach([&gridpair, &grid, map](Corpse* corpse)
    {
        if (corpse->GetGrid() != gridpair)
        {
            return;
        }

        // On an instanceable map take only the corpses belonging to this instance.
        if (map->Instanceable())
        {
            if (corpse->GetInstanceId() == map->GetInstanceId())
            {
                grid.AddWorldObject(corpse);
            }
        }
        else
        {
            grid.AddWorldObject(corpse);
        }
    });
}

Corpse* CorpseManager::ConvertCorpseForPlayer(ObjectGuid playerGuid, bool insignia)
{
    Corpse* corpse = FindForPlayer(playerGuid);
    if (!corpse)
    {
        // Called from several places that cannot know whether a corpse exists;
        // absence is normal, not an error.
        return nullptr;
    }

    DEBUG_LOG("Deleting Corpse and spawning bones.");

    Remove(corpse);

    // Drop the resurrectable corpse from the grid, but never load the map just
    // to do so.
    Map* map = sMapMgr.FindMap(corpse->GetMapId(), corpse->GetInstanceId());
    if (map)
    {
        map->Remove(corpse, false);
    }

    corpse->DeleteFromDB();

    Corpse* bones = nullptr;

    // Bones appear only where the grid is actually loaded. Insignia handling
    // overrides the configuration switch.
    const bool bonesAllowed = insignia
        || (map && map->IsBattleGround()
                ? sWorld.getConfig(CONFIG_BOOL_DEATH_BONES_BG)
                : sWorld.getConfig(CONFIG_BOOL_DEATH_BONES_WORLD));

    if (map && bonesAllowed
        && !map->IsRemovalGrid(corpse->Where().X(), corpse->Where().Y()))
    {
        bones = new Corpse;
        bones->Create(corpse->GetGUIDLow());

        // Copy every field except the GUID and object type, which the new object
        // has already set for itself.
        for (int i = 3; i < CORPSE_END; ++i)
        {
            bones->SetUInt32Value(i, corpse->GetUInt32Value(i));
        }

        bones->SetGrid(corpse->GetGrid());
        bones->Place().MoveTo(corpse->Where().X(), corpse->Where().Y(), corpse->Where().Z(), corpse->Where().Facing());
        // TBC has no phasing, so there is no mask to carry over to the bones.
        bones->SetUInt32Value(CORPSE_FIELD_FLAGS, CORPSE_FLAG_UNK2 | CORPSE_FLAG_BONES);
        bones->SetGuidValue(CORPSE_FIELD_OWNER, ObjectGuid());

        for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (corpse->GetUInt32Value(CORPSE_FIELD_ITEM + i))
            {
                bones->SetUInt32Value(CORPSE_FIELD_ITEM + i, 0);
            }
        }

        map->Add(bones);
    }

    // Every reference to the corpse is gone by this point.
    delete corpse;

    return bones;
}

void CorpseManager::RemoveOldCorpses()
{
    const time_t now = time(nullptr);

    // Collect first, convert after: ConvertCorpseForPlayer mutates the very index
    // being walked, and doing that under the container's shared lock would both
    // deadlock and invalidate the iteration.
    std::forward_list<ObjectGuid> expired;

    m_byOwner.ForEach([&now, &expired](Corpse* corpse)
    {
        if (corpse->IsExpired(now))
        {
            expired.emplace_front(corpse->GetOwnerGuid());
        }
    });

    for (const ObjectGuid& owner : expired)
    {
        ConvertCorpseForPlayer(owner);
    }
}
