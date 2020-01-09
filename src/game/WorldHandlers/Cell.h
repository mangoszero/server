/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

struct CellArea
{
    CellArea() {}
    CellArea(CellPair low, CellPair high) : low_bound(low), high_bound(high) {}

    bool operator!() const { return low_bound == high_bound; }

    void ResizeBorders(CellPair& begin_cell, CellPair& end_cell) const
    {
        begin_cell = low_bound;
        end_cell = high_bound;
    }

    CellPair low_bound;
    CellPair high_bound;
};

struct Cell
{
        Cell() { data.All = 0; }
        Cell(const Cell& cell) { data.All = cell.data.All; }
        explicit Cell(CellPair const& p);

        void Compute(uint32& x, uint32& y) const
        {
            x = data.Part.grid_x * MAX_NUMBER_OF_CELLS + data.Part.cell_x;
            y = data.Part.grid_y * MAX_NUMBER_OF_CELLS + data.Part.cell_y;
        }

        bool DiffCell(const Cell& cell) const
        {
            return(data.Part.cell_x != cell.data.Part.cell_x ||
                   data.Part.cell_y != cell.data.Part.cell_y);
        }

        bool DiffGrid(const Cell& cell) const
        {
            return(data.Part.grid_x != cell.data.Part.grid_x ||
                   data.Part.grid_y != cell.data.Part.grid_y);
        }

        uint32 CellX() const { return data.Part.cell_x; }
        uint32 CellY() const { return data.Part.cell_y; }
        uint32 GridX() const { return data.Part.grid_x; }
        uint32 GridY() const { return data.Part.grid_y; }
        bool NoCreate() const { return data.Part.nocreate; }
        void SetNoCreate() { data.Part.nocreate = 1; }

        GridPair gridPair() const { return GridPair(GridX(), GridY()); }

        CellPair cellPair() const
        {
            return CellPair(
                       data.Part.grid_x * MAX_NUMBER_OF_CELLS + data.Part.cell_x,
                       data.Part.grid_y * MAX_NUMBER_OF_CELLS + data.Part.cell_y);
        }

        Cell& operator=(const Cell& cell)
        {
            data.All = cell.data.All;
            return *this;
        }

        bool operator==(const Cell& cell) const { return (data.All == cell.data.All); }
        bool operator!=(const Cell& cell) const { return !operator==(cell); }
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
