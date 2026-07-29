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

#include <vector>
#include "TestHarness.h"

#include "AdtParser.hpp"
#include "M2Parser.hpp"
#include "WdtParser.hpp"
#include "WmoParser.hpp"

#include <cmath>
#include <cstring>

using namespace world::terrain;

namespace
{
    struct Blob
    {
        std::vector<uint8_t> b;

        void U8(uint8_t v) { b.push_back(v); }
        void U16(uint16_t v) { b.push_back(uint8_t(v & 0xFF)); b.push_back(uint8_t(v >> 8)); }
        void U32(uint32_t v) { U16(uint16_t(v & 0xFFFF)); U16(uint16_t(v >> 16)); }
        void F32(float f) { uint32_t u; std::memcpy(&u, &f, 4); U32(u); }
        void Pad(size_t n) { b.insert(b.end(), n, uint8_t(0)); }
        void Append(const Blob& o) { b.insert(b.end(), o.b.begin(), o.b.end()); }
        size_t Size() const { return b.size(); }

        void PatchU32(size_t at, uint32_t v)
        {
            b[at + 0] = uint8_t(v & 0xFF);
            b[at + 1] = uint8_t((v >> 8) & 0xFF);
            b[at + 2] = uint8_t((v >> 16) & 0xFF);
            b[at + 3] = uint8_t(v >> 24);
        }

        void PatchF32(size_t at, float f)
        {
            uint32_t u;
            std::memcpy(&u, &f, 4);
            PatchU32(at, u);
        }
    };

    void PutChunk(Blob& out, const char* tag, const Blob& body)
    {
        out.U8(uint8_t(tag[3]));
        out.U8(uint8_t(tag[2]));
        out.U8(uint8_t(tag[1]));
        out.U8(uint8_t(tag[0]));
        out.U32(uint32_t(body.Size()));
        out.Append(body);
    }

    // An MCNK whose MCVT holds `mcvt[145]`, laid out the way the client does.
    Blob MakeMcnk(uint32_t ix, uint32_t iy, float baseZ, uint16_t holes, uint16_t areaId,
                  const float mcvt[145], const Blob* mclq = nullptr, uint32_t flags = 0)
    {
        Blob hdr;
        hdr.Pad(128);
        hdr.PatchU32(0x00, flags);
        hdr.PatchU32(0x04, ix);
        hdr.PatchU32(0x08, iy);
        hdr.PatchU32(0x14, 8 + 128);  // ofsMCVT, measured from the MCNK tag
        hdr.PatchU32(0x34, areaId);
        hdr.PatchU32(0x3C, holes);
        hdr.PatchF32(0x70, baseZ);

        Blob heights;
        for (int i = 0; i < 145; ++i)
        {
            heights.F32(mcvt[i]);
        }

        Blob body = hdr;
        PutChunk(body, "MCVT", heights);

        if (mclq)
        {
            hdr.PatchU32(0x60, uint32_t(8 + body.Size()));  // ofsMCLQ from the MCNK tag
            hdr.PatchU32(0x64, uint32_t(mclq->Size() + 8));
            Blob rebuilt = hdr;
            PutChunk(rebuilt, "MCVT", heights);
            PutChunk(rebuilt, "MCLQ", *mclq);
            return rebuilt;
        }
        return body;
    }

    Blob MakeMclq(float surfaceZ, const uint8_t cellFlags[64])
    {
        Blob lq;
        lq.F32(surfaceZ);
        lq.F32(surfaceZ);
        for (int i = 0; i < 81; ++i)
        {
            lq.U32(0);
            lq.F32(surfaceZ);
        }
        for (int i = 0; i < 64; ++i)
        {
            lq.U8(cellFlags[i]);
        }
        return lq;
    }

    struct Mh2oLayer
    {
        uint16_t entry = 13;
        uint16_t lvf = 0;
        float minHeight = 10.f;
        uint8_t xOfs = 0, yOfs = 0, w = 8, h = 8;
        bool withExists = false;
        uint64_t existsBits = ~uint64_t(0);
        bool withHeights = false;
        float heightBias = 0.f;
    };

    // One MH2O chunk carrying a single layer on chunk (iy,ix).
    Blob MakeMh2o(int ix, int iy, const Mh2oLayer& l)
    {
        constexpr uint32_t TABLE = 12 * 256;
        Blob body;
        body.Pad(TABLE);

        const uint32_t instOfs = TABLE;
        Blob tail;
        tail.U16(l.entry);
        tail.U16(l.lvf);
        tail.F32(l.minHeight);
        tail.F32(l.minHeight);
        tail.U8(l.xOfs);
        tail.U8(l.yOfs);
        tail.U8(l.w);
        tail.U8(l.h);

        const uint32_t afterInstance = instOfs + 24;
        uint32_t existsOfs = 0;
        uint32_t vertsOfs = 0;
        uint32_t cursor = afterInstance;
        if (l.withExists)
        {
            existsOfs = cursor;
            cursor += 8;
        }
        if (l.withHeights)
        {
            vertsOfs = cursor;
        }
        tail.U32(existsOfs);
        tail.U32(vertsOfs);

        if (l.withExists)
        {
            for (int i = 0; i < 8; ++i)
            {
                tail.U8(uint8_t((l.existsBits >> (i * 8)) & 0xFF));
            }
        }
        if (l.withHeights)
        {
            for (int y = 0; y <= l.h; ++y)
            {
                for (int x = 0; x <= l.w; ++x)
                {
                    tail.F32(l.heightBias + float(y * (l.w + 1) + x));
                }
            }
        }

        body.Append(tail);
        body.PatchU32(size_t(iy * 16 + ix) * 12 + 0, instOfs);
        body.PatchU32(size_t(iy * 16 + ix) * 12 + 4, 1);
        return body;
    }

    void FillRamp(float mcvt[145], float base)
    {
        for (int i = 0; i < 145; ++i)
        {
            mcvt[i] = base + float(i);
        }
    }

    Blob MakeMogpGroup(uint32_t mogpFlags, uint32_t groupLiquid, uint32_t uniqueId,
                       const Blob& nested)
    {
        Blob hdr;
        hdr.Pad(68);
        hdr.PatchU32(0x08, mogpFlags);
        hdr.PatchU32(0x34, groupLiquid);
        hdr.PatchU32(0x38, uniqueId);
        hdr.Append(nested);

        Blob file;
        Blob ver;
        ver.U32(17);
        PutChunk(file, "MVER", ver);
        PutChunk(file, "MOGP", hdr);
        return file;
    }

    Blob MakeMliq(uint32_t xtiles, uint32_t ytiles, float z, uint8_t tileFlag)
    {
        const uint32_t xverts = xtiles + 1, yverts = ytiles + 1;
        Blob m;
        m.U32(xverts);
        m.U32(yverts);
        m.U32(xtiles);
        m.U32(ytiles);
        m.F32(0.f);
        m.F32(0.f);
        m.F32(0.f);
        m.U16(0xBEEF);  // materialId, never the liquid type
        for (uint32_t i = 0; i < xverts * yverts; ++i)
        {
            m.U32(0);
            m.F32(z);
        }
        for (uint32_t i = 0; i < xtiles * ytiles; ++i)
        {
            m.U8(tileFlag);
        }
        return m;
    }
}

TEST(AdtHeightGridIsNotTransposed)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(/*ix*/ 1, /*iy*/ 2, /*baseZ*/ 100.f, 0, 0, mcvt));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    REQUIRE(d.hasTerrain);

    // MCNK IndexY drives the FIRST index; IndexX the second. Swapping them puts the
    // whole map at 90 degrees and every height query lands on the wrong chunk.
    CHECK_EQ(d.v9[(2 * 8 + 0) * ADT_V9 + (1 * 8 + 0)], 100.f + mcvt[0]);
    CHECK_EQ(d.v9[(2 * 8 + 3) * ADT_V9 + (1 * 8 + 5)], 100.f + mcvt[3 * 17 + 5]);
    CHECK_EQ(d.v8[(2 * 8 + 3) * ADT_GRID + (1 * 8 + 5)], 100.f + mcvt[3 * 17 + 9 + 5]);
}

TEST(AdtHolesAndAreaId)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(3, 4, 0.f, 0x0021, 1519, mcvt));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    CHECK_EQ(d.holes[4 * 16 + 3], uint16_t(0x0021));
    CHECK_EQ(d.areaIds[4 * 16 + 3], uint16_t(1519));
    CHECK_EQ(d.holes[3 * 16 + 4], uint16_t(0));
}

TEST(AdtMh2oFlatLayerFillsWholeChunk)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Mh2oLayer l;
    l.entry = 13;
    l.minHeight = 42.5f;

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(1, 1, 0.f, 0, 0, mcvt));
    PutChunk(adt, "MH2O", MakeMh2o(1, 1, l));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    REQUIRE(d.hasLiquid);
    CHECK(d.hasMh2o);

    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            const size_t cell = size_t(8 + y) * ADT_GRID + (8 + x);
            CHECK_EQ(d.liquidShow[cell], uint8_t(1));
            CHECK_EQ(d.liquidEntry[cell], uint16_t(13));
        }
    }
    CHECK_EQ(d.liquidHeight[size_t(8) * ADT_V9 + 8], 42.5f);
    CHECK_EQ(d.liquidShow[0], uint8_t(0));
}

TEST(AdtMh2oHonoursSubRectAndExistsBitmap)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Mh2oLayer l;
    l.xOfs = 2;
    l.yOfs = 3;
    l.w = 2;
    l.h = 2;
    l.withExists = true;
    l.existsBits = 0x1;  // only cell (y=0,x=0) of the sub-rect exists

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(0, 0, 0.f, 0, 0, mcvt));
    PutChunk(adt, "MH2O", MakeMh2o(0, 0, l));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    REQUIRE(d.hasLiquid);

    CHECK_EQ(d.liquidShow[size_t(3) * ADT_GRID + 2], uint8_t(1));
    CHECK_EQ(d.liquidShow[size_t(3) * ADT_GRID + 3], uint8_t(0));
    CHECK_EQ(d.liquidShow[size_t(4) * ADT_GRID + 2], uint8_t(0));
    CHECK_EQ(d.liquidShow[size_t(0) * ADT_GRID + 0], uint8_t(0));
}

TEST(AdtMh2oReadsPerVertexHeights)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Mh2oLayer l;
    l.w = 2;
    l.h = 2;
    l.withHeights = true;
    l.heightBias = 1000.f;

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(0, 0, 0.f, 0, 0, mcvt));
    PutChunk(adt, "MH2O", MakeMh2o(0, 0, l));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    REQUIRE(d.hasLiquid);

    CHECK_EQ(d.liquidHeight[size_t(0) * ADT_V9 + 0], 1000.f);
    CHECK_EQ(d.liquidHeight[size_t(1) * ADT_V9 + 2], 1000.f + float(1 * 3 + 2));
}

TEST(AdtMh2oDepthOnlyLayerUsesMinHeight)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    // Vertex format 2 carries no height array at all; reading one anyway lands on the
    // depth bytes and produces denormal surface heights.
    Mh2oLayer l;
    l.lvf = 2;
    l.w = 1;
    l.h = 1;
    l.minHeight = -7.25f;
    l.withHeights = true;

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(0, 0, 0.f, 0, 0, mcvt));
    PutChunk(adt, "MH2O", MakeMh2o(0, 0, l));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    CHECK_EQ(d.liquidHeight[0], -7.25f);
    CHECK_EQ(d.liquidNoLight[0], uint8_t(0));
}

TEST(AdtMh2oMarksTextureCoordLayersAsUnlit)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Mh2oLayer l;
    l.lvf = 1;
    l.w = 1;
    l.h = 1;
    l.withHeights = true;

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(0, 0, 0.f, 0, 0, mcvt));
    PutChunk(adt, "MH2O", MakeMh2o(0, 0, l));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    CHECK_EQ(d.liquidNoLight[0], uint8_t(1));
}

TEST(AdtMh2oRejectsOutOfRangeInstance)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Blob body;
    body.Pad(12 * 256);
    body.PatchU32(0, 0xF0000000);  // instance offset far past the chunk
    body.PatchU32(4, 1);

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(0, 0, 0.f, 0, 0, mcvt));
    PutChunk(adt, "MH2O", body);

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    CHECK(!d.hasLiquid);
}

TEST(AdtMh2oRejectsSubRectLeavingTheChunk)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    Mh2oLayer l;
    l.xOfs = 6;
    l.w = 8;  // 6 + 8 > 8

    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(0, 0, 0.f, 0, 0, mcvt));
    PutChunk(adt, "MH2O", MakeMh2o(0, 0, l));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    CHECK(!d.hasLiquid);
}

TEST(AdtMclqFallbackTypesAndDarkWater)
{
    float mcvt[145];
    FillRamp(mcvt, 0.f);

    uint8_t cells[64];
    for (int i = 0; i < 64; ++i)
    {
        cells[i] = 0x0F;
    }
    cells[0] = 0x00;
    cells[9] = 0x80;  // dark water

    Blob mclq = MakeMclq(55.f, cells);
    Blob adt;
    PutChunk(adt, "MCNK", MakeMcnk(0, 0, 0.f, 0, 0, mcvt, &mclq, 1u << 5));

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    REQUIRE(d.hasLiquid);
    CHECK(!d.hasMh2o);

    CHECK_EQ(d.liquidShow[0], uint8_t(1));
    CHECK_EQ(d.liquidEntry[0], uint16_t(4));  // slime, not water
    CHECK_EQ(d.liquidDark[0], uint8_t(0));
    CHECK_EQ(d.liquidShow[size_t(1) * ADT_GRID + 1], uint8_t(1));
    CHECK_EQ(d.liquidDark[size_t(1) * ADT_GRID + 1], uint8_t(1));
    CHECK_EQ(d.liquidShow[size_t(0) * ADT_GRID + 1], uint8_t(0));
    CHECK_EQ(d.liquidHeight[0], 55.f);
}

TEST(AdtStopsOnTruncatedChunk)
{
    Blob adt;
    adt.U8('K'); adt.U8('N'); adt.U8('C'); adt.U8('M');
    adt.U32(0x7FFFFFFF);
    adt.Pad(16);

    AdtData d;
    REQUIRE(ParseAdt(adt.b, d));
    CHECK(!d.hasTerrain);
}

TEST(WdtGridAndGlobalWmo)
{
    Blob mphd;
    mphd.U32(0x0001);
    mphd.Pad(28);

    Blob main;
    for (int i = 0; i < 64 * 64; ++i)
    {
        main.U32((i == 32 * 64 + 48) ? 1u : 0u);
        main.U32(0);
    }

    Blob mwmo;
    const char* name = "World\\wmo\\Dungeon\\Test.wmo";
    mwmo.b.insert(mwmo.b.end(), name, name + std::strlen(name) + 1);

    Blob modf;
    modf.Pad(64);
    modf.PatchF32(8, 17.f);
    modf.PatchF32(12, 18.f);
    modf.PatchF32(16, 19.f);

    Blob wdt;
    PutChunk(wdt, "MPHD", mphd);
    PutChunk(wdt, "MAIN", main);
    PutChunk(wdt, "MWMO", mwmo);
    PutChunk(wdt, "MODF", modf);

    WdtData d;
    REQUIRE(ParseWdt(wdt.b, d));
    CHECK(d.hasMainChunk);
    CHECK(d.hasGlobalWmo);
    // MAIN entry 32*64+48 is tx=32, ty=48. Reading the grid transposed still finds a
    // plausible-looking tile on most maps, and simply bakes the wrong half of them.
    CHECK(d.HasAdt(32, 48));
    CHECK(!d.HasAdt(48, 32));
    CHECK(!d.HasAdt(-1, 0));
    CHECK(!d.HasAdt(0, 64));
    CHECK(d.globalWmoName == name);
    REQUIRE(d.globalWmoPlacement.has_value());
    CHECK_EQ(d.globalWmoPlacement->pos.y, 18.f);
}

namespace
{
    // A model whose real bounding block sits at `block`; the other candidate offset is
    // filled with counts that would produce a different, wrong hull.
    Blob MakeM2(uint32_t version, size_t block, size_t decoy)
    {
        Blob m;
        m.b.insert(m.b.end(), {'M', 'D', '2', '0'});
        m.U32(version);
        m.Pad(400 - 8);

        const uint32_t ofsVerts = 400;
        const uint32_t ofsTris = ofsVerts + 3 * 12;

        m.PatchU32(block + 0, 3);          // index count
        m.PatchU32(block + 4, ofsTris);
        m.PatchU32(block + 8, 3);          // vertex count
        m.PatchU32(block + 12, ofsVerts);

        m.PatchU32(decoy + 0, 3);
        m.PatchU32(decoy + 4, 0);
        m.PatchU32(decoy + 8, 1);
        m.PatchU32(decoy + 12, 0);

        for (int i = 0; i < 3; ++i)
        {
            m.F32(float(i));
            m.F32(float(10 + i));
            m.F32(float(20 + i));
        }
        m.U16(0);
        m.U16(1);
        m.U16(2);
        return m;
    }
}

TEST(M2WotlkUsesTheShorterHeader)
{
    Blob m = MakeM2(264, 216, 236);

    M2Data d;
    REQUIRE(ParseM2(m.b, d));
    REQUIRE(d.verts.size() == 3);
    REQUIRE(d.tris.size() == 1);
    CHECK_EQ(d.verts[1].x, 1.f);
    CHECK_EQ(d.verts[1].y, -11.f);  // M2 Y is negated into model space
    CHECK_EQ(d.verts[1].z, 21.f);
}

TEST(M2LegacyUsesTheLongerHeader)
{
    Blob m = MakeM2(260, 236, 216);

    M2Data d;
    REQUIRE(ParseM2(m.b, d));
    CHECK(d.verts.size() == 3);
    CHECK(d.tris.size() == 1);
}

TEST(M2RejectsHostileCounts)
{
    Blob m;
    m.b.insert(m.b.end(), {'M', 'D', '2', '0'});
    m.U32(264);
    m.Pad(400 - 8);
    m.PatchU32(216 + 0, 0xFFFFFFFF);
    m.PatchU32(216 + 4, 0);
    m.PatchU32(216 + 8, 0xFFFFFFFF);
    m.PatchU32(216 + 12, 0);

    M2Data d;
    REQUIRE(ParseM2(m.b, d));
    CHECK(d.verts.empty());
    CHECK(d.tris.empty());
}

TEST(M2RejectsNonModelBytes)
{
    Blob m;
    m.b.insert(m.b.end(), {'R', 'E', 'V', 'M'});
    m.Pad(400);

    M2Data d;
    CHECK(!ParseM2(m.b, d));
}

TEST(WmoGroupKeepsDetailFacesThatAlsoCollide)
{
    Blob movt;
    for (int i = 0; i < 4; ++i)
    {
        movt.F32(float(i));
        movt.F32(0.f);
        movt.F32(0.f);
    }

    Blob movi;
    for (int t = 0; t < 3; ++t)
    {
        movi.U16(0);
        movi.U16(1);
        movi.U16(2);
    }

    Blob mopy;
    mopy.U8(0x20); mopy.U8(0);  // render, collides
    mopy.U8(0x0C); mopy.U8(0);  // detail + collision, still collides
    mopy.U8(0x24); mopy.U8(0);  // render + detail, decoration only

    Blob nested;
    PutChunk(nested, "MOPY", mopy);
    PutChunk(nested, "MOVI", movi);
    PutChunk(nested, "MOVT", movt);

    Blob group = MakeMogpGroup(0, 15, 4242, nested);

    WmoGroupData g;
    REQUIRE(ParseWmoGroup(group.b, 0, g));
    CHECK_EQ(g.groupWmoId, uint32_t(4242));
    CHECK_EQ(g.verts.size(), size_t(4));
    CHECK_EQ(g.tris.size(), size_t(2));
}

TEST(WmoGroupLiquidUsesWotlkRows)
{
    struct Expect
    {
        uint32_t groupLiquid;
        uint32_t mogpFlags;
        uint16_t entry;
    };
    // Legacy codes 0..3 are water/ocean/magma/slime; 3.3.5a's rows for them are
    // 13/14/19/20, not the 1..4 that 2.4.3 uses.
    const Expect cases[] = {
        {0, 0, 13},
        {0, 0x80000, 14},
        {1, 0, 14},
        {2, 0, 19},
        {3, 0, 20},
    };

    for (const Expect& e : cases)
    {
        Blob nested;
        PutChunk(nested, "MLIQ", MakeMliq(2, 2, 33.f, 0));
        Blob group = MakeMogpGroup(e.mogpFlags, e.groupLiquid, 0, nested);

        WmoGroupData g;
        REQUIRE(ParseWmoGroup(group.b, 0, g));
        REQUIRE(g.hasLiquid);
        CHECK_EQ(g.liquid.entry, e.entry);
        CHECK_EQ(g.liquid.tilesX, uint32_t(2));
        CHECK_EQ(g.liquid.heights.size(), size_t(9));
        CHECK_EQ(g.liquid.heights[0], 33.f);
    }
}

TEST(WmoGroupLiquidTakesRawDbcIdWhenRootSaysSo)
{
    Blob nested;
    PutChunk(nested, "MLIQ", MakeMliq(1, 1, 5.f, 0));
    Blob group = MakeMogpGroup(0, 41, 0, nested);

    WmoGroupData g;
    REQUIRE(ParseWmoGroup(group.b, 0x4, g));
    REQUIRE(g.hasLiquid);
    CHECK_EQ(g.liquid.entry, uint16_t(41));
}

TEST(WmoGroupLiquidFallsBackToTileNibble)
{
    Blob nested;
    PutChunk(nested, "MLIQ", MakeMliq(1, 1, 5.f, 0x02));  // nibble 2 -> code 3 -> magma
    Blob group = MakeMogpGroup(0, 15, 0, nested);

    WmoGroupData g;
    REQUIRE(ParseWmoGroup(group.b, 0, g));
    REQUIRE(g.hasLiquid);
    CHECK_EQ(g.liquid.entry, uint16_t(19));
}

TEST(WmoGroupWithoutGeometryOrLiquidIsRejected)
{
    Blob nested;
    Blob group = MakeMogpGroup(0, 15, 0, nested);

    WmoGroupData g;
    CHECK(!ParseWmoGroup(group.b, 0, g));
}

TEST(WmoRootReadsHeaderAndDoodadNameOffsets)
{
    Blob mohd;
    mohd.Pad(64);
    mohd.PatchU32(0x04, 7);     // nGroups
    mohd.PatchU32(0x20, 1234);  // wmoID
    mohd.PatchU32(0x3C, 0x4);   // flags

    Blob modn;
    const char* a = "first.mdx";
    const char* b = "second.mdx";
    modn.b.insert(modn.b.end(), a, a + std::strlen(a) + 1);
    const uint32_t secondOfs = uint32_t(modn.Size());
    modn.b.insert(modn.b.end(), b, b + std::strlen(b) + 1);

    Blob mods;
    mods.Pad(32);
    mods.PatchU32(20, 0);
    mods.PatchU32(24, 2);

    Blob modd;
    modd.Pad(80);
    modd.PatchU32(0, 0xAB000000u | secondOfs);  // high byte is flags, low 24 the offset
    modd.PatchF32(32, 2.5f);
    modd.PatchU32(40 + 0, 0);
    modd.PatchF32(40 + 32, 0.f);

    Blob root;
    PutChunk(root, "MOHD", mohd);
    PutChunk(root, "MODS", mods);
    PutChunk(root, "MODN", modn);
    PutChunk(root, "MODD", modd);

    WmoRootData r;
    REQUIRE(ParseWmoRoot(root.b, r));
    CHECK_EQ(r.nGroups, uint32_t(7));
    CHECK_EQ(r.wmoId, uint32_t(1234));
    CHECK_EQ(r.flags, uint32_t(0x4));
    REQUIRE(r.doodads.size() == 2);
    CHECK(r.doodads[0].name == b);
    CHECK_EQ(r.doodads[0].scale, 2.5f);
    CHECK(r.doodads[1].name == a);
    CHECK_EQ(r.doodads[1].scale, 1.f);  // a zero scale is repaired, never kept
    REQUIRE(r.sets.size() == 1);
    CHECK_EQ(r.sets[0].count, uint32_t(2));
}

TEST(WmoGroupPathNaming)
{
    CHECK(WmoGroupPath("World\\wmo\\X.wmo", 3) == "World\\wmo\\X_003.wmo");
    CHECK(WmoGroupPath("X", 12) == "X_012.wmo");
}

TEST(M2PathRewritesModelExtension)
{
    CHECK(M2PathOf("a\\b.MDX") == "a\\b.m2");
    CHECK(M2PathOf("a\\b.mdl") == "a\\b.m2");
    CHECK(M2PathOf("a\\b.m2") == "a\\b.m2");
}
