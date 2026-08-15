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

#include "WardenIncidentStore.h"

#include <algorithm>
#include <vector>

TEST(WardenIncidentWindow_excludes_exact_lower_boundary)
{
    std::vector<uint64> const timestamps{100, 101, 500, 700, 800, 900};
    auto const state = warden::ClassifyIncidentWindow(
        timestamps, 1000, 900, 5);

    CHECK_EQ(state.recentCount, uint32(5));
    CHECK_EQ(state.aggressiveUntil, uint64(1001));
}

TEST(WardenIncidentWindow_zero_and_subthreshold_events_are_not_aggressive)
{
    auto const empty = warden::ClassifyIncidentWindow({}, 1000, 900, 5);
    CHECK_EQ(empty.recentCount, uint32(0));
    CHECK_EQ(empty.aggressiveUntil, uint64(0));

    auto const below = warden::ClassifyIncidentWindow(
        {999, 900, 800, 700}, 1000, 900, 5);
    CHECK_EQ(below.recentCount, uint32(4));
    CHECK_EQ(below.aggressiveUntil, uint64(0));
}

TEST(WardenIncidentWindow_more_than_threshold_uses_threshold_newest_expiry)
{
    std::vector<uint64> timestamps{101, 500, 700, 800, 900, 950};
    auto const ordered = warden::ClassifyIncidentWindow(
        timestamps, 1000, 900, 5);

    std::reverse(timestamps.begin(), timestamps.end());
    auto const reversed = warden::ClassifyIncidentWindow(
        timestamps, 1000, 900, 5);

    CHECK_EQ(ordered.recentCount, uint32(6));
    CHECK_EQ(ordered.aggressiveUntil, uint64(1400));
    CHECK_EQ(reversed.recentCount, ordered.recentCount);
    CHECK_EQ(reversed.aggressiveUntil, ordered.aggressiveUntil);
}

TEST(WardenIncidentDeadline_rebases_database_clock_to_server_clock)
{
    CHECK_EQ(warden::RebaseIncidentDeadline(1300, 1000, 5000),
        uint64(5300));
    CHECK_EQ(warden::RebaseIncidentDeadline(1000, 1000, 5000),
        uint64(0));
    CHECK_EQ(warden::RebaseIncidentDeadline(900, 1000, 5000),
        uint64(0));
    CHECK_EQ(warden::RebaseIncidentDeadline(1300, 0, 5000), uint64(0));

    uint64 const maximum = std::numeric_limits<uint64>::max();
    CHECK_EQ(warden::RebaseIncidentDeadline(maximum, maximum - 10u,
        maximum - 5u), maximum);
}

TEST(WardenIncidentOutcome_accepts_only_negative_memory_classifications)
{
    CHECK(!warden::ToIncidentOutcome(
        warden::MemOutcome::Match).has_value());

    auto const mismatch = warden::ToIncidentOutcome(
        warden::MemOutcome::ByteMismatch);
    REQUIRE(mismatch.has_value());
    CHECK_EQ(uint32(*mismatch), uint32(1));

    auto const unavailable = warden::ToIncidentOutcome(
        warden::MemOutcome::Unavailable);
    REQUIRE(unavailable.has_value());
    CHECK_EQ(uint32(*unavailable), uint32(2));
}

TEST(WardenIncidentApplication_failed_write_kicks_without_durable_summary)
{
    warden::WardenIncidentWriteResult result;
    result.status = warden::WardenIncidentWriteStatus::Failed;
    result.recentCount = 10;
    result.permanentBanActive = true;

    auto const application = warden::ClassifyIncidentWriteResult(result);
    CHECK(application.mustKick);
    CHECK(!application.durable);
    CHECK(!application.summaryKnown);
    CHECK_EQ(application.recentCount, uint32(0));
    CHECK(!application.permanentBanActive);
}

TEST(WardenIncidentApplication_committed_write_exposes_durable_summary)
{
    warden::WardenIncidentWriteResult result;
    result.status = warden::WardenIncidentWriteStatus::Committed;
    result.recentCount = 10;
    result.permanentBanActive = true;

    auto const application = warden::ClassifyIncidentWriteResult(result);
    CHECK(application.mustKick);
    CHECK(application.durable);
    CHECK(application.summaryKnown);
    CHECK_EQ(application.recentCount, uint32(10));
    CHECK(application.permanentBanActive);
}

TEST(WardenIncidentApplication_committed_unknown_state_never_invents_summary)
{
    warden::WardenIncidentWriteResult result;
    result.status =
        warden::WardenIncidentWriteStatus::CommittedStateUnavailable;
    result.recentCount = 10;
    result.permanentBanActive = true;

    auto const application = warden::ClassifyIncidentWriteResult(result);
    CHECK(application.mustKick);
    CHECK(application.durable);
    CHECK(!application.summaryKnown);
    CHECK_EQ(application.recentCount, uint32(0));
    CHECK(!application.permanentBanActive);
}
