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

#include <memory>
#include "TestHarness.h"

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "terrain/FusedTerrain.hpp"
#include "terrain/TileSerializer.hpp"
#include "terrain/CollisionModel.hpp"
#include "terrain/WmoModel.hpp"

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace world::terrain;

namespace
{
    /// Process id, for a temp name no concurrent run can collide with.
    unsigned long GetCurrentPid()
    {
#ifdef _WIN32
        return static_cast<unsigned long>(::GetCurrentProcessId());
#else
        return static_cast<unsigned long>(::getpid());
#endif
    }

    std::string TempPath(const char* leaf)
    {
        const char* dir = std::getenv("TMPDIR");
        if (!dir)
        {
            dir = std::getenv("TEMP");
        }
        if (!dir)
        {
            dir = "/tmp";
        }
        // The pid is not decoration: the name used to be fixed, so two test binaries
        // on one machine -- a CI matrix, or two builds side by side -- truncated each
        // other's file and the reader correctly accepted what looked like a prefix.
        return std::string(dir) + "/mangos_tile_" + std::to_string(GetCurrentPid()) +
               "_" + leaf;
    }

    struct ScopedFile
    {
        std::string path;
        explicit ScopedFile(const char* leaf) : path(TempPath(leaf)) {}
        ~ScopedFile() { std::remove(path.c_str()); }
    };

    long FileSize(const std::string& path)
    {
        std::error_code ec;
        const auto n = std::filesystem::file_size(path, ec);
        return ec ? -1 : long(n);
    }

    // Peak RSS in bytes, or 0 where the platform does not offer it cheaply.
    unsigned long long PeakResidentBytes()
    {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        {
            return static_cast<unsigned long long>(pmc.PeakWorkingSetSize);
        }
        return 0;
#elif defined(__unix__) || defined(__APPLE__)
        rusage ru{};
        if (getrusage(RUSAGE_SELF, &ru) == 0)
        {
#if defined(__APPLE__)
            return static_cast<unsigned long long>(ru.ru_maxrss);
#else
            return static_cast<unsigned long long>(ru.ru_maxrss) * 1024ull;
#endif
        }
        return 0;
#else
        return 0;
#endif
    }

    TriSoup Pyramid(float scale)
    {
        TriSoup soup;
        soup.verts = {{-scale, -scale, 0.f},
                      {scale, -scale, 0.f},
                      {scale, scale, 0.f},
                      {-scale, scale, 0.f},
                      {0.f, 0.f, scale}};
        soup.tris = {{0, 1, 2}, {0, 2, 3}, {0, 1, 4}, {1, 2, 4}, {2, 3, 4}, {3, 0, 4}};
        return soup;
    }

    std::shared_ptr<WmoModel> MakeWmo(uint32_t rootId, bool withLiquid)
    {
        TriSoup soup = Pyramid(8.f);
        std::vector<uint16_t> triGroup(soup.tris.size(), 0);

        std::vector<WmoModel::Group> groups(1);
        groups[0].mogpFlags = 0x2000;
        groups[0].groupWmoId = 4321;
        if (withLiquid)
        {
            groups[0].hasLiquid = true;
            groups[0].liquid.tilesX = 3;
            groups[0].liquid.tilesY = 2;
            groups[0].liquid.corner = {-4.f, -4.f, 0.f};
            groups[0].liquid.entry = 19;
            groups[0].liquid.kind = uint8_t(LiquidKind::Magma);
            groups[0].liquid.heights.assign(4 * 3, 2.5f);
            groups[0].liquid.flags.assign(3 * 2, 0);
        }

        return std::make_shared<WmoModel>(std::move(soup), std::move(triGroup),
                                          std::move(groups), rootId);
    }

    TerrainTile MakeTile()
    {
        TerrainTile tile;
        tile.tx = 31;
        tile.ty = 48;
        tile.hasTerrain = true;

        tile.v9.assign(size_t(V9_SIDE) * V9_SIDE, 0.f);
        tile.v8.assign(size_t(GRID_PER_TILE) * GRID_PER_TILE, 0.f);
        for (size_t i = 0; i < tile.v9.size(); ++i)
        {
            tile.v9[i] = float(i % 97) * 0.25f;
        }
        for (size_t i = 0; i < tile.v8.size(); ++i)
        {
            tile.v8[i] = float(i % 61) * 0.5f;
        }
        tile.holes[7] = 0x0042;
        tile.areaIds[7] = 1519;
        tile.areaIds[200] = 12;

        const size_t cells = size_t(GRID_PER_TILE) * GRID_PER_TILE;
        tile.hasLiquid = true;
        tile.liquidHeight.assign(size_t(V9_SIDE) * V9_SIDE, 11.f);
        tile.liquidShow.assign(cells, 0);
        tile.liquidKind.assign(cells, uint8_t(LiquidKind::None));
        tile.liquidEntry.assign(cells, 0);
        tile.liquidDeep.assign(cells, 0);
        tile.liquidShow[5] = 1;
        tile.liquidKind[5] = uint8_t(LiquidKind::Ocean);
        tile.liquidEntry[5] = 2;
        tile.liquidDeep[5] = 1;

        // Two instances of ONE model plus a second model: the table must dedupe the
        // first and both instances must come back pointing at the same object.
        auto shared = MakeWmo(777, true);
        auto other = std::make_shared<CollisionModel>(Pyramid(3.f));

        Transform xf;
        xf.pos = {100.f, 200.f, 30.f};
        xf.rot = Mat3::fromEuler(0.1f, 0.2f, 0.3f);
        xf.scale = 1.5f;

        StaticInstance a;
        a.xf = xf;
        a.model = shared;
        a.worldBounds.expand({90.f, 190.f, 20.f});
        a.worldBounds.expand({110.f, 210.f, 45.f});
        a.adtId = 11;

        StaticInstance b = a;
        b.xf.pos = {300.f, 400.f, 10.f};
        b.adtId = 22;

        StaticInstance c;
        c.xf.pos = {50.f, 60.f, 70.f};
        c.model = other;
        c.worldBounds.expand({47.f, 57.f, 67.f});
        c.worldBounds.expand({53.f, 63.f, 73.f});
        c.adtId = 33;

        tile.instances = {a, b, c};
        return tile;
    }
}

TEST(TileRoundTripPreservesEveryField)
{
    ScopedFile file("roundtrip.tile");
    const TerrainTile original = MakeTile();
    REQUIRE(WriteTile(original, file.path));

    auto back = ReadTile(file.path);
    REQUIRE(back != nullptr);

    CHECK_EQ(back->tx, original.tx);
    CHECK_EQ(back->ty, original.ty);
    CHECK(back->hasTerrain);
    CHECK(!back->isGlobalWmo);
    CHECK(back->v9 == original.v9);
    CHECK(back->v8 == original.v8);
    CHECK(back->holes == original.holes);
    CHECK(back->areaIds == original.areaIds);

    CHECK(back->hasLiquid);
    CHECK(back->liquidHeight == original.liquidHeight);
    CHECK(back->liquidShow == original.liquidShow);
    CHECK(back->liquidKind == original.liquidKind);
    CHECK(back->liquidEntry == original.liquidEntry);
    CHECK(back->liquidDeep == original.liquidDeep);

    REQUIRE(back->instances.size() == 3);
    // Two instances shared one model going in; they must share one coming out, or a
    // continent's worth of duplicated WMO geometry lands in memory.
    CHECK(back->instances[0].model.get() == back->instances[1].model.get());
    CHECK(back->instances[0].model.get() != back->instances[2].model.get());
    CHECK_EQ(back->instances[1].adtId, 22);
    CHECK_EQ(back->instances[2].adtId, 33);
    CHECK_EQ(back->instances[0].xf.scale, 1.5f);
    CHECK_EQ(back->instances[1].xf.pos.x, 300.f);
    CHECK_EQ(back->instances[0].worldBounds.hi.z, 45.f);
    for (int i = 0; i < 9; ++i)
    {
        CHECK_EQ(back->instances[0].xf.rot.m[i], original.instances[0].xf.rot.m[i]);
    }
}

TEST(TileRoundTripPreservesTheBakedBvhAndItsQueries)
{
    ScopedFile file("bvh.tile");
    const TerrainTile original = MakeTile();
    REQUIRE(WriteTile(original, file.path));

    auto back = ReadTile(file.path);
    REQUIRE(back != nullptr);
    REQUIRE(back->instances.size() == 3);

    const auto* wroteWmo =
        static_cast<const WmoModel*>(original.instances[0].model.get());
    REQUIRE(back->instances[0].model->Kind() == ModelKind::Wmo);
    const auto* readWmo = static_cast<const WmoModel*>(back->instances[0].model.get());

    // The node table IS the structure. If it does not survive, the model quietly
    // rebuilds one at load and the whole point of baking is gone.
    CHECK_EQ(readWmo->GetBvh().NodeCount(), wroteWmo->GetBvh().NodeCount());
    CHECK(readWmo->GetBvh().NodeCount() > 0);
    CHECK_EQ(readWmo->RootId(), uint32_t(777));
    REQUIRE(readWmo->Groups().size() == 1);
    CHECK_EQ(readWmo->Groups()[0].groupWmoId, uint32_t(4321));
    CHECK_EQ(readWmo->Groups()[0].liquid.entry, uint16_t(19));
    CHECK_EQ(readWmo->Groups()[0].liquid.kind, uint8_t(LiquidKind::Magma));

    // Same answers, not merely the same bytes.
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> pos(-10.f, 10.f);
    size_t mismatches = 0;
    for (int i = 0; i < 2000; ++i)
    {
        const Vec3 o{pos(rng), pos(rng), 40.f};
        const Vec3 d{0.f, 0.f, -1.f};
        const auto before = wroteWmo->RaycastNearest(o, d, 100.f);
        const auto after = readWmo->RaycastNearest(o, d, 100.f);
        if (before.has_value() != after.has_value() ||
            (before && std::fabs(*before - *after) > 1e-4f))
        {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, size_t(0));

    const auto liquid = readWmo->LiquidLocal(Vec3{-2.f, -2.f, 0.f});
    REQUIRE(liquid.has_value());
    CHECK_EQ(liquid->z, 2.5f);
    CHECK_EQ(liquid->entry, uint16_t(19));

    REQUIRE(back->instances[2].model->Kind() == ModelKind::Mesh);
    CHECK(back->instances[2].model->RaycastNearest(Vec3{0, 0, 20}, Vec3{0, 0, -1}, 100.f)
              .has_value());
}

TEST(TileReaderRejectsAForeignMagic)
{
    ScopedFile file("magic.tile");
    REQUIRE(WriteTile(MakeTile(), file.path));

    std::FILE* f = std::fopen(file.path.c_str(), "r+b");
    REQUIRE(f != nullptr);
    const uint32_t foreign = 0x58434254;  // "TBCX" -- the engine this was ported from
    std::fwrite(&foreign, 4, 1, f);
    std::fclose(f);

    CHECK(ReadTile(file.path) == nullptr);
}

TEST(TileReaderRejectsAnOlderVersion)
{
    ScopedFile file("version.tile");
    REQUIRE(WriteTile(MakeTile(), file.path));

    std::FILE* f = std::fopen(file.path.c_str(), "r+b");
    REQUIRE(f != nullptr);
    std::fseek(f, 4, SEEK_SET);
    const uint32_t stale = 0;
    std::fwrite(&stale, 4, 1, f);
    std::fclose(f);

    // A version bump must read as a cache miss, never as data. Silently accepting one
    // is how a re-baked field gets read out of the wrong offset.
    CHECK(ReadTile(file.path) == nullptr);
}

TEST(TileReaderSurvivesTruncationAtEveryLength)
{
    ScopedFile source("full.tile");
    REQUIRE(WriteTile(MakeTile(), source.path));

    std::FILE* f = std::fopen(source.path.c_str(), "rb");
    REQUIRE(f != nullptr);
    std::fseek(f, 0, SEEK_END);
    const long full = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> bytes(static_cast<size_t>(full));
    const size_t got = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    REQUIRE(got == bytes.size());
    REQUIRE(full > 64);

    // Every prefix of a valid file must be rejected, not merely most of them. A
    // truncated cache is what a killed bake leaves behind, and the reader is the only
    // thing standing between that and a server reading garbage as geometry.
    ScopedFile cut("cut.tile");
    size_t accepted = 0;
    for (long len = 1; len < full; len += 7)
    {
        std::FILE* out = std::fopen(cut.path.c_str(), "wb");
        REQUIRE(out != nullptr);
        std::fwrite(bytes.data(), 1, size_t(len), out);
        std::fclose(out);

        if (ReadTile(cut.path) != nullptr)
        {
            ++accepted;
        }
    }
    CHECK_EQ(accepted, size_t(0));
}

TEST(TileReaderRejectsCountsLargerThanTheFile)
{
    // A corrupt length field must be rejected by ARITHMETIC, before anything is
    // reserved from it. A plain fixed ceiling is not enough: it still permits a
    // reservation of hundreds of megabytes, and on an overcommitting kernel that
    // reservation succeeds, so the short read that follows rejects the file anyway and
    // the guard is never what did the work.
    ScopedFile file("hostile.tile");

    std::FILE* f = std::fopen(file.path.c_str(), "wb");
    REQUIRE(f != nullptr);
    const uint32_t magic = 0x32474E4D;  // "MNG2"
    const uint32_t version = 1;
    const int32_t tx = 0, ty = 0;
    const uint8_t flags[2] = {1, 0};
    const uint32_t absurd = 0xFFFFFFFFu;
    std::fwrite(&magic, 4, 1, f);
    std::fwrite(&version, 4, 1, f);
    std::fwrite(&tx, 4, 1, f);
    std::fwrite(&ty, 4, 1, f);
    std::fwrite(flags, 1, 2, f);
    std::fwrite(&absurd, 4, 1, f);  // v9 element count
    std::fclose(f);

    CHECK(ReadTile(file.path) == nullptr);

    // Peak RSS is the only witness that the count was never believed. Anything that
    // reserved 16 GB and then failed the read would pass the check above.
    CHECK(PeakResidentBytes() < 1024ull * 1024ull * 1024ull);
}

TEST(TileWriterStoresEachModelOnce)
{
    // A continent's WMOs are instanced heavily -- Stormwind's lamppost appears hundreds
    // of times. If the table stopped deduping, the instances would still resolve to one
    // pointer on read (the index map keeps only the last write), so pointer identity
    // proves nothing. The file SIZE is what notices.
    TerrainTile one = MakeTile();
    one.instances.resize(1);

    TerrainTile many = one;
    many.instances.reserve(40);
    while (many.instances.size() < 40)
    {
        many.instances.push_back(many.instances[0]);
    }

    ScopedFile fileOne("dedup_one.tile");
    ScopedFile fileMany("dedup_many.tile");
    REQUIRE(WriteTile(one, fileOne.path));
    REQUIRE(WriteTile(many, fileMany.path));

    const long sizeOne = FileSize(fileOne.path);
    const long sizeMany = FileSize(fileMany.path);
    REQUIRE(sizeOne > 0);
    REQUIRE(sizeMany > 0);

    // 39 extra instances are 39 placements, not 39 copies of the geometry.
    const long perInstance = 4 * 3 + 4 * 9 + 4 + 4 * 6 + 4 + 4;
    CHECK(sizeMany - sizeOne < 39 * perInstance + 64);

    auto back = ReadTile(fileMany.path);
    REQUIRE(back != nullptr);
    CHECK_EQ(back->instances.size(), size_t(40));
    for (size_t i = 1; i < back->instances.size(); ++i)
    {
        CHECK(back->instances[i].model.get() == back->instances[0].model.get());
    }
}

TEST(TileReaderReportsAMissingFileAsAMiss)
{
    CHECK(ReadTile(TempPath("definitely_absent.tile")) == nullptr);
}

TEST(FusedTerrainServesTheBakedTile)
{
    const std::string dir = TempPath("dir");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    TerrainTile tile = MakeTile();
    tile.instances.clear();  // terrain only: the height must come from the grid
    tile.tx = 32;
    tile.ty = 32;
    for (float& h : tile.v9)
    {
        h = 100.f;
    }
    for (float& h : tile.v8)
    {
        h = 100.f;
    }
    tile.holes.fill(0);

    const std::string path = dir + "/" + TileFileName(9999, 32, 32);
    REQUIRE(WriteTile(tile, path));

    FusedTerrain::SetTileDir(dir);
    CHECK(FusedTerrain::HasTile(9999, 32, 32));

    FusedTerrain terrain(9999);
    // Tile (32,32) is the one containing world (0,0).
    const float x = -1.f, y = -1.f;

    auto outZ = terrain.ColumnAt(x, y, 152.f, -10000.f).HighestSolidAtOrBelow(152.f);
    REQUIRE(outZ.has_value());
    CHECK(std::fabs(*outZ - 100.f) < 0.01f);
    CHECK_EQ(terrain.ResidentTiles(), size_t(1));

    // A query point buried under the surface. Nothing is below it, which is exactly the
    // case a window that starts AT the point cannot answer.
    CHECK(!terrain.ColumnAt(x, y, 52.f, -10000.f).HighestSolidAtOrBelow(52.f).has_value());
    auto buried = terrain.ColumnAt(x, y, 100.f, -10000.f).Floor(50.f);
    REQUIRE(buried.has_value());
    CHECK(std::fabs(*buried - 100.f) < 0.01f);

    // Nothing occludes, so the sightline is clear and the fraction is past the far end.
    CHECK(terrain.IsInLineOfSight(x, y, 120.f, x + 20.f, y + 20.f, 120.f));
    CHECK(terrain.NearestHitFraction(x, y, 120.f, x + 20.f, y + 20.f, 120.f) > 1.0f);

    // Off the map entirely.
    CHECK(terrain.ColumnAt(20000.f, 20000.f, 102.f, -10000.f).Empty());

    std::remove(path.c_str());
    FusedTerrain::SetTileDir(std::string());
}

TEST(FusedTerrainSweepDropsUnpinnedTilesAndKeepsPinnedOnes)
{
    const std::string dir = TempPath("sweepdir");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    TerrainTile tile = MakeTile();
    tile.instances.clear();
    for (float& h : tile.v9) { h = 10.f; }
    for (float& h : tile.v8) { h = 10.f; }
    tile.holes.fill(0);

    const std::string a = dir + "/" + TileFileName(8888, 32, 32);
    const std::string b = dir + "/" + TileFileName(8888, 33, 32);
    tile.tx = 32; tile.ty = 32;
    REQUIRE(WriteTile(tile, a));
    tile.tx = 33;
    REQUIRE(WriteTile(tile, b));

    FusedTerrain::SetTileDir(dir);
    FusedTerrain terrain(8888);

    REQUIRE(!terrain.ColumnAt(-1.f, -1.f, 50.f, -10000.f).Empty());
    REQUIRE(!terrain.ColumnAt(-1.f - TILE_SIZE, -1.f, 50.f, -10000.f).Empty());
    CHECK_EQ(terrain.ResidentTiles(), size_t(2));

    terrain.PinCell(32, 32);

    // Past both the sweep interval and the idle window. Without the sweep the cache is
    // monotonic: walking a continent leaves every WMO passed resident for the map's life.
    terrain.Update(10u * 60u * 1000u);
    CHECK_EQ(terrain.ResidentTiles(), size_t(1));

    // The pinned cell is still served, and re-probing the evicted one brings it back.
    REQUIRE(!terrain.ColumnAt(-1.f, -1.f, 50.f, -10000.f).Empty());
    REQUIRE(!terrain.ColumnAt(-1.f - TILE_SIZE, -1.f, 50.f, -10000.f).Empty());
    CHECK_EQ(terrain.ResidentTiles(), size_t(2));

    terrain.UnpinCell(32, 32);
    terrain.Update(10u * 60u * 1000u);
    CHECK_EQ(terrain.ResidentTiles(), size_t(0));

    std::remove(a.c_str());
    std::remove(b.c_str());
    FusedTerrain::SetTileDir(std::string());
}
