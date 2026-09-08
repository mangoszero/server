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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_CUSTODY_RECONCILER_H
#define MANGOS_CUSTODY_RECONCILER_H

#include "CustodyLedger.h"

#include <string>
#include <unordered_map>
#include <vector>

enum CustodyFindingReason
{
    CUSTODY_FINDING_MISSING,
    CUSTODY_FINDING_DUPLICATE,
    CUSTODY_FINDING_MISMATCHED,
    CUSTODY_FINDING_UNEXPECTED,
    CUSTODY_FINDING_ORPHAN_PLAYER,
    CUSTODY_FINDING_INVALID_MARKER,
    CUSTODY_FINDING_DUPLICATE_MARKER,
    CUSTODY_FINDING_SWEEP_OWNED_MARKER,
};

enum CustodyRepairOwnership
{
    CUSTODY_REPAIR_GENERIC,
    CUSTODY_REPAIR_MANUAL_ONLY,
    CUSTODY_REPAIR_BOT_SWEEP,
};

enum CustodyFindingState
{
    CUSTODY_FINDING_CONFIRMED,
    CUSTODY_FINDING_PENDING,
};

enum CustodyScanContext
{
    CUSTODY_SCAN_BOOT,
    CUSTODY_SCAN_RUNTIME,
};

struct CustodyFinding
{
    CustodyRow row;
    CustodyFindingReason reason;
    CustodyRepairOwnership repairOwnership;
    CustodyFindingState state;
};

struct CustodyReconcileReport
{
    std::vector<CustodyFinding> findings;
    uint32 confirmedDriftCount;
    uint32 pendingBidCount;
    uint32 sweepOwnedCount;
    uint64 rowVisits;
};

struct CustodyMaintenancePlan
{
    bool reconcile;
    bool prune;
    bool sweepBotMaterializations;
};

class CustodyDetailBudget
{
    public:
        explicit CustodyDetailBudget(uint32 cap);

        bool Take();
        uint32 Allowed() const { return m_allowed; }
        uint32 Suppressed() const { return m_suppressed; }

    private:
        uint32 m_cap;
        uint32 m_allowed;
        uint32 m_suppressed;
};

char const* CustodyFindingReasonName(CustodyFindingReason reason);
char const* CustodyRepairOwnershipName(CustodyRepairOwnership ownership);
char const* CustodyFindingStateName(CustodyFindingState state);

class CustodyReconciler
{
    public:
        void Scan(std::vector<CustodySnapshotGroup> const& groups, uint64 now,
                  CustodyScanContext context, CustodyReconcileReport& report);

        void Reset();

    private:
        struct PendingBidMismatch
        {
            std::string fingerprint;
            uint64 firstSeen;
        };

        std::unordered_map<uint32, PendingBidMismatch> m_pendingBidMismatches;
};

#endif // MANGOS_CUSTODY_RECONCILER_H
