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

#include "WardenEvidence.h"

namespace
{
bool IsLegalIdentity(warden::WardenCheckType type,
    warden::WardenEvidenceClass evidenceClass)
{
    switch (type)
    {
        case warden::WardenCheckType::Timing:
            return evidenceClass ==
                warden::WardenEvidenceClass::ProtocolHealth;
        case warden::WardenCheckType::Mpq:
            return evidenceClass ==
                    warden::WardenEvidenceClass::IntegrityInvariant ||
                evidenceClass == warden::WardenEvidenceClass::Corroboration;
        case warden::WardenCheckType::Lua:
            return evidenceClass ==
                warden::WardenEvidenceClass::Corroboration;
        case warden::WardenCheckType::Mem:
            return evidenceClass ==
                    warden::WardenEvidenceClass::IntegrityInvariant ||
                evidenceClass ==
                    warden::WardenEvidenceClass::ThreatSignature ||
                evidenceClass == warden::WardenEvidenceClass::Corroboration;
    }
    return false;
}

bool IsNonHealthType(warden::WardenCheckType type)
{
    return type == warden::WardenCheckType::Mpq ||
        type == warden::WardenCheckType::Lua ||
        type == warden::WardenCheckType::Mem;
}
}

namespace warden
{
bool NeedsConfirmation(WardenEvidence const& evidence)
{
    return evidence.checkId != 0 && IsNonHealthType(evidence.checkType) &&
        IsLegalIdentity(evidence.checkType, evidence.evidenceClass) &&
        (evidence.outcome == WardenCheckOutcome::Mismatch ||
            evidence.outcome == WardenCheckOutcome::Unavailable);
}

WardenConfirmedDisposition ClassifyConfirmedEvidence(
    WardenEnforcementMode mode, WardenEvidence const& evidence)
{
    if (!evidence.checkId || !IsNonHealthType(evidence.checkType) ||
        !IsLegalIdentity(evidence.checkType, evidence.evidenceClass))
        return WardenConfirmedDisposition::Invalid;

    if (evidence.outcome == WardenCheckOutcome::Match)
        return WardenConfirmedDisposition::Cleared;
    if (evidence.outcome == WardenCheckOutcome::Unavailable)
        return WardenConfirmedDisposition::Audit;
    if (evidence.outcome != WardenCheckOutcome::Mismatch)
        return WardenConfirmedDisposition::Invalid;
    if (!IsActionableEvidenceClass(evidence.evidenceClass) ||
        mode == WardenEnforcementMode::Observe)
        return WardenConfirmedDisposition::Audit;
    return WardenConfirmedDisposition::Incident;
}

char const* ToString(WardenCheckType type)
{
    switch (type)
    {
        case WardenCheckType::Timing: return "Timing";
        case WardenCheckType::Lua: return "Lua";
        case WardenCheckType::Mpq: return "MPQ";
        case WardenCheckType::Mem: return "MEM";
    }
    return "Unknown";
}

char const* ToString(WardenEvidenceClass evidenceClass)
{
    switch (evidenceClass)
    {
        case WardenEvidenceClass::ProtocolHealth: return "ProtocolHealth";
        case WardenEvidenceClass::IntegrityInvariant:
            return "IntegrityInvariant";
        case WardenEvidenceClass::ThreatSignature: return "ThreatSignature";
        case WardenEvidenceClass::Corroboration: return "Corroboration";
    }
    return "Unknown";
}

char const* ToString(WardenCheckOutcome outcome)
{
    switch (outcome)
    {
        case WardenCheckOutcome::Match: return "Match";
        case WardenCheckOutcome::Mismatch: return "Mismatch";
        case WardenCheckOutcome::Unavailable: return "Unavailable";
        case WardenCheckOutcome::Stable: return "Stable";
        case WardenCheckOutcome::Unstable: return "Unstable";
    }
    return "Unknown";
}
}
