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

#ifndef MANGOS_WARDEN_ENFORCEMENT_POLICY_H
#define MANGOS_WARDEN_ENFORCEMENT_POLICY_H

#include "WardenConfiguration.h"
#include "WardenEvidence.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace warden
{
struct WardenLifecycleEvent;

/** A secret-free instruction for the session-level enforcement adapter. */
enum class WardenPolicyAction : uint8
{
    None,
    QueueConfirmation,
    ConfirmationCleared,
    PersistAudit,
    PersistAndKick,
    Kick
};

struct WardenPolicyDecision
{
    WardenPolicyAction action = WardenPolicyAction::None;
    uint32 checkId = 0;
    WardenCheckType checkType = WardenCheckType::Timing;
    WardenEvidenceClass evidenceClass = WardenEvidenceClass::ProtocolHealth;
    WardenCheckOutcome outcome = WardenCheckOutcome::Match;
};

/**
 * Confirms typed Warden evidence without knowing accounts, sessions,
 * databases, logs, packet bytes, addresses, or expected/returned bytes.
 */
class WardenEnforcementPolicy
{
public:
    explicit WardenEnforcementPolicy(WardenEnforcementMode mode);

    std::vector<WardenPolicyDecision> EvaluateBatch(
        WardenEvidenceBatch const& batch);
    WardenPolicyDecision EvaluateLifecycle(
        WardenLifecycleEvent const& event);

private:
    struct PendingConfirmation
    {
        WardenCheckType checkType = WardenCheckType::Timing;
        WardenEvidenceClass evidenceClass =
            WardenEvidenceClass::ProtocolHealth;
    };

    WardenPolicyDecision ConfirmationContractViolation() const;
    static uint64 AuditKey(uint32 checkId, WardenCheckOutcome outcome);

    WardenEnforcementMode m_mode;
    std::unordered_map<uint32, PendingConfirmation> m_pendingConfirmations;
    std::unordered_set<uint64> m_confirmedAudits;
};
}

#endif
