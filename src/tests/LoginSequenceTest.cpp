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

#include "CharacterEnumMapSnapshot.h"
#include "LoginEffectPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

TEST(LoginEffectPackets_builds_success_result)
{
    WorldPacket packet = LoginEffectPackets::BuildCastResult();
    CHECK_EQ(int(packet.GetOpcode()), int(SMSG_CAST_FAILED));
    CHECK_HEX(packet.contents(), packet.size(), "4403000000");
}

TEST(LoginEffectPackets_builds_retail_start)
{
    WorldPacket packet = LoginEffectPackets::BuildStart(0x0000000001020304ULL);
    CHECK_EQ(int(packet.GetOpcode()), int(SMSG_SPELL_START));
    CHECK_HEX(packet.contents(), packet.size(),
        "0f040302010f04030201440300000200000000000000");
}

TEST(LoginEffectPackets_builds_retail_go)
{
    WorldPacket packet = LoginEffectPackets::BuildGo(0x0000000001020304ULL);
    CHECK_EQ(int(packet.GetOpcode()), int(SMSG_SPELL_GO));
    CHECK_HEX(packet.contents(), packet.size(),
        "0f040302010f0403020144030000000101040302010000000000020000");
}

TEST(CharacterEnumMapSnapshot_requires_matching_guid_and_map)
{
    CharacterEnumMapSnapshot snapshot;
    CHECK(!snapshot.Matches(0x11, 0));

    snapshot.Replace({{0x11, 0}, {0x22, 1}});
    CHECK(snapshot.Matches(0x11, 0));
    CHECK(!snapshot.Matches(0x11, 1));
    CHECK(!snapshot.Matches(0x33, 0));
}

TEST(CharacterEnumMapSnapshot_replaces_the_previous_response)
{
    CharacterEnumMapSnapshot snapshot;
    snapshot.Replace({{0x11, 0}});
    snapshot.Replace({{0x22, 1}});

    CHECK(!snapshot.Matches(0x11, 0));
    CHECK(snapshot.Matches(0x22, 1));
}
