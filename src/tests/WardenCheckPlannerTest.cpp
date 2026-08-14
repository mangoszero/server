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

#include "WardenCheckPlanner.h"
#include "WardenEvidence.h"

TEST(WardenEvidence_timing_outcomes_have_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::TimingOutcome::Stable), "Stable");
    CHECK_STR(warden::ToString(warden::TimingOutcome::Unstable), "Unstable");
}

TEST(WardenCheckPlanner_waits_one_cumulative_eligible_second_once)
{
    warden::WardenCheckPlanner planner;

    CHECK(!planner.Update(false, 5000).has_value());
    CHECK(!planner.Update(true, 600).has_value());
    CHECK(!planner.Update(true, 399).has_value());

    auto const plan = planner.Update(true, 1);
    REQUIRE(plan.has_value());
    CHECK(plan->kind == warden::CheckKind::Timing);
    CHECK_EQ(plan->requestId, uint32(1));

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
    CHECK(plan->kind == warden::CheckKind::Timing);
}
