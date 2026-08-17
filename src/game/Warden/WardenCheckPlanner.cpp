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
#include "Util.h"

#include <algorithm>
#include <utility>

namespace
{
bool DefinitionOrder(warden::WardenCheckDefinition const& left,
    warden::WardenCheckDefinition const& right)
{
    if (left.sortOrder != right.sortOrder)
        return left.sortOrder < right.sortOrder;
    return warden::GetWardenCheckId(left) < warden::GetWardenCheckId(right);
}
}

namespace warden
{
WardenCheckPlanner::WardenCheckPlanner(WardenConfiguration configuration,
    uint32 eligibilityDelayMs, std::vector<WardenCheckDefinition> checks,
    WardenRandomRange randomRange)
    : m_configuration(configuration),
      m_eligibilityDelayMs(eligibilityDelayMs), m_checks(std::move(checks)),
      m_randomRange(std::move(randomRange))
{
    std::sort(m_checks.begin(), m_checks.end(), DefinitionOrder);
    for (WardenCheckDefinition const& check : m_checks)
    {
        if (GetWardenCheckType(check) != WardenCheckType::Timing)
            m_nonHealthChecks.push_back(check);
        if (IsActionableEvidenceClass(check.evidenceClass))
            m_actionableChecks.push_back(check);
    }

    if (!m_randomRange)
    {
        m_randomRange = [](uint32 minimum, uint32 maximum)
        {
            return urand(minimum, maximum);
        };
    }
}

std::optional<CheckPlan> WardenCheckPlanner::Update(bool eligible,
    uint32 diffMs)
{
    if (m_outstanding || m_checks.empty())
        return std::nullopt;

    // Confirmations are immediate and isolated, even if eligibility changed
    // after the originating batch completed.
    while (!m_confirmationQueue.empty())
    {
        uint32 const checkId = m_confirmationQueue.front();
        m_confirmationQueue.pop_front();
        auto const position = std::find_if(m_nonHealthChecks.begin(),
            m_nonHealthChecks.end(), [checkId](WardenCheckDefinition const& check)
            {
                return GetWardenCheckId(check) == checkId;
            });
        if (position != m_nonHealthChecks.end())
        {
            return BeginPlan(CheckPlanPurpose::Confirmation, {*position});
        }
        m_queuedConfirmationIds.erase(checkId);
    }

    // Repeated offenders receive one actionable-only batch as soon as the
    // module is ready at character selection. Observation-only profiles stay
    // inert without retrying the empty batch on every update.
    if (m_aggressive && !m_aggressiveImmediateIssued)
    {
        m_aggressiveImmediateIssued = true;
        std::vector<WardenCheckDefinition> checks = BuildAggressiveChecks();
        if (!checks.empty())
        {
            return BeginPlan(CheckPlanPurpose::AggressiveImmediate,
                std::move(checks));
        }
    }

    if (!m_initialComplete)
    {
        if (!eligible)
        {
            m_eligibleMs = 0;
            return std::nullopt;
        }

        if (m_eligibleMs < m_eligibilityDelayMs &&
            diffMs < m_eligibilityDelayMs - m_eligibleMs)
        {
            m_eligibleMs += diffMs;
            return std::nullopt;
        }
        return BeginPlan(CheckPlanPurpose::Initial, BuildInitialChecks());
    }

    // Recurring time advances only while the player is eligible and never
    // catches up after loading screens or another completed plan.
    if (!eligible)
        return std::nullopt;
    if (!m_recurringTargetMs)
        ScheduleNextInterval();
    if (m_recurringElapsedMs < m_recurringTargetMs &&
        diffMs < m_recurringTargetMs - m_recurringElapsedMs)
    {
        m_recurringElapsedMs += diffMs;
        return std::nullopt;
    }

    if (m_aggressive)
    {
        std::vector<WardenCheckDefinition> checks = BuildAggressiveChecks();
        if (checks.empty())
        {
            ScheduleNextInterval();
            return std::nullopt;
        }
        return BeginPlan(CheckPlanPurpose::AggressiveRecurring,
            std::move(checks));
    }

    std::vector<WardenCheckDefinition> checks = BuildNormalRecurringChecks();
    if (checks.empty())
    {
        ScheduleNextInterval();
        return std::nullopt;
    }
    return BeginPlan(CheckPlanPurpose::Recurring, std::move(checks));
}

bool WardenCheckPlanner::QueueConfirmation(uint32 checkId)
{
    auto const definition = std::find_if(m_nonHealthChecks.begin(),
        m_nonHealthChecks.end(), [checkId](WardenCheckDefinition const& check)
        {
            return GetWardenCheckId(check) == checkId &&
                IsConfirmationEligible(check);
        });
    if (definition == m_nonHealthChecks.end() ||
        !m_queuedConfirmationIds.insert(checkId).second)
        return false;

    m_confirmationQueue.push_back(checkId);
    return true;
}

void WardenCheckPlanner::Complete(CheckPlan const& plan)
{
    if (!m_outstanding || plan.requestId != m_outstandingRequestId)
        return;

    m_outstanding = false;
    m_outstandingRequestId = 0;
    if (plan.purpose == CheckPlanPurpose::Confirmation &&
        plan.checks.size() == 1)
        m_queuedConfirmationIds.erase(GetWardenCheckId(plan.checks[0]));
    else if (plan.purpose == CheckPlanPurpose::Initial)
    {
        m_initialComplete = true;
        m_eligibleMs = m_eligibilityDelayMs;
    }

    if (plan.purpose == CheckPlanPurpose::Initial ||
        plan.purpose == CheckPlanPurpose::Recurring ||
        plan.purpose == CheckPlanPurpose::AggressiveRecurring ||
        plan.purpose == CheckPlanPurpose::Confirmation)
        ScheduleNextInterval();
}

void WardenCheckPlanner::SetAggressive(bool aggressive)
{
    if (m_aggressive == aggressive)
        return;
    m_aggressive = aggressive;
    m_recurringElapsedMs = 0;
    m_recurringTargetMs = 0;
}

CheckPlan WardenCheckPlanner::BeginPlan(CheckPlanPurpose purpose,
    std::vector<WardenCheckDefinition> checks)
{
    CheckPlan plan;
    plan.requestId = m_nextRequestId++;
    plan.purpose = purpose;
    plan.checks = std::move(checks);
    m_outstanding = true;
    m_outstandingRequestId = plan.requestId;
    return plan;
}

std::vector<WardenCheckDefinition>
WardenCheckPlanner::BuildInitialChecks() const
{
    return m_checks;
}

std::vector<WardenCheckDefinition>
WardenCheckPlanner::BuildNormalRecurringChecks()
{
    if (m_nonHealthChecks.empty())
    {
        auto const timing = std::find_if(m_checks.begin(), m_checks.end(),
            [](WardenCheckDefinition const& check)
            {
                return GetWardenCheckType(check) == WardenCheckType::Timing;
            });
        return timing == m_checks.end() ?
            std::vector<WardenCheckDefinition>() :
            std::vector<WardenCheckDefinition>{*timing};
    }
    if (m_normalBagOffset >= m_normalBag.size())
    {
        m_normalBag = m_nonHealthChecks;
        Shuffle(m_normalBag);
        m_normalBagOffset = 0;
    }

    std::vector<WardenCheckDefinition> checks;
    auto const timing = std::find_if(m_checks.begin(), m_checks.end(),
        [](WardenCheckDefinition const& check)
        {
            return GetWardenCheckType(check) == WardenCheckType::Timing;
        });
    if (timing != m_checks.end())
        checks.push_back(*timing);
    for (uint32 count = 0;
        count < 2 && m_normalBagOffset < m_normalBag.size(); ++count)
        checks.push_back(m_normalBag[m_normalBagOffset++]);
    std::sort(checks.begin(), checks.end(), DefinitionOrder);
    return checks;
}

std::vector<WardenCheckDefinition>
WardenCheckPlanner::BuildAggressiveChecks() const
{
    return m_actionableChecks;
}

void WardenCheckPlanner::Shuffle(
    std::vector<WardenCheckDefinition>& checks)
{
    // Fisher-Yates controls selection only. The chosen wire subset is sorted
    // back into canonical catalogue order before BeginPlan.
    for (size_t remaining = checks.size(); remaining > 1; --remaining)
    {
        uint32 const selected = m_randomRange(0,
            static_cast<uint32>(remaining - 1));
        std::swap(checks[remaining - 1], checks[selected]);
    }
}

void WardenCheckPlanner::ScheduleNextInterval()
{
    uint32 const seconds = m_aggressive ?
        m_randomRange(m_configuration.aggressiveMinSeconds,
            m_configuration.aggressiveMaxSeconds) :
        m_randomRange(m_configuration.normalMinSeconds,
            m_configuration.normalMaxSeconds);
    m_recurringElapsedMs = 0;
    m_recurringTargetMs = seconds * uint32(1000);
}

std::vector<CheckPlan> BuildWardenPreflightPlans(
    WardenCheckProfile const& profile)
{
    if (profile.checks.empty())
        return {};

    bool hasTiming = false;
    bool hasNonHealth = false;
    for (WardenCheckDefinition const& check : profile.checks)
    {
        if (GetWardenCheckType(check) == WardenCheckType::Timing)
            hasTiming = true;
        else
            hasNonHealth = true;
    }
    if (!hasTiming || !hasNonHealth)
        return {};

    std::vector<CheckPlan> plans;
    plans.reserve(1 + profile.checks.size());
    CheckPlan initial;
    initial.requestId = 1;
    initial.purpose = CheckPlanPurpose::Initial;
    initial.checks = profile.checks;
    plans.push_back(std::move(initial));

    for (WardenCheckDefinition const& check : profile.checks)
    {
        if (!IsConfirmationEligible(check))
            continue;
        CheckPlan confirmation;
        confirmation.requestId = 1;
        confirmation.purpose = CheckPlanPurpose::Confirmation;
        confirmation.checks.push_back(check);
        plans.push_back(std::move(confirmation));
    }
    return plans;
}
}
