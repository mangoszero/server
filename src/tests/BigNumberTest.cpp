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

#include "Auth/BigNumber.h"

/**
 * @file
 * @brief Regression tests for BigNumber::AsByteArray's zero padding.
 *
 * These exist because of a real defect: the old implementation called
 * BN_bn2bin, which writes the number's minimal big-endian encoding at offset 0,
 * and then reversed the whole buffer. When the value serialised shorter than the
 * requested length, the zero padding ended up on the wrong side and every byte of
 * the value was shifted.
 *
 * The reason it survived for years is that it needs a short serialisation to
 * show, and SRP6 values are effectively uniform: a 32-byte quantity has a
 * leading zero byte about one time in 256. The symptom was a login that failed
 * for no reason and then worked again -- indistinguishable from a flaky network.
 *
 * So these cases use values that are *deterministically* short. Revert
 * BigNumber.cpp to BN_bn2bin and both go red immediately; a test built on a
 * random value would pass 255 runs out of 256 and prove nothing.
 */

TEST(BigNumber_little_endian_pads_at_the_high_end)
{
    // 1 in a 4-byte little-endian buffer is 01 00 00 00.
    // The old code produced 00 00 00 01 -- the value shifted three bytes up.
    BigNumber n;
    n.SetDword(1);

    const uint8* le = n.AsByteArray(4);

    CHECK_EQ(int(le[0]), 1);
    CHECK_EQ(int(le[1]), 0);
    CHECK_EQ(int(le[2]), 0);
    CHECK_EQ(int(le[3]), 0);
}

TEST(BigNumber_big_endian_pads_at_the_low_end)
{
    // The same number big-endian is 00 00 00 01. The old code produced
    // 01 00 00 00, because it wrote at offset 0 and then did not reverse.
    BigNumber n;
    n.SetDword(1);

    const uint8* be = n.AsByteArray(4, false);

    CHECK_EQ(int(be[0]), 0);
    CHECK_EQ(int(be[1]), 0);
    CHECK_EQ(int(be[2]), 0);
    CHECK_EQ(int(be[3]), 1);
}

TEST(BigNumber_padding_preserves_a_multi_byte_value)
{
    // 0x0102 asked for in 4 bytes. Little-endian: 02 01 00 00.
    BigNumber n;
    n.SetDword(0x0102);

    const uint8* le = n.AsByteArray(4);

    CHECK_EQ(int(le[0]), 0x02);
    CHECK_EQ(int(le[1]), 0x01);
    CHECK_EQ(int(le[2]), 0x00);
    CHECK_EQ(int(le[3]), 0x00);
}

TEST(BigNumber_exact_length_needs_no_padding)
{
    // No padding path at all: the value already fills the buffer. Guards against
    // a "fix" that pads unconditionally.
    BigNumber n;
    n.SetDword(0x01020304);

    const uint8* le = n.AsByteArray(4);

    CHECK_EQ(int(le[0]), 0x04);
    CHECK_EQ(int(le[1]), 0x03);
    CHECK_EQ(int(le[2]), 0x02);
    CHECK_EQ(int(le[3]), 0x01);
}

TEST(BigNumber_session_key_sized_padding)
{
    // The shape that actually bit: a 40-byte session key whose top byte is zero.
    // Warden and the login proof both ask for exactly 40 bytes.
    BigNumber n;
    n.SetDword(0xAABBCCDD);

    const uint8* le = n.AsByteArray(40);

    CHECK_EQ(int(le[0]), 0xDD);
    CHECK_EQ(int(le[1]), 0xCC);
    CHECK_EQ(int(le[2]), 0xBB);
    CHECK_EQ(int(le[3]), 0xAA);

    bool restIsZero = true;
    for (int i = 4; i < 40; ++i)
    {
        if (le[i] != 0)
        {
            restIsZero = false;
        }
    }
    CHECK(restIsZero);
}
