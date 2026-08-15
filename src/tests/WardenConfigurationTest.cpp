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

#include "WardenConfiguration.h"

#include <limits>

namespace
{
warden::WardenRawConfiguration ValidCustomConfiguration()
{
    warden::WardenRawConfiguration raw;
    raw.enforcementMode = 1;
    raw.normalMinSeconds = 31;
    raw.normalMaxSeconds = 45;
    raw.aggressiveMinSeconds = 11;
    raw.aggressiveMaxSeconds = 15;
    raw.aggressiveThreshold = 6;
    raw.banThreshold = 12;
    raw.incidentWindowSeconds = 600;
    return raw;
}
}

TEST(WardenConfiguration_defaults_match_approved_production_policy)
{
    warden::WardenRawConfiguration raw;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(uint32(result.value.enforcementMode), uint32(2));
    CHECK_EQ(result.value.normalMinSeconds, uint32(30));
    CHECK_EQ(result.value.normalMaxSeconds, uint32(60));
    CHECK_EQ(result.value.aggressiveMinSeconds, uint32(10));
    CHECK_EQ(result.value.aggressiveMaxSeconds, uint32(20));
    CHECK_EQ(result.value.aggressiveThreshold, uint32(5));
    CHECK_EQ(result.value.banThreshold, uint32(10));
    CHECK_EQ(result.value.incidentWindowSeconds, uint32(900));
    CHECK(result.corrections ==
        warden::WardenConfigurationCorrection::None);
}

TEST(WardenConfiguration_accepts_each_enforcement_mode)
{
    for (uint32 mode = 0; mode <= 2; ++mode)
    {
        warden::WardenRawConfiguration raw;
        raw.enforcementMode = mode;
        auto const result = warden::NormalizeWardenConfiguration(raw);

        CHECK_EQ(uint32(result.value.enforcementMode), mode);
        CHECK(!warden::HasWardenConfigurationCorrection(result.corrections,
            warden::WardenConfigurationCorrection::EnforcementMode));
    }
}

TEST(WardenConfiguration_preserves_a_valid_custom_policy)
{
    auto const raw = ValidCustomConfiguration();
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(uint32(result.value.enforcementMode), uint32(1));
    CHECK_EQ(result.value.normalMinSeconds, uint32(31));
    CHECK_EQ(result.value.normalMaxSeconds, uint32(45));
    CHECK_EQ(result.value.aggressiveMinSeconds, uint32(11));
    CHECK_EQ(result.value.aggressiveMaxSeconds, uint32(15));
    CHECK_EQ(result.value.aggressiveThreshold, uint32(6));
    CHECK_EQ(result.value.banThreshold, uint32(12));
    CHECK_EQ(result.value.incidentWindowSeconds, uint32(600));
    CHECK(result.corrections ==
        warden::WardenConfigurationCorrection::None);
}

TEST(WardenConfiguration_invalid_mode_falls_back_without_changing_other_groups)
{
    auto raw = ValidCustomConfiguration();
    raw.enforcementMode = 3;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(uint32(result.value.enforcementMode), uint32(2));
    CHECK_EQ(result.value.normalMinSeconds, uint32(31));
    CHECK_EQ(result.value.normalMaxSeconds, uint32(45));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::EnforcementMode));
}

TEST(WardenConfiguration_invalid_normal_interval_falls_back_as_a_pair)
{
    auto raw = ValidCustomConfiguration();
    raw.normalMinSeconds = 46;
    raw.normalMaxSeconds = 45;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(result.value.normalMinSeconds, uint32(30));
    CHECK_EQ(result.value.normalMaxSeconds, uint32(60));
    CHECK_EQ(result.value.aggressiveMinSeconds, uint32(11));
    CHECK_EQ(result.value.aggressiveMaxSeconds, uint32(15));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::NormalInterval));
}

TEST(WardenConfiguration_interval_overflow_falls_back_as_a_pair)
{
    auto raw = ValidCustomConfiguration();
    uint32 const maximumSeconds =
        std::numeric_limits<uint32>::max() / uint32(1000);
    raw.normalMinSeconds = maximumSeconds;
    raw.normalMaxSeconds = maximumSeconds;
    raw.aggressiveMinSeconds = maximumSeconds + 1;
    raw.aggressiveMaxSeconds = maximumSeconds + 1;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(result.value.normalMinSeconds, maximumSeconds);
    CHECK_EQ(result.value.normalMaxSeconds, maximumSeconds);
    CHECK_EQ(result.value.aggressiveMinSeconds, uint32(10));
    CHECK_EQ(result.value.aggressiveMaxSeconds, uint32(20));
    CHECK(!warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::NormalInterval));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::AggressiveInterval));
}

TEST(WardenConfiguration_zero_aggressive_interval_falls_back_as_a_pair)
{
    auto raw = ValidCustomConfiguration();
    raw.aggressiveMinSeconds = 0;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(result.value.aggressiveMinSeconds, uint32(10));
    CHECK_EQ(result.value.aggressiveMaxSeconds, uint32(20));
    CHECK_EQ(result.value.normalMinSeconds, uint32(31));
    CHECK_EQ(result.value.normalMaxSeconds, uint32(45));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::AggressiveInterval));
}

TEST(WardenConfiguration_invalid_threshold_pair_falls_back_together)
{
    auto raw = ValidCustomConfiguration();
    raw.aggressiveThreshold = 12;
    raw.banThreshold = 12;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(result.value.aggressiveThreshold, uint32(5));
    CHECK_EQ(result.value.banThreshold, uint32(10));
    CHECK_EQ(result.value.incidentWindowSeconds, uint32(600));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::Thresholds));
}

TEST(WardenConfiguration_zero_window_falls_back_without_changing_thresholds)
{
    auto raw = ValidCustomConfiguration();
    raw.incidentWindowSeconds = 0;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(result.value.incidentWindowSeconds, uint32(900));
    CHECK_EQ(result.value.aggressiveThreshold, uint32(6));
    CHECK_EQ(result.value.banThreshold, uint32(12));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::IncidentWindow));
}

TEST(WardenConfiguration_combines_corrections_for_all_invalid_groups)
{
    warden::WardenRawConfiguration raw;
    raw.enforcementMode = 99;
    raw.normalMinSeconds = 61;
    raw.normalMaxSeconds = 60;
    raw.aggressiveMinSeconds = 0;
    raw.aggressiveThreshold = 10;
    raw.banThreshold = 10;
    raw.incidentWindowSeconds = 0;
    auto const result = warden::NormalizeWardenConfiguration(raw);

    CHECK_EQ(uint32(result.value.enforcementMode), uint32(2));
    CHECK_EQ(result.value.normalMinSeconds, uint32(30));
    CHECK_EQ(result.value.normalMaxSeconds, uint32(60));
    CHECK_EQ(result.value.aggressiveMinSeconds, uint32(10));
    CHECK_EQ(result.value.aggressiveMaxSeconds, uint32(20));
    CHECK_EQ(result.value.aggressiveThreshold, uint32(5));
    CHECK_EQ(result.value.banThreshold, uint32(10));
    CHECK_EQ(result.value.incidentWindowSeconds, uint32(900));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::EnforcementMode));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::NormalInterval));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::AggressiveInterval));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::Thresholds));
    CHECK(warden::HasWardenConfigurationCorrection(result.corrections,
        warden::WardenConfigurationCorrection::IncidentWindow));
}
