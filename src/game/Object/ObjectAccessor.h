/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2017  MaNGOS project <https://getmangos.eu>
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
#include <ace/Thread_Mutex.h>
#include <ace/RW_Thread_Mutex.h>
#include "Utilities/UnorderedMapSet.h"
#include "Policies/ThreadingModel.h"

#include "UpdateData.h"

#include "GridDefines.h"
#include "Object.h"
#include "Player.h"
#include "Corpse.h"

#include <set>
#include <list>

class Unit;
class WorldObject;
class Map;

template <class T>
class HashMapHolder
{
    public:

        typedef UNORDERED_MAP<ObjectGuid, T*>   MapType;
        typedef ACE_RW_Thread_Mutex LockType;

        static void Insert(T* o)
        {
            ACE_WRITE_GUARD(LockType, guard, i_lock)
            m_objectMap[o->GetObjectGuid()] = o;
        }

        static void Remove(T* o)
        {
            ACE_WRITE_GUARD(LockType, guard, i_lock)
            m_objectMap.erase(o->GetObjectGuid());
        }

        static T* Find(ObjectGuid guid)
        {
            ACE_READ_GUARD_RETURN (LockType, guard, i_lock, NULL)
            typename MapType::iterator itr = m_objectMap.find(guid);
            return (itr != m_objectMap.end()) ? itr->second : NULL;
        }

        static MapType& GetContainer() { return m_objectMap; }

        static LockType& GetLock() { return i_lock; }

    private:

        // Non instanceable only static
        HashMapHolder() {}

        static LockType i_lock;
        static MapType  m_objectMap;
};

class ObjectAccessor : public MaNGOS::Singleton<ObjectAccessor, MaNGOS::ClassLevelLockable<ObjectAccessor, ACE_Thread_Mutex> >
{
        friend class MaNGOS::OperatorNew<ObjectAccessor>;

        ObjectAccessor();
        ~ObjectAccessor();
        ObjectAccessor(const ObjectAccessor&);
        ObjectAccessor& operator=(const ObjectAccessor&);

    public:
        typedef UNORDERED_MAP<ObjectGuid, Corpse*> Player2CorpsesMapType;

        // Search player at any map in world and other objects at same map with `obj`
        // Note: recommended use Map::GetUnit version if player also expected at same map only
        static Unit* GetUnit(WorldObject const& obj, ObjectGuid guid);

        // Player access
        static Player* FindPlayer(ObjectGuid guid, bool inWorld = true);// if need player at specific map better use Map::GetPlayer
        static Player* FindPlayerByName(const char* name);
        static void KickPlayer(ObjectGuid guid);

        HashMapHolder<Player>::MapType& GetPlayers()
        {
            return HashMapHolder<Player>::GetContainer();
        }

        void SaveAllPlayers();

        // Corpse access
        Corpse* GetCorpseForPlayerGUID(ObjectGuid guid);
        static Corpse* GetCorpseInMap(ObjectGuid guid, uint32 mapid);
        void RemoveCorpse(Corpse* corpse);
        void AddCorpse(Corpse* corpse);
        void AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map);
        Corpse* ConvertCorpseForPlayer(ObjectGuid player_guid, bool insignia = false);
        void RemoveOldCorpses();

        // For call from Player/Corpse AddToWorld/RemoveFromWorld only
        void AddObject(Corpse* object) { HashMapHolder<Corpse>::Insert(object); }
        void AddObject(Player* object) { HashMapHolder<Player>::Insert(object); }
        void RemoveObject(Corpse* object) { HashMapHolder<Corpse>::Remove(object); }
        void RemoveObject(Player* object) { HashMapHolder<Player>::Remove(object); }

    private:

        Player2CorpsesMapType   i_player2corpse;

        typedef ACE_Thread_Mutex LockType;

        LockType i_playerGuard;
        char _cache_guard[1024];
        LockType i_corpseGuard;
};

#define sObjectAccessor ObjectAccessor::Instance()

#endif
