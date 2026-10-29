/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "TestHarness.h"

#include "terrain/CollisionModel.hpp"
#include "terrain/Column.hpp"
#include "terrain/Terrain.hpp"
#include "terrain/WmoModel.hpp"

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

using namespace world::terrain;

namespace
{
    // Brute force over the same soup is the only arbiter that assumes nothing. Never
    // score one acceleration structure against another.
    std::optional<float> BruteForce(const TriSoup& soup, const Vec3& o, const Vec3& d,
                                    float tMax)
    {
        std::optional<float> best;
        for (uint32_t i = 0; i < soup.tris.size(); ++i)
        {
            if (auto t = rayTri(o, d, soup.At(i)))
            {
                if (*t >= 0.f && *t < tMax && (!best || *t < *best))
                {
                    best = *t;
                }
            }
        }
        return best;
    }

    TriSoup RandomSoup(std::mt19937& rng, int count, float spread)
    {
        std::uniform_real_distribution<float> pos(-spread, spread);
        std::uniform_real_distribution<float> edge(-3.f, 3.f);

        TriSoup soup;
        soup.verts.reserve(size_t(count) * 3);
        soup.tris.reserve(size_t(count));
        for (int i = 0; i < count; ++i)
        {
            const Vec3 a{pos(rng), pos(rng), pos(rng)};
            const Vec3 b{a.x + edge(rng), a.y + edge(rng), a.z + edge(rng)};
            const Vec3 c{a.x + edge(rng), a.y + edge(rng), a.z + edge(rng)};
            const uint32_t base = uint32_t(soup.verts.size());
            soup.verts.push_back(a);
            soup.verts.push_back(b);
            soup.verts.push_back(c);
            soup.tris.push_back({base, base + 1, base + 2});
        }
        return soup;
    }

    WmoModel::Group FlatLiquidGroup(uint32_t tilesX, uint32_t tilesY, float z,
                                    uint8_t tileFlag, uint16_t entry, uint8_t kind)
    {
        WmoModel::Group g;
        g.hasLiquid = true;
        g.liquid.tilesX = tilesX;
        g.liquid.tilesY = tilesY;
        g.liquid.corner = {0.f, 0.f, 0.f};
        g.liquid.entry = entry;
        g.liquid.kind = kind;
        g.liquid.heights.assign(size_t(tilesX + 1) * (tilesY + 1), z);
        g.liquid.flags.assign(size_t(tilesX) * tilesY, tileFlag);
        return g;
    }
}

TEST(BvhAgreesWithBruteForceOnEveryRay)
{
    std::mt19937 rng(0xC0FFEE);
    TriSoup soup = RandomSoup(rng, 900, 60.f);

    Bvh bvh;
    bvh.Build(soup, nullptr, 4);
    REQUIRE(!bvh.Empty());
    CHECK(bvh.MaxDepth() <= Bvh::MAX_DEPTH + 1);

    std::uniform_real_distribution<float> pos(-70.f, 70.f);
    std::uniform_real_distribution<float> dir(-1.f, 1.f);

    size_t mismatches = 0;
    for (int i = 0; i < 4000; ++i)
    {
        const Vec3 o{pos(rng), pos(rng), pos(rng)};
        Vec3 d{dir(rng), dir(rng), dir(rng)};
        if (d.squaredMagnitude() < 1e-6f)
        {
            continue;
        }
        d = d.direction();

        const auto fast = bvh.Raycast(soup, o, d, 500.f);
        const auto slow = BruteForce(soup, o, d, 500.f);

        if (fast.has_value() != slow.has_value())
        {
            ++mismatches;
        }
        else if (fast && std::fabs(*fast - *slow) > 1e-3f)
        {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, size_t(0));
}

TEST(BvhAgreesWithBruteForceOnDownwardColumnRays)
{
    // A floor probe is the query the server actually makes, and it is the one an
    // over-tight broadphase breaks: the ray runs parallel to two of the three slabs.
    std::mt19937 rng(7);
    TriSoup soup = RandomSoup(rng, 700, 40.f);

    Bvh bvh;
    bvh.Build(soup, nullptr, 4);

    std::uniform_real_distribution<float> pos(-45.f, 45.f);
    size_t mismatches = 0;
    for (int i = 0; i < 6000; ++i)
    {
        const Vec3 o{pos(rng), pos(rng), 200.f};
        const Vec3 d{0.f, 0.f, -1.f};

        const auto fast = bvh.Raycast(soup, o, d, 400.f);
        const auto slow = BruteForce(soup, o, d, 400.f);

        if (fast.has_value() != slow.has_value() ||
            (fast && std::fabs(*fast - *slow) > 1e-3f))
        {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, size_t(0));
}

TEST(BvhBroadphaseIsNeverTighterThanTheNarrowphase)
{
    // The ray sits 0.05 mm outside the upper triangle's own x-bound, with dx == 0.
    // rayTri's barycentric slop accepts that hit deliberately, so a ray cannot slip
    // through the crack between two adjacent faces -- but an unpadded box test is exact
    // and rejects the very leaf that owns the triangle. The traversal then skips the
    // floor and returns the one thirty yards below it. Random rays do not find this;
    // it has to be aimed.
    TriSoup soup;
    soup.verts = {{0.f, 0.f, 0.f},     {10.f, 0.f, 0.f},   {0.f, 10.f, 0.f},
                  {-50.f, -50.f, -30.f}, {50.f, -50.f, -30.f}, {0.f, 50.f, -30.f}};
    soup.tris = {{0, 1, 2}, {3, 4, 5}};

    Bvh bvh;
    bvh.Build(soup, nullptr, 1);

    const Vec3 o{-5e-5f, 3.f, 100.f};
    const Vec3 d{0.f, 0.f, -1.f};

    const auto fast = bvh.Raycast(soup, o, d, 400.f);
    const auto slow = BruteForce(soup, o, d, 400.f);
    REQUIRE(slow.has_value());
    CHECK(std::fabs(*slow - 100.f) < 1e-3f);
    REQUIRE(fast.has_value());
    CHECK(std::fabs(*fast - *slow) < 1e-3f);
}

TEST(BvhPermutationKeepsTheParallelArrayAligned)
{
    std::mt19937 rng(11);
    TriSoup soup = RandomSoup(rng, 200, 20.f);

    // Tag each triangle with its own index, so any drift between the two permutations
    // shows up as a triangle whose tag no longer names its own first vertex.
    std::vector<uint16_t> tag(soup.tris.size());
    std::vector<Vec3> firstVertexOf(soup.tris.size());
    for (size_t i = 0; i < soup.tris.size(); ++i)
    {
        tag[i] = uint16_t(i);
        firstVertexOf[i] = soup.verts[soup.tris[i][0]];
    }

    Bvh bvh;
    bvh.Build(soup, &tag, 4);

    REQUIRE(tag.size() == soup.tris.size());
    size_t drift = 0;
    for (size_t i = 0; i < soup.tris.size(); ++i)
    {
        const Vec3& v = soup.verts[soup.tris[i][0]];
        if (!(v == firstVertexOf[tag[i]]))
        {
            ++drift;
        }
    }
    CHECK_EQ(drift, size_t(0));
}

TEST(BvhEmptySoupIsQueryableAndMisses)
{
    TriSoup soup;
    Bvh bvh;
    bvh.Build(soup, nullptr, 4);
    CHECK(bvh.Empty());
    CHECK(!bvh.Raycast(soup, Vec3{0, 0, 10}, Vec3{0, 0, -1}, 100.f).has_value());
}

TEST(CollisionModelBoundsCoverEveryTriangle)
{
    TriSoup soup;
    soup.verts = {{0, 0, 0}, {10, 0, 0}, {0, 10, 5}};
    soup.tris = {{0, 1, 2}};

    CollisionModel m(std::move(soup));
    CHECK(!m.Empty());
    CHECK_EQ(m.TriangleCount(), size_t(1));
    CHECK_EQ(m.Bounds().lo.x, 0.f);
    CHECK_EQ(m.Bounds().hi.x, 10.f);
    CHECK_EQ(m.Bounds().hi.z, 5.f);

    const auto t = m.RaycastNearest(Vec3{1.f, 1.f, 50.f}, Vec3{0, 0, -1}, 100.f);
    REQUIRE(t.has_value());
    CHECK(std::fabs(*t - 49.5f) < 0.5f);
}

TEST(TransformPreservesTheRayParameter)
{
    // The floor query transforms the ray into model space and uses the returned t as a
    // world distance. That is only valid because worldToLocalDirection divides by the
    // same scale the position does -- if it ever stops, every scaled model's floor moves.
    Transform xf;
    xf.pos = {123.f, -45.f, 6.f};
    xf.rot = Mat3::fromEuler(0.3f, -0.7f, 1.1f);
    xf.scale = 2.5f;

    const Vec3 originWorld{5.f, 7.f, 90.f};
    const Vec3 dirWorld{0.f, 0.f, -1.f};
    const float t = 33.25f;

    const Vec3 hitWorld = originWorld + dirWorld * t;
    const Vec3 originModel = xf.worldToLocal(originWorld);
    const Vec3 dirModel = xf.worldToLocalDirection(dirWorld);
    const Vec3 hitModel = originModel + dirModel * t;

    const Vec3 back = xf.localToWorld(hitModel);
    CHECK((back - hitWorld).magnitude() < 1e-2f);
}

TEST(WmoLiquidRejectsPointsOutsideTheFootprint)
{
    std::vector<WmoModel::Group> groups;
    groups.push_back(FlatLiquidGroup(2, 2, 12.f, 0, 13, uint8_t(LiquidKind::Water)));

    WmoModel m(TriSoup{}, {}, std::move(groups), 0);

    const auto inside = m.LiquidLocal(Vec3{1.f, 1.f, 0.f});
    REQUIRE(inside.has_value());
    CHECK_EQ(inside->z, 12.f);
    CHECK_EQ(inside->entry, uint16_t(13));

    // Truncating a negative offset to int yields 0, so a point up to one tile outside
    // the low corner used to land on tile (0,0) and report liquid that is not there.
    CHECK(!m.LiquidLocal(Vec3{-1.f, 1.f, 0.f}).has_value());
    CHECK(!m.LiquidLocal(Vec3{1.f, -1.f, 0.f}).has_value());
    CHECK(!m.LiquidLocal(Vec3{1000.f, 1.f, 0.f}).has_value());
}

TEST(WmoLiquidHonoursTheDryTileNibble)
{
    std::vector<WmoModel::Group> groups;
    groups.push_back(FlatLiquidGroup(2, 2, 12.f, 0x0F, 13, uint8_t(LiquidKind::Water)));

    WmoModel m(TriSoup{}, {}, std::move(groups), 0);
    CHECK(!m.LiquidLocal(Vec3{1.f, 1.f, 0.f}).has_value());
}

TEST(WmoLiquidOnlyGroupIsNotEmpty)
{
    std::vector<WmoModel::Group> groups;
    groups.push_back(FlatLiquidGroup(2, 2, 12.f, 0, 20, uint8_t(LiquidKind::Slime)));

    WmoModel m(TriSoup{}, {}, std::move(groups), 0);
    CHECK(!m.Empty());
    CHECK(m.Bounds().valid());
    CHECK(m.Bounds().coversColumn(1.f, 1.f));
}

TEST(WmoAreaInfoNamesTheGroupTheRayHit)
{
    TriSoup soup;
    soup.verts = {{-5, -5, 0}, {5, -5, 0}, {0, 5, 0},
                  {-5, -5, 20}, {5, -5, 20}, {0, 5, 20}};
    soup.tris = {{0, 1, 2}, {3, 4, 5}};

    std::vector<uint16_t> triGroup = {0, 1};
    std::vector<WmoModel::Group> groups(2);
    groups[0].groupWmoId = 111;
    groups[0].mogpFlags = 0x2000;
    groups[1].groupWmoId = 222;
    groups[1].mogpFlags = 0x8;

    WmoModel m(std::move(soup), std::move(triGroup), std::move(groups), 999);
    CHECK_EQ(m.RootId(), uint32_t(999));

    const auto hit = m.AreaInfo(Vec3{0.f, 0.f, 50.f}, Vec3{0, 0, -1}, 200.f);
    REQUIRE(hit.has_value());
    CHECK_EQ(hit->groupId, uint32_t(222));  // the upper floor is hit first
    CHECK_EQ(hit->mogpFlags, uint32_t(0x8));

    const auto below = m.AreaInfo(Vec3{0.f, 0.f, 10.f}, Vec3{0, 0, -1}, 200.f);
    REQUIRE(below.has_value());
    CHECK_EQ(below->groupId, uint32_t(111));
}

namespace
{
    LiquidInfo AdtWater(float level, std::optional<float> terrainZ)
    {
        LiquidInfo info;
        info.level = level;
        info.kind = LiquidKind::Water;
        info.entry = 1;
        info.terrainZ = terrainZ;
        return info;
    }
}

TEST(AdtLiquidDoesNotReachAPointFarUnderItsTerrain)
{
    // The Undercity, Miles Dexter: Tirisfal's water at 32.93, under its own
    // terrain at 34.14, the city's floor 95 yards down. The reference GridMap
    // left it dry.
    Column c;
    c.AddSolid(-62.28f, SurfaceKind::Static);
    c.AddLiquid(AdtWater(32.93f, 34.14f));
    CHECK(!c.LiquidFor(-62.2f).has_value());
    // still there for a caller that asks for any liquid
    CHECK(c.HighestLiquid().has_value());
}

TEST(AdtLiquidReachesAPointAboveOrJustUnderItsTerrain)
{
    Column c;
    c.AddSolid(10.f, SurfaceKind::Terrain);
    c.AddLiquid(AdtWater(15.f, 10.f));
    const auto swimming = c.LiquidFor(12.f);
    REQUIRE(swimming.has_value());
    CHECK_EQ(swimming->z, 15.f);
    CHECK(c.LiquidFor(10.f - ADT_LIQUID_BURIED_LIMIT + 0.5f).has_value());
    CHECK(!c.LiquidFor(10.f - ADT_LIQUID_BURIED_LIMIT - 0.5f).has_value());
}

TEST(AdtLiquidUnderItsOwnTerrainIsNoLiquid)
{
    Column c;
    c.AddLiquid(AdtWater(32.93f, 34.14f));
    CHECK(!c.LiquidFor(40.f).has_value());
}

TEST(AdtLiquidOverAHoleAndModelLiquidAreNotFiltered)
{
    // No terrain under it (a hole cut for a WMO entrance) or a WMO's own
    // liquid: the floor checks of the caller decide, as before.
    Column c;
    c.AddLiquid(AdtWater(32.93f, std::nullopt));
    LiquidInfo wmo = AdtWater(-60.f, std::nullopt);
    wmo.kind = LiquidKind::Slime;
    c.AddLiquid(wmo);
    const auto hole = c.LiquidFor(-62.2f);
    REQUIRE(hole.has_value());
    CHECK_EQ(hole->z, 32.93f);
}

TEST(BuriedAdtLiquidLeavesAModelLiquidBelowIt)
{
    // The canal of a city under a lake: the lake is out, the canal is the
    // answer.
    Column c;
    c.AddLiquid(AdtWater(32.93f, 30.f));
    LiquidInfo canal = AdtWater(-63.f, std::nullopt);
    canal.kind = LiquidKind::Slime;
    c.AddLiquid(canal);
    const auto liquid = c.LiquidFor(-64.f);
    REQUIRE(liquid.has_value());
    CHECK_EQ(liquid->z, -63.f);
    CHECK(liquid->liquid == LiquidKind::Slime);
}
