/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "TestHarness.h"
#include "SessionProtocolPolicy.h"

using namespace std::chrono;

TEST(SessionPingTracker_first_ping_is_not_fast)
{
    SessionPingTracker tracker;
    CHECK_EQ(tracker.Record(
        SessionPingTracker::Clock::time_point(seconds(100))), 0);
    CHECK(!tracker.ShouldKick(1, true));
}

TEST(SessionPingTracker_counts_and_resets_fast_runs)
{
    SessionPingTracker tracker;
    SessionPingTracker::Clock::time_point start(seconds(100));
    CHECK_EQ(tracker.Record(start), 0);
    CHECK_EQ(tracker.Record(start + seconds(10)), 1);
    CHECK_EQ(tracker.Record(start + seconds(20)), 2);
    CHECK_EQ(tracker.Record(start + seconds(50)), 0);
}

TEST(SessionPingTracker_enforces_threshold_only_for_players)
{
    SessionPingTracker tracker;
    SessionPingTracker::Clock::time_point start(seconds(100));
    tracker.Record(start);
    tracker.Record(start + seconds(1));
    tracker.Record(start + seconds(2));

    CHECK(tracker.ShouldKick(1, true));
    CHECK(!tracker.ShouldKick(0, true));
    CHECK(!tracker.ShouldKick(1, false));
}
