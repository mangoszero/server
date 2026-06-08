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

#ifndef MANGOS_CELL_H
#define MANGOS_CELL_H

#include "GameSystem/TypeContainerVisitor.h"
#include "GridDefines.h"

class Map;
class WorldObject;

/**
 * @brief Cell area structure
 */
struct CellArea
{

    /**
     * @brief Default constructor
     */
    CellArea() {}

    /**
     * @brief Constructor with bounds
     * @param low Low bound
     * @param high High bound
     */
    CellArea(CellPair low, CellPair high) : low_bound(low), high_bound(high) {}

    /**
     * @brief Check if empty
     * @return True if empty
     */
    bool operator!() const { return low_bound == high_bound; }

    /**
     * @brief Resize borders
     * @param begin_cell Begin cell output
     * @param end_cell End cell output
     */
    void ResizeBorders(CellPair& begin_cell, CellPair& end_cell) const
    {
        begin_cell = low_bound;
        end_cell = high_bound;
    }

    CellPair low_bound; ///< Low bound
    CellPair high_bound; ///< High bound
};

/**
 * @brief Cell structure
 */
struct Cell
{

    /**
     * @brief Default constructor
     */
    Cell()
    {
        data.All = 0;
    }

    /**
     * @brief Copy constructor
     * @param cell Source cell
     */
    Cell(const Cell& cell)
    {
        data.All = cell.data.All;
    }

    /**
     * @brief Constructor from cell pair
     * @param p Cell pair
     */
    explicit Cell(CellPair const& p);

    /**
     * @brief Compute coordinates
     * @param x X-coordinate output
     * @param y Y-coordinate output
     */
    void Compute(uint32& x, uint32& y) const
    {
        x = data.Part.grid_x * MAX_NUMBER_OF_CELLS + data.Part.cell_x;
        y = data.Part.grid_y * MAX_NUMBER_OF_CELLS + data.Part.cell_y;
    }

    /**
     * @brief Check if different cell
     * @param cell Cell to compare
     * @return True if different cell
     */
    bool DiffCell(const Cell& cell) const
    {
        return(data.Part.cell_x != cell.data.Part.cell_x ||
            data.Part.cell_y != cell.data.Part.cell_y);
    }

    /**
     * @brief Check if different grid
     * @param cell Cell to compare
     * @return True if different grid
     */
    bool DiffGrid(const Cell& cell) const
    {
        return(data.Part.grid_x != cell.data.Part.grid_x ||
            data.Part.grid_y != cell.data.Part.grid_y);
    }

    /**
     * @brief Get cell X coordinate
     * @return Cell X
     */
    uint32 CellX() const
    {
        return data.Part.cell_x;
    }

    /**
     * @brief Get cell Y coordinate
     * @return Cell Y
     */
    uint32 CellY() const
    {
        return data.Part.cell_y;
    }

    /**
     * @brief Get grid X coordinate
     * @return Grid X
     */
    uint32 GridX() const
    {
        return data.Part.grid_x;
    }

    /**
     * @brief Get grid Y coordinate
     * @return Grid Y
     */
    uint32 GridY() const
    {
        return data.Part.grid_y;
    }

    /**
     * @brief Check if no create flag is set
     * @return True if no create
     */
    bool NoCreate() const
    {
        return data.Part.nocreate;
    }

    /**
     * @brief Set no create flag
     */
    void SetNoCreate()
    {
        data.Part.nocreate = 1;
    }

    /**
     * @brief Get grid pair
     * @return Grid pair
     */
    GridPair gridPair() const
    {
        return GridPair(GridX(), GridY());
    }

    /**
     * @brief Get cell pair
     * @return Cell pair
     */
    CellPair cellPair() const
    {
        return CellPair(
            data.Part.grid_x * MAX_NUMBER_OF_CELLS + data.Part.cell_x,
            data.Part.grid_y * MAX_NUMBER_OF_CELLS + data.Part.cell_y);
    }

    /**
     * @brief Assignment operator
     * @param cell Source cell
     * @return Reference to this cell
     */
    Cell& operator=(const Cell& cell)
    {
        data.All = cell.data.All;
        return *this;
    }

    /**
     * @brief Equality operator
     * @param cell Cell to compare
     * @return True if equal
     */
    bool operator==(const Cell& cell) const
    {
        return (data.All == cell.data.All);
    }

    /**
     * @brief Inequality operator
     * @param cell Cell to compare
     * @return True if not equal
     */
    bool operator!=(const Cell& cell) const
    {
        return !operator==(cell);
    }
    union
    {
        struct
        {
            unsigned grid_x : 6;
            unsigned grid_y : 6;
            unsigned cell_x : 6;
            unsigned cell_y : 6;
            unsigned nocreate : 1;
            unsigned reserved : 7;
        } Part;
        uint32 All;
    } data;

    template<class T, class CONTAINER> void Visit(const CellPair& cellPair, TypeContainerVisitor<T, CONTAINER> &visitor, Map& m, float x, float y, float radius) const;
    template<class T, class CONTAINER> void Visit(const CellPair& cellPair, TypeContainerVisitor<T, CONTAINER> &visitor, Map& m, const WorldObject& obj, float radius) const;

    static CellArea CalculateCellArea(float x, float y, float radius);

    template<class T> static void VisitGridObjects(const WorldObject* obj, T& visitor, float radius, bool dont_load = true);
    template<class T> static void VisitWorldObjects(const WorldObject* obj, T& visitor, float radius, bool dont_load = true);
    template<class T> static void VisitAllObjects(const WorldObject* obj, T& visitor, float radius, bool dont_load = true);

    template<class T> static void VisitGridObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load = true);
    template<class T> static void VisitWorldObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load = true);
    template<class T> static void VisitAllObjects(float x, float y, Map* map, T& visitor, float radius, bool dont_load = true);

    private:
        template<class T, class CONTAINER> void VisitCircle(TypeContainerVisitor<T, CONTAINER> &, Map&, const CellPair& , const CellPair&) const;
};

#endif
