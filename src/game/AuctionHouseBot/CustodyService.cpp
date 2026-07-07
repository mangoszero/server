/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "CustodyService.h"

#include "Config/Config.h"
#include "CustodyLedger.h"
#include "AuctionHouseMgr.h"
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
    uint64 const CUSTODY_RECONCILE_MIN_ROW_AGE = 60;         // seconds

    bool IsMature(CustodyRow const& row, uint64 now)
    {
        return row.createdTime == 0 || row.createdTime + CUSTODY_RECONCILE_MIN_ROW_AGE <= now;
    }

    AuctionEntry* FindLiveAuction(uint32 auctionId)
    {
        for (uint32 i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
        {
            AuctionHouseObject* auctions = sAuctionMgr.GetAuctionsMap(AuctionHouseType(i));
            AuctionEntry* auction = auctions->GetAuction(auctionId);
            if (auction)
            {
                return auction;
            }
        }

        return NULL;
    }

    bool HasMatureCustodyRow(std::vector<CustodyRow> const& rows, uint32 auctionId, uint64 now)
    {
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].auctionId == auctionId && IsMature(rows[i], now))
            {
                return true;
            }
        }

        return false;
    }

    CustodyRow const* FindRowByKey(std::vector<CustodyRow> const& rows,
                                   std::string const& key)
    {
        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].idemKey == key)
            {
                return &rows[i];
            }
        }

        return NULL;
    }

    void CollectLiveBidRows(std::vector<CustodyRow> const& rows, uint32 auctionId,
                            std::vector<CustodyRow const*>& out)
    {
        for (size_t i = 0; i < rows.size(); ++i)
        {
            CustodyRow const& row = rows[i];
            if (row.auctionId == auctionId &&
                row.kind == CUSTODY_GOLD &&
                row.role == ROLE_BID &&
                row.state == CST_RESERVED)
            {
                out.push_back(&row);
            }
        }
    }

    CustodyRow ExpectedRow(std::string const& key, uint8 kind, uint8 role,
                           uint32 ownerGuid, uint32 amount, uint32 itemGuid,
                           uint32 auctionId)
    {
        CustodyRow row;
        row.id = 0;
        row.idemKey = key;
        row.kind = kind;
        row.role = role;
        row.state = CST_RESERVED;
        row.ownerGuid = ownerGuid;
        row.beneficiaryGuid = 0;
        row.amount = amount;
        row.itemGuid = itemGuid;
        row.auctionId = auctionId;
        row.createdTime = 0;
        row.resolvedTime = 0;
        return row;
    }

    void AddDrift(std::vector<CustodyRow>& out, CustodyRow const& row,
                  char const* reason)
    {
        out.push_back(row);
        sLog.outError("custody drift: %s key=%s auction=%u kind=%u role=%u state=%u",
                      reason, row.idemKey.c_str(), row.auctionId,
                      uint32(row.kind), uint32(row.role), uint32(row.state));
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

    void CheckExpectedKey(std::vector<CustodyRow> const& rows,
                          std::vector<CustodyRow>& drift,
                          std::string const& key,
                          uint8 kind, uint8 role, uint32 ownerGuid,
                          uint32 amount, uint32 itemGuid, uint32 auctionId,
                          char const* reason)
    {
        CustodyRow const* row = FindRowByKey(rows, key);
        if (!row)
        {
            AddDrift(drift, ExpectedRow(key, kind, role, ownerGuid, amount, itemGuid, auctionId), reason);
            return;
        }

        if (!MatchesExpected(*row, kind, role, ownerGuid, amount, itemGuid, auctionId))
        {
            AddDrift(drift, *row, reason);
        }
    }

    void CheckAuctionExpectedRows(AuctionEntry const* auction,
                                  std::vector<CustodyRow> const& rows,
                                  std::vector<CustodyRow>& drift)
    {
        std::string auctionId = std::to_string(auction->Id);

        CheckExpectedKey(rows, drift, "item:" + auctionId, CUSTODY_ITEM, ROLE_ITEM,
                         auction->owner, 0, auction->itemGuidLow, auction->Id,
                         "missing or invalid item row");

        CheckExpectedKey(rows, drift, "dep:" + auctionId, CUSTODY_GOLD, ROLE_DEPOSIT,
                         auction->owner, auction->deposit, 0, auction->Id,
                         "missing or invalid deposit row");

        std::vector<CustodyRow const*> liveBidRows;
        CollectLiveBidRows(rows, auction->Id, liveBidRows);
        if (auction->bidder != 0)
        {
            if (liveBidRows.empty())
            {
                AddDrift(drift, ExpectedRow("bid:" + auctionId + ":missing",
                                            CUSTODY_GOLD, ROLE_BID,
                                            auction->bidder, auction->bid, 0,
                                            auction->Id),
                         "missing live bid row");
            }
            else if (liveBidRows.size() > 1)
            {
                for (size_t i = 0; i < liveBidRows.size(); ++i)
                {
                    AddDrift(drift, *liveBidRows[i], "ambiguous live bid row");
                }
            }
            else if (liveBidRows[0]->ownerGuid != auction->bidder ||
                     liveBidRows[0]->amount != auction->bid)
            {
                AddDrift(drift, *liveBidRows[0], "invalid live bid row");
            }
        }
        else
        {
            for (size_t i = 0; i < liveBidRows.size(); ++i)
            {
                AddDrift(drift, *liveBidRows[i], "unexpected live bid row");
            }
        }
    }
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

void CustodyService::MaybeCrash(std::string const& phase)
{
    if (CrashPhase() == phase)
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

void CustodyService::ReconcileScan(bool dryRun, std::vector<CustodyRow>& orphans)
{
    (void)dryRun;

    uint64 const now = static_cast<uint64>(time(NULL));
    std::vector<CustodyRow> nonTerminal;
    CustodyLedger::LoadNonTerminal(nonTerminal);

    for (size_t i = 0; i < nonTerminal.size(); ++i)
    {
        CustodyRow const& row = nonTerminal[i];
        if (!IsMature(row, now))
        {
            continue;
        }

        if (!FindLiveAuction(row.auctionId))
        {
            AddDrift(orphans, row, "orphan row");
        }
    }

    for (uint32 i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        AuctionHouseObject* auctions = sAuctionMgr.GetAuctionsMap(AuctionHouseType(i));
        AuctionHouseObject::AuctionEntryMap const& map = auctions->GetAuctions();
        for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = map.begin(); itr != map.end(); ++itr)
        {
            AuctionEntry const* auction = itr->second;
            if (!HasMatureCustodyRow(nonTerminal, auction->Id, now))
            {
                continue;
            }

            CheckAuctionExpectedRows(auction, nonTerminal, orphans);
        }
    }
}
