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

#ifndef MANGOS_WARDEN_CONFIGURATION_H
#define MANGOS_WARDEN_CONFIGURATION_H

#include "Platform/Define.h"

namespace warden
{
/** Controls which confirmed failures may affect a connected account. */
enum class WardenEnforcementMode : uint8
{
    Observe = 0,
    Kick = 1,
    KickAndBan = 2
};

/** Untrusted values read directly from mangosd.conf. */
struct WardenRawConfiguration
{
    uint32 enforcementMode = 2;
    uint32 normalMinSeconds = 30;
    uint32 normalMaxSeconds = 60;
    uint32 aggressiveMinSeconds = 10;
    uint32 aggressiveMaxSeconds = 20;
    uint32 aggressiveThreshold = 5;
    uint32 banThreshold = 10;
    uint32 incidentWindowSeconds = 900;
};

/** Values safe for planners and enforcement policy to consume. */
struct WardenConfiguration
{
    WardenEnforcementMode enforcementMode =
        WardenEnforcementMode::KickAndBan;
    uint32 normalMinSeconds = 30;
    uint32 normalMaxSeconds = 60;
    uint32 aggressiveMinSeconds = 10;
    uint32 aggressiveMaxSeconds = 20;
    uint32 aggressiveThreshold = 5;
    uint32 banThreshold = 10;
    uint32 incidentWindowSeconds = 900;
};

/** Invalid groups are reported separately so the loader can explain repairs. */
enum class WardenConfigurationCorrection : uint32
{
    None = 0,
    EnforcementMode = 1u << 0,
    NormalInterval = 1u << 1,
    AggressiveInterval = 1u << 2,
    Thresholds = 1u << 3,
    IncidentWindow = 1u << 4
};

struct WardenConfigurationNormalization
{
    WardenConfiguration value;
    WardenConfigurationCorrection corrections =
        WardenConfigurationCorrection::None;
};

/**
 * Replaces each invalid logical group with its approved safe defaults while
 * preserving every independent valid group.
 */
WardenConfigurationNormalization NormalizeWardenConfiguration(
    WardenRawConfiguration const& raw);

bool HasWardenConfigurationCorrection(
    WardenConfigurationCorrection corrections,
    WardenConfigurationCorrection correction);
}

#endif
