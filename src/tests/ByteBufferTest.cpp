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

#include "TestHarness.h"

#include "Utilities/ByteBuffer.h"

/**
 * @file
 * @brief ByteBuffer edge cases, chiefly the empty buffer.
 *
 * contents() used to return &_storage[0]. Indexing element zero of an empty
 * vector is undefined behaviour: it is not merely "returns null" -- a debug STL
 * asserts, and a release build silently hands back whatever operator[] computes.
 * It was reachable in ordinary traffic, because plenty of packets are a bare
 * opcode with no payload at all.
 */

TEST(ByteBuffer_contents_on_an_empty_buffer_is_defined)
{
    ByteBuffer buf;

    CHECK_EQ(int(buf.size()), 0);
    CHECK(buf.empty());

    // The call is what is under test: operator[](0) on an empty vector is UB,
    // data() is not. Nothing can be asserted about the value -- null and
    // non-dereferenceable are both legal -- so the evidence is that this
    // completes at all, ideally under a debug STL or ASan.
    const uint8* first  = buf.contents();
    const uint8* second = buf.contents();
    CHECK(first == second);
}

TEST(ByteBuffer_contents_points_at_the_first_byte_when_non_empty)
{
    ByteBuffer buf;
    buf << uint8(0x41);
    buf << uint8(0x42);

    CHECK_EQ(int(buf.size()), 2);

    const uint8* p = buf.contents();
    REQUIRE(p != nullptr);
    CHECK_EQ(int(p[0]), 0x41);
    CHECK_EQ(int(p[1]), 0x42);
}

TEST(ByteBuffer_round_trips_integers)
{
    ByteBuffer buf;
    buf << uint8(0x12);
    buf << uint16(0x3456);
    buf << uint32(0x789ABCDE);

    uint8  a = 0;
    uint16 b = 0;
    uint32 c = 0;
    buf >> a >> b >> c;

    CHECK_EQ(int(a), 0x12);
    CHECK_EQ(int(b), 0x3456);
    CHECK_EQ(uint32(c), uint32(0x789ABCDE));
}

TEST(ByteBuffer_reading_past_the_end_throws)
{
    ByteBuffer buf;
    buf << uint8(1);

    uint8  a = 0;
    uint32 tooBig = 0;
    buf >> a;

    bool threw = false;
    try
    {
        buf >> tooBig;
    }
    catch (const ByteBufferException&)
    {
        threw = true;
    }

    CHECK(threw);
}

TEST(ByteBuffer_clear_resets_to_empty)
{
    ByteBuffer buf;
    buf << uint32(0xFFFFFFFF);
    CHECK_EQ(int(buf.size()), 4);

    buf.clear();

    CHECK_EQ(int(buf.size()), 0);
    CHECK(buf.empty());
    CHECK_EQ(int(buf.rpos()), 0);
    CHECK_EQ(int(buf.wpos()), 0);
}
