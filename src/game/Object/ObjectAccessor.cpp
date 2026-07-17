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

#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Policies/Singleton.h"
#include "Player.h"
#include "Item.h"
#include "Corpse.h"
#include "MapManager.h"
#include "Map.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include "ObjectGuid.h"
#include "World.h"

#include <algorithm>

// Singleton instantiation is implicit now (MaNGOS::Singleton is a Meyers singleton).

/**
 * @brief Initializes the global object accessor.
 */
ObjectAccessor::ObjectAccessor() : i_playerMap(), i_corpseMap(), i_corpseGuard()
{
}

/**
 * @brief Destroys the object accessor and releases tracked corpses.
 */
ObjectAccessor::~ObjectAccessor()
{
    for (Player2CorpsesMapType::const_iterator itr = i_player2corpse.begin(); itr != i_player2corpse.end(); ++itr)
    {
        itr->second->RemoveFromWorld();
        delete itr->second;
    }
}

/**
 * @brief Retrieves a unit by GUID relative to a world object.
 *
 * @param u The world object providing map context.
 * @param guid The GUID of the unit to find.
 * @return Pointer to the matching unit, or nullptr if not found.
 */
Unit* ObjectAccessor::GetUnit(WorldObject const& u, ObjectGuid guid)
{
    if (!guid)
    {
        return nullptr;
    }

    if (guid.IsPlayer())
    {
        return FindPlayer(guid);
    }

    if (!u.IsInWorld())
    {
        return nullptr;
    }

    return u.GetMap()->GetAnyTypeCreature(guid);
}

/**
 * @brief Finds a corpse by GUID on a specific map.
 *
 * @param guid The corpse GUID.
 * @param mapid The map identifier to validate against.
 * @return Pointer to the corpse, or nullptr if not found on that map.
 */
Corpse* ObjectAccessor::GetCorpseInMap(ObjectGuid guid, uint32 mapid)
{
    Corpse* ret = i_corpseMap.Find(guid);
    if (!ret)
    {
        return nullptr;
    }
    if (ret->GetMapId() != mapid)
    {
        return nullptr;
    }

    return ret;
}

/**
 * @brief Finds a player by GUID.
 *
 * @param guid The player GUID.
 * @param inWorld true to require the player to be currently in world.
 * @return Pointer to the player, or nullptr if not found.
 */
Player* ObjectAccessor::FindPlayer(ObjectGuid guid, bool inWorld /*= true*/)
{
    if (!guid)
    {
        return nullptr;
    }

    Player* plr = i_playerMap.Find(guid);
    if (!plr || (!plr->IsInWorld() && inWorld))
    {
        return nullptr;
    }

    return plr;
}

/**
 * @brief Finds an online player by character name.
 *
 * @param name The exact player name to search for.
 * @return Pointer to the player, or nullptr if not found.
 */
Player* ObjectAccessor::FindPlayerByName(const char* name)
{
    std::shared_lock<HashMapHolder<Player>::LockType> guard(i_playerMap.GetLock());
    for (auto& iter : i_playerMap.GetContainer())
    {
        if (iter.second->IsInWorld() && (::strcmp(name, iter.second->GetName()) == 0))
        {
            return iter.second;
        }
    }
    return nullptr;
}

//This method should not be here

/**
 * @brief Saves all players currently present in the world.
 */
void ObjectAccessor::SaveAllPlayers()
{
    SessionMap const& smap = sWorld.GetAllSessions();
    SessionMap::const_iterator iter;
    for (iter = smap.begin(); iter != smap.end(); ++iter)
    {
        if (Player* player = iter->second->GetPlayer())
        {
            if (player->IsInWorld())
            {
                player->SaveToDB();
            }
        }
    }
}

/**
 * @brief Disconnects and logs out a player by GUID.
 *
 * @param guid The GUID of the player to remove.
 */
void ObjectAccessor::KickPlayer(ObjectGuid guid)
{
    if (Player* p = FindPlayer(guid, false))
    {
        WorldSession* s = p->GetSession();
        s->KickPlayer();                            // mark session to remove at next session list update
        s->LogoutPlayer(false);                     // logout player without waiting next session list update
    }
}

/**
 * @brief Retrieves the active corpse assigned to a player.
 *
 * @param guid The player's GUID.
 * @return Pointer to the corpse, or nullptr if none exists.
 */
Corpse* ObjectAccessor::GetCorpseForPlayerGUID(ObjectGuid guid)
{
    std::lock_guard<LockType> guard(i_corpseGuard);

    Player2CorpsesMapType::iterator iter = i_player2corpse.find(guid);

    if (iter == i_player2corpse.end())
    {
        return nullptr;
    }

    MANGOS_ASSERT(iter->second->GetType() != CORPSE_BONES);
    return iter->second;
}

/**
 * @brief Removes a corpse from tracking and world storage.
 *
 * @param corpse The corpse to remove.
 */
void ObjectAccessor::RemoveCorpse(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    std::lock_guard<LockType> guard(i_corpseGuard);

    Player2CorpsesMapType::iterator iter = i_player2corpse.find(corpse->GetOwnerGuid());
    if (iter == i_player2corpse.end())
    {
        return;
    }

    // build mapid*cellid -> guid_set map
    CellPair cell_pair = MaNGOS::ComputeCellPair(corpse->GetPositionX(), corpse->GetPositionY());
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    sObjectMgr.DeleteCorpseCellData(corpse->GetMapId(), cell_id, corpse->GetOwnerGuid().GetCounter());
    corpse->RemoveFromWorld();

    i_player2corpse.erase(iter);
}

/**
 * @brief Adds a corpse to player and cell tracking.
 *
 * @param corpse The corpse to register.
 */
void ObjectAccessor::AddCorpse(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    std::lock_guard<LockType> guard(i_corpseGuard);

    MANGOS_ASSERT(i_player2corpse.find(corpse->GetOwnerGuid()) == i_player2corpse.end());
    i_player2corpse[corpse->GetOwnerGuid()] = corpse;

    // build mapid*cellid -> guid_set map
    CellPair cell_pair = MaNGOS::ComputeCellPair(corpse->GetPositionX(), corpse->GetPositionY());
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    sObjectMgr.AddCorpseCellData(corpse->GetMapId(), cell_id, corpse->GetOwnerGuid().GetCounter(), corpse->GetInstanceId());
}

/**
 * @brief Adds corpses belonging to a grid into the active grid object store.
 *
 * @param gridpair The grid coordinates being populated.
 * @param grid The grid container receiving the corpses.
 * @param map The map owning the grid.
 */
void ObjectAccessor::AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map)
{
    std::lock_guard<LockType> guard(i_corpseGuard);

    for (Player2CorpsesMapType::iterator iter = i_player2corpse.begin(); iter != i_player2corpse.end(); ++iter)
    {
        if (iter->second->GetGrid() == gridpair)
        {
            // verify, if the corpse in our instance (add only corpses which are)
            if (map->Instanceable())
            {
                if (iter->second->GetInstanceId() == map->GetInstanceId())
                {
                    grid.AddWorldObject(iter->second);
                }
            }
            else
            {
                grid.AddWorldObject(iter->second);
            }
        }
    }
}

/**
 * @brief Converts a player's corpse into bones when appropriate.
 *
 * @param player_guid The owning player's GUID.
 * @param insignia true when conversion is triggered by insignia handling.
 * @return Pointer to the created bones corpse, or nullptr if no bones were created.
 */
Corpse* ObjectAccessor::ConvertCorpseForPlayer(ObjectGuid player_guid, bool insignia)
{
    Corpse* corpse = GetCorpseForPlayerGUID(player_guid);
    if (!corpse)
    {
        // in fact this function is called from several places
        // even when player doesn't have a corpse, not an error
        // sLog.outError("Try remove corpse that not in map for GUID %ul", player_guid);
        return nullptr;
    }

    DEBUG_LOG("Deleting Corpse and spawning bones.");

    // remove corpse from player_guid -> corpse map
    RemoveCorpse(corpse);

    // remove resurrectable corpse from grid object registry (loaded state checked into call)
    // do not load the map if it's not loaded
    Map* map = sMapMgr.FindMap(corpse->GetMapId(), corpse->GetInstanceId());
    if (map)
    {
        map->Remove(corpse, false);
    }

    // remove corpse from DB
    corpse->DeleteFromDB();

    Corpse* bones = nullptr;
    // create the bones only if the map and the grid is loaded at the corpse's location
    // ignore bones creating option in case insignia
    if (map && (insignia ||
        (map->IsBattleGround() ? sWorld.getConfig(CONFIG_BOOL_DEATH_BONES_BG) : sWorld.getConfig(CONFIG_BOOL_DEATH_BONES_WORLD))) &&
        !map->IsRemovalGrid(corpse->GetPositionX(), corpse->GetPositionY()))
    {
        // Create bones, don't change Corpse
        bones = new Corpse;
        bones->Create(corpse->GetGUIDLow());

        for (int i = 3; i < CORPSE_END; ++i)                // don't overwrite guid and object type
        {
            bones->SetUInt32Value(i, corpse->GetUInt32Value(i));
        }

        bones->SetGrid(corpse->GetGrid());
        // bones->m_time = m_time;                          // don't overwrite time
        // bones->m_inWorld = m_inWorld;                    // don't overwrite world state
        // bones->m_type = m_type;                          // don't overwrite type
        bones->Relocate(corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ(), corpse->GetOrientation());

        bones->SetUInt32Value(CORPSE_FIELD_FLAGS, CORPSE_FLAG_UNK2 | CORPSE_FLAG_BONES);
        bones->SetGuidValue(CORPSE_FIELD_OWNER, ObjectGuid());

        for (int i = 0; i < EQUIPMENT_SLOT_END; ++i)
        {
            if (corpse->GetUInt32Value(CORPSE_FIELD_ITEM + i))
            {
                bones->SetUInt32Value(CORPSE_FIELD_ITEM + i, 0);
            }
        }

        // add bones in grid store if grid loaded where corpse placed
        map->Add(bones);
    }

    // all references to the corpse should be removed at this point
    delete corpse;

    return bones;
}

/**
 * @brief Converts expired corpses into bones.
 */
void ObjectAccessor::RemoveOldCorpses()
{
    time_t now = time(nullptr);
    Player2CorpsesMapType::iterator next;
    for (Player2CorpsesMapType::iterator itr = i_player2corpse.begin(); itr != i_player2corpse.end(); itr = next)
    {
        next = itr;
        ++next;

        if (!itr->second->IsExpired(now))
        {
            continue;
        }

        ConvertCorpseForPlayer(itr->first);
    }
}

/**
 * @brief Finds a corpse by GUID.
 *
 * @param guid The corpse GUID.
 * @return Pointer to the corpse, or nullptr if not found.
 */
Corpse* ObjectAccessor::FindCorpse(ObjectGuid guid)
{
    if (!guid)
    {
        return nullptr;
    }

    return i_corpseMap.Find(guid);
}
