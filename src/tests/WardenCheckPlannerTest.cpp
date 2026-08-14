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
 */

#include "TestHarness.h"

#include "WardenCheckCatalog.h"
#include "WardenCheckPlanner.h"
#include "WardenEvidence.h"

#include <variant>

namespace
{
warden::MpqCheckProfile TestMpqProfile()
{
    warden::MpqCheckProfile profile;
    profile.checkId = 1;
    profile.path = "DBFilesClient\\AreaTable.dbc";
    profile.expectedSha1 =
    {
        0x7D, 0x88, 0x15, 0x4D, 0x34, 0x11, 0x81, 0x19,
        0x85, 0xF5, 0xD8, 0x11, 0x77, 0xC5, 0x45, 0x32,
        0x48, 0x13, 0x34, 0x43
    };
    return profile;
}

warden::LuaCheckProfile TestLuaProfile()
{
    return {2, "OKAY", "Okay"};
}
}

TEST(WardenEvidence_timing_outcomes_have_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::TimingOutcome::Stable), "Stable");
    CHECK_STR(warden::ToString(warden::TimingOutcome::Unstable), "Unstable");
}

TEST(WardenEvidence_mpq_outcomes_have_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::MpqOutcome::Match), "Match");
    CHECK_STR(warden::ToString(warden::MpqOutcome::DigestMismatch),
        "DigestMismatch");
    CHECK_STR(warden::ToString(warden::MpqOutcome::Unavailable),
        "Unavailable");
}

TEST(WardenEvidence_lua_outcomes_have_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::LuaOutcome::Match), "Match");
    CHECK_STR(warden::ToString(warden::LuaOutcome::TextMismatch),
        "TextMismatch");
    CHECK_STR(warden::ToString(warden::LuaOutcome::Unavailable),
        "Unavailable");
}

TEST(WardenCheckPlanner_waits_one_cumulative_eligible_second_once)
{
    warden::WardenCheckPlanner planner;

    CHECK(!planner.Update(false, 5000).has_value());
    CHECK(!planner.Update(true, 600).has_value());
    CHECK(!planner.Update(true, 399).has_value());

    auto const plan = planner.Update(true, 1);
    REQUIRE(plan.has_value());
    CHECK_EQ(plan->requestId, uint32(1));
    REQUIRE(plan->checks.size() == 1u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));

    CHECK(!planner.Update(true, 60000).has_value());
    CHECK(!planner.Update(false, 60000).has_value());
}

TEST(WardenCheckPlanner_resets_partial_delay_when_ineligible)
{
    warden::WardenCheckPlanner planner;

    CHECK(!planner.Update(true, 900).has_value());
    CHECK(!planner.Update(false, 1).has_value());
    CHECK(!planner.Update(true, 999).has_value());

    auto const plan = planner.Update(true, 1);
    REQUIRE(plan.has_value());
    REQUIRE(plan->checks.size() == 1u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));
}

TEST(WardenCheckPlanner_emits_timing_then_exact_mpq_once)
{
    warden::WardenCheckPlanner planner(1000, TestMpqProfile());

    auto const plan = planner.Update(true, 1000);
    REQUIRE(plan.has_value());
    CHECK_EQ(plan->requestId, uint32(1));
    REQUIRE(plan->checks.size() == 2u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));
    REQUIRE(std::holds_alternative<warden::MpqCheckProfile>(
        plan->checks[1]));
    warden::MpqCheckProfile const& mpq =
        std::get<warden::MpqCheckProfile>(plan->checks[1]);
    CHECK_EQ(mpq.checkId, uint32(1));
    CHECK_STR(mpq.path.c_str(), "DBFilesClient\\AreaTable.dbc");
    CHECK_HEX(mpq.expectedSha1.data(), mpq.expectedSha1.size(),
        "7d88154d3411811985f5d81177c5453248133443");

    CHECK(!planner.Update(true, 60000).has_value());
    CHECK(!planner.Update(false, 60000).has_value());
}

TEST(WardenCheckPlanner_emits_timing_mpq_then_exact_lua_once)
{
    warden::WardenCheckPlanner planner(1000, TestMpqProfile(),
        TestLuaProfile());

    auto const plan = planner.Update(true, 1000);
    REQUIRE(plan.has_value());
    CHECK_EQ(plan->requestId, uint32(1));
    REQUIRE(plan->checks.size() == 3u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));
    CHECK(std::holds_alternative<warden::MpqCheckProfile>(plan->checks[1]));
    REQUIRE(std::holds_alternative<warden::LuaCheckProfile>(plan->checks[2]));
    warden::LuaCheckProfile const& lua =
        std::get<warden::LuaCheckProfile>(plan->checks[2]);
    CHECK_EQ(lua.checkId, uint32(2));
    CHECK_STR(lua.query.c_str(), "OKAY");
    CHECK_STR(lua.expectedText.c_str(), "Okay");

    CHECK(!planner.Update(true, 60000).has_value());
}

TEST(WardenCheckPlanner_emits_timing_then_lua_without_mpq)
{
    warden::WardenCheckPlanner planner(1000, std::nullopt, TestLuaProfile());

    auto const plan = planner.Update(true, 1000);
    REQUIRE(plan.has_value());
    REQUIRE(plan->checks.size() == 2u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));
    CHECK(std::holds_alternative<warden::LuaCheckProfile>(plan->checks[1]));
}
