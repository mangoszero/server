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

#ifndef MANGOS_WARDEN_INCIDENT_STORE_H
#define MANGOS_WARDEN_INCIDENT_STORE_H

#include "WardenConfiguration.h"
#include "WardenEvidence.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace warden
{
enum class WardenIncidentOutcome : uint8
{
    ByteMismatch = 1,
    Unavailable = 2
};

struct WardenIncidentContext
{
    uint32 accountId = 0;
    uint32 realmId = 0;
    uint32 clientBuild = 0;
    std::string clientLocale;
    uint32 checkId = 0;
    WardenIncidentOutcome outcome = WardenIncidentOutcome::Unavailable;
};

struct WardenIncidentWindowState
{
    uint32 recentCount = 0;
    uint64 aggressiveUntil = 0;
};

enum class WardenIncidentWriteStatus : uint8
{
    Failed,
    Committed,
    CommittedStateUnavailable
};

struct WardenIncidentWriteResult
{
    WardenIncidentWriteStatus status = WardenIncidentWriteStatus::Failed;
    uint32 recentCount = 0;
    uint64 aggressiveUntil = 0;
    bool aggressiveTransition = false;
    bool permanentBanTriggered = false;
};

/** Session action derived without exposing database implementation details. */
struct WardenIncidentApplication
{
    bool mustKick = true;
    bool durable = false;
    bool summaryKnown = false;
    uint32 recentCount = 0;
    uint64 aggressiveUntil = 0;
    bool aggressiveTransition = false;
    bool permanentBanTriggered = false;
};

namespace detail
{
inline WardenIncidentWindowState ClassifyRecentIncidentWindow(
    std::vector<uint64> recent, uint32 incidentWindowSeconds,
    uint32 aggressiveThreshold)
{
    WardenIncidentWindowState state;
    if (incidentWindowSeconds == 0 || aggressiveThreshold == 0)
        return state;

    std::sort(recent.begin(), recent.end(), std::greater<uint64>());
    state.recentCount = static_cast<uint32>(std::min<size_t>(recent.size(),
        std::numeric_limits<uint32>::max()));
    if (recent.size() < aggressiveThreshold)
        return state;

    uint64 const thresholdNewest = recent[aggressiveThreshold - 1u];
    uint64 const maximum = std::numeric_limits<uint64>::max();
    state.aggressiveUntil =
        thresholdNewest > maximum - incidentWindowSeconds ?
            maximum : thresholdNewest + incidentWindowSeconds;
    return state;
}
}

inline std::optional<WardenIncidentOutcome> ToIncidentOutcome(
    MemOutcome outcome)
{
    switch (outcome)
    {
        case MemOutcome::ByteMismatch:
            return WardenIncidentOutcome::ByteMismatch;
        case MemOutcome::Unavailable:
            return WardenIncidentOutcome::Unavailable;
        case MemOutcome::Match:
            return std::nullopt;
    }

    return std::nullopt;
}

/** Applies the same exclusive lower boundary used by the Realm query. */
inline WardenIncidentWindowState ClassifyIncidentWindow(
    std::vector<uint64> const& timestamps, uint64 now,
    uint32 incidentWindowSeconds, uint32 aggressiveThreshold)
{
    if (incidentWindowSeconds == 0 || aggressiveThreshold == 0)
        return {};

    std::vector<uint64> recent;
    recent.reserve(timestamps.size());
    for (uint64 timestamp : timestamps)
    {
        if (now < incidentWindowSeconds ||
            timestamp > now - incidentWindowSeconds)
        {
            recent.push_back(timestamp);
        }
    }

    return detail::ClassifyRecentIncidentWindow(std::move(recent),
        incidentWindowSeconds, aggressiveThreshold);
}

/** Failed or incompletely reloaded writes never manufacture durable facts. */
inline WardenIncidentApplication ClassifyIncidentWriteResult(
    WardenIncidentWriteResult const& result)
{
    WardenIncidentApplication application;
    if (result.status == WardenIncidentWriteStatus::Failed)
        return application;

    application.durable = true;
    if (result.status ==
        WardenIncidentWriteStatus::CommittedStateUnavailable)
    {
        return application;
    }

    application.summaryKnown = true;
    application.recentCount = result.recentCount;
    application.aggressiveUntil = result.aggressiveUntil;
    application.aggressiveTransition = result.aggressiveTransition;
    application.permanentBanTriggered = result.permanentBanTriggered;
    return application;
}

/** The only adapter allowed to persist Warden enforcement data in Realm. */
class WardenIncidentStore
{
public:
    static WardenIncidentStore& Instance();

    std::optional<WardenIncidentWindowState> Load(uint32 accountId,
        uint32 incidentWindowSeconds,
        uint32 aggressiveThreshold) const;

    WardenIncidentWriteResult Record(WardenIncidentContext const& context,
        WardenConfiguration const& configuration) const;
};
}

#endif
