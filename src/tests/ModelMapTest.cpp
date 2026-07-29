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

#include "TestHarness.h"

#include "terrain/CollisionModel.hpp"
#include "terrain/FusedTerrain.hpp"
#include "terrain/ModelTileSource.hpp"

#include <cmath>
#include <memory>
#include <optional>
#include <string>

using namespace world::terrain;

// A map whose terrain is one baked model, which is what a vessel's hull becomes. The map
// id is GameObjectDisplayInfo 3015 -- the transport_ship WMO, the display id every
// "Ship" row of two_world.transports carries, because that is the key a real vessel
// map would be built under.
//
// The geometry here is synthetic so the suite stays hermetic. It was checked against the
// real thing off-line: the 117 two_world.creature_transport spawns of Orgrim's Hammer
// (display 8253) and The Skybreaker (8254), served through ModelTileSource, all land on
// the baked hull and agree with a direct model raycast to within 1e-3.

namespace
{
    constexpr uint32_t SHIP_TRANSPORTSHIP = 3015;

    // A hull in MODEL space, the space a baked model arrives in and is queried in: a floor
    // at z = 5 spanning +/-10 yards, a bulkhead across x = 0 rising 3 yards off it, and an
    // upper platform at z = 12 over the port half, which is what puts TWO horizontal
    // surfaces in one column.
    TriSoup HullSoup()
    {
        TriSoup soup;

        auto quad = [&soup](const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d)
        {
            const uint32_t base = uint32_t(soup.verts.size());
            soup.verts.push_back(a);
            soup.verts.push_back(b);
            soup.verts.push_back(c);
            soup.verts.push_back(d);
            soup.tris.push_back({base, base + 1, base + 2});
            soup.tris.push_back({base, base + 2, base + 3});
        };

        quad({-10.f, -10.f, 5.f}, {10.f, -10.f, 5.f}, {10.f, 10.f, 5.f}, {-10.f, 10.f, 5.f});
        quad({0.f, -10.f, 5.f}, {0.f, 10.f, 5.f}, {0.f, 10.f, 8.f}, {0.f, -10.f, 8.f});
        quad({-10.f, -10.f, 12.f}, {-2.f, -10.f, 12.f}, {-2.f, 10.f, 12.f}, {-10.f, 10.f, 12.f});
        return soup;
    }

    std::shared_ptr<ITileSource> HullSource()
    {
        return std::make_shared<ModelTileSource>(
            std::make_shared<CollisionModel>(HullSoup()));
    }

    bool Approx(float a, float b, float tol = 1e-3f)
    {
        return std::fabs(a - b) < tol;
    }

    // What the game layer means by "the floor here": sweep from a little above, fall back
    // to the surface overhead when the point is buried under everything.
    std::optional<float> FloorAt(const FusedTerrain& terrain, float x, float y, float z)
    {
        return terrain.ColumnAt(x, y, z + 50.f, z - 10000.f).Floor(z);
    }
}

TEST(ModelMap_TheHullIsTheGround)
{
    FusedTerrain hull(SHIP_TRANSPORTSHIP, HullSource());

    // (5,5) and (-5,-5) fall either side of the tile boundary at the map centre, so this
    // also pins that a model-backed map answers from its one tile whatever the cell.
    auto z = hull.ColumnAt(5.f, 5.f, 8.f, -10.f).HighestSolidAtOrBelow(6.f);
    REQUIRE(z.has_value());
    CHECK(Approx(*z, 5.f));

    z = hull.ColumnAt(-5.f, -5.f, 8.f, -10.f).HighestSolidAtOrBelow(6.f);
    REQUIRE(z.has_value());
    CHECK(Approx(*z, 5.f));
}

TEST(ModelMap_ModelCoordinatesAreMapCoordinates)
{
    FusedTerrain hull(SHIP_TRANSPORTSHIP, HullSource());

    auto z = hull.ColumnAt(9.5f, -9.5f, 9.f, -10.f).HighestSolidAtOrBelow(7.f);
    REQUIRE(z.has_value());
    CHECK(Approx(*z, 5.f));
}

TEST(ModelMap_OverTheRailThereIsNoGround)
{
    FusedTerrain hull(SHIP_TRANSPORTSHIP, HullSource());

    CHECK(hull.ColumnAt(50.f, 50.f, 8.f, -10.f).Empty());
    CHECK(hull.ColumnAt(10.5f, 0.f, 8.f, -10.f).Empty());
}

TEST(ModelMap_TheColumnSeesEveryDeckNotJustTheTopOne)
{
    // What a nearest-hit probe cannot report, and the whole reason a column exists: under
    // the upper platform there is another floor, and a ray that stops at the first surface
    // answers "12" to someone standing at 6.
    FusedTerrain hull(SHIP_TRANSPORTSHIP, HullSource());

    const Column column = hull.ColumnAt(-5.f, 0.f, 20.f, -10.f);

    auto below = column.HighestSolidAtOrBelow(6.f);
    REQUIRE(below.has_value());
    CHECK(Approx(*below, 5.f));

    auto above = column.LowestSolidAbove(6.f);
    REQUIRE(above.has_value());
    CHECK(Approx(*above, 12.f));

    // Starboard of the platform only the one floor is there, so the column is not simply
    // reporting everything the model contains.
    CHECK(!hull.ColumnAt(5.f, 0.f, 20.f, -10.f).LowestSolidAbove(6.f).has_value());
}

TEST(ModelMap_TheFloorIsFoundFromWellBelowIt)
{
    FusedTerrain hull(SHIP_TRANSPORTSHIP, HullSource());

    auto z = FloorAt(hull, 5.f, 5.f, -20.f);
    REQUIRE(z.has_value());
    CHECK(Approx(*z, 5.f));
}

TEST(ModelMap_TheBulkheadBlocksSightAcrossIt)
{
    FusedTerrain hull(SHIP_TRANSPORTSHIP, HullSource());

    CHECK(!hull.IsInLineOfSight(-5.f, 0.f, 6.f, 5.f, 0.f, 6.f));
    CHECK(hull.IsInLineOfSight(-5.f, 0.f, 6.f, -2.f, 0.f, 6.f));

    // Over the top of it: the bulkhead stops at z = 8.
    CHECK(hull.IsInLineOfSight(-5.f, 0.f, 8.5f, 5.f, 0.f, 8.5f));
}

TEST(ModelMap_AModelWithNoGeometryMakesNoMap)
{
    ModelTileSource empty(std::make_shared<CollisionModel>(TriSoup{}));
    CHECK(empty.Empty());

    FusedTerrain nowhere(SHIP_TRANSPORTSHIP,
                         std::make_shared<ModelTileSource>(nullptr));
    CHECK(nowhere.ColumnAt(0.f, 0.f, 8.f, -10.f).Empty());
}

TEST(ModelMap_WithoutASourceADisplayIdNamesNoTerrainAtAll)
{
    // What the engine could say before a source could be supplied: a map id with no
    // t_/w_ file on disk has no ground anywhere, whatever geometry was baked for it.
    const std::string saved = FusedTerrain::TileDir();
    FusedTerrain::SetTileDir(std::string());

    FusedTerrain bare(SHIP_TRANSPORTSHIP);
    CHECK(bare.ColumnAt(5.f, 5.f, 8.f, -10.f).Empty());
    CHECK(!FloorAt(bare, 5.f, 5.f, 6.f).has_value());

    FusedTerrain::SetTileDir(saved);
}
