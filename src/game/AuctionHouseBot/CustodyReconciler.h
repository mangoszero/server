/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
