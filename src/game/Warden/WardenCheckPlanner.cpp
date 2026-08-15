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

namespace warden
{
WardenCheckPlanner::WardenCheckPlanner(WardenConfiguration configuration,
    uint32 eligibilityDelayMs,
    std::optional<MpqCheckProfile> mpqCheck,
    std::optional<LuaCheckProfile> luaCheck,
    std::vector<MemCheckProfile> memChecks, WardenRandomRange randomRange)
    : m_configuration(configuration),
      m_eligibilityDelayMs(eligibilityDelayMs), m_mpqCheck(std::move(mpqCheck)),
      m_luaCheck(std::move(luaCheck)), m_memChecks(std::move(memChecks)),
      m_randomRange(std::move(randomRange))
{
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
    if (m_outstanding)
        return std::nullopt;

    // Confirmations are immediate and isolated, even if eligibility changed
    // after the originating batch completed.
    if (!m_confirmationQueue.empty())
    {
        uint32 const checkId = m_confirmationQueue.front();
        m_confirmationQueue.pop_front();
        auto const position = std::find_if(m_memChecks.begin(),
            m_memChecks.end(), [checkId](MemCheckProfile const& profile)
            {
                return profile.checkId == checkId;
            });
        if (position != m_memChecks.end())
        {
            std::vector<PlannedCheck> checks;
            checks.emplace_back(*position);
            return BeginPlan(CheckPlanPurpose::Confirmation,
                std::move(checks));
        }
        m_queuedConfirmationIds.erase(checkId);
    }

    // Repeated offenders receive one MEM-only batch as soon as the module is
    // ready at character selection. Empty catalogues cannot form a wire plan.
    if (m_aggressive && !m_aggressiveImmediateIssued)
    {
        m_aggressiveImmediateIssued = true;
        std::vector<PlannedCheck> checks = BuildAggressiveChecks(false);
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

        // Compare against the remaining delay before adding, so a large world
        // update cannot wrap the cumulative initial timer.
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
        return BeginPlan(CheckPlanPurpose::AggressiveRecurring,
            BuildAggressiveChecks(true));
    }
    return BeginPlan(CheckPlanPurpose::Recurring,
        BuildNormalRecurringChecks());
}

bool WardenCheckPlanner::QueueConfirmation(uint32 checkId)
{
    auto const profile = std::find_if(m_memChecks.begin(), m_memChecks.end(),
        [checkId](MemCheckProfile const& candidate)
        {
            return candidate.checkId == checkId;
        });
    if (profile == m_memChecks.end() ||
        !m_queuedConfirmationIds.insert(checkId).second)
    {
        return false;
    }

    m_confirmationQueue.push_back(checkId);
    return true;
}

void WardenCheckPlanner::Complete(CheckPlan const& plan)
{
    if (!m_outstanding || plan.requestId != m_outstandingRequestId)
        return;

    m_outstanding = false;
    m_outstandingRequestId = 0;

    if (plan.purpose == CheckPlanPurpose::Confirmation)
    {
        for (PlannedCheck const& check : plan.checks)
        {
            if (auto const* mem = std::get_if<MemCheckProfile>(&check))
            {
                m_queuedConfirmationIds.erase(mem->checkId);
                break;
            }
        }
    }
    else if (plan.purpose == CheckPlanPurpose::Initial)
    {
        m_initialComplete = true;
        m_eligibleMs = m_eligibilityDelayMs;
    }

    if (plan.purpose == CheckPlanPurpose::Initial ||
        plan.purpose == CheckPlanPurpose::Recurring ||
        plan.purpose == CheckPlanPurpose::AggressiveRecurring ||
        plan.purpose == CheckPlanPurpose::Confirmation)
    {
        ScheduleNextInterval();
    }
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
    std::vector<PlannedCheck> checks)
{
    CheckPlan plan;
    plan.requestId = m_nextRequestId++;
    plan.purpose = purpose;
    plan.checks = std::move(checks);
    m_outstanding = true;
    m_outstandingRequestId = plan.requestId;
    return plan;
}

std::vector<PlannedCheck> WardenCheckPlanner::BuildInitialChecks() const
{
    std::vector<PlannedCheck> checks;
    checks.emplace_back(TimingCheck{});
    if (m_mpqCheck)
        checks.emplace_back(*m_mpqCheck);
    if (m_luaCheck)
        checks.emplace_back(*m_luaCheck);
    for (MemCheckProfile const& memCheck : m_memChecks)
        checks.emplace_back(memCheck);
    return checks;
}

std::vector<PlannedCheck> WardenCheckPlanner::BuildNormalRecurringChecks()
{
    if (m_normalMemBagOffset >= m_normalMemBag.size())
    {
        m_normalMemBag = m_memChecks;
        Shuffle(m_normalMemBag);
        m_normalMemBagOffset = 0;
    }

    std::vector<PlannedCheck> checks;
    checks.emplace_back(TimingCheck{});
    for (uint32 count = 0;
        count < 2 && m_normalMemBagOffset < m_normalMemBag.size(); ++count)
    {
        checks.emplace_back(m_normalMemBag[m_normalMemBagOffset++]);
    }
    return checks;
}

std::vector<PlannedCheck> WardenCheckPlanner::BuildAggressiveChecks(
    bool shuffle)
{
    std::vector<MemCheckProfile> profiles = m_memChecks;
    if (shuffle)
        Shuffle(profiles);

    std::vector<PlannedCheck> checks;
    checks.reserve(profiles.size());
    for (MemCheckProfile const& profile : profiles)
        checks.emplace_back(profile);
    return checks;
}

void WardenCheckPlanner::Shuffle(std::vector<MemCheckProfile>& checks)
{
    // Fisher-Yates uses the same injectable inclusive range source as cadence,
    // making exact coverage and order deterministic in tests.
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
}
