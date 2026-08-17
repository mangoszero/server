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

#ifndef MANGOS_WARDEN_CHECK_PLANNER_H
#define MANGOS_WARDEN_CHECK_PLANNER_H

#include "WardenCheckPlan.h"
#include "WardenConfiguration.h"

#include <cstddef>
#include <deque>
#include <functional>
#include <optional>
#include <unordered_set>
#include <vector>

namespace warden
{
using WardenRandomRange = std::function<uint32(uint32, uint32)>;

/**
 * Owns initial eligibility, recurring cadence, bounded MEM coverage, and
 * isolated confirmation ordering without transport or punishment knowledge.
 */
class WardenCheckPlanner
{
public:
    explicit WardenCheckPlanner(WardenConfiguration configuration = {},
        uint32 eligibilityDelayMs = 1000,
        std::vector<WardenCheckDefinition> checks = {},
        WardenRandomRange randomRange = {});

    std::optional<CheckPlan> Update(bool eligible, uint32 diffMs);
    bool QueueConfirmation(uint32 checkId);
    void Complete(CheckPlan const& plan);
    void SetAggressive(bool aggressive);

private:
    CheckPlan BeginPlan(CheckPlanPurpose purpose,
        std::vector<WardenCheckDefinition> checks);
    std::vector<WardenCheckDefinition> BuildInitialChecks() const;
    std::vector<WardenCheckDefinition> BuildNormalRecurringChecks();
    std::vector<WardenCheckDefinition> BuildAggressiveChecks() const;
    void Shuffle(std::vector<WardenCheckDefinition>& checks);
    void ScheduleNextInterval();

    WardenConfiguration m_configuration;
    uint32 m_eligibilityDelayMs;
    std::vector<WardenCheckDefinition> m_checks;
    std::vector<WardenCheckDefinition> m_nonHealthChecks;
    std::vector<WardenCheckDefinition> m_actionableChecks;
    WardenRandomRange m_randomRange;

    uint32 m_eligibleMs = 0;
    uint32 m_nextRequestId = 1;
    uint32 m_outstandingRequestId = 0;
    uint32 m_recurringElapsedMs = 0;
    uint32 m_recurringTargetMs = 0;
    bool m_initialComplete = false;
    bool m_outstanding = false;
    bool m_aggressive = false;
    bool m_aggressiveImmediateIssued = false;

    std::vector<WardenCheckDefinition> m_normalBag;
    size_t m_normalBagOffset = 0;
    std::deque<uint32> m_confirmationQueue;
    std::unordered_set<uint32> m_queuedConfirmationIds;
};

std::vector<CheckPlan> BuildWardenPreflightPlans(
    WardenCheckProfile const& profile);
}

#endif
