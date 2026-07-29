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

#include <string>
#include <mutex>
#include "Utilities/Errors.h"
#include "MapManager.h"

/**
 * @file GridMap.cpp
 * @brief Map grid data loading and management
 *
 * This file implements GridMap which loads and manages terrain data
 * for a single map grid (64x64 yard cell). Features:
 *
 * - Height map loading from .map files
 * - Area/zone ID loading
 * - Liquid data (water, lava) loading
 * - Hole data for terrain gaps
 * - Terrain height queries
 * - Area ID queries
 * - Liquid level queries
 *
 * GridMaps are loaded on-demand and cached by the Map system.
 *
 * @see GridMap for the grid class
 * @see Map for the map container
 */

#include "Log.h"
#include "GridStates.h"
#include "CellImpl.h"
#include "Map.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "GridMap.h"
#include "terrain/TileSerializer.hpp"
#include "MoveMap.h"
#include "World.h"
#include "Policies/Singleton.h"
#include "Util.h"

char const* MAP_MAGIC         = "MAPS";
char const* MAP_VERSION_MAGIC = "v1.5";
char const* MAP_AREA_MAGIC    = "AREA";
char const* MAP_HEIGHT_MAGIC  = "MHGT";
char const* MAP_LIQUID_MAGIC  = "MLIQ";

static uint16 holetab_h[4] = { 0x1111, 0x2222, 0x4444, 0x8888 };
static uint16 holetab_v[4] = { 0x000F, 0x00F0, 0x0F00, 0xF000 };

namespace
{
    using world::terrain::FusedTerrain;
    using world::terrain::LiquidInfo;

    // Ids the client hard-codes rather than reads from the DBC.
    const uint32 OUTLAND_MAP_ID = 530;
    const uint32 LIQUID_OCEAN_ROW = 2;
    const uint32 LIQUID_OUTLAND_OCEAN_ROW = 15;
    const uint32 LIQUID_FIRST_OVERRIDABLE_ROW = 21;

    // MAP_LIQUID_TYPE_* is one bit per family in the order water, ocean, magma, slime.
    // 1.12 has no SoundBank column -- that arrives in 3.3.5a -- so the family comes from
    // LiquidType.dbc's Type, which uses a DIFFERENT encoding: 0 magma, 2 slime, 3 water.
    // Ocean is not expressible there at all and the row id decides it instead.
    uint32 LiquidFlagsOfRow(uint32 entry, uint32& soundBank)
    {
        soundBank = 0;
        if (LiquidTypeEntry const* liq = sLiquidTypeStore.LookupEntry(entry))
        {
            switch (liq->Type)
            {
                case 0:  soundBank = 2; break;              // magma
                case 2:  soundBank = 3; break;              // slime
                default: soundBank = (entry == LIQUID_OCEAN_ROW ||
                                      entry == LIQUID_OUTLAND_OCEAN_ROW) ? 1 : 0; break;
            }
        }
        return 1u << soundBank;
    }

    // The tile names an area by its AreaTable.dbc id; the server passes area BITS.
    uint16 AreaBitOfId(uint16 areaId)
    {
        if (!areaId)
        {
            return 0;
        }
        AreaTableEntry const* entry = GetAreaEntryByAreaID(areaId);
        return entry ? uint16(entry->AreaBit) : uint16(0);
    }

    bool IsOutdoorWMO(uint32 mogpFlags, WMOAreaTableEntry const* wmoEntry,
                      AreaTableEntry const* atEntry)
    {
        // 3.3.5a lets AreaTable.dbc override the WMO's own answer through
        // AREA_FLAG_INSIDE/OUTSIDE. Those bits do not exist in 1.12 -- its area flags stop
        // at 0x00100000 -- so the WMO's group flags are the only authority here.
        (void)atEntry;

        bool outdoor = (mogpFlags & 0x8) != 0;
        if (wmoEntry)
        {
            if (wmoEntry->Flags & 4)
            {
                return true;
            }
            if (wmoEntry->Flags & 2)
            {
                outdoor = false;
            }
        }
        return outdoor;
    }
}

TerrainInfo::TerrainInfo(uint32 mapid) : m_mapId(mapid), m_terrain(mapid), m_refMutex()
{
    for (int k = 0; k < MAX_NUMBER_OF_GRIDS; ++k)
    {
        for (int i = 0; i < MAX_NUMBER_OF_GRIDS; ++i)
        {
            m_GridRef[i][k] = 0;
        }
    }

    i_timer.SetInterval(60 * 1000);
    i_timer.SetCurrent(urand(20, 40) * 1000);
}

TerrainInfo::~TerrainInfo()
{
    MMAP::MMapFactory::createOrGetMMapManager()->unloadMap(m_mapId);
}

bool TerrainInfo::ExistTile(uint32 mapid, int gx, int gy)
{
    if (FusedTerrain::HasTile(mapid, gx, gy))
    {
        return true;
    }
    sLog.outError("Please check for the existence of terrain tile '%s/%s'",
                  FusedTerrain::TileDir().c_str(),
                  world::terrain::TileFileName(mapid, gx, gy).c_str());
    return false;
}

bool TerrainInfo::Load(const uint32 x, const uint32 y)
{
    MANGOS_ASSERT(x < MAX_NUMBER_OF_GRIDS);
    MANGOS_ASSERT(y < MAX_NUMBER_OF_GRIDS);

    bool firstReference = false;
    {
        std::lock_guard<LOCK_TYPE> lock(m_refMutex);
        firstReference = (++m_GridRef[x][y] == 1);
    }

    // Pins the cell's tile against the cache sweep for as long as a grid stands on it.
    // The tile data itself still loads lazily, on the first query that reaches it.
    m_terrain.PinCell(int(x), int(y));

    // The navmesh tile is loaded by the FIRST referent only -- the refcount above is what
    // makes several owners of one grid legal, and Unload already releases on the last.
    // Loading unconditionally made every second owner ask for a tile the first had already
    // brought in, which the mmap manager rejects and logs. Common now that a vessel is an
    // active object holding grids a player then walks into.
    if (firstReference)
    {
        MMAP::MMapFactory::createOrGetMMapManager()->loadMap(m_mapId, x, y);
    }
    return true;
}

void TerrainInfo::Unload(const uint32 x, const uint32 y)
{
    MANGOS_ASSERT(x < MAX_NUMBER_OF_GRIDS);
    MANGOS_ASSERT(y < MAX_NUMBER_OF_GRIDS);

    bool released = false;
    {
        std::lock_guard<LOCK_TYPE> lock(m_refMutex);
        if (m_GridRef[x][y] > 0)
        {
            released = (--m_GridRef[x][y] == 0);
        }
    }

    if (released)
    {
        m_terrain.UnpinCell(int(x), int(y));
    }
}

void TerrainInfo::CleanUpGrids(const uint32 diff)
{
    m_terrain.Update(diff);

    i_timer.Update(diff);
    if (!i_timer.Passed())
    {
        return;
    }

    for (int y = 0; y < MAX_NUMBER_OF_GRIDS; ++y)
    {
        for (int x = 0; x < MAX_NUMBER_OF_GRIDS; ++x)
        {
            std::lock_guard<LOCK_TYPE> lock(m_refMutex);
            if (m_GridRef[x][y] == 0)
            {
                MMAP::MMapFactory::createOrGetMMapManager()->unloadMap(m_mapId, x, y);
            }
        }
    }

    i_timer.Reset();
}

world::terrain::Column TerrainInfo::ColumnAt(float x, float y, float zTop, float zBottom,
                                             const world::terrain::ILiveGeometry* live,
                                             uint32 phasemask) const
{
    return m_terrain.ColumnAt(x, y, zTop, zBottom, live, phasemask);
}

std::optional<float> TerrainInfo::StaticFloor(float x, float y, float z) const
{
    return ColumnAt(x, y, z + FLOOR_BURIED_LIFT, z - FLOOR_SEARCH_DOWN)
           .Floor(z, FLOOR_SEARCH_UP);
}

bool TerrainInfo::GetAreaInfo(float x, float y, float z, uint32& flags, int32& adtId,
                              int32& rootId, int32& groupId) const
{
    float groundZ = 0.0f;
    return m_terrain.GetAreaInfo(x, y, z, flags, adtId, rootId, groupId, groundZ);
}

bool TerrainInfo::IsOutdoors(float x, float y, float z) const
{
    uint32 mogpFlags = 0;
    int32 adtId = 0, rootId = 0, groupId = 0;
    if (!GetAreaInfo(x, y, z, mogpFlags, adtId, rootId, groupId))
    {
        return true;
    }

    AreaTableEntry const* atEntry = 0;
    WMOAreaTableEntry const* wmoEntry = GetWMOAreaTableEntryByTripple(rootId, adtId, groupId);
    if (wmoEntry)
    {
        atEntry = GetAreaEntryByAreaID(wmoEntry->AreaTableID);
    }
    return IsOutdoorWMO(mogpFlags, wmoEntry, atEntry);
}

uint16 TerrainInfo::GetAreaFlag(float x, float y, float z, bool* isOutdoors) const
{
    uint32 mogpFlags = 0;
    int32 adtId = 0, rootId = 0, groupId = 0;
    WMOAreaTableEntry const* wmoEntry = 0;
    AreaTableEntry const* atEntry = 0;

    const bool haveAreaInfo = GetAreaInfo(x, y, z, mogpFlags, adtId, rootId, groupId);
    if (haveAreaInfo)
    {
        wmoEntry = GetWMOAreaTableEntryByTripple(rootId, adtId, groupId);
        if (wmoEntry)
        {
            atEntry = GetAreaEntryByAreaID(wmoEntry->AreaTableID);
        }
    }

    uint16 areaflag;
    if (atEntry)
    {
        areaflag = atEntry->AreaBit;
    }
    else if (uint16 bit = AreaBitOfId(m_terrain.GetAreaId(x, y)))
    {
        areaflag = bit;
    }
    else
    {
        areaflag = GetAreaFlagByMapId(m_mapId);
    }

    if (isOutdoors)
    {
        *isOutdoors = haveAreaInfo ? IsOutdoorWMO(mogpFlags, wmoEntry, atEntry) : true;
    }
    return areaflag;
}

uint8 TerrainInfo::GetTerrainType(float x, float y) const
{
    const auto liquid = ColumnAt(x, y, MAX_HEIGHT, -MAX_HEIGHT).HighestLiquid();
    if (!liquid || !liquid->liquidEntry)
    {
        return 0;
    }
    uint32 soundBank = 0;
    return uint8(LiquidFlagsOfRow(liquid->liquidEntry, soundBank));
}

uint32 TerrainInfo::GetAreaId(float x, float y, float z) const
{
    return TerrainManager::GetAreaIdByAreaFlag(GetAreaFlag(x, y, z), m_mapId);
}

uint32 TerrainInfo::GetZoneId(float x, float y, float z) const
{
    return TerrainManager::GetZoneIdByAreaFlag(GetAreaFlag(x, y, z), m_mapId);
}

void TerrainInfo::GetZoneAndAreaId(uint32& zoneid, uint32& areaid, float x, float y,
                                   float z) const
{
    TerrainManager::GetZoneAndAreaIdByAreaFlag(zoneid, areaid, GetAreaFlag(x, y, z), m_mapId);
}

GridMapLiquidStatus TerrainInfo::getLiquidStatus(float x, float y, float z,
                                                 uint8 ReqLiquidType,
                                                 GridMapLiquidData* data) const
{
    // One sweep answers both halves of this: the liquid surface AND the floor under it,
    // which the depth and the "is there really water here" test below both need.
    const world::terrain::Column column =
        ColumnAt(x, y, z + FLOOR_BURIED_LIFT, z - FLOOR_SEARCH_DOWN);

    const auto liquid = column.HighestLiquid();
    if (!liquid || !liquid->liquidEntry)
    {
        return LIQUID_MAP_NO_WATER;
    }
    const LiquidInfo info = liquid->AsLiquid();

    uint32 entry = info.entry;
    // Hard-coded in the client: Outland's ocean is its own row.
    if (m_mapId == OUTLAND_MAP_ID && entry == LIQUID_OCEAN_ROW)
    {
        entry = LIQUID_OUTLAND_OCEAN_ROW;
    }

    uint32 soundBank = 0;
    uint32 typeFlags = LiquidFlagsOfRow(entry, soundBank);

    // An area may override the liquid row for its own family, which is what gives a
    // zone's water its aura. Only the canonical rows are overridable.
    if (entry < LIQUID_FIRST_OVERRIDABLE_ROW)
    {
        if (AreaTableEntry const* area =
                GetAreaEntryByAreaFlagAndMap(GetAreaFlag(x, y, z), m_mapId))
        {
            uint32 overrideLiquid = area->LiquidTypeID_3;
            if (!overrideLiquid && area->ParentAreaID)
            {
                if (AreaTableEntry const* parent = GetAreaEntryByAreaID(area->ParentAreaID))
                {
                    overrideLiquid = parent->LiquidTypeID_3;
                }
            }
            if (overrideLiquid && sLiquidTypeStore.LookupEntry(overrideLiquid))
            {
                entry = overrideLiquid;
                typeFlags = LiquidFlagsOfRow(entry, soundBank);
            }
        }
    }

    if (info.deep)
    {
        typeFlags |= MAP_LIQUID_TYPE_DARK_WATER;
    }

    if (ReqLiquidType && !(typeFlags & ReqLiquidType))
    {
        return LIQUID_MAP_NO_WATER;
    }

    const auto floor = column.Floor(z, FLOOR_SEARCH_UP);
    const float groundZ = floor ? *floor : INVALID_HEIGHT;
    if (info.level <= groundZ || z <= groundZ - 2.0f)
    {
        return LIQUID_MAP_NO_WATER;
    }

    if (data)
    {
        data->level = info.level;
        data->depth_level = groundZ;
        data->entry = entry;
        data->type_flags = typeFlags;
    }

    // Compared as ints for speed, exactly as the original did.
    const int delta = int((info.level - z) * 10);
    if (delta > 20)
    {
        return LIQUID_MAP_UNDER_WATER;
    }
    if (delta > 0)
    {
        return LIQUID_MAP_IN_WATER;
    }
    if (delta > -1)
    {
        return LIQUID_MAP_WATER_WALK;
    }
    return LIQUID_MAP_ABOVE_WATER;
}

bool TerrainInfo::IsInWater(float x, float y, float pZ, GridMapLiquidData* data) const
{
    GridMapLiquidData liquidStatus;
    GridMapLiquidData* out = data ? data : &liquidStatus;
    return (getLiquidStatus(x, y, pZ, MAP_ALL_LIQUIDS, out) &
            (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)) != 0;
}

bool TerrainInfo::IsAboveWater(float x, float y, float z, float* pWaterZ) const
{
    GridMapLiquidData liquidStatus;
    if (!(getLiquidStatus(x, y, z, MAP_ALL_LIQUIDS, &liquidStatus) &
          (LIQUID_MAP_ABOVE_WATER | LIQUID_MAP_WATER_WALK)))
    {
        return false;
    }
    if (pWaterZ)
    {
        *pWaterZ = liquidStatus.level;
    }
    return true;
}

bool TerrainInfo::IsUnderWater(float x, float y, float z) const
{
    return (getLiquidStatus(x, y, z, MAP_LIQUID_TYPE_WATER | MAP_LIQUID_TYPE_OCEAN) &
            LIQUID_MAP_UNDER_WATER) != 0;
}

bool TerrainInfo::IsSwimmable(float x, float y, float z, float radius,
                              GridMapLiquidData* data) const
{
    GridMapLiquidData liquidStatus;
    GridMapLiquidData* liquid = data ? data : &liquidStatus;
    if (!getLiquidStatus(x, y, z, MAP_ALL_LIQUIDS, liquid))
    {
        return false;
    }
    return liquid->level - liquid->depth_level > radius;
}

std::optional<float> TerrainInfo::GetWaterLevel(float x, float y, float z,
                                                float* pGround) const
{
    const auto floor = StaticFloor(x, y, z);
    const float groundZ = floor ? *floor : INVALID_HEIGHT;
    if (pGround)
    {
        *pGround = groundZ;
    }

    GridMapLiquidData liquidStatus;
    if (!(getLiquidStatus(x, y, groundZ, MAP_ALL_LIQUIDS, &liquidStatus) &
          (LIQUID_MAP_ABOVE_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)))
    {
        return std::nullopt;
    }
    return liquidStatus.level;
}

float TerrainInfo::GetWaterOrGroundLevel(float x, float y, float z, float* pGround,
                                         bool swim) const
{
    const auto floor = StaticFloor(x, y, z);
    const float groundZ = floor ? *floor : INVALID_HEIGHT;
    if (pGround)
    {
        *pGround = groundZ;
    }

    GridMapLiquidData liquid_status;
    if (!(getLiquidStatus(x, y, groundZ, MAP_ALL_LIQUIDS, &liquid_status) &
          (LIQUID_MAP_ABOVE_WATER | LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER)))
    {
        return groundZ;
    }
    return swim ? liquid_status.level - 2.0f : liquid_status.level;
}

bool TerrainInfo::IsInLineOfSight(float x1, float y1, float z1, float x2, float y2,
                                  float z2) const
{
    return m_terrain.IsInLineOfSight(x1, y1, z1, x2, y2, z2);
}

float TerrainInfo::NearestHitFraction(float x1, float y1, float z1, float x2, float y2,
                                      float z2) const
{
    return m_terrain.NearestHitFraction(x1, y1, z1, x2, y2, z2);
}

TerrainManager::TerrainManager() : m_mutex()
{
}

TerrainManager::~TerrainManager()
{
    for (TerrainDataMap::iterator it = i_TerrainMap.begin(); it != i_TerrainMap.end(); ++it)
    {
        delete it->second;
    }
}

/**
 * @brief Loads or creates terrain information for a map.
 *
 * @param mapId The map id.
 * @return The terrain info instance.
 */
TerrainInfo* TerrainManager::LoadTerrain(const uint32 mapId)
{
    std::lock_guard<LOCK_TYPE> _guard(m_mutex);

    TerrainDataMap::const_iterator iter = i_TerrainMap.find(mapId);
    if (iter == i_TerrainMap.end())
    {
        TerrainInfo* ti = new TerrainInfo(mapId);
        i_TerrainMap[mapId] = ti;
        return ti;
    }

    return (*iter).second;
}

/**
 * @brief Unloads terrain information for a map when no longer referenced.
 *
 * @param mapId The map id.
 */
void TerrainManager::UnloadTerrain(const uint32 mapId)
{
    if (sWorld.getConfig(CONFIG_BOOL_GRID_UNLOAD) == 0)
    {
        return;
    }

    std::lock_guard<LOCK_TYPE> _guard(m_mutex);

    TerrainDataMap::iterator iter = i_TerrainMap.find(mapId);
    if (iter != i_TerrainMap.end())
    {
        TerrainInfo* ptr = (*iter).second;
        // lets check if this object can be actually freed
        if (ptr->IsReferenced() == false)
        {
            i_TerrainMap.erase(iter);
            delete ptr;
        }
    }
}

/**
 * @brief Updates terrain cleanup timers for all loaded maps.
 *
 * @param diff Elapsed update time in milliseconds.
 */
void TerrainManager::Update(const uint32 diff)
{
    // global garbage collection for GridMap objects and VMaps
    for (TerrainDataMap::iterator iter = i_TerrainMap.begin(); iter != i_TerrainMap.end(); ++iter)
    {
        iter->second->CleanUpGrids(diff);
    }
}

/**
 * @brief Unloads all cached terrain information.
 */
void TerrainManager::UnloadAll()
{
    for (TerrainDataMap::iterator it = i_TerrainMap.begin(); it != i_TerrainMap.end(); ++it)
    {
        delete it->second;
    }

    i_TerrainMap.clear();
}

/**
 * @brief Resolves an area id from an explore flag and map id.
 *
 * @param areaflag The area explore flag.
 * @param map_id The map id.
 * @return The resolved area id.
 */
uint32 TerrainManager::GetAreaIdByAreaFlag(uint16 areaflag, uint32 map_id)
{
    AreaTableEntry const* entry = GetAreaEntryByAreaFlagAndMap(areaflag, map_id);

    if (entry)
    {
        return entry->ID;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Resolves a zone id from an explore flag and map id.
 *
 * @param areaflag The area explore flag.
 * @param map_id The map id.
 * @return The resolved zone id.
 */
uint32 TerrainManager::GetZoneIdByAreaFlag(uint16 areaflag, uint32 map_id)
{
    AreaTableEntry const* entry = GetAreaEntryByAreaFlagAndMap(areaflag, map_id);

    if (entry)
    {
        return (entry->ParentAreaID != 0) ? entry->ParentAreaID : entry->ID;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Resolves both zone id and area id from an explore flag and map id.
 *
 * @param zoneid Receives the zone id.
 * @param areaid Receives the area id.
 * @param areaflag The area explore flag.
 * @param map_id The map id.
 */
void TerrainManager::GetZoneAndAreaIdByAreaFlag(uint32& zoneid, uint32& areaid, uint16 areaflag, uint32 map_id)
{
    AreaTableEntry const* entry = GetAreaEntryByAreaFlagAndMap(areaflag, map_id);

    areaid = entry ? entry->ID : 0;
    zoneid = entry ? ((entry->ParentAreaID != 0) ? entry->ParentAreaID : entry->ID) : 0;
}
