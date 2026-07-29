/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#include <cmath>
#include "TestHarness.h"

#include "nav/NavMeshBuilder.hpp"

#include <random>

using world::nav::SubTileSpan;
using world::nav::CellRect;
using world::nav::NeighbourCellRect;

namespace
{
    // The real bake's numbers: a 533.33 yard tile at 0.266666 yards per cell, 80 cells
    // to a sub-tile, so 25 sub-tiles a side, bordered by walkableRadius + 3.
    constexpr float CELL = 0.266666f;
    constexpr int SUB_TILE_CELLS = 80;
    constexpr int SIDE = 25;
    constexpr int BORDER = 5;
    constexpr float WIDTH = SUB_TILE_CELLS * CELL;
    constexpr float PAD = BORDER * CELL;
    constexpr float ORIGIN = -1234.5f;

    // What a sub-tile actually rasterises, straight from BuildSubTile's own arithmetic.
    void SubTileBounds(int s, float& lo, float& hi)
    {
        lo = ORIGIN + float(s * SUB_TILE_CELLS - BORDER) * CELL;
        hi = ORIGIN + float((s + 1) * SUB_TILE_CELLS + BORDER) * CELL;
    }

    bool Overlaps(float aLo, float aHi, float bLo, float bHi)
    {
        return aLo <= bHi && aHi >= bLo;
    }
}

TEST(NavBinningNeverDropsATriangleASubTileWouldRasterise)
{
    // The property that matters. Binning is what decides which triangles Recast ever
    // sees, so a span one bin too narrow silently punches a hole in the navmesh -- and a
    // hole is invisible until something refuses to path across it. Every interval that
    // overlaps a sub-tile's padded bounds MUST land in that sub-tile's range.
    std::mt19937 rng(20260722);
    std::uniform_real_distribution<float> pos(ORIGIN - 3.f * WIDTH,
                                              ORIGIN + float(SIDE + 3) * WIDTH);
    std::uniform_real_distribution<float> len(0.f, 4.f * WIDTH);

    size_t dropped = 0;
    size_t checked = 0;

    for (int i = 0; i < 20000; ++i)
    {
        const float lo = pos(rng);
        const float hi = lo + len(rng);

        int first = 0, last = 0;
        SubTileSpan(lo, hi, ORIGIN, WIDTH, PAD, SIDE, first, last);

        for (int s = 0; s < SIDE; ++s)
        {
            float sLo, sHi;
            SubTileBounds(s, sLo, sHi);
            if (!Overlaps(lo, hi, sLo, sHi))
            {
                continue;
            }
            ++checked;
            if (s < first || s > last)
            {
                ++dropped;
            }
        }
    }

    CHECK(checked > 0);
    CHECK_EQ(dropped, size_t(0));
}

TEST(NavBinningStaysInsideTheGrid)
{
    // An interval entirely off the tile must clamp, not index out of the bin array.
    int first = 0, last = 0;

    SubTileSpan(ORIGIN - 1000.f, ORIGIN - 900.f, ORIGIN, WIDTH, PAD, SIDE, first, last);
    CHECK(first >= 0);
    CHECK(last <= SIDE - 1);

    SubTileSpan(ORIGIN + 1e6f, ORIGIN + 1e6f + 10.f, ORIGIN, WIDTH, PAD, SIDE, first, last);
    CHECK(first >= 0);
    CHECK(last <= SIDE - 1);

    // A point in the middle of sub-tile 12 must at least name sub-tile 12.
    const float middle = ORIGIN + 12.5f * WIDTH;
    SubTileSpan(middle, middle, ORIGIN, WIDTH, PAD, SIDE, first, last);
    CHECK(first <= 12);
    CHECK(last >= 12);
}

TEST(NavBinningCoversTheWholeTile)
{
    // An interval spanning the tile must name every sub-tile, or the bake would leave
    // whole columns of the grid empty.
    int first = 0, last = 0;
    SubTileSpan(ORIGIN, ORIGIN + float(SIDE) * WIDTH, ORIGIN, WIDTH, PAD, SIDE, first, last);
    CHECK_EQ(first, 0);
    CHECK_EQ(last, SIDE - 1);
}

TEST(NavBinningIncludesTheBorderNeighbour)
{
    // A triangle just OUTSIDE sub-tile 5's core still lands in its border ring, so it
    // has to be binned there too. This is the case a naive floor() drops.
    float sLo, sHi;
    SubTileBounds(5, sLo, sHi);

    // Sits between sub-tile 5's core start and its padded start.
    const float justOutside = ORIGIN + float(5 * SUB_TILE_CELLS) * CELL - PAD * 0.5f;
    CHECK(justOutside >= sLo);

    int first = 0, last = 0;
    SubTileSpan(justOutside, justOutside, ORIGIN, WIDTH, PAD, SIDE, first, last);
    CHECK(first <= 5);
    CHECK(last >= 5);
}

TEST(NavNeighbourCellRectSelectsTheAdjacentCellStrip)
{
    CellRect rect{};

    CHECK(NeighbourCellRect(-1, 0, rect));
    CHECK_EQ(rect.ixFirst, 127);
    CHECK_EQ(rect.ixLast, 127);
    CHECK_EQ(rect.iyFirst, 0);
    CHECK_EQ(rect.iyLast, 127);

    CHECK(NeighbourCellRect(1, 0, rect));
    CHECK_EQ(rect.ixFirst, 0);
    CHECK_EQ(rect.ixLast, 0);
    CHECK_EQ(rect.iyFirst, 0);
    CHECK_EQ(rect.iyLast, 127);

    CHECK(NeighbourCellRect(0, -1, rect));
    CHECK_EQ(rect.ixFirst, 0);
    CHECK_EQ(rect.ixLast, 127);
    CHECK_EQ(rect.iyFirst, 127);
    CHECK_EQ(rect.iyLast, 127);

    CHECK(NeighbourCellRect(0, 1, rect));
    CHECK_EQ(rect.ixFirst, 0);
    CHECK_EQ(rect.ixLast, 127);
    CHECK_EQ(rect.iyFirst, 0);
    CHECK_EQ(rect.iyLast, 0);
}

TEST(NavNeighbourCellRectRejectsNonOrthogonalNeighbours)
{
    CellRect rect{};
    CHECK(!NeighbourCellRect(0, 0, rect));
    CHECK(!NeighbourCellRect(1, 1, rect));
    CHECK(!NeighbourCellRect(-2, 0, rect));
}
