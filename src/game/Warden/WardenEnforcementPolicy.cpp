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

#include "WardenEnforcementPolicy.h"

#include "WardenServer.h"

namespace warden
{
WardenEnforcementPolicy::WardenEnforcementPolicy(
    WardenEnforcementMode mode) : m_mode(mode)
{
}

std::vector<WardenPolicyDecision> WardenEnforcementPolicy::EvaluateBatch(
    WardenEvidenceBatch const& batch)
{
    if (batch.purpose == CheckPlanPurpose::Confirmation)
    {
        if (batch.evidence.size() != 1u)
            return {ConfirmationContractViolation()};

        MemEvidence const* memory =
            std::get_if<MemEvidence>(&batch.evidence.front());
        if (!memory || m_pendingConfirmations.find(memory->checkId) ==
            m_pendingConfirmations.end())
        {
            return {ConfirmationContractViolation()};
        }

        m_pendingConfirmations.erase(memory->checkId);
        if (memory->outcome == MemOutcome::Match)
        {
            return {{WardenPolicyAction::ConfirmationCleared,
                memory->checkId, memory->outcome}};
        }

        WardenPolicyAction const action =
            m_mode == WardenEnforcementMode::Observe ?
                WardenPolicyAction::ConfirmedObservation :
                WardenPolicyAction::PersistAndKick;
        return {{action, memory->checkId, memory->outcome}};
    }

    std::vector<WardenPolicyDecision> decisions;
    for (WardenEvidence const& evidence : batch.evidence)
    {
        MemEvidence const* memory = std::get_if<MemEvidence>(&evidence);
        if (!memory || memory->outcome == MemOutcome::Match)
            continue;

        if (!m_pendingConfirmations.insert(memory->checkId).second)
            continue;

        decisions.push_back({WardenPolicyAction::QueueConfirmation,
            memory->checkId, memory->outcome});
    }

    return decisions;
}

WardenPolicyDecision WardenEnforcementPolicy::EvaluateLifecycle(
    WardenLifecycleEvent const& event)
{
    if (event.state != WardenState::Failed)
        return {};

    m_pendingConfirmations.clear();
    if (m_mode == WardenEnforcementMode::Observe)
        return {};

    return {WardenPolicyAction::Kick};
}

WardenPolicyDecision
WardenEnforcementPolicy::ConfirmationContractViolation() const
{
    if (m_mode == WardenEnforcementMode::Observe)
        return {};

    return {WardenPolicyAction::Kick};
}
}
