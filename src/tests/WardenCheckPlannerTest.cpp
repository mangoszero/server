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

#include "WardenCheckFixtures.h"
#include "WardenCheckPlanner.h"
#include "WardenEvidence.h"

#include <algorithm>
#include <deque>
#include <map>
#include <vector>

namespace
{
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

std::vector<warden::WardenCheckDefinition> ExactChecks()
{
    warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    warden::WardenCheckProfile const* profile =
        catalog.Find(5875, "Win", "enUS");
    return profile ? profile->checks :
        std::vector<warden::WardenCheckDefinition>();
}

std::vector<uint32> CheckIds(warden::CheckPlan const& plan)
{
    std::vector<uint32> ids;
    for (warden::WardenCheckDefinition const& check : plan.checks)
        ids.push_back(warden::GetWardenCheckId(check));
    return ids;
}

std::vector<uint32> NonTimingIds(warden::CheckPlan const& plan)
{
    std::vector<uint32> ids;
    for (warden::WardenCheckDefinition const& check : plan.checks)
    {
        if (warden::GetWardenCheckType(check) !=
            warden::WardenCheckType::Timing)
            ids.push_back(warden::GetWardenCheckId(check));
    }
    return ids;
}

std::vector<warden::WardenCheckDefinition> ObservationOnlyChecks()
{
    std::vector<warden::WardenCheckDefinition> checks = ExactChecks();
    checks.erase(std::remove_if(checks.begin(), checks.end(),
        [](warden::WardenCheckDefinition const& check)
        {
            return warden::IsActionableEvidenceClass(check.evidenceClass);
        }), checks.end());
    return checks;
}

std::vector<warden::WardenCheckDefinition> OddNonHealthChecks()
{
    std::vector<warden::WardenCheckDefinition> checks;
    for (warden::WardenCheckDefinition const& check : ExactChecks())
    {
        uint32 const id = warden::GetWardenCheckId(check);
        if (warden::GetWardenCheckType(check) ==
                warden::WardenCheckType::Timing ||
            id == 1 || id == 2 || id == 1107)
            checks.push_back(check);
    }
    return checks;
}
}

TEST(WardenEvidence_normalized_values_have_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::WardenCheckType::Timing), "Timing");
    CHECK_STR(warden::ToString(warden::WardenCheckType::Mpq), "MPQ");
    CHECK_STR(warden::ToString(
        warden::WardenEvidenceClass::ThreatSignature), "ThreatSignature");
    CHECK_STR(warden::ToString(warden::WardenCheckOutcome::Match), "Match");
    CHECK_STR(warden::ToString(warden::WardenCheckOutcome::Mismatch),
        "Mismatch");
    CHECK_STR(warden::ToString(warden::WardenCheckOutcome::Unavailable),
        "Unavailable");
    CHECK_STR(warden::ToString(warden::WardenCheckOutcome::Stable), "Stable");
    CHECK_STR(warden::ToString(warden::WardenCheckOutcome::Unstable),
        "Unstable");
}

TEST(WardenCheckPlanner_profileless_planner_remains_inert)
{
    ScriptedRandom random{{30, 30}, {10, 10}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        {}, random.Callback());
    CHECK(!planner.Update(false, 60000).has_value());
    CHECK(!planner.Update(true, 1000).has_value());
    CHECK(!planner.QueueConfirmation(1));
    planner.SetAggressive(true);
    CHECK(!planner.Update(false, 60000).has_value());
    CHECK(!planner.Update(true, 60000).has_value());
}

TEST(WardenCheckPlanner_initial_plan_waits_and_preserves_catalogue_order)
{
    std::vector<warden::WardenCheckDefinition> const checks = ExactChecks();
    REQUIRE(checks.size() == 7u);
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        checks);

    CHECK(!planner.Update(false, 5000).has_value());
    CHECK(!planner.Update(true, 600).has_value());
    CHECK(!planner.Update(true, 399).has_value());
    std::optional<warden::CheckPlan> const plan = planner.Update(true, 1);
    REQUIRE(plan.has_value());
    CHECK_EQ(plan->requestId, uint32(1));
    CHECK(plan->purpose == warden::CheckPlanPurpose::Initial);
    REQUIRE(plan->checks.size() == 7u);
    for (size_t index = 1; index < plan->checks.size(); ++index)
        CHECK(plan->checks[index - 1].sortOrder < plan->checks[index].sortOrder);
    CHECK_EQ(warden::GetWardenCheckId(plan->checks.front()), uint32(65536));
    CHECK_EQ(warden::GetWardenCheckId(plan->checks.back()), uint32(1135));
    CHECK(!planner.Update(true, 60000).has_value());
}

TEST(WardenCheckPlanner_resets_partial_initial_delay_when_ineligible)
{
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        ExactChecks());
    CHECK(!planner.Update(true, 900).has_value());
    CHECK(!planner.Update(false, 1).has_value());
    CHECK(!planner.Update(true, 999).has_value());
    CHECK(planner.Update(true, 1).has_value());
}

TEST(WardenCheckPlanner_normal_rotation_covers_every_nonhealth_check_once)
{
    ScriptedRandom random{{30, 30, 30, 30}, {}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        ExactChecks(), random.Callback());
    std::optional<warden::CheckPlan> initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    std::map<uint32, uint32> counts;
    for (uint32 batch = 0; batch < 3; ++batch)
    {
        std::optional<warden::CheckPlan> recurring =
            planner.Update(true, 30000);
        REQUIRE(recurring.has_value());
        CHECK(recurring->purpose == warden::CheckPlanPurpose::Recurring);
        REQUIRE(recurring->checks.size() == 3u);
        CHECK(warden::GetWardenCheckType(recurring->checks[0]) ==
            warden::WardenCheckType::Timing);
        for (size_t index = 1; index < recurring->checks.size(); ++index)
            CHECK(recurring->checks[index - 1].sortOrder <
                recurring->checks[index].sortOrder);
        std::vector<uint32> const ids = NonTimingIds(*recurring);
        REQUIRE(ids.size() == 2u);
        for (uint32 id : ids)
            ++counts[id];
        planner.Complete(*recurring);
    }

    CHECK_EQ(counts.size(), size_t(6));
    for (uint32 id : {uint32(1), uint32(2), uint32(1107), uint32(827),
            uint32(1566), uint32(1135)})
        CHECK_EQ(counts[id], uint32(1));
}

TEST(WardenCheckPlanner_odd_rotation_refills_each_two_check_batch)
{
    // The second shuffle puts the just-consumed final ID first, forcing the
    // duplicate-skip branch before the batch can select its second ID.
    ScriptedRandom random{{30, 30, 30, 30, 30}, {}, {0, 0, 2, 1}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        OddNonHealthChecks(), random.Callback());
    std::optional<warden::CheckPlan> initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    for (uint32 batch = 0; batch < 4; ++batch)
    {
        std::optional<warden::CheckPlan> recurring =
            planner.Update(true, 30000);
        REQUIRE(recurring.has_value());
        REQUIRE(recurring->checks.size() == 3u);
        std::vector<uint32> const ids = NonTimingIds(*recurring);
        REQUIRE(ids.size() == 2u);
        CHECK(ids[0] != ids[1]);
        planner.Complete(*recurring);
    }
}

TEST(WardenCheckPlanner_recurring_countdown_pauses_without_catch_up)
{
    ScriptedRandom random{{30, 30}, {}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        ExactChecks(), random.Callback());
    std::optional<warden::CheckPlan> initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);
    CHECK(!planner.Update(true, 10000).has_value());
    CHECK(!planner.Update(false, 60000).has_value());
    CHECK(!planner.Update(true, 19999).has_value());
    std::optional<warden::CheckPlan> recurring = planner.Update(true, 1);
    REQUIRE(recurring.has_value());
    planner.Complete(*recurring);
    CHECK(!planner.Update(true, 0).has_value());
}

TEST(WardenCheckPlanner_aggressive_plans_include_only_actionable_checks)
{
    ScriptedRandom random{{}, {10, 20}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        ExactChecks(), random.Callback());
    planner.SetAggressive(true);

    std::optional<warden::CheckPlan> immediate = planner.Update(false, 0);
    REQUIRE(immediate.has_value());
    CHECK(immediate->purpose ==
        warden::CheckPlanPurpose::AggressiveImmediate);
    CHECK(CheckIds(*immediate) ==
        std::vector<uint32>({1107, 827, 1566}));
    planner.Complete(*immediate);

    std::optional<warden::CheckPlan> initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);
    CHECK(!planner.Update(true, 9999).has_value());
    std::optional<warden::CheckPlan> recurring = planner.Update(true, 1);
    REQUIRE(recurring.has_value());
    CHECK(recurring->purpose ==
        warden::CheckPlanPurpose::AggressiveRecurring);
    CHECK(CheckIds(*recurring) ==
        std::vector<uint32>({1107, 827, 1566}));
}

TEST(WardenCheckPlanner_observation_only_aggressive_mode_uses_normal_checks)
{
    ScriptedRandom random{{}, {10, 10, 10}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        ObservationOnlyChecks(), random.Callback());
    planner.SetAggressive(true);
    CHECK(!planner.Update(false, 0).has_value());

    std::optional<warden::CheckPlan> initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);
    CHECK(!planner.Update(true, 9999).has_value());
    std::optional<warden::CheckPlan> recurring = planner.Update(true, 1);
    REQUIRE(recurring.has_value());
    CHECK(recurring->purpose == warden::CheckPlanPurpose::Recurring);
    REQUIRE(recurring->checks.size() == 3u);
    CHECK_EQ(NonTimingIds(*recurring).size(), size_t(2));
    planner.Complete(*recurring);
    CHECK(!planner.Update(true, 9999).has_value());
    CHECK(planner.Update(true, 1).has_value());
}

TEST(WardenCheckPlanner_generic_confirmation_preempts_and_resets_cadence)
{
    ScriptedRandom random{{30, 60, 30, 30}, {}, {}};
    warden::WardenCheckPlanner planner(warden::WardenConfiguration{}, 1000,
        ExactChecks(), random.Callback());
    std::optional<warden::CheckPlan> initial = planner.Update(true, 1000);
    REQUIRE(initial.has_value());
    planner.Complete(*initial);

    CHECK(planner.QueueConfirmation(1));
    CHECK(planner.QueueConfirmation(2));
    CHECK(planner.QueueConfirmation(1566));
    CHECK(!planner.QueueConfirmation(65536));
    CHECK(!planner.QueueConfirmation(999999));
    CHECK(!planner.QueueConfirmation(1));

    for (uint32 expected : {uint32(1), uint32(2), uint32(1566)})
    {
        std::optional<warden::CheckPlan> confirmation =
            planner.Update(true, 30000);
        REQUIRE(confirmation.has_value());
        CHECK(confirmation->purpose ==
            warden::CheckPlanPurpose::Confirmation);
        REQUIRE(confirmation->checks.size() == 1u);
        CHECK_EQ(warden::GetWardenCheckId(confirmation->checks[0]), expected);
        CHECK(!planner.Update(true, 60000).has_value());
        planner.Complete(*confirmation);
    }

    CHECK(!planner.Update(true, 29999).has_value());
    CHECK(planner.Update(true, 1).has_value());
}

TEST(WardenCheckPlanner_preflight_is_linear_and_uses_exact_purposes)
{
    warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    warden::WardenCheckProfile const* exact =
        catalog.Find(5875, "Win", "enUS");
    REQUIRE(exact != nullptr);
    std::vector<warden::CheckPlan> plans =
        warden::BuildWardenPreflightPlans(*exact);
    REQUIRE(plans.size() == 7u);
    CHECK(plans[0].purpose == warden::CheckPlanPurpose::Initial);
    CHECK_EQ(plans[0].requestId, uint32(1));
    CHECK_EQ(plans[0].checks.size(), size_t(7));
    for (size_t index = 1; index < plans.size(); ++index)
    {
        CHECK(plans[index].purpose ==
            warden::CheckPlanPurpose::Confirmation);
        CHECK_EQ(plans[index].requestId, uint32(1));
        CHECK_EQ(plans[index].checks.size(), size_t(1));
    }

    warden::WardenCheckProfile large;
    large.key = {5875, "Win", "enUS"};
    large.checks.push_back(exact->checks[0]);
    warden::WardenCheckDefinition prototype = exact->checks[3];
    warden::MemCheckProfile mem =
        std::get<warden::MemCheckProfile>(prototype.payload);
    mem.expectedBytes.assign(1, 0x90);
    for (uint32 index = 0; index < 1000; ++index)
    {
        prototype.sortOrder = static_cast<uint16>(index + 1);
        mem.checkId = 100000 + index;
        prototype.payload = mem;
        large.checks.push_back(prototype);
    }
    large.totalRows = static_cast<uint32>(large.checks.size());
    plans = warden::BuildWardenPreflightPlans(large);
    CHECK_EQ(plans.size(), size_t(1001));
}
