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

#include <deque>
#include <map>
#include <variant>
#include <vector>

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

std::vector<warden::MemCheckProfile> TestMemProfiles()
{
    return
    {
        {1107, "", 0x00618900, {0x55, 0x8B, 0xEC}},
        {827, "", 0x007C6206, {0x25, 0xFF, 0xFF}},
        {1566, "", 0x00494A50, {0xA1, 0xC0, 0xEA, 0xCE, 0x00}},
        {1135, "", 0x0080DFFC, {0xBB, 0x8D, 0x24, 0x3F}}
    };
}

struct ScriptedRandom
{
    std::deque<uint32> normalIntervals;
    std::deque<uint32> aggressiveIntervals;
    std::deque<uint32> shuffleSelections;

    warden::WardenRandomRange Callback()
    {
        return [this](uint32 minimum, uint32 maximum)
        {
            std::deque<uint32>* samples = nullptr;
            if (minimum == 30 && maximum == 60)
                samples = &normalIntervals;
            else if (minimum == 10 && maximum == 20)
                samples = &aggressiveIntervals;

            // Shuffle selections use other ranges and deterministically choose
            // their lower bound. Cadence samples must be supplied explicitly.
            if (!samples)
            {
                if (shuffleSelections.empty())
                    return minimum;
                uint32 const value = shuffleSelections.front();
                shuffleSelections.pop_front();
                CHECK(value >= minimum && value <= maximum);
                return value;
            }
            CHECK(!samples->empty());
            if (samples->empty())
                return minimum;
            uint32 const value = samples->front();
            samples->pop_front();
            CHECK(value >= minimum && value <= maximum);
            return value;
        };
    }
};

std::vector<uint32> MemIds(warden::CheckPlan const& plan)
{
    std::vector<uint32> ids;
    for (warden::PlannedCheck const& check : plan.checks)
    {
        if (auto const* mem = std::get_if<warden::MemCheckProfile>(&check))
            ids.push_back(mem->checkId);
    }
    return ids;
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

TEST(WardenEvidence_mem_outcomes_have_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::MemOutcome::Match), "Match");
    CHECK_STR(warden::ToString(warden::MemOutcome::ByteMismatch),
        "ByteMismatch");
    CHECK_STR(warden::ToString(warden::MemOutcome::Unavailable),
        "Unavailable");
}

TEST(WardenCheckPlanner_initial_plan_waits_one_cumulative_eligible_second)
{
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000);

    CHECK(!planner.Update(false, 5000).has_value());
    CHECK(!planner.Update(true, 600).has_value());
    CHECK(!planner.Update(true, 399).has_value());

    auto const plan = planner.Update(true, 1);
    REQUIRE(plan.has_value());
    CHECK_EQ(plan->requestId, uint32(1));
    CHECK(plan->purpose == warden::CheckPlanPurpose::Initial);
    REQUIRE(plan->checks.size() == 1u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));

    // The planner must not overlap a second request before completion.
    CHECK(!planner.Update(true, 60000).has_value());
}

TEST(WardenCheckPlanner_resets_partial_delay_when_ineligible)
{
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000);

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
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        TestMpqProfile());

    auto const plan = planner.Update(true, 1000);
    REQUIRE(plan.has_value());
    CHECK_EQ(plan->requestId, uint32(1));
    CHECK(plan->purpose == warden::CheckPlanPurpose::Initial);
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
}

TEST(WardenCheckPlanner_emits_timing_mpq_then_exact_lua_once)
{
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        TestMpqProfile(), TestLuaProfile());

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
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, TestLuaProfile());

    auto const plan = planner.Update(true, 1000);
    REQUIRE(plan.has_value());
    REQUIRE(plan->checks.size() == 2u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));
    CHECK(std::holds_alternative<warden::LuaCheckProfile>(plan->checks[1]));
}

TEST(WardenCheckPlanner_initial_plan_preserves_content_and_mem_order)
{
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        TestMpqProfile(), TestLuaProfile(), TestMemProfiles());

    auto const plan = planner.Update(true, 1000);
    REQUIRE(plan.has_value());
    CHECK(plan->purpose == warden::CheckPlanPurpose::Initial);
    REQUIRE(plan->checks.size() == 7u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));
    CHECK(std::holds_alternative<warden::MpqCheckProfile>(plan->checks[1]));
    CHECK(std::holds_alternative<warden::LuaCheckProfile>(plan->checks[2]));
    std::vector<uint32> const ids = MemIds(*plan);
    REQUIRE(ids.size() == 4u);
    CHECK_EQ(ids[0], uint32(1107));
    CHECK_EQ(ids[1], uint32(827));
    CHECK_EQ(ids[2], uint32(1566));
    CHECK_EQ(ids[3], uint32(1135));
}

TEST(WardenCheckPlanner_emits_timing_then_mem_without_content_checks)
{
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles());

    auto const plan = planner.Update(true, 1000);
    REQUIRE(plan.has_value());
    REQUIRE(plan->checks.size() == 5u);
    CHECK(std::holds_alternative<warden::TimingCheck>(plan->checks[0]));
    CHECK(std::holds_alternative<warden::MemCheckProfile>(plan->checks[1]));
    CHECK(std::holds_alternative<warden::MemCheckProfile>(plan->checks[2]));
}

TEST(WardenCheckPlanner_normal_recurring_interval_is_inclusive)
{
    ScriptedRandom random{{30, 60}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());

    auto const initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    CHECK(!planner.Update(true, 29999).has_value());
    auto const first = planner.Update(true, 1);
    REQUIRE(first.has_value());
    CHECK(first->purpose == warden::CheckPlanPurpose::Recurring);
    REQUIRE(first->checks.size() == 3u);
    CHECK(std::holds_alternative<warden::TimingCheck>(first->checks[0]));
    CHECK_EQ(MemIds(*first).size(), size_t(2));
    planner.Complete(*first);

    CHECK(!planner.Update(true, 59999).has_value());
    auto const second = planner.Update(true, 1);
    REQUIRE(second.has_value());
    CHECK(second->purpose == warden::CheckPlanPurpose::Recurring);
}

TEST(WardenCheckPlanner_recurring_countdown_pauses_without_catch_up)
{
    ScriptedRandom random{{30, 30}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());

    auto const initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    CHECK(!planner.Update(true, 10000).has_value());
    CHECK(!planner.Update(false, 60000).has_value());
    CHECK(!planner.Update(true, 19999).has_value());
    auto const recurring = planner.Update(true, 1);
    REQUIRE(recurring.has_value());
    planner.Complete(*recurring);

    // Excess time from the prior update is discarded, never caught up.
    CHECK(!planner.Update(true, 0).has_value());
}

TEST(WardenCheckPlanner_two_normal_plans_cover_the_four_mem_checks_once)
{
    ScriptedRandom random{{30, 30, 30}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());

    auto const initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    std::map<uint32, uint32> counts;
    for (uint32 batch = 0; batch < 2; ++batch)
    {
        auto const recurring = planner.Update(true, 30000);
        REQUIRE(recurring.has_value());
        std::vector<uint32> const ids = MemIds(*recurring);
        REQUIRE(ids.size() == 2u);
        for (uint32 id : ids)
            ++counts[id];
        planner.Complete(*recurring);
    }

    CHECK_EQ(counts.size(), size_t(4));
    CHECK_EQ(counts[1107], uint32(1));
    CHECK_EQ(counts[827], uint32(1));
    CHECK_EQ(counts[1566], uint32(1));
    CHECK_EQ(counts[1135], uint32(1));
}

TEST(WardenCheckPlanner_aggressive_immediate_runs_at_character_selection)
{
    ScriptedRandom random{{}, {10}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());
    planner.SetAggressive(true);

    auto const immediate = planner.Update(false, 0);
    REQUIRE(immediate.has_value());
    CHECK(immediate->purpose ==
        warden::CheckPlanPurpose::AggressiveImmediate);
    CHECK_EQ(MemIds(*immediate).size(), size_t(4));
    CHECK_EQ(immediate->checks.size(), size_t(4));
    planner.Complete(*immediate);

    CHECK(!planner.Update(false, 60000).has_value());
}

TEST(WardenCheckPlanner_aggressive_recurring_interval_is_inclusive)
{
    ScriptedRandom random{{}, {10, 20}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());
    planner.SetAggressive(true);

    auto const immediate = planner.Update(false, 0);
    REQUIRE(immediate.has_value());
    planner.Complete(*immediate);
    auto const initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    CHECK(!planner.Update(true, 9999).has_value());
    auto const first = planner.Update(true, 1);
    REQUIRE(first.has_value());
    CHECK(first->purpose == warden::CheckPlanPurpose::AggressiveRecurring);
    CHECK_EQ(first->checks.size(), size_t(4));
    CHECK_EQ(MemIds(*first).size(), size_t(4));
    planner.Complete(*first);

    CHECK(!planner.Update(true, 19999).has_value());
    auto const second = planner.Update(true, 1);
    REQUIRE(second.has_value());
    CHECK(second->purpose == warden::CheckPlanPurpose::AggressiveRecurring);
}

TEST(WardenCheckPlanner_aggressive_recurring_reshuffles_each_batch)
{
    ScriptedRandom random{{}, {10, 10}, {0, 0, 0, 3, 2, 1}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());
    planner.SetAggressive(true);

    auto const immediate = planner.Update(false, 0);
    REQUIRE(immediate.has_value());
    planner.Complete(*immediate);
    auto const initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    auto const first = planner.Update(true, 10000);
    REQUIRE(first.has_value());
    std::vector<uint32> const firstIds = MemIds(*first);
    planner.Complete(*first);
    auto const second = planner.Update(true, 10000);
    REQUIRE(second.has_value());
    std::vector<uint32> const secondIds = MemIds(*second);

    REQUIRE(firstIds.size() == 4u);
    REQUIRE(secondIds.size() == 4u);
    CHECK(firstIds != secondIds);
}

TEST(WardenCheckPlanner_aggressive_expiry_returns_to_normal_cadence)
{
    ScriptedRandom random{{30}, {10}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());
    planner.SetAggressive(true);

    auto const immediate = planner.Update(false, 0);
    REQUIRE(immediate.has_value());
    planner.Complete(*immediate);
    auto const initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    planner.SetAggressive(false);
    CHECK(!planner.Update(true, 29999).has_value());
    auto const normal = planner.Update(true, 1);
    REQUIRE(normal.has_value());
    CHECK(normal->purpose == warden::CheckPlanPurpose::Recurring);
    CHECK(std::holds_alternative<warden::TimingCheck>(normal->checks[0]));
    CHECK_EQ(MemIds(*normal).size(), size_t(2));
}

TEST(WardenCheckPlanner_confirmation_preempts_and_resets_recurring_work)
{
    ScriptedRandom random{{30, 60}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        std::nullopt, std::nullopt, TestMemProfiles(), random.Callback());

    auto const initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    CHECK(planner.QueueConfirmation(1566));
    CHECK(!planner.QueueConfirmation(1566));
    CHECK(!planner.QueueConfirmation(999999));
    auto const confirmation = planner.Update(true, 30000);
    REQUIRE(confirmation.has_value());
    CHECK(confirmation->purpose == warden::CheckPlanPurpose::Confirmation);
    REQUIRE(confirmation->checks.size() == 1u);
    REQUIRE(std::holds_alternative<warden::MemCheckProfile>(
        confirmation->checks[0]));
    CHECK_EQ(std::get<warden::MemCheckProfile>(
        confirmation->checks[0]).checkId, uint32(1566));

    CHECK(!planner.Update(true, 60000).has_value());
    planner.Complete(*confirmation);
    CHECK(!planner.Update(true, 59999).has_value());
    auto const recurring = planner.Update(true, 1);
    REQUIRE(recurring.has_value());
    CHECK(recurring->purpose == warden::CheckPlanPurpose::Recurring);
}
