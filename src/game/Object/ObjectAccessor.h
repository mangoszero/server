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
 * @file ObjectAccessor.h
 * @brief Global object accessor singleton for player and corpse lookup.
 *
 * This file contains the ObjectAccessor singleton class which provides centralized
 * access to all players and corpses in the game world. It maintains hash maps for
 * fast GUID-based lookups with thread-safe access using ACE synchronization primitives.
 *
 * Key responsibilities:
 * - Global player lookups by GUID or name
 * - Corpse management and access
 * - Thread-safe object registry for players and corpses
 * - Conversion of player corpses to insignia for PvP
 * - Saving all players to database
 *
 * The accessor uses templated HashMapHolder for reusable container management
 * and provides both thread-safe and lock-based access patterns.
 *
 * @see ObjectAccessor for the singleton implementation
 * @see HashMapHolder for the thread-safe container template
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

/// @brief Global singleton for thread-safe object (player/corpse) lookups.
///
/// ObjectAccessor provides centralized management and rapid access to all active
/// players and corpses in the game world. It maintains thread-safe hash maps indexed
/// by ObjectGuid for O(1) lookups and supports flexible filtering.
///
/// The accessor is responsible for:
/// - Player registration/deregistration from world
/// - Corpse management and lifecycle
/// - Thread-safe object queries across the entire world
/// - Player-to-corpse associations for PvP mechanics
/// - Batch operations (save all players, etc.)
///
/// All access is protected by ACE synchronization primitives to prevent
/// race conditions during concurrent reads and writes.
///
/// @note This is a thread-safe singleton, should not be instantiated directly
/// @see sObjectAccessor for the singleton accessor macro
class ObjectAccessor : public MaNGOS::Singleton<ObjectAccessor, MaNGOS::ClassLevelLockable<ObjectAccessor, ACE_Recursive_Thread_Mutex> >
{
        friend class MaNGOS::OperatorNew<ObjectAccessor>;

        ObjectAccessor();
        ~ObjectAccessor();
        ObjectAccessor(const ObjectAccessor&);
        ObjectAccessor& operator=(const ObjectAccessor&);

        /// @brief Thread-safe hash map holder template for GUID-indexed objects.
        ///
        /// This template provides a generic, thread-safe container for objects indexed
        /// by ObjectGuid. It encapsulates an unordered_map with ACE synchronization.
        ///
        /// The holder supports:
        /// - Thread-safe insertion of objects
        /// - Thread-safe removal of objects
        /// - Thread-safe lookup by GUID
        /// - Read access to the underlying container and lock
        ///
        /// @tparam T The object type to store (Player, Corpse, etc.)
        ///
        /// @note Uses ACE_RW_Thread_Mutex for read/write locking
        /// @see Insert() for adding objects
        /// @see Remove() for removing objects
        /// @see Find() for looking up objects
        template <class T>
        struct HashMapHolder
        {
            /// @brief Underlying hash map type (ObjectGuid -> Object pointer)
            using MapType = std::unordered_map<ObjectGuid, T*>;
            /// @brief Lock type for synchronization
            using LockType = ACE_RW_Thread_Mutex;

            /// @brief Constructor initializes empty map
            HashMapHolder() : i_lock(nullptr), m_objectMap() {}

            /// @brief Insert an object into the map with write lock.
            ///
            /// @param o Pointer to object to insert
            void Insert(T* o)
            {
                ACE_WRITE_GUARD(LockType, guard, i_lock)
                m_objectMap[o->GetObjectGuid()] = o;
            }

            /// @brief Remove an object from the map with write lock.
            ///
            /// @param o Pointer to object to remove
            void Remove(T* o)
            {
                ACE_WRITE_GUARD(LockType, guard, i_lock)
                m_objectMap.erase(o->GetObjectGuid());
            }

            /// @brief Find an object by GUID with read lock.
            ///
            /// @param guid Object GUID to search for
            /// @return Pointer to object if found, nullptr otherwise
            T* Find(ObjectGuid guid)
            {
                ACE_READ_GUARD_RETURN (LockType, guard, i_lock, nullptr)
                auto itr = m_objectMap.find(guid);
                return (itr != m_objectMap.end()) ? itr->second : nullptr;
            }

            /// @brief Get reference to the underlying container (for external iteration).
            ///
            /// @return Reference to the unordered_map
            /// @warning Caller must acquire lock separately before iterating
            inline MapType& GetContainer() { return m_objectMap; }

            /// @brief Get reference to the synchronization lock.
            ///
            /// @return Reference to the ACE RW thread mutex
            /// @note Use ACE guard macros when accessing this
            inline LockType& GetLock() { return i_lock; }

            LockType i_lock;
            MapType  m_objectMap;
            /// @brief Cache line guard to prevent false sharing (512 bytes padding)
            char _cache_guard[512];

        };

        using Player2CorpsesMapType = std::unordered_map<ObjectGuid, Corpse*>;
        using LockType = ACE_Recursive_Thread_Mutex;

    public:

        /// @brief Get a unit by GUID from any map in the world.
        ///
        /// Searches for a unit (player or creature) with the specified GUID
        /// across all maps. If the unit is found, it's returned regardless of
        /// which map it's on.
        ///
        /// @param obj Reference object used for context
        /// @param guid GUID of the unit to find
        /// @return Pointer to the unit if found, nullptr otherwise
        ///
        /// @note For players, use FindPlayer() instead if appropriate
        /// @note If unit is on same map as obj, consider using Map::GetUnit()
        /// @see FindPlayer() for player-specific lookups
        Unit* GetUnit(WorldObject const& obj, ObjectGuid guid);

        // Player access
        /// @brief Find a player by GUID.
        ///
        /// Searches for an online player with the specified GUID.
        /// Can optionally restrict search to players currently in the world.
        ///
        /// @param guid Player GUID to search for
        /// @param inWorld If true (default), only returns players in world; if false, includes offline players
        /// @return Pointer to the player if found, nullptr otherwise
        ///
        /// @note If player is on a specific map, consider using Map::GetPlayer() instead
        /// @note This is faster than FindPlayerByName() for GUID-based lookups
        Player* FindPlayer(ObjectGuid guid, bool inWorld = true);

        /// @brief Find a player by character name.
        ///
        /// Performs a linear search through all players to find one matching
        /// the given character name.
        ///
        /// @param name Character name to search for (case-insensitive)
        /// @return Pointer to the player if found, nullptr otherwise
        ///
        /// @note This is O(n) and slower than GUID-based lookups
        /// @note Name matching is case-insensitive
        Player* FindPlayerByName(const char* name);

        /// @brief Kick a player from the game by GUID.
        ///
        /// Disconnects and removes a player from the world.
        ///
        /// @param guid GUID of the player to kick
        void KickPlayer(ObjectGuid guid);

        /// @brief Save all online players to the database.
        ///
        /// Iterates through all online players and persists their data
        /// to the character database.
        ///
        /// @note This is called periodically and on server shutdown
        void SaveAllPlayers();

        // Corpse access
        /// @brief Find a corpse by GUID.
        ///
        /// @param guid Corpse GUID to search for
        /// @return Pointer to the corpse if found, nullptr otherwise
        Corpse* FindCorpse(ObjectGuid guid);

        /// @brief Get the corpse for a specific player.
        ///
        /// Retrieves the player's corpse based on the player-corpse association.
        ///
        /// @param guid Player GUID
        /// @return Pointer to the player's corpse if found, nullptr otherwise
        Corpse* GetCorpseForPlayerGUID(ObjectGuid guid);

        /// @brief Get a corpse that exists on a specific map.
        ///
        /// Finds a corpse by GUID and verifies it's on the specified map.
        ///
        /// @param guid Corpse GUID to search for
        /// @param mapid Map ID to verify corpse is on
        /// @return Pointer to the corpse if found on that map, nullptr otherwise
        Corpse* GetCorpseInMap(ObjectGuid guid, uint32 mapid);

        /// @brief Remove a corpse from the object accessor.
        ///
        /// @param corpse Pointer to the corpse to remove
        void RemoveCorpse(Corpse* corpse);

        /// @brief Add a corpse to the object accessor.
        ///
        /// @param corpse Pointer to the corpse to add
        void AddCorpse(Corpse* corpse);

        /// @brief Add corpses to a grid for area visibility.
        ///
        /// @param gridpair Grid coordinates
        /// @param grid Reference to the grid to populate
        /// @param map Pointer to the map containing the grid
        void AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map);

        /// @brief Convert a player corpse to insignia for PvP purposes.
        ///
        /// Transforms the corpse into an insignia item if appropriate.
        ///
        /// @param player_guid GUID of the player whose corpse to convert
        /// @param insignia If true, convert to insignia; if false, standard conversion
        /// @return Pointer to the converted corpse
        Corpse* ConvertCorpseForPlayer(ObjectGuid player_guid, bool insignia = false);

        /// @brief Remove old/expired corpses from the world.
        ///
        /// Cleans up corpses that have exceeded their decay timers.
        void RemoveOldCorpses();

        // For call from Player/Corpse AddToWorld/RemoveFromWorld only
        /// @brief Add a corpse to the global registry.
        ///
        /// @param object Pointer to the corpse
        /// @note Internal use only - called by Corpse::AddToWorld()
        void AddObject(Corpse* object) { i_corpseMap.Insert(object); }

        /// @brief Add a player to the global registry.
        ///
        /// @param object Pointer to the player
        /// @note Internal use only - called by Player::AddToWorld()
        void AddObject(Player* object) { i_playerMap.Insert(object); }

        /// @brief Remove a corpse from the global registry.
        ///
        /// @param object Pointer to the corpse
        /// @note Internal use only - called by Corpse::RemoveFromWorld()
        void RemoveObject(Corpse* object) { i_corpseMap.Remove(object); }

        /// @brief Remove a player from the global registry.
        ///
        /// @param object Pointer to the player
        /// @note Internal use only - called by Player::RemoveFromWorld()
        void RemoveObject(Player* object) { i_playerMap.Remove(object); }

        /// @brief Execute a function for all online players.
        ///
        /// Iterates through all registered players with read lock and
        /// invokes the provided function for each one.
        ///
        /// @tparam F Lambda or function type that accepts Player* parameter
        /// @param f Function/lambda to execute for each player
        ///
        /// @note Lock is held for duration of iteration - keep function fast
        /// @note Function receives nullptr checks - may be called with nullptr players
        template<typename F>
        void DoForAllPlayers(F&& f)
        {
            ACE_READ_GUARD(HashMapHolder<Player>::LockType, g, i_playerMap.GetLock())
            for (auto& iter : i_playerMap.GetContainer())
            {
                if (iter.second != nullptr)
                {
                    std::forward<F>(f)(iter.second);
                }
            }
        }

    private:
        Player2CorpsesMapType  i_player2corpse;    ///> Player to corpse mapping for resurrection/PvP mechanics
        HashMapHolder<Player>  i_playerMap;        ///> Global player registry indexed by GUID
        HashMapHolder<Corpse>  i_corpseMap;        ///> Global corpse registry indexed by GUID
        LockType i_corpseGuard;                    ///> Lock for corpse access synchronization
};

/// @brief Singleton accessor macro for ObjectAccessor instance
/// @see ObjectAccessor for the class definition
#define sObjectAccessor ObjectAccessor::Instance()

#endif
