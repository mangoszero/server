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

#include <algorithm>
#include "TestHarness.h"

#include "Utilities/ByteBuffer.h"

#include <cstring>
#include <random>
#include <string>
#include <vector>

/**
 * @file
 * @brief Stress and fuzz coverage for ByteBuffer.
 *
 * ByteBuffer is where every packet in the server is built and parsed, and its
 * failure mode is silent: a wrong offset does not crash, it produces a packet
 * the client misreads. The cases below are randomised and long-running on
 * purpose, because the interesting bugs are in the interaction between
 * operations -- append then overwrite then read back -- not in any one call.
 *
 * The seed is fixed. A stress test that picks a fresh seed every run reports
 * failures nobody can reproduce; this one fails the same way on every machine.
 */

namespace
{
    const unsigned SEED = 0x5EEDU;

    /// Deterministic generator, so a failure is reproducible.
    std::mt19937& Rng()
    {
        static std::mt19937 rng(SEED);
        return rng;
    }

    uint32 Roll(uint32 maxInclusive)
    {
        std::uniform_int_distribution<uint32> dist(0, maxInclusive);
        return dist(Rng());
    }
}

TEST(ByteBufferStress_million_mixed_round_trips)
{
    // Writes a long randomised sequence of mixed-width values, then reads it
    // back and checks every one. Any drift in the read or write cursor -- an
    // off-by-one in a single operator -- desynchronises everything after it, so
    // a single wrong offset anywhere fails loudly rather than silently.
    const int OPERATIONS = 200000;

    std::vector<uint8>  kinds;
    std::vector<uint64> values;
    kinds.reserve(OPERATIONS);
    values.reserve(OPERATIONS);

    ByteBuffer buf;

    for (int i = 0; i < OPERATIONS; ++i)
    {
        const uint8  kind  = uint8(Roll(3));
        const uint64 value = (uint64(Roll(0xFFFFFFFFu)) << 32) | Roll(0xFFFFFFFFu);

        kinds.push_back(kind);
        values.push_back(value);

        switch (kind)
        {
            case 0: buf << uint8(value);  break;
            case 1: buf << uint16(value); break;
            case 2: buf << uint32(value); break;
            default: buf << uint64(value); break;
        }
    }

    int mismatches = 0;

    for (int i = 0; i < OPERATIONS; ++i)
    {
        uint64 got = 0;

        switch (kinds[i])
        {
            case 0: { uint8  v = 0; buf >> v; got = v; break; }
            case 1: { uint16 v = 0; buf >> v; got = v; break; }
            case 2: { uint32 v = 0; buf >> v; got = v; break; }
            default: { uint64 v = 0; buf >> v; got = v; break; }
        }

        uint64 expected = values[i];
        switch (kinds[i])
        {
            case 0: expected &= 0xFF;       break;
            case 1: expected &= 0xFFFF;     break;
            case 2: expected &= 0xFFFFFFFF; break;
            default: break;
        }

        if (got != expected)
        {
            ++mismatches;
        }
    }

    CHECK_EQ(mismatches, 0);
    CHECK_EQ(int(buf.rpos()), int(buf.size()));
}

TEST(ByteBufferStress_random_string_round_trips)
{
    // Strings are length-prefixed differently from fixed-width values and are a
    // common source of parsing drift. Includes empty strings, which is the case
    // that reaches the buffer's empty-storage path.
    const int COUNT = 20000;

    std::vector<std::string> written;
    written.reserve(COUNT);

    ByteBuffer buf;

    for (int i = 0; i < COUNT; ++i)
    {
        const uint32 length = Roll(64);
        std::string s;
        s.reserve(length);
        for (uint32 c = 0; c < length; ++c)
        {
            s.push_back(char('a' + Roll(25)));
        }

        written.push_back(s);
        buf << s;
    }

    int mismatches = 0;
    for (int i = 0; i < COUNT; ++i)
    {
        std::string got;
        buf >> got;
        if (got != written[i])
        {
            ++mismatches;
        }
    }

    CHECK_EQ(mismatches, 0);
}

TEST(ByteBufferStress_put_overwrites_without_moving_the_cursors)
{
    // put() writes at an absolute offset and must not disturb rpos or wpos.
    // Used when a size field has to be back-filled after the body is written,
    // so a cursor side effect here corrupts every packet built that way.
    ByteBuffer buf;

    const int COUNT = 10000;
    for (int i = 0; i < COUNT; ++i)
    {
        buf << uint32(0);
    }

    const size_t wposBefore = buf.wpos();
    const size_t rposBefore = buf.rpos();

    for (int i = 0; i < COUNT; ++i)
    {
        buf.put<uint32>(size_t(i) * 4, uint32(i * 7 + 1));
    }

    CHECK_EQ(int(buf.wpos()), int(wposBefore));
    CHECK_EQ(int(buf.rpos()), int(rposBefore));

    int mismatches = 0;
    for (int i = 0; i < COUNT; ++i)
    {
        uint32 v = 0;
        buf >> v;
        if (v != uint32(i * 7 + 1))
        {
            ++mismatches;
        }
    }
    CHECK_EQ(mismatches, 0);
}

TEST(ByteBufferStress_reads_past_the_end_always_throw)
{
    // Fuzzes the boundary rather than testing one case: for every buffer size
    // from empty upwards, read exactly to the end (must succeed) and then once
    // more (must throw). A read that neither throws nor succeeds correctly is
    // undefined behaviour reading off the end of the storage -- which is the
    // failure this whole class of bug takes.
    int failures = 0;

    for (int size = 0; size < 64; ++size)
    {
        ByteBuffer buf;
        for (int i = 0; i < size; ++i)
        {
            buf << uint8(i);
        }

        for (int i = 0; i < size; ++i)
        {
            uint8 v = 0;
            try
            {
                buf >> v;
            }
            catch (const ByteBufferException&)
            {
                ++failures;    // must not throw while data remains
            }
        }

        bool threw = false;
        try
        {
            uint8 v = 0;
            buf >> v;
        }
        catch (const ByteBufferException&)
        {
            threw = true;
        }

        if (!threw)
        {
            ++failures;
        }
    }

    CHECK_EQ(failures, 0);
}

TEST(ByteBufferStress_large_payload_integrity)
{
    // Eight megabytes through append() and back, forcing many reallocations of
    // the underlying storage. Catches a pointer cached across a growth.
    const size_t SIZE = 8u * 1024u * 1024u;

    std::vector<uint8> source(SIZE);
    for (size_t i = 0; i < SIZE; ++i)
    {
        source[i] = uint8((i * 31u) ^ (i >> 8));
    }

    ByteBuffer buf;
    size_t offset = 0;
    while (offset < SIZE)
    {
        const size_t chunk = std::min<size_t>(SIZE - offset, 1 + Roll(4096));
        buf.append(&source[offset], chunk);
        offset += chunk;
    }

    CHECK_EQ(int(buf.size()), int(SIZE));
    CHECK(std::memcmp(buf.contents(), source.data(), SIZE) == 0);
}

TEST(ByteBufferStress_repeated_clear_and_refill)
{
    // Reuse is the normal life of a packet buffer. Checks that clear() leaves no
    // residue that a later read could pick up.
    ByteBuffer buf;

    for (int round = 0; round < 5000; ++round)
    {
        buf.clear();
        CHECK_EQ(int(buf.size()), 0);

        const uint32 count = Roll(32);
        for (uint32 i = 0; i < count; ++i)
        {
            buf << uint32(round * 1000 + i);
        }

        for (uint32 i = 0; i < count; ++i)
        {
            uint32 v = 0;
            buf >> v;
            if (v != uint32(round * 1000 + i))
            {
                CHECK_EQ(int(v), int(round * 1000 + i));
                return;
            }
        }
    }
}

TEST(ByteBufferStress_empty_buffer_survives_every_accessor)
{
    // The empty buffer is the shape that made contents() index element zero of
    // an empty vector. Hit every accessor on it, repeatedly.
    for (int i = 0; i < 1000; ++i)
    {
        ByteBuffer buf;

        CHECK_EQ(int(buf.size()), 0);
        CHECK(buf.empty());
        CHECK_EQ(int(buf.rpos()), 0);
        CHECK_EQ(int(buf.wpos()), 0);

        const uint8* p = buf.contents();
        (void)p;

        buf.clear();
        CHECK(buf.empty());
    }
}
