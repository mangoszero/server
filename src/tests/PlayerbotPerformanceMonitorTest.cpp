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

#include "../modules/Bots/playerbot/PlayerbotPerformanceMonitor.h"

TEST(PlayerbotPerformanceMonitorAggregatesReportingWindows)
{
    ai::PlayerbotPerformanceMonitor monitor;
    ai::PlayerbotPerformanceSnapshot snapshot;

    CHECK(!monitor.TakeSnapshotIfDue(1000, 500, snapshot));
    CHECK(!monitor.TakeSnapshotIfDue(2000, 0, snapshot));

    monitor.RecordBuiltPacket(128);
    monitor.RecordBuiltPacket(64);
    monitor.RecordEventQuery(120);
    monitor.RecordEventQuery(300);
    monitor.RecordEventQuery(75);
    monitor.RecordAiEvaluation(450);
    monitor.RecordAiDeferred();
    monitor.RecordAiDeferred();

    CHECK(!monitor.TakeSnapshotIfDue(1499, 500, snapshot));
    CHECK(monitor.TakeSnapshotIfDue(1500, 500, snapshot));
    CHECK_EQ(snapshot.builtPacketCount, 2u);
    CHECK_EQ(snapshot.builtPacketBytes, 192u);
    CHECK_EQ(snapshot.eventQueryCount, 3u);
    CHECK_EQ(snapshot.eventQueryTotalMicros, 495u);
    CHECK_EQ(snapshot.eventQueryMaxMicros, 300u);
    CHECK_EQ(snapshot.aiEvaluationCount, 1u);
    CHECK_EQ(snapshot.aiDeferredCount, 2u);
    CHECK_EQ(snapshot.aiEvaluationTotalMicros, 450u);
    CHECK_EQ(snapshot.aiEvaluationMaxMicros, 450u);

    monitor.RecordBuiltPacket(32);
    monitor.RecordEventQuery(25);
    monitor.RecordAiEvaluation(50);

    CHECK(!monitor.TakeSnapshotIfDue(1999, 500, snapshot));
    CHECK(monitor.TakeSnapshotIfDue(2000, 500, snapshot));
    CHECK_EQ(snapshot.builtPacketCount, 1u);
    CHECK_EQ(snapshot.builtPacketBytes, 32u);
    CHECK_EQ(snapshot.eventQueryCount, 1u);
    CHECK_EQ(snapshot.eventQueryTotalMicros, 25u);
    CHECK_EQ(snapshot.eventQueryMaxMicros, 25u);
    CHECK_EQ(snapshot.aiEvaluationCount, 1u);
    CHECK_EQ(snapshot.aiDeferredCount, 0u);
    CHECK_EQ(snapshot.aiEvaluationTotalMicros, 50u);
    CHECK_EQ(snapshot.aiEvaluationMaxMicros, 50u);
}
