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

#ifndef MANGOS_GRIDDEFINES_H
#define MANGOS_GRIDDEFINES_H

#include "Common.h"
#include "GameSystem/NGrid.h"
#include <cmath>

// Forward class definitions
class Corpse;
class Creature;
class DynamicObject;
class GameObject;
class Pet;
class Player;
class Camera;

#define MAX_NUMBER_OF_GRIDS      64

#define SIZE_OF_GRIDS            533.33333f
#define CENTER_GRID_ID           (MAX_NUMBER_OF_GRIDS/2)

#define CENTER_GRID_OFFSET      (SIZE_OF_GRIDS/2)

#define MIN_GRID_DELAY          (MINUTE*IN_MILLISECONDS)
#define MIN_MAP_UPDATE_DELAY    50

#define MAX_NUMBER_OF_CELLS     16
#define SIZE_OF_GRID_CELL       (SIZE_OF_GRIDS/MAX_NUMBER_OF_CELLS)

#define CENTER_GRID_CELL_ID     (MAX_NUMBER_OF_CELLS*MAX_NUMBER_OF_GRIDS/2)
#define CENTER_GRID_CELL_OFFSET (SIZE_OF_GRID_CELL/2)

#define TOTAL_NUMBER_OF_CELLS_PER_MAP    (MAX_NUMBER_OF_GRIDS*MAX_NUMBER_OF_CELLS)

#define MAP_RESOLUTION 128

#define MAP_SIZE                (SIZE_OF_GRIDS*MAX_NUMBER_OF_GRIDS)
#define MAP_HALFSIZE            (MAP_SIZE/2)

template <typename...Ts> struct TypeList;

// Creature used instead pet to simplify *::Visit templates (not required duplicate code for Creature->Pet case)
// Cameras in world list just because linked with Player objects

using GridTypeMapContainer  = TypeMapContainer<TypeList<GameObject, Creature/*except pets*/, DynamicObject, Corpse/*bones*/>>;
using WorldTypeMapContainer = TypeMapContainer<TypeList<Player, Creature/*pets*/, Corpse/*resurrectable*/, Camera>>;

typedef GridRefManager<Camera>          CameraMapType;
typedef GridRefManager<Corpse>          CorpseMapType;
typedef GridRefManager<Creature>        CreatureMapType;
typedef GridRefManager<DynamicObject>   DynamicObjectMapType;
typedef GridRefManager<GameObject>      GameObjectMapType;
typedef GridRefManager<Player>          PlayerMapType;

typedef Grid<Player, WorldTypeMapContainer, GridTypeMapContainer> GridType;
typedef NGrid<MAX_NUMBER_OF_CELLS, Player, WorldTypeMapContainer, GridTypeMapContainer> NGridType;

/**
 * @brief A structure representing a pair of coordinates.
 *
 * @tparam LIMIT The maximum limit for the coordinates.
 */
template<const unsigned int LIMIT>
struct CoordPair
{
    CoordPair(uint32 x = 0, uint32 y = 0) : x_coord(x), y_coord(y) {}
    CoordPair(const CoordPair<LIMIT>& obj) : x_coord(obj.x_coord), y_coord(obj.y_coord) {}
    bool operator==(const CoordPair<LIMIT>& obj) const { return (obj.x_coord == x_coord && obj.y_coord == y_coord); }
    bool operator!=(const CoordPair<LIMIT>& obj) const { return !operator==(obj); }
    CoordPair<LIMIT>& operator=(const CoordPair<LIMIT>& obj)
    {
        if (this != &obj) // Check for self-assignment
        {
            x_coord = obj.x_coord;
            y_coord = obj.y_coord;
        }
        return *this;
    }

    void operator<<(const uint32 val)
    {
        if (x_coord > val)
        {
            x_coord -= val;
        }
        else
        {
            x_coord = 0;
        }
    }

    void operator>>(const uint32 val)
    {
        if (x_coord + val < LIMIT)
        {
            x_coord += val;
        }
        else
        {
            x_coord = LIMIT - 1;
        }
    }

    void operator-=(const uint32 val)
    {
        if (y_coord > val)
        {
            y_coord -= val;
        }
        else
        {
            y_coord = 0;
        }
    }

    void operator+=(const uint32 val)
    {
        if (y_coord + val < LIMIT)
        {
            y_coord += val;
        }
        else
        {
            y_coord = LIMIT - 1;
        }
    }

    CoordPair& normalize()
    {
        x_coord = std::min(x_coord, LIMIT - 1);
        y_coord = std::min(y_coord, LIMIT - 1);
        return *this;
    }

    uint32 x_coord; ///< The x-coordinate.
    uint32 y_coord; ///< The y-coordinate.
};

typedef CoordPair<MAX_NUMBER_OF_GRIDS> GridPair;
typedef CoordPair<TOTAL_NUMBER_OF_CELLS_PER_MAP> CellPair;

namespace MaNGOS
{
    /**
     * @brief Computes the grid or cell pair for given coordinates.
     *
     * @tparam RET_TYPE The return type.
     * @tparam CENTER_VAL The center value.
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @param center_offset The center offset.
     * @param size The size of the grid or cell.
     * @return RET_TYPE The computed grid or cell pair.
     */
    template<class RET_TYPE, int CENTER_VAL>
    inline RET_TYPE Compute(float x, float y, float center_offset, float size)
    {
        // calculate and store temporary values in double format for having same result as same mySQL calculations
        double x_offset = (double(x) - center_offset) / size;
        double y_offset = (double(y) - center_offset) / size;

        int x_val = int(x_offset + CENTER_VAL + 0.5);
        int y_val = int(y_offset + CENTER_VAL + 0.5);
        return RET_TYPE(x_val, y_val);
    }

    /**
     * @brief Computes the grid pair for given coordinates.
     *
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @return GridPair The computed grid pair.
     */
    inline GridPair ComputeGridPair(float x, float y)
    {
        return Compute<GridPair, CENTER_GRID_ID>(x, y, CENTER_GRID_OFFSET, SIZE_OF_GRIDS);
    }

    /**
     * @brief Computes the cell pair for given coordinates.
     *
     * @param x The x-coordinate.
     * @param y The y-coordinate.
     * @return CellPair The computed cell pair.
     */
    inline CellPair ComputeCellPair(float x, float y)
    {
        return Compute<CellPair, CENTER_GRID_CELL_ID>(x, y, CENTER_GRID_CELL_OFFSET, SIZE_OF_GRID_CELL);
    }

    /**
     * @brief Normalizes the map coordinate.
     *
     * @param c The coordinate to normalize.
     */
    inline void NormalizeMapCoord(float& c)
    {
        if (c > MAP_HALFSIZE - 0.5)
        {
            c = MAP_HALFSIZE - 0.5;
        }
        else if (c < -(MAP_HALFSIZE - 0.5))
        {
            c = -(MAP_HALFSIZE - 0.5);
        }
    }

    /**
     * @brief Checks if the map coordinate is valid.
     *
     * @param c The coordinate to check.
     * @return true If the coordinate is valid.
     * @return false If the coordinate is not valid.
     */
    inline bool IsValidMapCoord(float c)
    {
        return finite(c) && (std::fabs(c) <= MAP_HALFSIZE - 0.5);
    }

    /**
     * @brief Checks if the map coordinates are valid.
     *
     * @param x The x-coordinate to check.
     * @param y The y-coordinate to check.
     * @return true If the coordinates are valid.
     * @return false If the coordinates are not valid.
     */
    inline bool IsValidMapCoord(float x, float y)
    {
        return IsValidMapCoord(x) && IsValidMapCoord(y);
    }

    /**
     * @brief Checks if the map coordinates are valid.
     *
     * @param x The x-coordinate to check.
     * @param y The y-coordinate to check.
     * @param z The z-coordinate to check.
     * @return true If the coordinates are valid.
     * @return false If the coordinates are not valid.
     */
    inline bool IsValidMapCoord(float x, float y, float z)
    {
        return IsValidMapCoord(x, y) && finite(z);
    }

    /**
     * @brief Checks if the map coordinates are valid.
     *
     * @param x The x-coordinate to check.
     * @param y The y-coordinate to check.
     * @param z The z-coordinate to check.
     * @param o The orientation to check.
     * @return true If the coordinates and orientation are valid.
     * @return false If the coordinates and orientation are not valid.
     */
    inline bool IsValidMapCoord(float x, float y, float z, float o)
    {
        return IsValidMapCoord(x, y, z) && finite(o);
    }
}
#endif

