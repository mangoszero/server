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

#ifndef MANGOS_WARDEN_EVIDENCE_H
#define MANGOS_WARDEN_EVIDENCE_H

#include "WardenCheckPlan.h"
#include "WardenConfiguration.h"

#include <vector>

namespace warden
{
/** Secret-free classification for one exact ordered check definition. */
struct WardenEvidence
{
    uint32 requestId = 0;
    uint32 checkId = 0;
    WardenCheckType checkType = WardenCheckType::Timing;
    WardenEvidenceClass evidenceClass = WardenEvidenceClass::ProtocolHealth;
    WardenCheckOutcome outcome = WardenCheckOutcome::Unstable;
    uint32 clientTick = 0;
};

enum class WardenConfirmedDisposition : uint8
{
    Invalid,
    Cleared,
    Audit,
    Incident
};

bool NeedsConfirmation(WardenEvidence const& evidence);
WardenConfirmedDisposition ClassifyConfirmedEvidence(
    WardenEnforcementMode mode, WardenEvidence const& evidence);

/** One fully validated request result with no raw client payload. */
struct WardenEvidenceBatch
{
    uint32 requestId = 0;
    CheckPlanPurpose purpose = CheckPlanPurpose::Initial;
    std::vector<WardenEvidence> evidence;
};

char const* ToString(WardenCheckType type);
char const* ToString(WardenEvidenceClass evidenceClass);
char const* ToString(WardenCheckOutcome outcome);
}

#endif
