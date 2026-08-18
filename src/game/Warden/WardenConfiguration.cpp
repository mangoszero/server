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

#include "WardenConfiguration.h"

#include <limits>

namespace
{
uint32 constexpr MaxPlannerSeconds =
    std::numeric_limits<uint32>::max() / uint32(1000);

bool IsValidInterval(uint32 minimum, uint32 maximum)
{
    return minimum > 0 && minimum <= maximum &&
        maximum <= MaxPlannerSeconds;
}

void AddCorrection(warden::WardenConfigurationCorrection& corrections,
    warden::WardenConfigurationCorrection correction)
{
    corrections = static_cast<warden::WardenConfigurationCorrection>(
        static_cast<uint32>(corrections) | static_cast<uint32>(correction));
}
}

namespace warden
{
WardenConfigurationNormalization NormalizeWardenConfiguration(
    WardenRawConfiguration const& raw)
{
    WardenConfigurationNormalization result;
    result.value.requireExactProfile = raw.requireExactProfile;

    if (raw.enforcementMode <=
        static_cast<uint32>(WardenEnforcementMode::KickAndBan))
    {
        result.value.enforcementMode =
            static_cast<WardenEnforcementMode>(raw.enforcementMode);
    }
    else
    {
        AddCorrection(result.corrections,
            WardenConfigurationCorrection::EnforcementMode);
    }

    if (IsValidInterval(raw.normalMinSeconds, raw.normalMaxSeconds))
    {
        result.value.normalMinSeconds = raw.normalMinSeconds;
        result.value.normalMaxSeconds = raw.normalMaxSeconds;
    }
    else
    {
        AddCorrection(result.corrections,
            WardenConfigurationCorrection::NormalInterval);
    }

    if (IsValidInterval(raw.aggressiveMinSeconds,
        raw.aggressiveMaxSeconds))
    {
        result.value.aggressiveMinSeconds = raw.aggressiveMinSeconds;
        result.value.aggressiveMaxSeconds = raw.aggressiveMaxSeconds;
    }
    else
    {
        AddCorrection(result.corrections,
            WardenConfigurationCorrection::AggressiveInterval);
    }

    if (raw.aggressiveThreshold > 0 &&
        raw.aggressiveThreshold < raw.banThreshold)
    {
        result.value.aggressiveThreshold = raw.aggressiveThreshold;
        result.value.banThreshold = raw.banThreshold;
    }
    else
    {
        AddCorrection(result.corrections,
            WardenConfigurationCorrection::Thresholds);
    }

    if (raw.incidentWindowSeconds > 0)
        result.value.incidentWindowSeconds = raw.incidentWindowSeconds;
    else
        AddCorrection(result.corrections,
            WardenConfigurationCorrection::IncidentWindow);

    return result;
}

bool HasWardenConfigurationCorrection(
    WardenConfigurationCorrection corrections,
    WardenConfigurationCorrection correction)
{
    return (static_cast<uint32>(corrections) &
        static_cast<uint32>(correction)) != 0;
}
}
