/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Chat.h"
#include "Language.h"
#include "World.h"
#include "MoveMap.h"
#include "PathFinder.h" // for mmap manager
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"          // for mmap manager
#include "CellImpl.h"
#include "movement/MoveSplineInit.h"
#include "GameTime.h"
#include <fstream>
#include <map>
#include <typeinfo>

bool ChatHandler::HandleMmapPathCommand(char* args)
{
    if (!MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(m_session->GetPlayer()->GetMapId()))
    {
        PSendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    PSendSysMessage("mmap path:");

    // units
    Player* player = m_session->GetPlayer();
    Unit* target = getSelectedUnit();
    if (!player || !target)
    {
        PSendSysMessage("Invalid target/source selection.");
        return true;
    }

    char* para = strtok(args, " ");

    bool useStraightPath = false;
    bool followPath = false;
    bool unitToPlayer = false;
    if (para)
    {
        if (strcmp(para, "go") == 0)
        {
            followPath = true;
            para = strtok(NULL, " ");
            if (para && strcmp(para, "straight") == 0)
            {
                useStraightPath = true;
            }
        }
        else if (strcmp(para, "straight") == 0)
        {
            useStraightPath = true;
        }
        else if (strcmp(para, "to_me") == 0)
        {
            unitToPlayer = true;
        }
        else
        {
            PSendSysMessage("Use '.mmap path go' to move on target.");
            PSendSysMessage("Use '.mmap path straight' to generate straight path.");
            PSendSysMessage("Use '.mmap path to_me' to generate path from the target to you.");
        }
    }

    Unit* destinationUnit;
    Unit* originUnit;
    if (unitToPlayer)
    {
        destinationUnit = player;
        originUnit = target;
    }
    else
    {
        destinationUnit = target;
        originUnit = player;
    }

    // unit locations
    float x, y, z;
    destinationUnit->GetPosition(x, y, z);

    // path
    PathFinder path(originUnit);
    path.setUseStrightPath(useStraightPath);
    path.calculate(x, y, z);

    PointsArray pointPath = path.getPath();
    PSendSysMessage("%s's path to %s:", originUnit->GetName(), destinationUnit->GetName());
    PSendSysMessage("Building %s", useStraightPath ? "StraightPath" : "SmoothPath");
    PSendSysMessage("length " SIZEFMTD " type %u", pointPath.size(), path.getPathType());

    Vector3 start = path.getStartPosition();
    Vector3 end = path.getEndPosition();
    Vector3 actualEnd = path.getActualEndPosition();

    PSendSysMessage("start      (%.3f, %.3f, %.3f)", start.x, start.y, start.z);
    PSendSysMessage("end        (%.3f, %.3f, %.3f)", end.x, end.y, end.z);
    PSendSysMessage("actual end (%.3f, %.3f, %.3f)", actualEnd.x, actualEnd.y, actualEnd.z);

    if (!player->isGameMaster())
    {
        PSendSysMessage("Enable GM mode to see the path points.");
    }

    for (uint32 i = 0; i < pointPath.size(); ++i)
    {
        player->SummonCreature(VISUAL_WAYPOINT, pointPath[i].x, pointPath[i].y, pointPath[i].z, 0, TEMPSPAWN_TIMED_DESPAWN, 9000);
    }

    if (followPath)
    {
        Movement::MoveSplineInit init(*player);
        init.MovebyPath(pointPath);
        init.SetWalk(false);
        init.Launch();
    }

    return true;
}

bool ChatHandler::HandleMmapLocCommand(char* /*args*/)
{
    PSendSysMessage("mmap tileloc:");

    // grid tile location
    Player* player = m_session->GetPlayer();

    int32 gx = 32 - player->GetPositionX() / SIZE_OF_GRIDS;
    int32 gy = 32 - player->GetPositionY() / SIZE_OF_GRIDS;

    PSendSysMessage("%03u%02i%02i.mmtile", player->GetMapId(), gy, gx);
    PSendSysMessage("gridloc [%i,%i]", gx, gy);

    // calculate navmesh tile location
    const dtNavMesh* navmesh = MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(player->GetMapId());
    const dtNavMeshQuery* navmeshquery = MMAP::MMapFactory::createOrGetMMapManager()->GetNavMeshQuery(player->GetMapId(), player->GetInstanceId());
    if (!navmesh || !navmeshquery)
    {
        PSendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    const float* min = navmesh->getParams()->orig;

    float x, y, z;
    player->GetPosition(x, y, z);
    float location[VERTEX_SIZE] = {y, z, x};
    float extents[VERTEX_SIZE] = {3.0f, 5.0f, 3.0f};

    int32 tilex = int32((y - min[0]) / SIZE_OF_GRIDS);
    int32 tiley = int32((x - min[2]) / SIZE_OF_GRIDS);

    PSendSysMessage("Calc   [%02i,%02i]", tilex, tiley);

    // navmesh poly -> navmesh tile location
    dtQueryFilter filter = dtQueryFilter();
    dtPolyRef polyRef = INVALID_POLYREF;
    navmeshquery->findNearestPoly(location, extents, &filter, &polyRef, NULL);

    if (polyRef == INVALID_POLYREF)
    {
        PSendSysMessage("Dt     [??,??] (invalid poly, probably no tile loaded)");
    }
    else
    {
        const dtMeshTile* tile;
        const dtPoly* poly;
        dtStatus dtResult = navmesh->getTileAndPolyByRef(polyRef, &tile, &poly);
        if ((dtStatusSucceed(dtResult)) && tile)
        {
            PSendSysMessage("Dt     [%02i,%02i]", tile->header->x, tile->header->y);
        }
        else
        {
            PSendSysMessage("Dt     [??,??] (no tile loaded)");
        }
    }

    return true;
}

bool ChatHandler::HandleMmapLoadedTilesCommand(char* /*args*/)
{
    uint32 mapid = m_session->GetPlayer()->GetMapId();

    const dtNavMesh* navmesh = MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(mapid);
    const dtNavMeshQuery* navmeshquery = MMAP::MMapFactory::createOrGetMMapManager()->GetNavMeshQuery(mapid, m_session->GetPlayer()->GetInstanceId());
    if (!navmesh || !navmeshquery)
    {
        PSendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    PSendSysMessage("mmap loadedtiles:");

    for (int32 i = 0; i < navmesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = navmesh->getTile(i);
        if (!tile || !tile->header)
        {
            continue;
        }

        PSendSysMessage("[%02i,%02i]", tile->header->x, tile->header->y);
    }

    return true;
}

bool ChatHandler::HandleMmapStatsCommand(char* /*args*/)
{
    PSendSysMessage("mmap stats:");
    PSendSysMessage("  global mmap pathfinding is %sabled", sWorld.getConfig(CONFIG_BOOL_MMAP_ENABLED) ? "en" : "dis");

    MMAP::MMapManager* manager = MMAP::MMapFactory::createOrGetMMapManager();
    PSendSysMessage(" %u maps loaded with %u tiles overall", manager->getLoadedMapsCount(), manager->getLoadedTilesCount());

    const dtNavMesh* navmesh = manager->GetNavMesh(m_session->GetPlayer()->GetMapId());
    if (!navmesh)
    {
        PSendSysMessage("NavMesh not loaded for current map.");
        return true;
    }

    uint32 tileCount = 0;
    uint32 nodeCount = 0;
    uint32 polyCount = 0;
    uint32 vertCount = 0;
    uint32 triCount = 0;
    uint32 triVertCount = 0;
    uint32 dataSize = 0;
    for (int32 i = 0; i < navmesh->getMaxTiles(); ++i)
    {
        const dtMeshTile* tile = navmesh->getTile(i);
        if (!tile || !tile->header)
        {
            continue;
        }

        tileCount++;
        nodeCount += tile->header->bvNodeCount;
        polyCount += tile->header->polyCount;
        vertCount += tile->header->vertCount;
        triCount += tile->header->detailTriCount;
        triVertCount += tile->header->detailVertCount;
        dataSize += tile->dataSize;
    }

    PSendSysMessage("Navmesh stats on current map:");
    PSendSysMessage(" %u tiles loaded", tileCount);
    PSendSysMessage(" %u BVTree nodes", nodeCount);
    PSendSysMessage(" %u polygons (%u vertices)", polyCount, vertCount);
    PSendSysMessage(" %u triangles (%u vertices)", triCount, triVertCount);
    PSendSysMessage(" %.2f MB of data (not including pointers)", ((float)dataSize / sizeof(unsigned char)) / 1048576);

    return true;
}

bool ChatHandler::HandleMmap(char* args)
{
    bool on;
    if (ExtractOnOff(&args, on))
    {
        if (on)
        {
            sWorld.setConfig(CONFIG_BOOL_MMAP_ENABLED, true);
            SendSysMessage("WORLD: mmaps are now ENABLED (individual map settings still in effect)");
        }
        else
        {
            sWorld.setConfig(CONFIG_BOOL_MMAP_ENABLED, false);
            SendSysMessage("WORLD: mmaps are now DISABLED");
        }
        return true;
    }

    on = sWorld.getConfig(CONFIG_BOOL_MMAP_ENABLED);
    PSendSysMessage("mmaps are %sabled", on ? "en" : "dis");

    return true;
}

bool ChatHandler::HandleMmapTestArea(char* args)
{
    float radius = 40.0f;
    ExtractFloat(&args, radius);

    std::list<Creature*> creatureList;
    MaNGOS::AnyUnitInObjectRangeCheck go_check(m_session->GetPlayer(), radius);
    MaNGOS::CreatureListSearcher<MaNGOS::AnyUnitInObjectRangeCheck> go_search(creatureList, go_check);
    // Get Creatures
    Cell::VisitGridObjects(m_session->GetPlayer(), go_search, radius);

    if (!creatureList.empty())
    {
        PSendSysMessage("Found " SIZEFMTD " Creatures.", creatureList.size());

        uint32 paths = 0;
        uint32 uStartTime = GameTime::GetGameTimeMS();

        float gx, gy, gz;
        m_session->GetPlayer()->GetPosition(gx, gy, gz);
        for (std::list<Creature*>::iterator itr = creatureList.begin(); itr != creatureList.end(); ++itr)
        {
            PathFinder path(*itr);
            path.calculate(gx, gy, gz);
            ++paths;
        }

        uint32 uPathLoadTime = getMSTimeDiff(uStartTime, GameTime::GetGameTimeMS());
        PSendSysMessage("Generated %i paths in %i ms", paths, uPathLoadTime);
    }
    else
    {
        PSendSysMessage("No creatures in %f yard range.", radius);
    }

    return true;
}

// use ".mmap testheight 10" selecting any creature/player
bool ChatHandler::HandleMmapTestHeight(char* args)
{
    float radius = 0.0f;
    ExtractFloat(&args, radius);
    if (radius > 40.0f)
    {
        radius = 40.0f;
    }

    Unit* unit = getSelectedUnit();

    Player* player = m_session->GetPlayer();
    if (!unit)
    {
        unit = player;
    }

    if (unit->GetTypeId() == TYPEID_UNIT)
    {
        if (radius < 0.1f)
        {
            radius = static_cast<Creature*>(unit)->GetRespawnRadius();
        }
    }
    else
    {
        if (unit->GetTypeId() != TYPEID_PLAYER)
        {
            PSendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            return false;
        }
    }

    if (radius < 0.1f)
    {
        PSendSysMessage("Provided spawn radius for %s is too small. Using 5.0f instead.", unit->GetGuidStr().c_str());
        radius = 5.0f;
    }

    float gx, gy, gz;
    unit->GetPosition(gx, gy, gz);

    Creature* summoned = unit->SummonCreature(VISUAL_WAYPOINT, gx, gy, gz + 0.5f, 0, TEMPSPAWN_TIMED_DESPAWN, 20000);
    summoned->CastSpell(summoned, 8599, false);
    uint32 tries = 1;
    uint32 successes = 0;
    uint32 startTime = GameTime::GetGameTimeMS();
    for (; tries < 500; ++tries)
    {
        unit->GetPosition(gx, gy, gz);
        if (unit->GetMap()->GetReachableRandomPosition(unit, gx, gy, gz, radius))
        {
            unit->SummonCreature(VISUAL_WAYPOINT, gx, gy, gz, 0, TEMPSPAWN_TIMED_DESPAWN, 15000);
            ++successes;
            if (successes >= 100)
            {
                break;
            }
        }
    }
    uint32 genTime = getMSTimeDiff(startTime, GameTime::GetGameTimeMS());
    PSendSysMessage("Generated %u valid points for %u try in %ums.", successes, tries, genTime);
    return true;
}
