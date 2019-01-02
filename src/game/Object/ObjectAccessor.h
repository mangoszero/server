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

#ifndef MANGOS_OBJECTACCESSOR_H
#define MANGOS_OBJECTACCESSOR_H

#include "Common.h"
#include "Platform/Define.h"
#include "Policies/Singleton.h"
#include <ace/Recursive_Thread_Mutex.h>
#include "Policies/ThreadingModel.h"
#include "UpdateData.h"
#include "GridDefines.h"
#include "Object.h"
#include "Player.h"
#include "Corpse.h"

#include <unordered_map>

class Unit;
class WorldObject;
class Map;

class ObjectAccessor : public MaNGOS::Singleton<ObjectAccessor, MaNGOS::ClassLevelLockable<ObjectAccessor, ACE_Recursive_Thread_Mutex> >
{
        friend class MaNGOS::OperatorNew<ObjectAccessor>;

        ObjectAccessor();
        ~ObjectAccessor();
        ObjectAccessor(const ObjectAccessor&);
        ObjectAccessor& operator=(const ObjectAccessor&);

        template <class T>
        struct HashMapHolder
        {
            using MapType = std::unordered_map<ObjectGuid, T*>;
            using LockType = ACE_RW_Thread_Mutex;

            HashMapHolder() : i_lock(nullptr), m_objectMap() {}

            void Insert(T* o)
            {
                ACE_WRITE_GUARD(LockType, guard, i_lock)
                m_objectMap[o->GetObjectGuid()] = o;
            }

            void Remove(T* o)
            {
                ACE_WRITE_GUARD(LockType, guard, i_lock)
                m_objectMap.erase(o->GetObjectGuid());
            }

            T* Find(ObjectGuid guid)
            {
                ACE_READ_GUARD_RETURN (LockType, guard, i_lock, nullptr)
                auto itr = m_objectMap.find(guid);
                return (itr != m_objectMap.end()) ? itr->second : nullptr;
            }

            inline MapType& GetContainer() { return m_objectMap; }
            inline LockType& GetLock() { return i_lock; }

            LockType i_lock;
            MapType  m_objectMap;
            char _cache_guard[512];

        };

        using Player2CorpsesMapType = std::unordered_map<ObjectGuid, Corpse*>;
        using LockType = ACE_Recursive_Thread_Mutex;

    public:

        // Search player at any map in world and other objects at same map with `obj`
        // Note: recommended use Map::GetUnit version if player also expected at same map only
        Unit* GetUnit(WorldObject const& obj, ObjectGuid guid);

        // Player access
        Player* FindPlayer(ObjectGuid guid, bool inWorld = true);// if need player at specific map better use Map::GetPlayer
        Player* FindPlayerByName(const char* name);
        void KickPlayer(ObjectGuid guid);

        void SaveAllPlayers();

        // Corpse access
        Corpse* FindCorpse(ObjectGuid guid);
        Corpse* GetCorpseForPlayerGUID(ObjectGuid guid);
        Corpse* GetCorpseInMap(ObjectGuid guid, uint32 mapid);
        void RemoveCorpse(Corpse* corpse);
        void AddCorpse(Corpse* corpse);
        void AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map);
        Corpse* ConvertCorpseForPlayer(ObjectGuid player_guid, bool insignia = false);
        void RemoveOldCorpses();

        // For call from Player/Corpse AddToWorld/RemoveFromWorld only
        void AddObject(Corpse* object) { i_corpseMap.Insert(object); }
        void AddObject(Player* object) { i_playerMap.Insert(object); }
        void RemoveObject(Corpse* object) { i_corpseMap.Remove(object); }
        void RemoveObject(Player* object) { i_playerMap.Remove(object); }

        template<typename F>
        void DoForAllPlayers(F&& f)
        {
            ACE_READ_GUARD(HashMapHolder<Player>::LockType, g, i_playerMap.GetLock())
            for (auto& iter : i_playerMap.GetContainer())
                if(iter.second != nullptr)
                { std::forward<F>(f)(iter.second); }
        }

    private:
        Player2CorpsesMapType  i_player2corpse;
        HashMapHolder<Player>  i_playerMap;
        HashMapHolder<Corpse>  i_corpseMap;
        LockType i_corpseGuard;
};

#define sObjectAccessor ObjectAccessor::Instance()

#endif
