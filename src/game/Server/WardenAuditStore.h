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

#ifndef MANGOS_WARDEN_AUDIT_STORE_H
#define MANGOS_WARDEN_AUDIT_STORE_H

#include "WardenEvidence.h"

#include <cstddef>
#include <optional>
#include <string>

namespace warden
{
enum class WardenAuditOutcome : uint8
{
    Mismatch = 1,
    Unavailable = 2
};

struct WardenAuditContext
{
    uint32 accountId = 0;
    uint32 realmId = 0;
    uint32 clientBuild = 0;
    std::string clientPlatform;
    std::string clientLocale;
    uint32 checkId = 0;
    WardenCheckType checkType = WardenCheckType::Timing;
    WardenEvidenceClass evidenceClass = WardenEvidenceClass::ProtocolHealth;
    WardenAuditOutcome outcome = WardenAuditOutcome::Mismatch;
};

inline std::optional<WardenAuditOutcome> ToAuditOutcome(
    WardenCheckOutcome outcome)
{
    if (outcome == WardenCheckOutcome::Mismatch)
        return WardenAuditOutcome::Mismatch;
    if (outcome == WardenCheckOutcome::Unavailable)
        return WardenAuditOutcome::Unavailable;
    return std::nullopt;
}

inline bool IsValidWardenAuditContext(WardenAuditContext const& context)
{
    auto validToken = [](std::string const& value, size_t minimum,
        size_t maximum)
    {
        if (value.size() < minimum || value.size() > maximum)
            return false;
        for (unsigned char byte : value)
        {
            if (byte == 0 || byte == 0x09 || byte == 0x0A || byte == 0x0B ||
                byte == 0x0C || byte == 0x0D || byte == 0x20)
                return false;
        }
        return true;
    };
    auto legalPair = [](WardenCheckType type,
        WardenEvidenceClass evidenceClass)
    {
        if (type == WardenCheckType::Mpq)
        {
            return evidenceClass == WardenEvidenceClass::IntegrityInvariant ||
                evidenceClass == WardenEvidenceClass::Corroboration;
        }
        if (type == WardenCheckType::Lua)
            return evidenceClass == WardenEvidenceClass::Corroboration;
        if (type == WardenCheckType::Mem)
            return evidenceClass != WardenEvidenceClass::ProtocolHealth &&
                uint32(evidenceClass) <=
                    uint32(WardenEvidenceClass::Corroboration);
        return false;
    };

    return context.accountId != 0 && context.checkId != 0 &&
        context.clientBuild != 0 && context.clientBuild <= 0xFFFFu &&
        validToken(context.clientPlatform, 1, 4) &&
        validToken(context.clientLocale, 4, 4) &&
        legalPair(context.checkType, context.evidenceClass) &&
        (context.outcome == WardenAuditOutcome::Mismatch ||
            context.outcome == WardenAuditOutcome::Unavailable);
}

/** Best-effort append-only storage for confirmed non-actionable findings. */
class WardenAuditStore
{
public:
    static WardenAuditStore& Instance();
    bool Record(WardenAuditContext const& context) const;
};
}

#endif
