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
 * @file MapManager.cpp
 * @brief Map manager implementation
 *
 * This file implements MapManager, a singleton that manages all map
 * instances on the server. Key responsibilities:
 *
 * - Map instance creation and destruction
 * - Map lookup by ID and instance ID
 * - Continent map management (shared instances)
 * - Instance map management (separate instances)
 * - Player-to-map routing
 * - Map update scheduling
 * - Transport system management
 *
 * MapManager ensures proper cleanup of unused maps and efficient
 * routing of players to their current map instance.
 *
 * @see MapManager for the manager class
 * @see Map for individual map implementation
 */

#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Transports.h"
#include "GridDefines.h"
#include "World.h"
#include "CellImpl.h"
#include "ObjectMgr.h"

#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */

// Singleton instantiation is implicit now (MaNGOS::Singleton is a Meyers singleton).

MapManager::MapManager()
    : i_gridCleanUpDelay(sWorld.getConfig(CONFIG_UINT32_INTERVAL_GRIDCLEAN)), m_lock()
{
    i_timer.SetInterval(sWorld.getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));
}

MapManager::~MapManager()
{
    for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
    {
        delete iter->second;
    }

    for (TransportSet::iterator i = m_Transports.begin(); i != m_Transports.end(); ++i)
    {
        delete *i;
    }

    DeleteStateMachine();
}

void
MapManager::Initialize()
{
    int num_threads(sWorld.getConfig(CONFIG_UINT32_NUMTHREADS));

#ifdef ENABLE_ELUNA
    if (sElunaConfig->IsElunaEnabled() && sElunaConfig->IsElunaCompatibilityMode() && num_threads > 1)
    {
        // Force 1 thread for Eluna if compatibility mode is enabled. Compatibility mode is single state and does not allow more update threads.
        sLog.outError("Map update threads set to %i, when Eluna in compatibility mode only allows 1, changing to 1", num_threads);
        num_threads = 1;
    }
#endif /* ENABLE_ELUNA */

    // Start mtmaps if needed.
    if (num_threads > 0 && m_updater.activate(num_threads) == -1)
    {
        abort();
    }

    InitStateMachine();
    InitMaxInstanceId();
}

/**
 * @brief Creates the grid state machine instances used by map updates.
 */
void MapManager::InitStateMachine()
{
    si_GridStates[GRID_STATE_INVALID] = new InvalidState;
    si_GridStates[GRID_STATE_ACTIVE] = new ActiveState;
    si_GridStates[GRID_STATE_IDLE] = new IdleState;
    si_GridStates[GRID_STATE_REMOVAL] = new RemovalState;
}

/**
 * @brief Destroys the grid state machine instances.
 */
void MapManager::DeleteStateMachine()
{
    delete si_GridStates[GRID_STATE_INVALID];
    delete si_GridStates[GRID_STATE_ACTIVE];
    delete si_GridStates[GRID_STATE_IDLE];
    delete si_GridStates[GRID_STATE_REMOVAL];
}

/**
 * @brief Updates a single grid through its current state handler.
 *
 * @param state The current grid state.
 * @param map The owning map.
 * @param ngrid The grid being updated.
 * @param ginfo The grid info structure.
 * @param x The grid X coordinate.
 * @param y The grid Y coordinate.
 * @param t_diff The elapsed update time.
 */
void MapManager::UpdateGridState(grid_state_t state, Map& map, NGridType& ngrid, GridInfo& ginfo, const uint32& x, const uint32& y, const uint32& t_diff)
{
    // TODO: The grid state array itself is static and therefore 100% safe, however, the data
    // the state classes in it accesses is not, since grids are shared across maps (for example
    // in instances), so some sort of locking will be necessary later.

    si_GridStates[state]->Update(map, ngrid, ginfo, x, y, t_diff);
}

/**
 * @brief Reinitializes visibility distance settings for all loaded maps.
 */
void MapManager::InitializeVisibilityDistanceInfo()
{
    for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
    {
        (*iter).second->InitVisibilityDistance();
    }
}

/// @param id - MapId of the to be created map. @param obj WorldObject for which the map is to be created. Must be player for Instancable maps.
Map* MapManager::CreateMap(uint32 id, const WorldObject* obj)
{
    std::lock_guard<LOCK_TYPE> _guard(m_lock);

    const MapEntry* entry = sMapStore.LookupEntry(id);
    if (!entry)
    {
        return NULL;
    }

    Map* m;
    if (entry->Instanceable())
    {
        MANGOS_ASSERT(obj && obj->GetTypeId() == TYPEID_PLAYER);
        // create DungeonMap object
        m = CreateInstance(id, (Player*)obj);
        // Load active objects for this map
        if (m != NULL)
        {
            LoadActiveEntities(m);
        }
    }
    else
    {
        // create regular non-instanceable map
        m = FindMap(id);
        if (m == NULL)
        {
            m = new WorldMap(id, i_gridCleanUpDelay);
            // add map into container
            i_maps[MapID(id)] = m;

            LoadActiveEntities(m);

            // non-instanceable maps always expected have saved state
            m->CreateInstanceData(true);
        }
    }

    return m;
}

/**
 * @brief Creates a battleground map instance.
 *
 * @param mapid The battleground map id.
 * @param bg The battleground owning the map.
 * @return Map* The created battleground map.
 */
Map* MapManager::CreateBgMap(uint32 mapid, BattleGround* bg)
{
    sTerrainMgr.LoadTerrain(mapid);

    std::lock_guard<LOCK_TYPE> _guard(m_lock);
    return CreateBattleGroundMap(mapid, sMapMgr.GenerateInstanceId(), bg);
}

/**
 * @brief Finds a loaded map by map id and instance id.
 *
 * @param mapid The map id.
 * @param instanceId The instance id.
 * @return Map* The loaded map, or null if not found.
 */
Map* MapManager::FindMap(uint32 mapid, uint32 instanceId) const
{
    std::lock_guard<LOCK_TYPE> _guard(m_lock);

    MapMapType::const_iterator iter = i_maps.find(MapID(mapid, instanceId));
    if (iter == i_maps.end())
    {
        return NULL;
    }

    // this is a small workaround for transports
    if (instanceId == 0 && iter->second->Instanceable())
    {
        assert(false);
        return NULL;
    }

    return iter->second;
}

/**
 * @brief Deletes a loaded instance map.
 *
 * @param mapid The map id.
 * @param instanceId The instance id.
 */
void MapManager::DeleteInstance(uint32 mapid, uint32 instanceId)
{
    std::lock_guard<LOCK_TYPE> _guard(m_lock);

    MapMapType::iterator iter = i_maps.find(MapID(mapid, instanceId));
    if (iter != i_maps.end())
    {
        Map* pMap = iter->second;
        if (pMap->Instanceable())
        {
            i_maps.erase(iter);

            pMap->UnloadAll(true);
            delete pMap;
        }
    }
}

/**
 * @brief Updates all loaded maps and transports.
 *
 * @param diff The elapsed update time.
 */
void MapManager::Update(uint32 diff)
{
    i_timer.Update(diff);
    if (!i_timer.Passed())
    {
        return;
    }

    for (MapMapType::iterator iter=i_maps.begin(); iter != i_maps.end(); ++iter)
    {
        if (m_updater.activated())
        {
            m_updater.schedule_update(*iter->second, (uint32)i_timer.GetCurrent());
        }
        else
        {
            iter->second->Update((uint32)i_timer.GetCurrent());
        }
    }

    if (m_updater.activated())
    {
        m_updater.wait();
    }

    for (TransportSet::iterator iter = m_Transports.begin(); iter != m_Transports.end(); ++iter)
    {
        WorldObject::UpdateHelper helper((*iter));
        helper.Update((uint32)i_timer.GetCurrent());
    }

    // remove all maps which can be unloaded
    MapMapType::iterator iter = i_maps.begin();
    while (iter != i_maps.end())
    {
        Map* pMap = iter->second;
        // check if map can be unloaded
        if (pMap->CanUnload((uint32)i_timer.GetCurrent()))
        {
            pMap->UnloadAll(true);
            delete pMap;

            i_maps.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }

    i_timer.SetCurrent(0);
}

/**
 * @brief Removes all objects pending deletion from all loaded maps.
 */
void MapManager::RemoveAllObjectsInRemoveList()
{
    for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
    {
        iter->second->RemoveAllObjectsInRemoveList();
    }
}

/**
 * @brief Checks whether map and vmap data exist for a location.
 *
 * @param mapid The map id.
 * @param x The X coordinate.
 * @param y The Y coordinate.
 * @return true if both map and vmap data exist; otherwise false.
 */
bool MapManager::ExistMapAndVMap(uint32 mapid, float x, float y)
{
    GridPair p = MaNGOS::ComputeGridPair(x, y);

    int gx = 63 - p.x_coord;
    int gy = 63 - p.y_coord;

    return GridMap::ExistMap(mapid, gx, gy) && GridMap::ExistVMap(mapid, gx, gy);
}

/**
 * @brief Checks whether a map id is valid for loading.
 *
 * @param mapid The map id.
 * @return true if the map is valid; otherwise false.
 */
bool MapManager::IsValidMAP(uint32 mapid)
{
    MapEntry const* mEntry = sMapStore.LookupEntry(mapid);
    return mEntry && (!mEntry->IsDungeon() || ObjectMgr::GetInstanceTemplate(mapid));
    // TODO: add check for battleground template
}

/**
 * @brief Unloads all maps and terrain data.
 */
void MapManager::UnloadAll()
{
    for (MapMapType::iterator iter = i_maps.begin(); iter != i_maps.end(); ++iter)
    {
        iter->second->UnloadAll(true);
    }

    while (!i_maps.empty())
    {
        delete i_maps.begin()->second;
        i_maps.erase(i_maps.begin());
    }

    TerrainManager::Instance().UnloadAll();

    if (m_updater.activated())
    {
        sLog.outString("[shutdown] MapManager::UnloadAll: deactivating MapUpdater...");
        m_updater.deactivate();
        sLog.outString("[shutdown] MapManager::UnloadAll: MapUpdater deactivated");
    }
}

/**
 * @brief Initializes the highest known instance id from the database.
 */
void MapManager::InitMaxInstanceId()
{
    i_MaxInstanceId = 0;

    QueryResult* result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `instance`");
    if (result)
    {
        i_MaxInstanceId = result->Fetch()[0].GetUInt32();
        delete result;
    }
}

/**
 * @brief Counts loaded dungeon instances.
 *
 * @return uint32 The number of loaded dungeon maps.
 */
uint32 MapManager::GetNumInstances()
{
    uint32 ret = 0;

    std::lock_guard<LOCK_TYPE> _guard(m_lock);
    for (MapMapType::iterator itr = i_maps.begin(); itr != i_maps.end(); ++itr)
    {
        Map* map = itr->second;
        if (!map->IsDungeon())
        {
            continue;
        }
        ret += 1;
    }
    return ret;
}

/**
 * @brief Counts players currently inside loaded dungeon instances.
 *
 * @return uint32 The number of players in loaded instances.
 */
uint32 MapManager::GetNumPlayersInInstances()
{
    uint32 ret = 0;

    std::lock_guard<LOCK_TYPE> _guard(m_lock);
    for (MapMapType::iterator itr = i_maps.begin(); itr != i_maps.end(); ++itr)
    {
        Map* map = itr->second;
        if (!map->IsDungeon())
        {
            continue;
        }
        ret += map->GetPlayers().getSize();
    }
    return ret;
}

///// returns a new or existing Instance
///// in case of battlegrounds it will only return an existing map, those maps are created by bg-system
Map* MapManager::CreateInstance(uint32 id, Player* player)
{
    Map* map = NULL;
    Map* pNewMap = NULL;
    uint32 NewInstanceId;                               // instanceId of the resulting map
    const MapEntry* entry = sMapStore.LookupEntry(id);

    if (entry->IsBattleGround())
    {
        // find existing bg map for player
        NewInstanceId = player->GetBattleGroundId();
        MANGOS_ASSERT(NewInstanceId);
        map = FindMap(id, NewInstanceId);
        MANGOS_ASSERT(map);
    }
    else if (DungeonPersistentState* pSave = player->GetBoundInstanceSaveForSelfOrGroup(id))
    {
        // solo/perm/group
        NewInstanceId = pSave->GetInstanceId();
        map = FindMap(id, NewInstanceId);
        // it is possible that the save exists but the map doesn't
        if (!map)
        {
            pNewMap = CreateDungeonMap(id, NewInstanceId, pSave);
        }
    }
    else
    {
        // if no instanceId via group members or instance saves is found
        // the instance will be created for the first time
        NewInstanceId = GenerateInstanceId();

        pNewMap = CreateDungeonMap(id, NewInstanceId);
    }

    // add a new map object into the registry
    if (pNewMap)
    {
        i_maps[MapID(id, NewInstanceId)] = pNewMap;
        map = pNewMap;
    }

    return map;
}

/**
 * @brief Creates a dungeon map instance.
 *
 * @param id The map id.
 * @param InstanceId The instance id.
 * @param save The optional persistent state to load from.
 * @return DungeonMap* The created dungeon map.
 */
DungeonMap* MapManager::CreateDungeonMap(uint32 id, uint32 InstanceId, DungeonPersistentState* save)
{
    // make sure we have a valid map id
    const MapEntry* entry = sMapStore.LookupEntry(id);
    if (!entry)
    {
        sLog.outError("CreateDungeonMap: no entry for map %d", id);
        MANGOS_ASSERT(false);
    }
    if (!ObjectMgr::GetInstanceTemplate(id))
    {
        sLog.outError("CreateDungeonMap: no instance template for map %d", id);
        MANGOS_ASSERT(false);
    }

    DEBUG_LOG("MapInstanced::CreateInstanceMap: %s map instance %d for %d created", save ? "" : "new ", InstanceId, id);

    DungeonMap* map = new DungeonMap(id, i_gridCleanUpDelay, InstanceId);

    // Dungeons can have saved instance data
    bool load_data = save != NULL;
    map->CreateInstanceData(load_data);

    return map;
}

/**
 * @brief Creates a battleground map instance and binds it to a battleground.
 *
 * @param id The map id.
 * @param InstanceId The instance id.
 * @param bg The battleground owning the map.
 * @return BattleGroundMap* The created battleground map.
 */
BattleGroundMap* MapManager::CreateBattleGroundMap(uint32 id, uint32 InstanceId, BattleGround* bg)
{
    DEBUG_LOG("MapInstanced::CreateBattleGroundMap: instance:%d for map:%d and bgType:%d created.", InstanceId, id, bg->GetTypeID());

    BattleGroundMap* map = new BattleGroundMap(id, i_gridCleanUpDelay, InstanceId);
    MANGOS_ASSERT(map->IsBattleGround());
    map->SetBG(bg);
    bg->SetBgMap(map);

    // add map into map container
    i_maps[MapID(id, InstanceId)] = map;

    // BGs/Arenas not have saved instance data
    map->CreateInstanceData(false);

    return map;
}

// Temporary startup accumulator for LivingWorld observability (written during LoadContinents only)
static MapManager::LivingWorldStartupStats s_livingWorldStats;
static bool s_livingWorldStartupPass = false;

/**
 * @brief Ensures the main continent maps are loaded.
 */
MapManager::LivingWorldStartupStats MapManager::LoadContinents()
{
    // Reset startup accumulator before continent creation
    s_livingWorldStats = MapManager::LivingWorldStartupStats();
    s_livingWorldStartupPass = true;

    uint32 continents[] = {0, 1, 369};
    Map* _map = NULL;

    for (uint8 i = 0; i < countof(continents); ++i)
    {
        _map = sMapMgr.FindMap(continents[i]);

        if (!_map)
        {
            _map = sMapMgr.CreateMap(continents[i], NULL);
        }

        if (!_map)
        {
            sLog.outError("MapManager::LoadContinents() - Unable to create map %u", continents[i]);
        }
    }

    s_livingWorldStartupPass = false;
    return s_livingWorldStats;
}

/**
 * @brief Loads active entities and forced grids for a map.
 *
 * @param m The map to prepare.
 */
void MapManager::LoadActiveEntities(Map* m)
{
    // Create all local transporters for this map
    m->LoadLocalTransports();

    uint32 localTransportCount = uint32(m->GetLocalTransports().size());

    bool forceLoad = sWorld.isForceLoadMap(m->GetId());

    uint32 forceLoadRequests = 0;
    uint32 uniqueGridCount = 0;
    uint32 newlyLoaded = 0;
    uint32 alreadyLoaded = 0;

    std::set<std::pair<uint32, uint32>> uniqueGrids;

    // Count active-creature GUIDs present on this map (extra-active, NOT the load driver on force-load maps)
    uint32 activeCreatureGuids = 0;
    {
        std::pair<ActiveCreatureGuidsOnMap::const_iterator, ActiveCreatureGuidsOnMap::const_iterator> activeBounds = sObjectMgr.GetActiveCreatureGuids()->equal_range(m->GetId());
        for (ActiveCreatureGuidsOnMap::const_iterator itr = activeBounds.first; itr != activeBounds.second; ++itr)
        {
            ++activeCreatureGuids;
        }
    }

    // Load grids for all objects on this map, if configured so
    if (forceLoad)
    {
        for (CreatureDataMap::const_iterator itr = sObjectMgr.GetCreatureDataMap()->begin(); itr != sObjectMgr.GetCreatureDataMap()->end(); ++itr)
        {
            if (itr->second.mapid == m->GetId())
            {
                ++forceLoadRequests;
                GridPair gridPair = MaNGOS::ComputeGridPair(itr->second.posX, itr->second.posY);
                uniqueGrids.insert(std::make_pair(gridPair.x_coord, gridPair.y_coord));

                if (m->IsLoaded(itr->second.posX, itr->second.posY))
                {
                    ++alreadyLoaded;
                }
                else
                {
                    ++newlyLoaded;
                }

                m->ForceLoadGrid(itr->second.posX, itr->second.posY);
            }
        }
    }
    else // Normal case - load only grids for npcs that are active
    {
        std::pair<ActiveCreatureGuidsOnMap::const_iterator, ActiveCreatureGuidsOnMap::const_iterator> bounds = sObjectMgr.GetActiveCreatureGuids()->equal_range(m->GetId());
        for (ActiveCreatureGuidsOnMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
        {
            CreatureData const* data = sObjectMgr.GetCreatureData(itr->second);
            ++forceLoadRequests;
            GridPair gridPair = MaNGOS::ComputeGridPair(data->posX, data->posY);
            uniqueGrids.insert(std::make_pair(gridPair.x_coord, gridPair.y_coord));

            if (m->IsLoaded(data->posX, data->posY))
            {
                ++alreadyLoaded;
            }
            else
            {
                ++newlyLoaded;
            }

            m->ForceLoadGrid(data->posX, data->posY);
        }
    }

    uniqueGridCount = uint32(uniqueGrids.size());

    // Only emit startup summary and accumulate totals during the LoadContinents startup pass
    if (s_livingWorldStartupPass)
    {
        // Accumulate to startup totals
        s_livingWorldStats.totalUniqueGrids += uniqueGridCount;
        s_livingWorldStats.totalNewlyLoaded += newlyLoaded;
        s_livingWorldStats.totalLocalTransports += localTransportCount;
        if (forceLoad)
        {
            ++s_livingWorldStats.forcedMaps;
        }

        // Per-map summary (O(1) log volume)
        if (forceLoad)
        {
            sLog.outString("[LivingWorld] map %u: force-load=ON, creature-rows=%u, ForceLoadGrid-requests=%u, unique-grids=%u, newly-loaded=%u (explicit-locks-set), already-loaded=%u, extra-active-creatures=%u, local-transports=%u",
                m->GetId(), forceLoadRequests, forceLoadRequests, uniqueGridCount, newlyLoaded, alreadyLoaded, activeCreatureGuids, localTransportCount);
        }
        else
        {
            sLog.outString("[LivingWorld] map %u: force-load=OFF, extra-active-creatures=%u, ForceLoadGrid-requests=%u, unique-grids=%u, newly-loaded=%u (explicit-locks-set), already-loaded=%u, local-transports=%u",
                m->GetId(), activeCreatureGuids, forceLoadRequests, uniqueGridCount, newlyLoaded, alreadyLoaded, localTransportCount);
        }
    }
}

/**
 * @brief Executes a worker for every loaded map.
 *
 * @param worker The callback to run for each map.
 */
void MapManager::DoForAllMaps(const std::function<void(Map*)>& worker)
{
    for (MapMapType::const_iterator itr = i_maps.begin(); itr != i_maps.end(); ++itr)
    {
        worker(itr->second);
    }
}
