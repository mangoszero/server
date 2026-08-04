/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "CustodyReconciler.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace
{
    uint64 const CUSTODY_RECONCILE_MIN_ROW_AGE = 60;

    bool IsMarkerKey(std::string const& key)
    {
        return key.compare(0, 8, "botlist:") == 0;
    }

    bool IsMature(CustodyRow const& row, uint64 now)
    {
        return row.createdTime == 0 ||
               (row.createdTime <= now &&
                now - row.createdTime >= CUSTODY_RECONCILE_MIN_ROW_AGE);
    }

    CustodyRow ExpectedRow(std::string const& key, uint8 kind, uint8 role,
                           uint32 ownerGuid, uint32 amount, uint32 itemGuid,
                           uint32 auctionId)
    {
        CustodyRow row = {};
        row.idemKey = key;
        row.kind = kind;
        row.role = role;
        row.state = CST_RESERVED;
        row.ownerGuid = ownerGuid;
        row.amount = amount;
        row.itemGuid = itemGuid;
        row.auctionId = auctionId;
        return row;
    }

    bool MatchesExpected(CustodyRow const& row, uint8 kind, uint8 role,
                         uint32 ownerGuid, uint32 amount, uint32 itemGuid,
                         uint32 auctionId)
    {
        return row.kind == kind &&
               row.role == role &&
               row.state == CST_RESERVED &&
               row.ownerGuid == ownerGuid &&
               row.amount == amount &&
               row.itemGuid == itemGuid &&
               row.auctionId == auctionId;
    }

    void AddFinding(CustodyReconcileReport& report, CustodyRow const& row,
                    CustodyFindingReason reason,
                    CustodyRepairOwnership ownership,
                    CustodyFindingState state)
    {
        CustodyFinding finding;
        finding.row = row;
        finding.reason = reason;
        finding.repairOwnership = ownership;
        finding.state = state;
        report.findings.push_back(finding);

        if (ownership == CUSTODY_REPAIR_BOT_SWEEP)
        {
            ++report.sweepOwnedCount;
        }
        else if (state == CUSTODY_FINDING_PENDING)
        {
            ++report.pendingBidCount;
        }
        else
        {
            ++report.confirmedDriftCount;
        }
    }

    bool BidTupleLess(CustodyRow const* lhs, CustodyRow const* rhs)
    {
        if (lhs->id != rhs->id)
        {
            return lhs->id < rhs->id;
        }
        if (lhs->idemKey != rhs->idemKey)
        {
            return lhs->idemKey < rhs->idemKey;
        }
        if (lhs->ownerGuid != rhs->ownerGuid)
        {
            return lhs->ownerGuid < rhs->ownerGuid;
        }
        return lhs->amount < rhs->amount;
    }

    std::string BidFingerprint(CustodyAuctionFacts const& auction,
                               std::vector<CustodyRow const*> bidRows)
    {
        std::sort(bidRows.begin(), bidRows.end(), BidTupleLess);
        std::ostringstream out;
        out << uint32(auction.exists) << '|'
            << auction.auctionId << '|'
            << auction.itemGuid << '|'
            << auction.ownerGuid << '|'
            << auction.bidderGuid << '|'
            << auction.bid << '|'
            << auction.deposit;
        for (size_t i = 0; i < bidRows.size(); ++i)
        {
            CustodyRow const& row = *bidRows[i];
            out << ';' << row.id << ',' << row.idemKey << ','
                << row.ownerGuid << ',' << row.amount;
        }
        return out.str();
    }

    bool FindingLess(CustodyFinding const& lhs, CustodyFinding const& rhs)
    {
        if (lhs.row.auctionId != rhs.row.auctionId)
        {
            return lhs.row.auctionId < rhs.row.auctionId;
        }
        if (lhs.row.idemKey != rhs.row.idemKey)
        {
            return lhs.row.idemKey < rhs.row.idemKey;
        }
        if (lhs.reason != rhs.reason)
        {
            return lhs.reason < rhs.reason;
        }
        return lhs.row.id < rhs.row.id;
    }
}

CustodyDetailBudget::CustodyDetailBudget(uint32 cap)
    : m_cap(cap), m_allowed(0), m_suppressed(0)
{
}

bool CustodyDetailBudget::Take()
{
    if (m_allowed < m_cap)
    {
        ++m_allowed;
        return true;
    }

    ++m_suppressed;
    return false;
}

char const* CustodyFindingReasonName(CustodyFindingReason reason)
{
    switch (reason)
    {
        case CUSTODY_FINDING_MISSING:            return "missing";
        case CUSTODY_FINDING_DUPLICATE:          return "duplicate";
        case CUSTODY_FINDING_MISMATCHED:         return "mismatched";
        case CUSTODY_FINDING_UNEXPECTED:         return "unexpected";
        case CUSTODY_FINDING_ORPHAN_PLAYER:      return "orphan-player";
        case CUSTODY_FINDING_INVALID_MARKER:     return "invalid-marker";
        case CUSTODY_FINDING_DUPLICATE_MARKER:   return "duplicate-marker";
        case CUSTODY_FINDING_SWEEP_OWNED_MARKER: return "sweep-owned-marker";
    }

    return "unknown";
}

char const* CustodyRepairOwnershipName(CustodyRepairOwnership ownership)
{
    switch (ownership)
    {
        case CUSTODY_REPAIR_GENERIC:     return "generic";
        case CUSTODY_REPAIR_MANUAL_ONLY: return "manual-only";
        case CUSTODY_REPAIR_BOT_SWEEP:   return "bot-sweep";
    }

    return "unknown";
}

char const* CustodyFindingStateName(CustodyFindingState state)
{
    switch (state)
    {
        case CUSTODY_FINDING_CONFIRMED: return "confirmed";
        case CUSTODY_FINDING_PENDING:   return "pending";
    }

    return "unknown";
}

void CustodyReconciler::Scan(std::vector<CustodySnapshotGroup> const& groups,
                             uint64 now, CustodyScanContext context,
                             CustodyReconcileReport& report)
{
    report.findings.clear();
    report.confirmedDriftCount = 0;
    report.pendingBidCount = 0;
    report.sweepOwnedCount = 0;
    report.rowVisits = 0;

    std::unordered_set<uint32> observedBidMismatches;
    for (size_t groupIndex = 0; groupIndex < groups.size(); ++groupIndex)
    {
        CustodySnapshotGroup const& group = groups[groupIndex];
        std::vector<CustodyRow const*> markers;
        std::vector<CustodyRow const*> nonMarkers;
        std::vector<CustodyRow const*> itemRows;
        std::vector<CustodyRow const*> depositRows;
        std::vector<CustodyRow const*> extraSellerRows;
        std::vector<CustodyRow const*> bidRows;
        bool hasSellerCandidate = false;
        bool allRowsMature = true;
        std::string const itemKey = "item:" + std::to_string(group.auctionId);
        std::string const depositKey = "dep:" + std::to_string(group.auctionId);

        for (size_t rowIndex = 0; rowIndex < group.rows.size(); ++rowIndex)
        {
            CustodyRow const& row = group.rows[rowIndex];
            ++report.rowVisits;
            if (!IsMature(row, now))
            {
                allRowsMature = false;
            }

            bool const marker = IsMarkerKey(row.idemKey);
            if (marker)
            {
                markers.push_back(&row);
            }
            else
            {
                nonMarkers.push_back(&row);
                bool const canonicalItem = row.idemKey == itemKey;
                bool const canonicalDeposit = row.idemKey == depositKey;
                bool const sellerRole = row.role == ROLE_ITEM ||
                                        row.role == ROLE_DEPOSIT;
                if (canonicalItem)
                {
                    itemRows.push_back(&row);
                }
                if (canonicalDeposit)
                {
                    depositRows.push_back(&row);
                }
                if (sellerRole && !canonicalItem && !canonicalDeposit)
                {
                    extraSellerRows.push_back(&row);
                }
                hasSellerCandidate = hasSellerCandidate || canonicalItem ||
                                     canonicalDeposit || sellerRole;
            }

            if (row.role == ROLE_BID)
            {
                bidRows.push_back(&row);
            }
        }

        if (!allRowsMature)
        {
            m_pendingBidMismatches.erase(group.auctionId);
            continue;
        }

        if (!group.auction.exists)
        {
            m_pendingBidMismatches.erase(group.auctionId);
            for (size_t i = 0; i < markers.size(); ++i)
            {
                AddFinding(report, *markers[i], CUSTODY_FINDING_SWEEP_OWNED_MARKER,
                    CUSTODY_REPAIR_BOT_SWEEP, CUSTODY_FINDING_CONFIRMED);
            }
            for (size_t i = 0; i < nonMarkers.size(); ++i)
            {
                AddFinding(report, *nonMarkers[i], CUSTODY_FINDING_ORPHAN_PLAYER,
                    CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED);
            }
            continue;
        }

        bool const markerOwned = !markers.empty();
        if (markerOwned)
        {
            if (markers.size() > 1)
            {
                CustodyRow duplicate = *markers[0];
                duplicate.id = 0;
                AddFinding(report, duplicate, CUSTODY_FINDING_DUPLICATE_MARKER,
                    CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_CONFIRMED);
            }
            for (size_t i = 0; i < markers.size(); ++i)
            {
                CustodyRow const& row = *markers[i];
                if (!MatchesExpected(row, CUSTODY_ITEM, ROLE_RESOLUTION,
                        group.auction.ownerGuid, 0, group.auction.itemGuid,
                        group.auctionId))
                {
                    AddFinding(report, row, CUSTODY_FINDING_INVALID_MARKER,
                        CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_CONFIRMED);
                }
            }
        }
        else if (hasSellerCandidate)
        {
            if (itemRows.empty())
            {
                AddFinding(report, ExpectedRow(itemKey, CUSTODY_ITEM, ROLE_ITEM,
                        group.auction.ownerGuid, 0, group.auction.itemGuid,
                        group.auctionId), CUSTODY_FINDING_MISSING,
                    CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED);
            }
            else if (itemRows.size() > 1)
            {
                for (size_t i = 0; i < itemRows.size(); ++i)
                {
                    AddFinding(report, *itemRows[i], CUSTODY_FINDING_DUPLICATE,
                        CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED);
                }
            }
            else if (!MatchesExpected(*itemRows[0], CUSTODY_ITEM, ROLE_ITEM,
                         group.auction.ownerGuid, 0, group.auction.itemGuid,
                         group.auctionId))
            {
                AddFinding(report, *itemRows[0], CUSTODY_FINDING_MISMATCHED,
                    CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED);
            }

            if (depositRows.empty())
            {
                AddFinding(report, ExpectedRow(depositKey, CUSTODY_GOLD,
                        ROLE_DEPOSIT, group.auction.ownerGuid,
                        group.auction.deposit, 0, group.auctionId),
                    CUSTODY_FINDING_MISSING, CUSTODY_REPAIR_GENERIC,
                    CUSTODY_FINDING_CONFIRMED);
            }
            else if (depositRows.size() > 1)
            {
                for (size_t i = 0; i < depositRows.size(); ++i)
                {
                    AddFinding(report, *depositRows[i], CUSTODY_FINDING_DUPLICATE,
                        CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED);
                }
            }
            else if (!MatchesExpected(*depositRows[0], CUSTODY_GOLD,
                         ROLE_DEPOSIT, group.auction.ownerGuid,
                         group.auction.deposit, 0, group.auctionId))
            {
                AddFinding(report, *depositRows[0], CUSTODY_FINDING_MISMATCHED,
                    CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED);
            }

            for (size_t i = 0; i < extraSellerRows.size(); ++i)
            {
                AddFinding(report, *extraSellerRows[i], CUSTODY_FINDING_UNEXPECTED,
                    CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED);
            }
        }

        std::vector<CustodyFinding> bidFindings;
        bool const requiresBid = (markerOwned || hasSellerCandidate) &&
                                 group.auction.bidderGuid != 0;
        if (bidRows.empty())
        {
            if (requiresBid)
            {
                CustodyFinding finding;
                finding.row = ExpectedRow("bid:" + std::to_string(group.auctionId) +
                    ":missing", CUSTODY_GOLD, ROLE_BID,
                    group.auction.bidderGuid, group.auction.bid, 0,
                    group.auctionId);
                finding.reason = CUSTODY_FINDING_MISSING;
                bidFindings.push_back(finding);
            }
        }
        else if (group.auction.bidderGuid == 0)
        {
            for (size_t i = 0; i < bidRows.size(); ++i)
            {
                CustodyFinding finding;
                finding.row = *bidRows[i];
                finding.reason = CUSTODY_FINDING_UNEXPECTED;
                bidFindings.push_back(finding);
            }
        }
        else if (bidRows.size() > 1)
        {
            for (size_t i = 0; i < bidRows.size(); ++i)
            {
                CustodyFinding finding;
                finding.row = *bidRows[i];
                finding.reason = CUSTODY_FINDING_DUPLICATE;
                bidFindings.push_back(finding);
            }
        }
        else if (!MatchesExpected(*bidRows[0], CUSTODY_GOLD, ROLE_BID,
                     group.auction.bidderGuid, group.auction.bid, 0,
                     group.auctionId))
        {
            CustodyFinding finding;
            finding.row = *bidRows[0];
            finding.reason = CUSTODY_FINDING_MISMATCHED;
            bidFindings.push_back(finding);
        }

        if (bidFindings.empty())
        {
            m_pendingBidMismatches.erase(group.auctionId);
            continue;
        }

        observedBidMismatches.insert(group.auctionId);
        std::string const fingerprint = BidFingerprint(group.auction, bidRows);
        std::unordered_map<uint32, PendingBidMismatch>::iterator pending =
            m_pendingBidMismatches.find(group.auctionId);
        if (pending == m_pendingBidMismatches.end() ||
            pending->second.fingerprint != fingerprint)
        {
            PendingBidMismatch observation;
            observation.fingerprint = fingerprint;
            observation.firstSeen = now;
            m_pendingBidMismatches[group.auctionId] = observation;
            pending = m_pendingBidMismatches.find(group.auctionId);
        }

        bool const confirmed = context == CUSTODY_SCAN_RUNTIME &&
            pending->second.firstSeen <= now &&
            now - pending->second.firstSeen >= CUSTODY_RECONCILE_MIN_ROW_AGE;
        CustodyFindingState const state = confirmed
            ? CUSTODY_FINDING_CONFIRMED : CUSTODY_FINDING_PENDING;
        for (size_t i = 0; i < bidFindings.size(); ++i)
        {
            AddFinding(report, bidFindings[i].row, bidFindings[i].reason,
                CUSTODY_REPAIR_MANUAL_ONLY, state);
        }
    }

    for (std::unordered_map<uint32, PendingBidMismatch>::iterator itr =
             m_pendingBidMismatches.begin();
         itr != m_pendingBidMismatches.end();)
    {
        if (observedBidMismatches.find(itr->first) == observedBidMismatches.end())
        {
            itr = m_pendingBidMismatches.erase(itr);
        }
        else
        {
            ++itr;
        }
    }

    std::sort(report.findings.begin(), report.findings.end(), FindingLess);
}

void CustodyReconciler::Reset()
{
    m_pendingBidMismatches.clear();
}
