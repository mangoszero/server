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
 * @file ObjectGridLoader.cpp
 * @brief Map grid object loading and unloading system
 *
 * This file implements ObjectGridLoader which manages the lifecycle of
 * game objects within map grids (cells). Responsibilities:
 *
 * - Loading creatures, game objects, and corpses from database
 * - Respawn coordination when grids unload/reload
 * - World object persistence during grid transitions
 * - Proper cleanup (stopping) of grid contents before unload
 *
 * The visitor pattern is used to process different object types efficiently.
 * Grid loading is triggered when players approach, and unloading occurs
 * when no players are nearby for a configured duration.
 *
 * @see ObjectGridLoader for the main loader class
 * @see ObjectGridStoper for the grid cleanup class
 * @see ObjectGridRespawnMover for respawn point correction
 */

#include "ObjectGridLoader.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "MapPersistentStateMgr.h"
#include "Creature.h"
#include "DynamicObject.h"
#include "Corpse.h"
#include "World.h"
#include "CellImpl.h"
#include "BattleGround/BattleGround.h"

/**
 * @class ObjectGridRespawnMover
 * @brief Helper to relocate creatures to their respawn points before grid unload
 *
 * When a grid is unloaded, any creatures in it with respawn points in other
 * grids need to be moved to their respawn location. This prevents creatures
 * from being "trapped" in an unloaded grid until it reloads.
 *
 * Uses the visitor pattern to process only Creature objects.
 */
class ObjectGridRespawnMover
{
    public:
        ObjectGridRespawnMover() {}

        /**
         * @brief Process entire grid for respawn relocations
         * @param grid Grid to process
         */
        void Move(GridType& grid);

        template<class T> void Visit(GridRefManager<T>&) {}

        /**
         * @brief Visit and relocate creatures as needed
         * @param m Creature container
         *
         * For each creature, checks if its respawn point is in a different
         * grid. If so, initiates relocation to respawn coordinates.
         */
        void Visit(CreatureMapType& m);
};

/**
 * @brief Execute respawn relocation visit on grid
 * @param grid Grid to process for respawn moves
 */
void
ObjectGridRespawnMover::Move(GridType& grid)
{
    TypeContainerVisitor<ObjectGridRespawnMover, GridTypeMapContainer > mover(*this);
    grid.Visit(mover);
}

/**
 * @brief Visit and relocate creatures to respawn points if needed
 * @param m Creature container to process
 *
 * Iterates all creatures in the grid. For each creature, determines if
 * its respawn coordinates are in a different grid than its current position.
 * If so, calls CreatureRespawnRelocation to move it to the correct grid.
 *
 * This ensures creatures respawn in their proper location even if their
 * corpse was in an unloaded grid.
 *
 * @warning Pets are not expected here (assertion enforced)
 */
void
ObjectGridRespawnMover::Visit(CreatureMapType& m)
{
    // Creature in unloading grid can have respawn point in another grid.
    // If it will be unloaded, it won't respawn in original grid until unload/load cycle.
    // Move to respawn point to prevent this case.
    for (CreatureMapType::iterator iter = m.begin(), next; iter != m.end(); iter = next)
    {
        next = iter; ++next;

        Creature* c = iter->getSource();

        MANGOS_ASSERT(!c->IsPet());                         // ObjectGridRespawnMover should not be called for pets

        Cell const& cur_cell  = c->GetCurrentCell();

        float resp_x, resp_y, resp_z;
        c->GetRespawnCoord(resp_x, resp_y, resp_z);
        CellPair resp_val = MaNGOS::ComputeCellPair(resp_x, resp_y);
        Cell resp_cell(resp_val);

        if (cur_cell.DiffGrid(resp_cell))
        {
            c->GetMap()->CreatureRespawnRelocation(c);
            // false result ignored: will be unloaded with other creatures at grid
        }
    }
}

/**
 * @class ObjectWorldLoader
 * @brief Visitor for loading world objects (corpses) into grid
 *
 * Loads persistent world objects that exist independently of grid state.
 * Currently handles corpse loading from database when grid loads.
 *
 * @note Unlike creatures/GOs, corpses are world objects that must be loaded
 *       separately as they persist across grid boundaries.
 */
class ObjectWorldLoader
{
    public:
        /**
         * @brief Construct world loader for a grid
         * @param gloader Parent grid loader with cell/grid/map references
         */
        explicit ObjectWorldLoader(ObjectGridLoader& gloader)
            : i_cell(gloader.i_cell), i_grid(gloader.i_grid), i_map(gloader.i_map), i_corpses(0)
        {}

        /**
         * @brief Load corpses into grid
         * @param m Corpse container to populate
         *
         * Queries database for corpses in this grid's cell range and
         * loads them into the grid. Increments i_corpses counter.
         */
        void Visit(CorpseMapType& m);

        template<class T> void Visit(GridRefManager<T>&) { }

    private:
        Cell i_cell;                    ///< Cell coordinates
        NGridType& i_grid;              ///< Grid being loaded
        Map* i_map;                     ///< Owning map
    public:
        uint32 i_corpses;               ///< Count of loaded corpses
};

/**
 * @brief Default helper that leaves unit cell state unchanged for unsupported object types.
 *
 * @tparam T The object type.
 * @param obj The object being loaded.
 * @param cell_pair The destination cell coordinates.
 */
template<class T> void addUnitState(T* /*obj*/, CellPair const& /*cell_pair*/)
{
}

/**
 * @brief Assigns the current cell to a creature being loaded into a grid.
 *
 * @param obj The creature being loaded.
 * @param cell_pair The destination cell coordinates.
 */
template<> void addUnitState(Creature* obj, CellPair const& cell_pair)
{
    Cell cell(cell_pair);

    obj->SetCurrentCell(cell);
}

template <class T>
/**
 * @brief Loads database-backed grid objects of a specific type into a cell.
 *
 * @tparam T The object type to load.
 * @param guid_set The guids to load.
 * @param cell The destination cell.
 * @param m The type map container.
 * @param count Receives the number of loaded objects.
 * @param map The owning map.
 * @param grid The target grid cell container.
 */
void LoadHelper(CellGuidSet const& guid_set, CellPair& cell, GridRefManager<T>& /*m*/, uint32& count, Map* map, GridType& grid)
{
    BattleGround* bg = map->IsBattleGround() ? ((BattleGroundMap*)map)->GetBG() : nullptr;

    for (CellGuidSet::const_iterator i_guid = guid_set.begin(); i_guid != guid_set.end(); ++i_guid)
    {
        uint32 guid = *i_guid;

        T* obj = new T;
        // sLog.outString("DEBUG: LoadHelper from table: %s for (guid: %u) Loading",table,guid);
        if (!obj->LoadFromDB(guid, map))
        {
            delete obj;
            continue;
        }

        grid.AddGridObject(obj);

        addUnitState(obj, cell);
        obj->SetMap(map);
        obj->AddToWorld();
        if (obj->IsActiveObject())
        {
            map->AddToActive(obj);
        }

        obj->GetViewPoint().Event_AddedToWorld(&grid);

        if (bg)
        {
            bg->OnObjectDBLoad(obj);
        }

        ++count;
    }
}

/**
 * @brief Loads corpse objects for a cell into the world grid.
 *
 * @param cell_corpses The corpse ownership and instance mapping.
 * @param cell The destination cell.
 * @param m The corpse map container.
 * @param count Receives the number of loaded corpses.
 * @param map The owning map.
 * @param grid The target grid cell container.
 */
void LoadHelper(CellCorpseSet const& cell_corpses, CellPair& cell, CorpseMapType& /*m*/, uint32& count, Map* map, GridType& grid)
{
    if (cell_corpses.empty())
    {
        return;
    }

    for (CellCorpseSet::const_iterator itr = cell_corpses.begin(); itr != cell_corpses.end(); ++itr)
    {
        if (itr->second != map->GetInstanceId())
        {
            continue;
        }

        uint32 player_lowguid = itr->first;

        Corpse* obj = sObjectAccessor.GetCorpseForPlayerGUID(ObjectGuid(HIGHGUID_PLAYER, player_lowguid));
        if (!obj)
        {
            continue;
        }

        grid.AddWorldObject(obj);

        addUnitState(obj, cell);
        obj->SetMap(map);
        obj->AddToWorld();
        if (obj->IsActiveObject())
        {
            map->AddToActive(obj);
        }

        ++count;
    }
}

void
ObjectGridLoader::Visit(GameObjectMapType& m)
{
    uint32 x = (i_cell.GridX() * MAX_NUMBER_OF_CELLS) + i_cell.CellX();
    uint32 y = (i_cell.GridY() * MAX_NUMBER_OF_CELLS) + i_cell.CellY();
    CellPair cell_pair(x, y);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids const& cell_guids = sObjectMgr.GetCellObjectGuids(i_map->GetId(), cell_id);

    GridType& grid = (*i_map->getNGrid(i_cell.GridX(), i_cell.GridY()))(i_cell.CellX(), i_cell.CellY());
    LoadHelper(cell_guids.gameobjects, cell_pair, m, i_gameObjects, i_map, grid);
    LoadHelper(i_map->GetPersistentState()->GetCellObjectGuids(cell_id).gameobjects, cell_pair, m, i_gameObjects, i_map, grid);
}

void
ObjectGridLoader::Visit(CreatureMapType& m)
{
    uint32 x = (i_cell.GridX() * MAX_NUMBER_OF_CELLS) + i_cell.CellX();
    uint32 y = (i_cell.GridY() * MAX_NUMBER_OF_CELLS) + i_cell.CellY();
    CellPair cell_pair(x, y);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids const& cell_guids = sObjectMgr.GetCellObjectGuids(i_map->GetId(), cell_id);

    GridType& grid = (*i_map->getNGrid(i_cell.GridX(), i_cell.GridY()))(i_cell.CellX(), i_cell.CellY());
    LoadHelper(cell_guids.creatures, cell_pair, m, i_creatures, i_map, grid);
    LoadHelper(i_map->GetPersistentState()->GetCellObjectGuids(cell_id).creatures, cell_pair, m, i_creatures, i_map, grid);
}

void
ObjectWorldLoader::Visit(CorpseMapType& m)
{
    uint32 x = (i_cell.GridX() * MAX_NUMBER_OF_CELLS) + i_cell.CellX();
    uint32 y = (i_cell.GridY() * MAX_NUMBER_OF_CELLS) + i_cell.CellY();
    CellPair cell_pair(x, y);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    // corpses are always added to spawn mode 0 and they are spawned by their instance id
    CellObjectGuids const& cell_guids = sObjectMgr.GetCellObjectGuids(i_map->GetId(), cell_id);
    GridType& grid = (*i_map->getNGrid(i_cell.GridX(), i_cell.GridY()))(i_cell.CellX(), i_cell.CellY());
    LoadHelper(cell_guids.corpses, cell_pair, m, i_corpses, i_map, grid);
}

void
ObjectGridLoader::Load(GridType& grid)
{
    {
        TypeContainerVisitor<ObjectGridLoader, GridTypeMapContainer > loader(*this);
        grid.Visit(loader);
    }

    {
        ObjectWorldLoader wloader(*this);
        TypeContainerVisitor<ObjectWorldLoader, WorldTypeMapContainer > loader(wloader);
        grid.Visit(loader);
        i_corpses = wloader.i_corpses;
    }
}

/**
 * @brief Loads all cells in the current grid and reports loaded object counts.
 */
void ObjectGridLoader::LoadN(void)
{
    i_gameObjects = 0; i_creatures = 0; i_corpses = 0;
    i_cell.data.Part.cell_y = 0;
    GridLoaderType loader;

    for (unsigned int x = 0; x < MAX_NUMBER_OF_CELLS; ++x)
    {
        i_cell.data.Part.cell_x = x;
        for (unsigned int y = 0; y < MAX_NUMBER_OF_CELLS; ++y)
        {
            i_cell.data.Part.cell_y = y;
            loader.Load(i_grid(x, y), *this);
        }
    }
    DEBUG_LOG("%u GameObjects, %u Creatures, and %u Corpses/Bones loaded for grid %u on map %u", i_gameObjects, i_creatures, i_corpses, i_grid.GetGridId(), i_map->GetId());
}

/**
 * @brief Moves respawnable objects in every cell of the grid back to their respawn state.
 */
void ObjectGridUnloader::MoveToRespawnN()
{
    for (unsigned int x = 0; x < MAX_NUMBER_OF_CELLS; ++x)
    {
        for (unsigned int y = 0; y < MAX_NUMBER_OF_CELLS; ++y)
        {
            ObjectGridRespawnMover mover;
            mover.Move(i_grid(x, y));
        }
    }
}

void
ObjectGridUnloader::Unload(GridType& grid)
{
    TypeContainerVisitor<ObjectGridUnloader, GridTypeMapContainer > unloader(*this);
    grid.Visit(unloader);
}

template<class T>
void
ObjectGridUnloader::Visit(GridRefManager<T>& m)
{
    // remove all cross-reference before deleting
    for (typename GridRefManager<T>::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        iter->getSource()->CleanupsBeforeDelete();
    }

    while (!m.isEmpty())
    {
        T* obj = m.getFirst()->getSource();
        // if option set then object already saved at this moment
        if (!sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
        {
            obj->SaveRespawnTime();
        }
        ///- object must be out of world before delete
        obj->RemoveFromWorld();
        ///- object will get delinked from the manager when deleted
        delete obj;
    }
}

void
ObjectGridStoper::Stop(GridType& grid)
{
    TypeContainerVisitor<ObjectGridStoper, GridTypeMapContainer > stoper(*this);
    grid.Visit(stoper);
}

void
ObjectGridStoper::Visit(CreatureMapType& m)
{
    // stop any fights at grid de-activation and remove dynobjects created at cast by creatures
    for (CreatureMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
    {
        iter->getSource()->CombatStop();
        iter->getSource()->DeleteThreatList();
        iter->getSource()->RemoveAllDynObjects();
    }
}

template void ObjectGridUnloader::Visit(GameObjectMapType&);
template void ObjectGridUnloader::Visit(DynamicObjectMapType&);
