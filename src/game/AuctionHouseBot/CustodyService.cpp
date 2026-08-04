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

#include "CustodyService.h"

#include "Config/Config.h"
#include "CustodyLedger.h"
#include "Log.h"
#include "Mail.h"
#include "Player.h"

#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{
    CustodyReconciler s_reconciler;
}

void CustodyService::ReserveGold(CustodyDeferred& d, uint32 ownerGuid,
                                 Player* ownerOnline, uint32 amount,
                                 std::string const& key, uint32 auctionId,
                                 uint8 role)
{
    if (ownerOnline)
    {
        ownerOnline->ModifyMoney(-int32(amount));
        ownerOnline->SaveInventoryAndGoldToDB();
    }

    CustodyRow r;
    r.id              = 0;
    r.idemKey         = key;
    r.kind            = CUSTODY_GOLD;
    r.role            = role;
    r.state           = CST_RESERVED;
    r.ownerGuid       = ownerGuid;
    r.beneficiaryGuid = 0;
    r.amount          = amount;
    r.itemGuid        = 0;
    r.auctionId       = auctionId;
    r.createdTime     = static_cast<uint64>(time(NULL));
    r.resolvedTime    = 0;
    CustodyLedger::Insert(r);

    // Suppress unused-parameter warning: d is the ordered deferred context;
    // ReserveGold does not itself push a mail effect, but callers may chain
    // DeferEffect() calls on the same d immediately after.
    (void)d;
}

void CustodyService::ReserveGoldAlreadyDebited(uint32 ownerGuid, uint32 amount,
                                               std::string const& key,
                                               uint32 auctionId, uint8 role)
{
    CustodyRow r;
    r.id              = 0;
    r.idemKey         = key;
    r.kind            = CUSTODY_GOLD;
    r.role            = role;
    r.state           = CST_RESERVED;
    r.ownerGuid       = ownerGuid;
    r.beneficiaryGuid = 0;
    r.amount          = amount;
    r.itemGuid        = 0;
    r.auctionId       = auctionId;
    r.createdTime     = static_cast<uint64>(time(NULL));
    r.resolvedTime    = 0;
    CustodyLedger::Insert(r);
}

void CustodyService::CommitGoldLedgerOnly(std::string const& key)
{
    CustodyLedger::SetState(key, CST_TERMINAL_OK,
                            static_cast<uint64>(time(NULL)));
}

void CustodyService::RollbackGoldLedgerOnly(std::string const& key)
{
    CustodyLedger::SetState(key, CST_TERMINAL_BACK,
                            static_cast<uint64>(time(NULL)));
}

void CustodyService::RollbackGoldRefund(CustodyDeferred& d,
                                        std::string const& key,
                                        MailDraft& refundMail,
                                        MailReceiver const& to,
                                        MailSender const& from)
{
    CustodyLedger::SetState(key, CST_TERMINAL_BACK,
                            static_cast<uint64>(time(NULL)));
    refundMail.SendMailToInTransaction(to, from, d);
}

void CustodyService::ReleaseGoldToWallet(CustodyDeferred& d, uint32 ownerGuid,
                                         Player* ownerOnline, uint32 amount,
                                         std::string const& key)
{
    if (ownerOnline)
    {
        ownerOnline->ModifyMoney(int32(amount));
        ownerOnline->SaveInventoryAndGoldToDB();
    }
    else
    {
        // Offline: credit the persisted wallet inside the caller's open txn.
        // This releases previously-reserved copper, so it cannot exceed what
        // the holder could legitimately carry -- no cap check needed.
        CharacterDatabase.PExecute(
            "UPDATE `characters` SET `money` = `money` + %u WHERE `guid` = %u",
            amount, ownerGuid);
    }
    CustodyLedger::SetState(key, CST_TERMINAL_BACK,
                            static_cast<uint64>(time(NULL)));
    (void)d;   // ordered-effects context; no mail effect is pushed here
}

void CustodyService::WriteResolutionApplied(uint32 auctionId, uint64 uuid)
{
    char key[32];
    snprintf(key, sizeof(key), "resolve:%llu",
             static_cast<unsigned long long>(uuid));

    CustodyRow r;
    r.id              = 0;
    r.idemKey         = key;
    r.kind            = CUSTODY_GOLD;
    r.role            = ROLE_RESOLUTION;
    r.state           = CST_TERMINAL_OK;
    r.ownerGuid       = 0;
    r.beneficiaryGuid = 0;
    r.amount          = 0;
    r.itemGuid        = 0;
    r.auctionId       = auctionId;
    r.createdTime     = static_cast<uint64>(time(NULL));
    r.resolvedTime    = static_cast<uint64>(time(NULL));
    CustodyLedger::Insert(r);
}

bool CustodyService::ResolutionApplied(uint64 uuid)
{
    char key[32];
    snprintf(key, sizeof(key), "resolve:%llu",
             static_cast<unsigned long long>(uuid));
    CustodyRow row;
    return CustodyLedger::Get(key, row);
}

void CustodyService::TopUpBid(std::string const& key, uint32 newAmount,
                              uint32 delta, Player* bidderOnline)
{
    if (bidderOnline)
    {
        bidderOnline->ModifyMoney(-int32(delta));
        bidderOnline->SaveInventoryAndGoldToDB();
    }
    CustodyLedger::SetAmount(key, newAmount);
}

void CustodyService::EscrowItem(uint32 ownerGuid, uint32 itemGuid,
                                std::string const& key, uint32 auctionId)
{
    CustodyRow r;
    r.id              = 0;
    r.idemKey         = key;
    r.kind            = CUSTODY_ITEM;
    r.role            = ROLE_ITEM;
    r.state           = CST_RESERVED;
    r.ownerGuid       = ownerGuid;
    r.beneficiaryGuid = 0;
    r.amount          = 0;
    r.itemGuid        = itemGuid;
    r.auctionId       = auctionId;
    r.createdTime     = static_cast<uint64>(time(NULL));
    r.resolvedTime    = 0;
    CustodyLedger::Insert(r);
}

void CustodyService::DeliverItem(CustodyDeferred& d, std::string const& key,
                                 MailDraft& itemMail, MailReceiver const& to,
                                 MailSender const& from, uint32 checked)
{
    CustodyLedger::SetState(key, CST_TERMINAL_OK,
                            static_cast<uint64>(time(NULL)));
    itemMail.SendMailToInTransaction(to, from, d,
                                     static_cast<MailCheckMask>(checked));
}

void CustodyService::DeferEffect(CustodyDeferred& d,
                                 std::function<void()> effect)
{
    d.effects.push_back(effect);
}

std::string CustodyService::CrashPhase()
{
    return sConfig.GetStringDefault("AH.Service.CustodyCrashAt", "");
}

bool CustodyService::ShouldCrashAtPhase(std::string const& configuredPhase,
                                        std::string const& phase)
{
    return !phase.empty() && configuredPhase == phase;
}

void CustodyService::MaybeCrash(std::string const& phase)
{
    if (ShouldCrashAtPhase(CrashPhase(), phase))
    {
        sLog.outError("custody crash-injection: _exit(3) at phase '%s' (TEST ONLY)", phase.c_str());
        fflush(NULL);                                       // flush ALL stdio streams (incl. the log FILE*) -- _exit skips cleanup
        _exit(3);
    }
}

bool CustodyService::CommitCheckedOrForcedFail(std::string const& phase)
{
    // One-shot per process: the FIRST finalize whose phase is armed rolls back
    // and reports failure (-> the caller runs its X6 in-memory undo and queues
    // the redrive); the redrive's re-attempt takes the real checked commit and
    // succeeds. This proves the worker book is never rolled back on a failed
    // mangosd-side finalize. Inert (a plain checked commit) on a live realm.
    static bool s_forcedFailFired = false;
    if (!s_forcedFailFired && !phase.empty() &&
        sConfig.GetStringDefault("AH.Service.CustodyFailCommitAt", "") == phase)
    {
        s_forcedFailFired = true;
        sLog.outError("custody forced commit-fail: rollback at phase '%s' (TEST ONLY, one-shot)", phase.c_str());
        CharacterDatabase.RollbackTransaction();
        return false;
    }
    return CharacterDatabase.CommitTransactionChecked();
}

void CustodyService::ReconcileScan(uint64 now, CustodyScanContext context,
                                   CustodyReconcileReport& report)
{
    std::vector<CustodySnapshotGroup> snapshot;
    CustodyLedger::LoadReconcileSnapshot(snapshot);
    s_reconciler.Scan(snapshot, now, context, report);
}

void CustodyService::LogReconcileReport(
    char const* phase, CustodyReconcileReport const& report)
{
    sLog.outString("custody reconcile %s: confirmed=%u pending=%u "
                   "sweep-owned=%u rows=" UI64FMTD,
                   phase, report.confirmedDriftCount,
                   report.pendingBidCount, report.sweepOwnedCount,
                   report.rowVisits);

    CustodyDetailBudget budget(100);
    for (size_t i = 0; i < report.findings.size(); ++i)
    {
        CustodyFinding const& finding = report.findings[i];
        if (!budget.Take())
        {
            continue;
        }

        sLog.outError("custody reconcile %s detail: auction=%u key=%s "
                      "reason=%s", phase, finding.row.auctionId,
                      finding.row.idemKey.c_str(),
                      CustodyFindingReasonName(finding.reason));
    }

    if (budget.Suppressed())
    {
        sLog.outString("custody reconcile %s: %u detail(s) suppressed",
                       phase, budget.Suppressed());
    }
}

CustodyMaintenancePlan CustodyService::GetMaintenancePlan(
    bool custodyEnabled, bool writeAuthorityEnabled)
{
    CustodyMaintenancePlan plan;
    plan.reconcile = custodyEnabled;
    plan.prune = custodyEnabled;
    plan.sweepBotMaterializations = writeAuthorityEnabled;
    return plan;
}
