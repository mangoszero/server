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
#include "InitialWorldEntry.h"
#include "LoginEffectPackets.h"
#include "Opcodes.h"
#include "../game/Object/SocialMgr.h"
#include "WorldPacket.h"

#include <type_traits>
#include <utility>

namespace
{
    // This compile-time seam protects pre-registry login delivery without
    // constructing Player or linking the complete game library into the test.
    template <typename T, typename = void>
    struct HasExplicitLoginSocialRecipient : std::false_type
    {
    };

    template <typename T>
    struct HasExplicitLoginSocialRecipient<T, std::void_t<
        decltype(std::declval<T&>().SendFriendList(
            static_cast<Player*>(nullptr))),
        decltype(std::declval<T&>().SendIgnoreList(
            static_cast<Player*>(nullptr)))>> : std::true_type
    {
    };
}

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

TEST(LoginVerifyDelivery_restores_suppressed_verify_on_admission_fallback)
{
    LoginVerifyDeliveryState delivery;
    CHECK(!delivery.TakeInitial(true));
    CHECK(delivery.TakeAdmissionFallback());
    CHECK(!delivery.TakeAdmissionFallback());
}

TEST(LoginVerifyDelivery_does_not_duplicate_an_initial_verify_on_fallback)
{
    LoginVerifyDeliveryState delivery;
    CHECK(delivery.TakeInitial(false));
    CHECK(!delivery.TakeAdmissionFallback());
}

TEST(LoginSocialLists_accept_the_loaded_player_before_registry_insertion)
{
    CHECK(HasExplicitLoginSocialRecipient<PlayerSocial>::value);
}

TEST(InitialWorldEntry_orders_established_packets)
{
    std::vector<InitialWorldEntryPacket> order =
        InitialWorldEntryPacketOrder(false);
    REQUIRE(order.size() == 3);
    CHECK_EQ(int(order[0]), int(InitialWorldEntryPacket::InitWorldStates));
    CHECK_EQ(int(order[1]), int(InitialWorldEntryPacket::LoginEffectResult));
    CHECK_EQ(int(order[2]), int(InitialWorldEntryPacket::LoginTimeSpeed));
}

TEST(InitialWorldEntry_inserts_cinematic_packets_before_result)
{
    std::vector<InitialWorldEntryPacket> order =
        InitialWorldEntryPacketOrder(true);
    REQUIRE(order.size() == 5);
    CHECK_EQ(int(order[0]), int(InitialWorldEntryPacket::InitWorldStates));
    CHECK_EQ(int(order[1]), int(InitialWorldEntryPacket::TriggerCinematic));
    CHECK_EQ(int(order[2]), int(InitialWorldEntryPacket::ExplorationExperience));
    CHECK_EQ(int(order[3]), int(InitialWorldEntryPacket::LoginEffectResult));
    CHECK_EQ(int(order[4]), int(InitialWorldEntryPacket::LoginTimeSpeed));
}

TEST(LoginEffectSequence_has_two_ordered_phases_and_cancels_out_of_world)
{
    LoginEffectSequenceState sequence;
    std::optional<LoginEffectPhase> first = sequence.TakeNext(true);
    std::optional<LoginEffectPhase> second = sequence.TakeNext(true);
    std::optional<LoginEffectPhase> done = sequence.TakeNext(true);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    CHECK_EQ(int(*first), int(LoginEffectPhase::Start));
    CHECK_EQ(int(*second), int(LoginEffectPhase::Go));
    CHECK(!done.has_value());

    LoginEffectSequenceState cancelled;
    CHECK(!cancelled.TakeNext(false).has_value());
    CHECK(cancelled.IsComplete());

    LoginEffectSequenceState interrupted;
    REQUIRE(interrupted.TakeNext(true).has_value());
    CHECK(!interrupted.TakeNext(false).has_value());
    CHECK(interrupted.IsComplete());
}

TEST(LoginEffectTiming_delays_start_past_the_initial_loading_transition)
{
    CHECK_EQ(LoginEffectDelayBefore(LoginEffectPhase::Start), uint32(1000));
}

TEST(LoginEffectTiming_keeps_go_on_the_following_event_tick)
{
    CHECK_EQ(LoginEffectDelayBefore(LoginEffectPhase::Go), uint32(1));
}

TEST(LoginCinematicRootOwnership_releases_exactly_once)
{
    LoginCinematicRootOwnership ownership;
    CHECK(ownership.Claim());
    CHECK(!ownership.Claim());
    CHECK(ownership.ReleaseOnce(true));
    CHECK(!ownership.ReleaseOnce(true));
    CHECK(!ownership.IsOwned());

    CHECK(ownership.Claim());
    ownership.Clear();
    CHECK(!ownership.ReleaseOnce(true));
}

TEST(LoginCinematicRootOwnership_retains_ownership_until_release_is_actionable)
{
    LoginCinematicRootOwnership ownership;
    CHECK(ownership.Claim());
    CHECK(!ownership.ReleaseOnce(false));
    CHECK(ownership.IsOwned());
    CHECK(ownership.ReleaseOnce(true));
    CHECK(!ownership.IsOwned());
}
