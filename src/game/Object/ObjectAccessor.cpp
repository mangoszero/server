/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#define CLASS_LOCK MaNGOS::ClassLevelLockable<ObjectAccessor, ACE_Recursive_Thread_Mutex>
INSTANTIATE_SINGLETON_2(ObjectAccessor, CLASS_LOCK);
INSTANTIATE_CLASS_MUTEX(ObjectAccessor, ACE_Recursive_Thread_Mutex);


ObjectAccessor::ObjectAccessor() : i_playerMap(), i_corpseMap(), i_corpseGuard()
{
}

ObjectAccessor::~ObjectAccessor()
{
    for (Player2CorpsesMapType::const_iterator itr = i_player2corpse.begin(); itr != i_player2corpse.end(); ++itr)
    {
        itr->second->RemoveFromWorld();
        delete itr->second;
    }
}

Unit*
ObjectAccessor::GetUnit(WorldObject const& u, ObjectGuid guid)
{
    if (!guid)
        { return nullptr; }

    if (guid.IsPlayer())
        { return FindPlayer(guid); }

    if (!u.IsInWorld())
        { return nullptr; }

    return u.GetMap()->GetAnyTypeCreature(guid);
}

Corpse* ObjectAccessor::GetCorpseInMap(ObjectGuid guid, uint32 mapid)
{
    Corpse* ret = i_corpseMap.Find(guid);
    if (!ret)
        { return nullptr; }
    if (ret->GetMapId() != mapid)
        { return nullptr; }

    return ret;
}

Player* ObjectAccessor::FindPlayer(ObjectGuid guid, bool inWorld /*= true*/)
{
    if (!guid)
        { return nullptr; }

    Player* plr = i_playerMap.Find(guid);
    if (!plr || (!plr->IsInWorld() && inWorld))
        { return nullptr; }

    return plr;
}

Player* ObjectAccessor::FindPlayerByName(const char* name)
{
    ACE_READ_GUARD_RETURN(HashMapHolder<Player>::LockType, guard, i_playerMap.GetLock(), nullptr)
    for (auto& iter : i_playerMap.GetContainer())
        if (iter.second->IsInWorld() && (::strcmp(name, iter.second->GetName()) == 0))
              { return iter.second; }
    return nullptr;
}

//This method should not be here
void ObjectAccessor::SaveAllPlayers()
{
   SessionMap const& smap = sWorld.GetAllSessions();
   SessionMap::const_iterator iter;
   for (iter = smap.begin(); iter != smap.end(); ++iter){
       if (Player* player = iter->second->GetPlayer()){
           if (player->IsInWorld()){
               player->SaveToDB();
           }
       }
   }
}

void ObjectAccessor::KickPlayer(ObjectGuid guid)
{
    if (Player* p = FindPlayer(guid, false))
    {
        WorldSession* s = p->GetSession();
        s->KickPlayer();                            // mark session to remove at next session list update
        s->LogoutPlayer(false);                     // logout player without waiting next session list update
    }
}

Corpse*
ObjectAccessor::GetCorpseForPlayerGUID(ObjectGuid guid)
{
    ACE_GUARD_RETURN(LockType, guard, i_corpseGuard, nullptr)

    Player2CorpsesMapType::iterator iter = i_player2corpse.find(guid);

    if (iter == i_player2corpse.end())
        { return nullptr; }

    MANGOS_ASSERT(iter->second->GetType() != CORPSE_BONES);
    return iter->second;
}

void
ObjectAccessor::RemoveCorpse(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    ACE_GUARD(LockType, guard, i_corpseGuard)

    Player2CorpsesMapType::iterator iter = i_player2corpse.find(corpse->GetOwnerGuid());
    if (iter == i_player2corpse.end())
        { return; }

    // build mapid*cellid -> guid_set map
    CellPair cell_pair = MaNGOS::ComputeCellPair(corpse->GetPositionX(), corpse->GetPositionY());
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    sObjectMgr.DeleteCorpseCellData(corpse->GetMapId(), cell_id, corpse->GetOwnerGuid().GetCounter());
    corpse->RemoveFromWorld();

    i_player2corpse.erase(iter);
}

void
ObjectAccessor::AddCorpse(Corpse* corpse)
{
    MANGOS_ASSERT(corpse && corpse->GetType() != CORPSE_BONES);

    ACE_GUARD(LockType, guard, i_corpseGuard)

    MANGOS_ASSERT(i_player2corpse.find(corpse->GetOwnerGuid()) == i_player2corpse.end());
    i_player2corpse[corpse->GetOwnerGuid()] = corpse;

    // build mapid*cellid -> guid_set map
    CellPair cell_pair = MaNGOS::ComputeCellPair(corpse->GetPositionX(), corpse->GetPositionY());
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    sObjectMgr.AddCorpseCellData(corpse->GetMapId(), cell_id, corpse->GetOwnerGuid().GetCounter(), corpse->GetInstanceId());
}

void
ObjectAccessor::AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map)
{
    ACE_GUARD(LockType, guard, i_corpseGuard)

    for (Player2CorpsesMapType::iterator iter = i_player2corpse.begin(); iter != i_player2corpse.end(); ++iter)
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

Corpse*
ObjectAccessor::ConvertCorpseForPlayer(ObjectGuid player_guid, bool insignia)
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
        { map->Remove(corpse, false); }

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
            { bones->SetUInt32Value(i, corpse->GetUInt32Value(i)); }

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
                { bones->SetUInt32Value(CORPSE_FIELD_ITEM + i, 0); }
        }

        // add bones in grid store if grid loaded where corpse placed
        map->Add(bones);
    }

    // all references to the corpse should be removed at this point
    delete corpse;

    return bones;
}

void ObjectAccessor::RemoveOldCorpses()
{
    time_t now = time(nullptr);
    Player2CorpsesMapType::iterator next;
    for (Player2CorpsesMapType::iterator itr = i_player2corpse.begin(); itr != i_player2corpse.end(); itr = next)
    {
        next = itr;
        ++next;

        if (!itr->second->IsExpired(now))
            { continue; }

        ConvertCorpseForPlayer(itr->first);
    }
}

Corpse* ObjectAccessor::FindCorpse(ObjectGuid guid)
{
    if (!guid)
        { return nullptr; }

    return i_corpseMap.Find(guid);
}