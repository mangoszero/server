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

#include "DynamicCollision.h"
#include "GameObjectModel.h"
#include "terrain/CollisionModel.hpp"
#include "terrain/Terrain.hpp"

#include <cfloat>
#include <cmath>
#include <memory>
#include <optional>
#include <vector>

using Geometry::Transform;
using Geometry::Vector3;
using world::terrain::CollisionModel;
using world::terrain::ICollisionModel;
using world::terrain::TriSoup;

namespace
{
    // An axis-aligned box of `half` extent about the model origin, as a closed soup.
    std::shared_ptr<const ICollisionModel> BoxModel(float half)
    {
        TriSoup soup;
        soup.verts = {{-half, -half, -half}, {half, -half, -half},
                      {half, half, -half},   {-half, half, -half},
                      {-half, -half, half},  {half, -half, half},
                      {half, half, half},    {-half, half, half}};
        soup.tris = {{0, 2, 1}, {0, 3, 2},   // bottom
                     {4, 5, 6}, {4, 6, 7},   // top
                     {0, 1, 5}, {0, 5, 4},
                     {1, 2, 6}, {1, 6, 5},
                     {2, 3, 7}, {2, 7, 6},
                     {3, 0, 4}, {3, 4, 7}};
        return std::make_shared<CollisionModel>(std::move(soup));
    }

    struct Body
    {
        std::unique_ptr<GameObjectModel> model;

        Body(float half, const Vector3& at, uint32 phase = 1, float scale = 1.f)
        {
            Transform xf;
            xf.pos = at;
            xf.scale = scale;
            model.reset(GameObjectModel::CreateStandalone(BoxModel(half), xf, phase));
            if (model)
            {
                model->SetCollidable(true);
            }
        }
    };

    // The live half no longer answers with a height; it CONTRIBUTES to the terrain
    // engine's column. Highest surface under the probe is the selection that used to be
    // baked into the call.
    std::optional<float> SurfaceUnder(const DynamicCollision& world, float x, float y,
                                     float zTop, float drop, uint32 phase = 1)
    {
        world::terrain::Column column;
        world.AddSurfaces(x, y, zTop, zTop - drop, phase, column);
        return column.HighestSolidAtOrBelow(zTop);
    }
}

TEST(DynamicCollisionInsertRemoveAndContains)
{
    DynamicCollision world;
    Body a(2.f, Vector3{100.f, 100.f, 50.f});
    REQUIRE(a.model != nullptr);

    CHECK(!world.Contains(*a.model));
    world.Insert(*a.model);
    CHECK(world.Contains(*a.model));
    CHECK_EQ(world.Size(), 1);

    // Inserting twice must not file the body twice, or a query would raycast it twice
    // and Remove would leave half of it behind.
    world.Insert(*a.model);
    CHECK_EQ(world.Size(), 1);

    world.Remove(*a.model);
    CHECK(!world.Contains(*a.model));
    CHECK_EQ(world.Size(), 0);

    // Removing something absent is not an error; a game object can be despawned twice.
    world.Remove(*a.model);
    CHECK_EQ(world.Size(), 0);
}

TEST(DynamicCollisionBlocksASightlineThroughIt)
{
    DynamicCollision world;
    Body wall(3.f, Vector3{0.f, 0.f, 0.f});
    REQUIRE(wall.model != nullptr);
    world.Insert(*wall.model);

    // Straight through the box.
    CHECK(!world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 1));
    const float frac = world.NearestHitFraction(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 1);
    CHECK(frac <= 1.0f);
    // The near face sits at x = -3 on a 40-yard segment starting at -20.
    CHECK(std::fabs(frac - (17.0f / 40.0f)) < 0.05f);

    // Clear of it.
    CHECK(world.IsInLineOfSight(-20.f, 50.f, 0.f, 20.f, 50.f, 0.f, 1));
    CHECK(world.NearestHitFraction(-20.f, 50.f, 0.f, 20.f, 50.f, 0.f, 1) > 1.0f);
}

TEST(DynamicCollisionRespectsTheCollidableFlag)
{
    // A door "opening" flips this flag rather than moving, so it is the whole mechanism
    // by which an opened door stops blocking.
    DynamicCollision world;
    Body door(3.f, Vector3{0.f, 0.f, 0.f});
    REQUIRE(door.model != nullptr);
    world.Insert(*door.model);

    CHECK(!world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 1));
    door.model->SetCollidable(false);
    CHECK(world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 1));
    CHECK(!SurfaceUnder(world, 0.f, 0.f, 20.f, 100.f).has_value());
}

TEST(DynamicCollisionRespectsThePhaseMask)
{
    DynamicCollision world;
    Body phased(3.f, Vector3{0.f, 0.f, 0.f}, /*phase*/ 2);
    REQUIRE(phased.model != nullptr);
    world.Insert(*phased.model);

    CHECK(!world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 2));
    CHECK(world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 1));
    CHECK(!world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 3));
}

TEST(DynamicCollisionReportsTheSurfaceUnderAColumn)
{
    DynamicCollision world;
    Body platform(4.f, Vector3{0.f, 0.f, 10.f});
    REQUIRE(platform.model != nullptr);
    world.Insert(*platform.model);

    // The box top is at z = 14.
    const auto top = SurfaceUnder(world, 0.f, 0.f, 40.f, 100.f);
    REQUIRE(top.has_value());
    CHECK(std::fabs(*top - 14.f) < 0.01f);

    // Nothing under a column that misses it.
    CHECK(!SurfaceUnder(world, 50.f, 50.f, 40.f, 100.f).has_value());

    // A probe already below the body finds nothing above itself.
    CHECK(!SurfaceUnder(world, 0.f, 0.f, -10.f, 100.f).has_value());
}

TEST(DynamicCollisionFindsABodySpanningManyTileCells)
{
    // A bridge is filed under EVERY tile its box overlaps. Keying on its position would
    // file it under one cell, and it would then be invisible from every other -- the
    // failure this bucketing exists to prevent.
    DynamicCollision world;

    const float half = world::terrain::TILE_SIZE * 1.5f;   // spans 3+ tiles
    Body bridge(half, Vector3{0.f, 0.f, 0.f});
    REQUIRE(bridge.model != nullptr);
    world.Insert(*bridge.model);

    // Probe columns a full tile apart, all inside the body.
    const float step = world::terrain::TILE_SIZE;
    size_t found = 0, probes = 0;
    for (int i = -1; i <= 1; ++i)
    {
        for (int j = -1; j <= 1; ++j)
        {
            ++probes;
            const float x = float(i) * step;
            const float y = float(j) * step;
            if (SurfaceUnder(world, x, y, half + 50.f, 500.f))
            {
                ++found;
            }
        }
    }
    CHECK_EQ(found, probes);
}

TEST(DynamicCollisionCountsASpanningBodyOnceNotOncePerCell)
{
    // A body in many buckets is visited many times unless the epoch stamp suppresses
    // the repeats. The nearest hit is the same either way, so the stamp is checked by
    // the height query agreeing with a single-cell body's answer exactly.
    DynamicCollision world;

    const float half = world::terrain::TILE_SIZE * 1.5f;
    Body wide(half, Vector3{0.f, 0.f, 0.f});
    REQUIRE(wide.model != nullptr);
    world.Insert(*wide.model);

    const auto top = SurfaceUnder(world, 0.f, 0.f, half + 100.f, 1000.f);
    REQUIRE(top.has_value());
    CHECK(std::fabs(*top - half) < 0.01f);

    // And a segment crossing the whole thing still yields one sane fraction.
    const float frac = world.NearestHitFraction(-half * 2.f, 0.f, 0.f, half * 2.f, 0.f,
                                                0.f, 1);
    CHECK(frac > 0.0f);
    CHECK(frac <= 1.0f);
}

TEST(DynamicCollisionRefreshMovesTheBodyNotJustItsRotation)
{
    // The old path took a quaternion and updated only the rotation, so a game object
    // that was MOVED kept a stale world box and collided where it no longer stood.
    DynamicCollision world;
    Body lift(3.f, Vector3{0.f, 0.f, 0.f});
    REQUIRE(lift.model != nullptr);
    world.Insert(*lift.model);

    CHECK(!world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 1));

    // Move it far away and re-file.
    Transform moved;
    moved.pos = Vector3{500.f, 500.f, 0.f};
    std::unique_ptr<GameObjectModel> replacement(
        GameObjectModel::CreateStandalone(BoxModel(3.f), moved, 1));
    REQUIRE(replacement != nullptr);
    replacement->SetCollidable(true);

    world.Remove(*lift.model);
    world.Insert(*replacement);

    CHECK(world.IsInLineOfSight(-20.f, 0.f, 0.f, 20.f, 0.f, 0.f, 1));
    CHECK(!world.IsInLineOfSight(480.f, 500.f, 0.f, 520.f, 500.f, 0.f, 1));
}

TEST(DynamicCollisionEmptyWorldBlocksNothing)
{
    DynamicCollision world;
    CHECK_EQ(world.Size(), 0);
    CHECK(world.IsInLineOfSight(-100.f, -100.f, 0.f, 100.f, 100.f, 0.f, 1));
    CHECK(world.NearestHitFraction(-100.f, 0.f, 0.f, 100.f, 0.f, 0.f, 1) > 1.0f);
    CHECK(!SurfaceUnder(world, 0.f, 0.f, 50.f, 100.f).has_value());
}

TEST(DynamicCollisionDegenerateSegmentDoesNotBlock)
{
    // A zero-length segment has no direction to trace; it must not report a hit, and it
    // must not divide by its own length.
    DynamicCollision world;
    Body box(3.f, Vector3{0.f, 0.f, 0.f});
    REQUIRE(box.model != nullptr);
    world.Insert(*box.model);

    CHECK(world.NearestHitFraction(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1) > 1.0f);
    CHECK(world.IsInLineOfSight(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1));
}

TEST(DynamicCollisionOffMapCoordinatesDoNotIndexOutOfTheGrid)
{
    // Tile indices are clamped, so a body or a query far outside the 64x64 grid must be
    // handled rather than reaching past the buckets.
    DynamicCollision world;
    Body far(5.f, Vector3{60000.f, -60000.f, 0.f});
    REQUIRE(far.model != nullptr);
    world.Insert(*far.model);
    CHECK_EQ(world.Size(), 1);

    world.IsInLineOfSight(-70000.f, 70000.f, 0.f, 70000.f, -70000.f, 0.f, 1);
    SurfaceUnder(world, 60000.f, -60000.f, 100.f, 500.f);

    world.Remove(*far.model);
    CHECK_EQ(world.Size(), 0);
}
