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

#include "WardenCheckPlanner.h"

namespace warden
{
WardenCheckPlanner::WardenCheckPlanner(uint32 eligibilityDelayMs,
    std::optional<MpqCheckProfile> mpqCheck,
    std::optional<LuaCheckProfile> luaCheck)
    : m_eligibilityDelayMs(eligibilityDelayMs), m_mpqCheck(mpqCheck),
      m_luaCheck(luaCheck)
{
}

std::optional<CheckPlan> WardenCheckPlanner::Update(bool eligible,
    uint32 diffMs)
{
    if (m_issued)
        return std::nullopt;

    if (!eligible)
    {
        m_eligibleMs = 0;
        return std::nullopt;
    }

    // Compare against the remaining delay instead of adding first, so an
    // unusually large world update cannot wrap the cumulative timer.
    if (m_eligibleMs < m_eligibilityDelayMs &&
        diffMs < m_eligibilityDelayMs - m_eligibleMs)
    {
        m_eligibleMs += diffMs;
        return std::nullopt;
    }

    CheckPlan plan;
    plan.requestId = 1;
    plan.checks.emplace_back(TimingCheck{});
    if (m_mpqCheck)
        plan.checks.emplace_back(*m_mpqCheck);
    if (m_luaCheck)
        plan.checks.emplace_back(*m_luaCheck);

    m_issued = true;
    return plan;
}
}
