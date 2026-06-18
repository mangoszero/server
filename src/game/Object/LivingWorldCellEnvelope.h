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

#ifndef MANGOS_LIVING_WORLD_CELL_ENVELOPE_H
#define MANGOS_LIVING_WORLD_CELL_ENVELOPE_H

#include "GridDefines.h" // MAX_NUMBER_OF_CELLS, TOTAL_NUMBER_OF_CELLS_PER_MAP

// Highest valid global cell index on one axis (0..1023 for 64 grids * 16 cells).
constexpr uint32 LW_GLOBAL_CELL_MAX = TOTAL_NUMBER_OF_CELLS_PER_MAP - 1;

// Clamp a (possibly out-of-range, e.g. center-1 at map edge) global cell axis into [0, MAX].
constexpr uint32 LwClampGlobalCell(int v)
{
    return v < 0 ? 0u : (v > int(LW_GLOBAL_CELL_MAX) ? LW_GLOBAL_CELL_MAX : uint32(v));
}

// Decompose a global cell axis into its grid index and local (0..15) cell index.
constexpr uint32 LwGlobalCellToGrid(uint32 globalCell)
{
    return globalCell / MAX_NUMBER_OF_CELLS;
}

constexpr uint32 LwGlobalCellToLocal(uint32 globalCell)
{
    return globalCell % MAX_NUMBER_OF_CELLS;
}

constexpr uint32 LwGridLocalToGlobalCell(uint32 grid, uint32 local)
{
    return grid * MAX_NUMBER_OF_CELLS + local;
}

// --- compile-time tests ---
static_assert(LwClampGlobalCell(-1) == 0,
    "global cell clamp must floor below-zero neighbours to 0 (map edge)");
static_assert(LwClampGlobalCell(int(LW_GLOBAL_CELL_MAX) + 1) == LW_GLOBAL_CELL_MAX,
    "global cell clamp must cap above-max neighbours to the last cell (map edge)");
static_assert(LwClampGlobalCell(500) == 500,
    "in-range global cells pass through unchanged");
static_assert(LwGlobalCellToGrid(0) == 0 && LwGlobalCellToLocal(0) == 0,
    "global cell 0 is grid 0 local 0");
static_assert(LwGlobalCellToGrid(MAX_NUMBER_OF_CELLS) == 1 && LwGlobalCellToLocal(MAX_NUMBER_OF_CELLS) == 0,
    "global cell 16 is the first cell of grid 1");
static_assert(LwGlobalCellToGrid(17) == 1 && LwGlobalCellToLocal(17) == 1,
    "global cell 17 is grid 1 local 1 (envelope spilling across a grid boundary)");
static_assert(LwGridLocalToGlobalCell(1, 1) == 17,
    "grid/local round-trips back to the global cell");

#endif
