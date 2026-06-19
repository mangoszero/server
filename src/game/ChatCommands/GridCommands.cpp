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
 * @file GridCommands.cpp
 * @brief Read-only LivingWorld grid/cell occupancy diagnostic commands.
 *
 * These GM-only, in-game-only commands report how many of a grid's 256 cells
 * contain DB-backed spawns, to help quantify whether future cell-cluster object
 * loading is worthwhile (LivingWorld Phase 1.5).
 *
 * IMPORTANT — all counts are STATIC DB spawn-definition counts (how many spawn
 * GUIDs are registered in each cell's spawn store), NOT live in-memory object
 * counts. The commands never load a grid, create a map, spawn anything, query
 * the database, or mutate world state:
 *  - occupancy is read from sObjectMgr's per-cell spawn store via the find-based,
 *    non-inserting GetCellObjectGuidsReadOnly();
 *  - load state is read via Map::IsGridLoaded() (never loads);
 *  - the anchor set is read via the existing ObjectMgr active-creature accessor.
 */

#include "Chat.h"
#include "Player.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Creature.h"
#include "GridDefines.h"

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

namespace
{
    uint32 const CELLS_PER_GRID = MAX_NUMBER_OF_CELLS * MAX_NUMBER_OF_CELLS; // 16 * 16 = 256

    struct CellOccupancy
    {
        uint32 cellX;
        uint32 cellY;
        uint32 creatures;
        uint32 gameobjects;
        uint32 corpses;
        uint32 Total() const { return creatures + gameobjects + corpses; }
    };

    struct GridOccupancy
    {
        uint32 occupiedCells;
        uint32 creatures;
        uint32 gameobjects;
        uint32 corpses;
        std::vector<CellOccupancy> occupied;

        GridOccupancy() : occupiedCells(0), creatures(0), gameobjects(0), corpses(0) {}
        uint32 TotalGuids() const { return creatures + gameobjects + corpses; }
    };

    // Static DB spawn-definition occupancy for one grid. Read-only: reads only the
    // ObjectMgr per-cell spawn store. Never loads/creates grids, never spawns.
    GridOccupancy ComputeGridOccupancy(uint16 mapId, uint32 gridX, uint32 gridY)
    {
        GridOccupancy occ;
        for (uint32 cellX = 0; cellX < MAX_NUMBER_OF_CELLS; ++cellX)
        {
            for (uint32 cellY = 0; cellY < MAX_NUMBER_OF_CELLS; ++cellY)
            {
                uint32 globalX = (gridX * MAX_NUMBER_OF_CELLS) + cellX;
                uint32 globalY = (gridY * MAX_NUMBER_OF_CELLS) + cellY;
                uint32 cellId = (globalY * TOTAL_NUMBER_OF_CELLS_PER_MAP) + globalX;

                CellObjectGuids const* guids = sObjectMgr.GetCellObjectGuidsReadOnly(mapId, cellId);
                if (!guids)
                {
                    continue;
                }

                uint32 nCreatures = uint32(guids->creatures.size());
                uint32 nGameobjects = uint32(guids->gameobjects.size());
                uint32 nCorpses = uint32(guids->corpses.size());
                if (nCreatures + nGameobjects + nCorpses == 0)
                {
                    continue;
                }

                CellOccupancy cell;
                cell.cellX = cellX;
                cell.cellY = cellY;
                cell.creatures = nCreatures;
                cell.gameobjects = nGameobjects;
                cell.corpses = nCorpses;
                occ.occupied.push_back(cell);

                ++occ.occupiedCells;
                occ.creatures += nCreatures;
                occ.gameobjects += nGameobjects;
                occ.corpses += nCorpses;
            }
        }
        return occ;
    }

    bool CellOccupancyMore(CellOccupancy const& a, CellOccupancy const& b)
    {
        return a.Total() > b.Total();
    }
}

/**
 * @brief .grid info [gridX gridY]
 *
 * Reports static DB spawn-definition occupancy for one grid on the player's
 * current map: occupied/empty cell counts, total spawn GUIDs by type, and the
 * busiest cells. With no args, uses the player's current grid. Read-only.
 */
bool ChatHandler::HandleGridInfoCommand(char* args)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
    {
        SendSysMessage("This command requires an in-game player.");
        SetSentErrorMessage(true);
        return false;
    }

    uint16 mapId = player->GetMapId();
    uint32 gridX = 0;
    uint32 gridY = 0;

    if (ExtractUInt32(&args, gridX))
    {
        if (!ExtractUInt32(&args, gridY))
        {
            SendSysMessage("Usage: .grid info [gridX gridY] "
                           "(0..63 each; omit for current grid)");
            SetSentErrorMessage(true);
            return false;
        }

        if (gridX >= MAX_NUMBER_OF_GRIDS || gridY >= MAX_NUMBER_OF_GRIDS)
        {
            PSendSysMessage("Grid indices out of range (valid 0..%u).",
                            uint32(MAX_NUMBER_OF_GRIDS - 1));
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        GridPair gp = MaNGOS::ComputeGridPair(player->GetPositionX(), player->GetPositionY());
        gridX = gp.x_coord;
        gridY = gp.y_coord;
    }

    bool gridLoaded = player->GetMap()->IsGridLoaded(gridX, gridY);
    GridOccupancy occ = ComputeGridOccupancy(mapId, gridX, gridY);

    bool isEnv = player->GetMap()->IsGridEnvelope(gridX, gridY);
    bool isFull = player->GetMap()->IsGridLoaded(gridX, gridY);
    uint32 loadedCells = player->GetMap()->GetGridLoadedCellCount(gridX, gridY);
    const char* lwState = "UNLOADED";
    if (isFull)
    {
        lwState = "FULL";
    }
    else if (isEnv)
    {
        lwState = "ENVELOPE";
    }

    PSendSysMessage("[LivingWorld] grid occupancy (static DB spawn-definition, "
                    "not live objects): map=%u grid=(%u,%u) loaded=%s",
                    uint32(mapId), gridX, gridY, gridLoaded ? "yes" : "no");
    PSendSysMessage("  live load-state: %s (loaded cells=%u/%u, occupied DB cells=%u)",
                    lwState, loadedCells, CELLS_PER_GRID, occ.occupiedCells);
    PSendSysMessage("  cells: occupied=%u/%u empty=%u",
                    occ.occupiedCells, CELLS_PER_GRID, CELLS_PER_GRID - occ.occupiedCells);
    PSendSysMessage("  db-guids: creatures=%u gameobjects=%u corpses=%u total=%u",
                    occ.creatures, occ.gameobjects, occ.corpses, occ.TotalGuids());

    if (occ.occupiedCells == 0)
    {
        SendSysMessage("  (no static DB spawns defined in this grid)");
        return true;
    }

    std::stable_sort(occ.occupied.begin(), occ.occupied.end(), CellOccupancyMore);

    uint32 const topN = 10;
    uint32 shown = uint32(occ.occupied.size()) < topN ? uint32(occ.occupied.size()) : topN;
    PSendSysMessage("  top cells (by total, showing %u of %u occupied):", shown, occ.occupiedCells);
    for (uint32 i = 0; i < shown; ++i)
    {
        CellOccupancy const& c = occ.occupied[i];
        PSendSysMessage("    cell=(%u,%u) creatures=%u gameobjects=%u corpses=%u total=%u",
                        c.cellX, c.cellY, c.creatures, c.gameobjects, c.corpses, c.Total());
    }

    return true;
}

/**
 * @brief .grid anchors
 *
 * Reports static DB spawn-definition occupancy for the ENABLED startup anchor
 * grids on the player's current map (the CREATURE_FLAG_EXTRA_ACTIVE set plus any
 * LivingWorld anchors enabled by the current AnchorPolicyMask) -- not every
 * possible DB anchor. Used to measure how sparse anchor-held grids are. Read-only.
 */
bool ChatHandler::HandleGridAnchorsCommand(char* args)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
    {
        SendSysMessage("This command requires an in-game player.");
        SetSentErrorMessage(true);
        return false;
    }

    uint16 mapId = player->GetMapId();

    // Startup-active anchor set restricted to the player's current map. This is the
    // *enabled* startup anchor set (extra-active + enabled LivingWorld anchors), not
    // every possible DB anchor. Dedupe to unique anchor grids and tally anchors per grid.
    ActiveCreatureGuidsOnMap const* activeGuids = sObjectMgr.GetActiveCreatureGuids();

    std::map<std::pair<uint32, uint32>, uint32> anchorGridSpawnCount;
    std::pair<ActiveCreatureGuidsOnMap::const_iterator, ActiveCreatureGuidsOnMap::const_iterator> range =
        activeGuids->equal_range(mapId);
    for (ActiveCreatureGuidsOnMap::const_iterator itr = range.first; itr != range.second; ++itr)
    {
        CreatureData const* data = sObjectMgr.GetCreatureData(itr->second);
        if (!data)
        {
            continue;
        }

        GridPair gp = MaNGOS::ComputeGridPair(data->posX, data->posY);
        std::pair<uint32, uint32> key(gp.x_coord, gp.y_coord);
        ++anchorGridSpawnCount[key];
    }

    PSendSysMessage("[LivingWorld] anchor grid occupancy (static DB "
                    "spawn-definition, not live objects): map=%u",
                    uint32(mapId));
    PSendSysMessage("  enabled startup anchor grids on this map "
                    "(extra-active + enabled LivingWorld anchors): %u",
                    uint32(anchorGridSpawnCount.size()));

    if (anchorGridSpawnCount.empty())
    {
        SendSysMessage("  (no startup-active anchors on this map)");
        return true;
    }

    std::vector<uint32> occupancies;
    occupancies.reserve(anchorGridSpawnCount.size());

    for (std::map<std::pair<uint32, uint32>, uint32>::const_iterator itr = anchorGridSpawnCount.begin();
         itr != anchorGridSpawnCount.end(); ++itr)
    {
        uint32 gridX = itr->first.first;
        uint32 gridY = itr->first.second;
        uint32 anchorsHere = itr->second;

        bool gridLoaded = player->GetMap()->IsGridLoaded(gridX, gridY);
        GridOccupancy occ = ComputeGridOccupancy(mapId, gridX, gridY);
        occupancies.push_back(occ.occupiedCells);

        PSendSysMessage("  grid=(%u,%u) anchors=%u loaded=%s occupied=%u/%u "
                        "creatures=%u gameobjects=%u corpses=%u",
                        gridX, gridY, anchorsHere, gridLoaded ? "yes" : "no",
                        occ.occupiedCells, CELLS_PER_GRID,
                        occ.creatures, occ.gameobjects, occ.corpses);
    }

    std::sort(occupancies.begin(), occupancies.end());
    uint32 minOccupied = occupancies.front();
    uint32 maxOccupied = occupancies.back();
    uint32 medianOccupied = occupancies[occupancies.size() / 2];
    PSendSysMessage("  anchor-grid sparsity (occupied cells /%u): "
                    "min=%u median=%u max=%u across %u grids",
                    CELLS_PER_GRID, minOccupied, medianOccupied, maxOccupied,
                    uint32(occupancies.size()));

    return true;
}

/**
 * @brief .grid lwstats
 *
 * Prints the per-map CellEnvelopeStats counters (envelopeLoads, accretions,
 * fills, and anomaly counters) for the player's current map.
 */
bool ChatHandler::HandleGridLwStatsCommand(char* /*args*/)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
    {
        SendSysMessage("This command requires an in-game player.");
        SetSentErrorMessage(true);
        return false;
    }

    Map::CellEnvelopeStats const& stats = player->GetMap()->GetCellEnvelopeStats();
    PSendSysMessage("[LivingWorld] cell-envelope stats for map %u:", player->GetMapId());
    PSendSysMessage("  loads=%u accretions=%u fills=%u",
                    stats.envelopeLoads, stats.accretions, stats.fills);
    PSendSysMessage("  anomalies: anchorOutside=%u scanPartial=%u",
                    stats.anomalyAnchorOutside, stats.anomalyScanPartial);
    PSendSysMessage("  unload: cellsUnloaded=%u downgrades=%u trailingUnloads=%u",
                    stats.cellsUnloaded, stats.downgrades, stats.trailingUnloads);

    return true;
}
