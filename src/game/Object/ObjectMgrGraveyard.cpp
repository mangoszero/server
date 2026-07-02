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



#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "DBCStores.h"
#include "Log.h"
#include "ProgressBar.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "SQLStorages.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

/**
 * @brief Loads graveyard to zone links from the database.
 */
void ObjectMgr::LoadGraveyardZones()
{
    mGraveYardMap.clear();                                  // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `id`,`ghost_zone`,`faction` FROM `game_graveyard_zone`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u graveyard-zone links", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 safeLocId = fields[0].GetUInt32();
        uint32 zoneId = fields[1].GetUInt32();
        uint32 team   = fields[2].GetUInt32();

        WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(safeLocId);
        if (!entry)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for not existing graveyard (WorldSafeLocs.dbc id) %u, skipped.", safeLocId);
            continue;
        }

        AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(zoneId);
        if (!areaEntry)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for not existing zone id (%u), skipped.", zoneId);
            continue;
        }

        if (areaEntry->zone != 0)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record subzone id (%u) instead of zone, skipped.", zoneId);
            continue;
        }

        if (team != TEAM_BOTH_ALLOWED && team != HORDE && team != ALLIANCE)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for non player faction (%u), skipped.", team);
            continue;
        }

        if (!AddGraveYardLink(safeLocId, zoneId, Team(team), false))
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has a duplicate record for Graveyard (ID: %u) and Zone (ID: %u), skipped.", safeLocId, zoneId);
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u graveyard-zone links", count);
    sLog.outString();
}

/**
 * @brief Gets the closest suitable graveyard for a location and faction.
 *
 * @param x The world x coordinate.
 * @param y The world y coordinate.
 * @param z The world z coordinate.
 * @param MapId The map id.
 * @param team The player's faction.
 * @return The closest matching graveyard safe location, or null if none is linked.
 */
WorldSafeLocsEntry const* ObjectMgr::GetClosestGraveYard(float x, float y, float z, uint32 MapId, Team team)
{
    // search for zone associated closest graveyard
    uint32 zoneId = sTerrainMgr.GetZoneId(MapId, x, y, z);

    // Simulate std. algorithm:
    //   found some graveyard associated to (ghost_zone,ghost_map)
    //
    //   if mapId == graveyard.mapId (ghost in plain zone or city or battleground) and search graveyard at same map
    //     then check faction
    //   if mapId != graveyard.mapId (ghost in instance) and search any graveyard associated
    //     then check faction
    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);

    if (bounds.first == bounds.second)
    {
        sLog.outErrorDb("Table `game_graveyard_zone` incomplete: Zone %u Team %u does not have a linked graveyard.", zoneId, uint32(team));
        return NULL;
    }

    // at corpse map
    bool foundNear = false;
    float distNear;
    WorldSafeLocsEntry const* entryNear = NULL;

    // at entrance map for corpse map
    bool foundEntr = false;
    float distEntr;
    WorldSafeLocsEntry const* entryEntr = NULL;

    // some where other
    WorldSafeLocsEntry const* entryFar = NULL;

    InstanceTemplate const* tempEntry = GetInstanceTemplate(MapId);

    for (GraveYardMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        GraveYardData const& data = itr->second;

        // Checked on load
        WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(data.safeLocId);

        // skip enemy faction graveyard
        // team == TEAM_BOTH_ALLOWED case can be at call from .neargrave
        // TEAM_INVALID != team for all teams
        if (data.team != TEAM_BOTH_ALLOWED && data.team != team && team != TEAM_BOTH_ALLOWED)
        {
            continue;
        }

        // find now nearest graveyard at other (continent) map
        if (MapId != entry->map_id)
        {
            // if find graveyard at different map from where entrance placed (or no entrance data), use any first
            if (!tempEntry ||
                tempEntry->ghostEntranceMap < 0 ||
                uint32(tempEntry->ghostEntranceMap) != entry->map_id ||
                (tempEntry->ghostEntranceX == 0.0f && tempEntry->ghostEntranceY == 0.0f))
            {
                // not have any coordinates for check distance anyway
                entryFar = entry;
                continue;
            }

            // at entrance map calculate distance (2D);
            float dist2 = (entry->x - tempEntry->ghostEntranceX) * (entry->x - tempEntry->ghostEntranceX) +
                (entry->y - tempEntry->ghostEntranceY) * (entry->y - tempEntry->ghostEntranceY);
            if (foundEntr)
            {
                if (dist2 < distEntr)
                {
                    distEntr = dist2;
                    entryEntr = entry;
                }
            }
            else
            {
                foundEntr = true;
                distEntr = dist2;
                entryEntr = entry;
            }
        }
        // find now nearest graveyard at same map
        else
        {
            float dist2 = (entry->x - x) * (entry->x - x) + (entry->y - y) * (entry->y - y) + (entry->z - z) * (entry->z - z);
            if (foundNear)
            {
                if (dist2 < distNear)
                {
                    distNear = dist2;
                    entryNear = entry;
                }
            }
            else
            {
                foundNear = true;
                distNear = dist2;
                entryNear = entry;
            }
        }
    }

    if (entryNear)
    {
        return entryNear;
    }

    if (entryEntr)
    {
        return entryEntr;
    }

    return entryFar;
}

/**
 * @brief Finds graveyard link data for a graveyard id and zone.
 *
 * @param id The graveyard safe location id.
 * @param zoneId The zone id.
 * @return The graveyard link data, or null if not found.
 */
GraveYardData const* ObjectMgr::FindGraveYardData(uint32 id, uint32 zoneId) const
{
    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);

    for (GraveYardMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second.safeLocId == id)
        {
            return &itr->second;
        }
    }

    return NULL;
}

/**
 * @brief Adds a graveyard link for a zone and optional database persistence.
 *
 * @param id The graveyard safe location id.
 * @param zoneId The zone id.
 * @param team The faction restriction for the link.
 * @param inDB true to also insert the link into the database.
 * @return true if the link was added; otherwise, false.
 */
bool ObjectMgr::AddGraveYardLink(uint32 id, uint32 zoneId, Team team, bool inDB)
{
    if (FindGraveYardData(id, zoneId))                      // This ensures that (safeLoc)Id,  zoneId is unique in mGraveYardMap
    {
        return false;
    }

    // add link to loaded data
    GraveYardData data;
    data.safeLocId = id;
    data.team = team;

    mGraveYardMap.insert(GraveYardMap::value_type(zoneId, data));

    // add link to DB
    if (inDB)
    {
        WorldDatabase.PExecuteLog("INSERT INTO `game_graveyard_zone` (`id`,`ghost_zone`,`faction`) VALUES ('%u', '%u','%u')", id, zoneId, uint32(team));
    }

    return true;
}

/**
 * @brief Updates the faction restriction for an existing graveyard link.
 *
 * @param id The graveyard safe location id.
 * @param zoneId The zone id.
 * @param team The faction restriction to set.
 */
void ObjectMgr::SetGraveYardLinkTeam(uint32 id, uint32 zoneId, Team team)
{
    std::pair<GraveYardMap::iterator, GraveYardMap::iterator> bounds = mGraveYardMap.equal_range(zoneId);

    for (GraveYardMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        GraveYardData& data = itr->second;

        // skip not matching safezone id
        if (data.safeLocId != id)
        {
            continue;
        }

        data.team = team;                                   // Validate link
        return;
    }

    if (team == TEAM_INVALID)
    {
        return;
    }

    // Link expected but not exist.
    sLog.outErrorDb("ObjectMgr::SetGraveYardLinkTeam called for safeLoc %u, zoneId %u, but no graveyard link for this found in database.", id, zoneId);
    AddGraveYardLink(id, zoneId, team);                     // Add to prevent further error message and correct mechanismn
}
