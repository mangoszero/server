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

/**
 * @file AhServiceBook.cpp
 * @brief The custody variants of the auction book's settlements.
 */

#include "AhServiceBook.h"
#include "AhService.h"
#include "AccountMgr.h"
#include "AuctionHouseMgr.h"
#include "CustodyLedger.h"
#include "CustodyService.h"
#include "Database/DatabaseEnv.h"
#include "Item.h"
#include "Language.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <sstream>
#include <string>
#include <vector>

void AhAuditCustodyReconcile(char const* phase)
{
    if (!sAhService.IsCustodyEnabled())
    {
        return;
    }

    CustodyReconcileReport report;
    CustodyService::ReconcileScan(static_cast<uint64>(time(NULL)),
                                  CUSTODY_SCAN_BOOT, report);
    CustodyService::LogReconcileReport(phase, report);
}

/**
 * @brief Custody co-commit variant of SendAuctionExpiredMail.
 *
 * Reproduces SendAuctionExpiredMail EXACTLY (owner-exists guard, expired subject,
 * item return to the seller by mail; destroy branch when the account is gone)
 * but co-commits the item return mail into the caller's already-open
 * CharacterDatabase transaction and flips the "item:<Id>" escrow row to
 * TERMINAL_OK. The online owner's SMSG_AUCTION_OWNER_NOTIFICATION (the "expired"
 * form, sold=false, bid/outbid/bidder all 0) is deferred into @p def BEFORE the
 * mail push so packet order stays notify-then-mail (legacy :307). RemoveAItem
 * (+ live item destroy on the no-owner branch) is deferred in legacy order (spec
 * S6 / C7). itemGuidLow is intentionally NOT zeroed on the custody path: success
 * deletes the AuctionEntry, and on rollback the original GUID must survive for
 * the next tick's re-resolution. Reads auction->owner/itemGuidLow, so the caller
 * MUST call this before mutating them (there is none in S6; expire deletes the
 * auction).
 *
 * @param auction The expired (no-bidder) auction entry.
 * @param def     Ordered deferred-effects queue for this co-commit.
 */
void AhSendAuctionExpiredMailInTransaction(AuctionEntry* auction, CustodyDeferred& def)
{
    // return an item in auction to its owner by mail
    Item* pItem = sAuctionMgr.GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        sLog.outError("Auction item (GUID: %u) not found, and lost.", auction->itemGuidLow);
        // The item is already absent from the live AH cache (legacy loses it too,
        // sending nothing). Still terminalize the "item:<Id>" escrow row
        // ledger-only: the caller (ExpireUnsoldCustody) deletes the auction
        // UNCONDITIONALLY after this returns, so leaving the row CST_RESERVED
        // would orphan a non-terminal custody row with no live auction (breaking
        // the reconciliation invariant). Do NOT touch item_instance here -- legacy
        // does not, and the DB row's state is unknown when the cache has drifted.
        CustodyService::CommitGoldLedgerOnly("item:" + std::to_string(auction->Id));
        return;
    }

    ObjectGuid owner_guid = ObjectGuid(HIGHGUID_PLAYER, auction->owner);
    Player* owner = sObjectMgr.GetPlayer(owner_guid);

    uint32 owner_accId = 0;
    if (!owner)
    {
        owner_accId = sObjectMgr.GetPlayerAccountIdByGUID(owner_guid);
    }

    // Snapshot the item guid-low before any deferral; the deferred RemoveAItem
    // closure must capture a stable value (the auction object itself is deleted
    // by the trailing deferred closure on commit success).
    uint32 const savedItemGuidLow = auction->itemGuidLow;

    // owner exist
    if (owner || owner_accId)
    {
        std::ostringstream subject;
        subject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_EXPIRED;

        // Defer the online owner-expired notification, snapshotting every field
        // BY VALUE (the auction is deleted in the same deferred run). Appended
        // BEFORE the RemoveAItem and the mail push so packet order stays
        // notify-then-mail (legacy :307). An unsold expiry carries bid==0,
        // outbid==0, bidder==0 (the "expired" form, sold=false). Re-resolves the
        // owner by GUID at run time, skipping the packet if offline -- the
        // durable mail row is authoritative (spec I2).
        if (owner)
        {
            uint32 ownerGuidLow = auction->owner;
            uint32 houseId  = auction->GetHouseId();
            uint32 aucId    = auction->Id;
            uint32 itemTpl  = auction->itemTemplate;
            int32  itemRand = auction->itemRandomPropertyId;
            def.effects.push_back([ownerGuidLow, houseId, aucId, itemTpl, itemRand]()
            {
                Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, ownerGuidLow));
                if (p)
                {
                    AhSendAuctionOwnerNotificationData(p->GetSession(), houseId, aucId, 0, 0, 0, itemTpl, itemRand, false);
                }
            });
        }

        // Defer RemoveAItem FIRST (legacy :310 ran before the mail;
        // SendMailToInTransaction appends its own push AFTER this). Mirror S5
        // cancel :979-982. Do NOT zero auction->itemGuidLow synchronously here:
        // that in-memory write would survive a checked-commit rollback (the
        // auction stays in the map with itemGuidLow==0, the next tick re-resolves
        // to GetAItem(0)==NULL -> item lost, "item:" row orphaned). The auction is
        // deleted by the trailing deferred closure on success, so the field never
        // needs zeroing; on rollback it must stay valid for re-resolution. S4's
        // SendAuctionWonMailInTransaction likewise never zeroes it.
        def.effects.push_back([savedItemGuidLow]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
        });

        // Return the item via DeliverItem: flips "item:<Id>" -> TERMINAL_OK AND
        // co-commits the return mail (queues the online owner's AddMItem
        // disposal). An expiry return needs no item_instance.owner_guid UPDATE
        // (the seller already owns the item). Mirror S5 cancel :984-987.
        MailDraft itemReturn(subject.str(), "");
        itemReturn.AddItem(pItem);
        CustodyService::DeliverItem(def, "item:" + std::to_string(auction->Id), itemReturn,
                                    MailReceiver(owner, owner_guid), MailSender(auction),
                                    MAIL_CHECK_MASK_COPIED);
    }
    // owner not found (destroy)
    else
    {
        // DELETE the item_instance row IN-TXN (appends to the caller's open txn).
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", savedItemGuidLow);

        // Terminalize the escrow row: this branch sends no mail, so DeliverItem
        // is NOT called and nothing else flips "item:". The §0 lesson -- without
        // this the row stays CST_RESERVED after the auction row is deleted ->
        // orphaned non-terminal row.
        CustodyService::CommitGoldLedgerOnly("item:" + std::to_string(auction->Id));

        // Defer RemoveAItem + the live `delete pItem` (X5: the destroy must run
        // only after the checked commit succeeds, NOT inside the txn -- on
        // rollback the row survives and so must the live Item*). Mirror
        // SendAuctionWonMailInTransaction's destroy branch :534-548. As in the
        // owner-exists branch, do NOT zero auction->itemGuidLow synchronously --
        // it must survive a rollback for the next tick's re-resolution.
        def.effects.push_back([savedItemGuidLow, pItem]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
            delete pItem;
        });
    }
}

/**
 * @brief Custody co-commit variant of SendAuctionSuccessfulMail.
 *
 * Reproduces SendAuctionSuccessfulMail EXACTLY (owner-exists guard, subject/body,
 * profit = bid + deposit - cut) but co-commits the seller payout mail into the
 * caller's already-open CharacterDatabase transaction via SendMailToInTransaction.
 * If the owner is online, the SMSG_AUCTION_OWNER_NOTIFICATION (sold) is snapshotted
 * BY VALUE and appended to @p def BEFORE the mail's own push closure, preserving
 * the legacy notify-then-mail packet order. Reads auction->bid/bidder, so the
 * caller MUST call this before any bid/bidder mutation (spec B / S4).
 *
 * @param auction The auction being resolved.
 * @param def     Ordered deferred-effects queue for this co-commit.
 */
void AhSendAuctionSuccessfulMailInTransaction(AuctionEntry* auction, CustodyDeferred& def)
{
    ObjectGuid owner_guid = ObjectGuid(HIGHGUID_PLAYER, auction->owner);
    Player* owner = sObjectMgr.GetPlayer(owner_guid);

    uint32 owner_accId = 0;
    if (!owner)
    {
        owner_accId = sObjectMgr.GetPlayerAccountIdByGUID(owner_guid);
    }

    // owner exist
    if (owner || owner_accId)
    {
        std::ostringstream msgAuctionSuccessfulSubject;
        msgAuctionSuccessfulSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_SUCCESSFUL;

        std::ostringstream auctionSuccessfulBody;
        uint32 auctionCut = auction->GetAuctionCut();

        auctionSuccessfulBody.width(16);
        auctionSuccessfulBody << std::right << std::hex << auction->bidder;
        auctionSuccessfulBody << std::dec << ":" << auction->bid << ":" << auction->buyout;
        auctionSuccessfulBody << ":" << auction->deposit << ":" << auctionCut;

        DEBUG_LOG("AuctionSuccessful body string : %s", auctionSuccessfulBody.str().c_str());

        uint32 profit = auction->bid + auction->deposit - auctionCut;

        // Defer the online owner-sold notification, snapshotting every field BY
        // VALUE (the auction is deleted in the same deferred run). Appended BEFORE
        // SendMailToInTransaction's own online push so packet order stays
        // notify-then-mail (legacy :264). Captures the owner low GUID + scalars
        // only and RE-RESOLVES the player by GUID at run time, skipping the packet
        // if offline -- the durable mail row is authoritative (spec I2).
        if (owner)
        {
            uint32 ownerGuidLow = auction->owner;
            uint32 houseId  = auction->GetHouseId();
            uint32 aucId    = auction->Id;
            uint32 bidValue = auction->bid;
            uint32 outbid   = auction->GetAuctionOutBid();
            uint32 bidder   = auction->bidder;
            uint32 itemTpl  = auction->itemTemplate;
            int32  itemRand = auction->itemRandomPropertyId;
            def.effects.push_back([ownerGuidLow, houseId, aucId, bidValue, outbid, bidder, itemTpl, itemRand]()
            {
                Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, ownerGuidLow));
                if (p)
                {
                    AhSendAuctionOwnerNotificationData(p->GetSession(), houseId, aucId, bidValue, outbid, bidder, itemTpl, itemRand, true);
                }
            });
        }

        MailDraft(msgAuctionSuccessfulSubject.str(), auctionSuccessfulBody.str())
            .SetMoney(profit)
            .SendMailToInTransaction(MailReceiver(owner, owner_guid), MailSender(auction), def.effects, MAIL_CHECK_MASK_COPIED);
    }
}

/**
 * @brief Custody co-commit variant of SendAuctionWonMail.
 *
 * Reproduces SendAuctionWonMail EXACTLY (GM-log block, subject/body, owner UPDATE
 * vs destroy branches) but co-commits the winner item mail into the caller's
 * already-open CharacterDatabase transaction. On the receiver-exists branch the
 * item_instance owner UPDATE appends in-txn and the bidder notification +
 * RemoveAItem + itemGuidLow=0 are deferred in legacy order; on the no-receiver
 * branch the item_instance DELETE appends in-txn and RemoveAItem + itemGuidLow=0 +
 * the live `delete pItem` are deferred (X5: destroy outside the txn, run only on
 * commit success). Reads auction->bidder, so the caller MUST set it before calling
 * (spec C / S4).
 *
 * @param auction The auction being resolved.
 * @param def     Ordered deferred-effects queue for this co-commit.
 */
void AhSendAuctionWonMailInTransaction(AuctionEntry* auction, CustodyDeferred& def)
{
    Item* pItem = sAuctionMgr.GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        return;
    }

    ObjectGuid bidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* bidder = sObjectMgr.GetPlayer(bidder_guid);

    uint32 bidder_accId = 0;

    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, auction->owner);
    // data for gm.log (kept identical to SendAuctionWonMail)
    if (sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        AccountTypes bidder_security;
        std::string bidder_name;
        if (bidder)
        {
            bidder_accId = bidder->GetSession()->GetAccountId();
            bidder_security = bidder->GetSession()->GetSecurity();
            bidder_name = bidder->GetName();
        }
        else
        {
            bidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(bidder_guid);
            bidder_security = bidder_accId ? sAccountMgr.GetSecurity(bidder_accId) : SEC_PLAYER;

            if (bidder_security > SEC_PLAYER)               // not do redundant DB requests
            {
                if (!sObjectMgr.GetPlayerNameByGUID(bidder_guid, bidder_name))
                {
                    bidder_name = sObjectMgr.GetMangosStringForDBCLocale(LANG_UNKNOWN);
                }
            }
        }

        if (bidder_security > SEC_PLAYER)
        {
            std::string owner_name;
            if (ownerGuid && !sObjectMgr.GetPlayerNameByGUID(ownerGuid, owner_name))
            {
                owner_name = sObjectMgr.GetMangosStringForDBCLocale(LANG_UNKNOWN);
            }

            uint32 owner_accid = sObjectMgr.GetPlayerAccountIdByGUID(ownerGuid);

            sLog.outCommand(bidder_accId, "GM %s (Account: %u) won item in auction (Entry: %u Count: %u) and pay money: %u. Original owner %s (Account: %u)",
                bidder_name.c_str(), bidder_accId, auction->itemTemplate, auction->itemCount, auction->bid, owner_name.c_str(), owner_accid);
        }
    }
    else if (!bidder)
    {
        bidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(bidder_guid);
    }

    // Snapshot the item guid-low before any deferral; the deferred RemoveAItem
    // closure must capture a stable value (the auction object itself is deleted
    // by the trailing deferred closure on commit success).
    uint32 const savedItemGuidLow = auction->itemGuidLow;

    // receiver exist
    if (bidder || bidder_accId)
    {
        std::ostringstream msgAuctionWonSubject;
        msgAuctionWonSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_WON;

        std::ostringstream msgAuctionWonBody;
        msgAuctionWonBody.width(16);
        msgAuctionWonBody << std::right << std::hex << auction->owner;
        msgAuctionWonBody << std::dec << ":" << auction->bid << ":" << auction->buyout;
        DEBUG_LOG("AuctionWon body string : %s", msgAuctionWonBody.str().c_str());

        // set owner to bidder (to prevent delete item with sender char deleting)
        // owner in `data` will set at mail receive and item extracting.
        // Appends to the caller's OPEN transaction (no own Begin/Commit) (spec C).
        CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", auction->bidder, savedItemGuidLow);

        // (1) Defer the online bidder-won notification, snapshotting every field BY
        //     VALUE. Appended BEFORE the RemoveAItem and the mail push so packet
        //     order = notify-then-mail (legacy :204). Re-resolves the bidder by
        //     GUID at run time, skipping the packet if offline (spec I2).
        if (bidder)
        {
            uint32 bidderGuidLow = auction->bidder;
            uint32 houseId  = auction->GetHouseId();
            uint32 aucId    = auction->Id;
            uint32 bidValue = auction->bid;
            uint32 outbid   = auction->GetAuctionOutBid();
            uint32 itemTpl  = auction->itemTemplate;
            int32  itemRand = auction->itemRandomPropertyId;
            def.effects.push_back([bidderGuidLow, houseId, aucId, bidValue, outbid, itemTpl, itemRand]()
            {
                Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, bidderGuidLow));
                if (p)
                {
                    AhSendAuctionBidderNotificationData(p->GetSession(), houseId, aucId, bidderGuidLow, bidValue, outbid, itemTpl, itemRand, true);
                }
            });
        }

        // (2) Defer RemoveAItem (legacy :207-208 ran before the mail;
        //     SendMailToInTransaction appends its own push AFTER this).
        def.effects.push_back([savedItemGuidLow]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
        });

        // (3) Co-commit the winner mail; its online push closure is appended AFTER
        //     (1)+(2), matching legacy notify -> RemoveAItem -> mail order.
        MailDraft(msgAuctionWonSubject.str(), msgAuctionWonBody.str())
            .AddItem(pItem)
            .SendMailToInTransaction(MailReceiver(bidder, bidder_guid), MailSender(auction), def.effects, MAIL_CHECK_MASK_COPIED);
    }
    // receiver not exist (destroy)
    else
    {
        // DELETE the item_instance row IN-TXN (appends to the caller's open txn).
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", savedItemGuidLow);

        // Defer RemoveAItem + the live `delete pItem` (X5: the destroy must run
        // only after the checked commit succeeds, NOT inside the txn -- on
        // rollback the row survives and so must the live Item*).
        def.effects.push_back([savedItemGuidLow, pItem]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
            delete pItem;
        });
    }
}

/**
 * @brief Dispatcher for the in-process browse fallback (C1/I1).
 *
 * Routes to one of the three existing BuildList* methods based on @p kind:
 *   0 (or default) = LIST public browse  -> BuildListAuctionItems
 *   1              = OWNER               -> BuildListOwnerItems
 *   2              = BIDDER              -> client-outbid prepend + BuildListBidderItems
 *
 * For BIDDER the @p clientOutbidIds entries from the stored request are
 * prepended in CLIENT ORDER ahead of the sweep, matching the live handler's
 * behaviour in HandleAuctionListBidderItems.  The three inner builders are
 * completely unchanged; this is a thin dispatcher only.
 *
 * @param kind           Browse kind: 0=LIST, 1=OWNER, 2=BIDDER.
 * @param data           Packet buffer to append auction entries to.
 * @param player         The resolving player (re-resolved by Task-11 fallback).
 * @param wname          Name search filter (LIST only).
 * @param listfrom       Starting offset (LIST only).
 * @param levelmin       Min required-level filter (LIST only).
 * @param levelmax       Max required-level filter (LIST only).
 * @param usable         Usability filter (LIST only).
 * @param invType        Inventory-type filter (LIST only).
 * @param itemClass      Item-class filter (LIST only).
 * @param itemSubClass   Item-subclass filter (LIST only).
 * @param quality        Minimum quality filter (LIST only).
 * @param clientOutbidIds Client-supplied outbid auction ids (BIDDER only).
 * @param count          Receives appended-entry count.
 * @param totalcount     Receives total matching count.
 */
void AhBuildListForKind(AuctionHouseObject& house, uint8 kind, WorldPacket& data, Player* player,
    const std::wstring& wname, uint32 listfrom, uint32 levelmin, uint32 levelmax,
    uint32 usable, uint32 invType, uint32 itemClass, uint32 itemSubClass,
    uint32 quality, const std::vector<uint32>& clientOutbidIds,
    uint32& count, uint32& totalcount)
{
    switch (kind)
    {
        case 1: // BROWSE_OWNER
            house.BuildListOwnerItems(data, player, count, totalcount);
            break;
        case 2: // BROWSE_BIDDER
        {
            // Client outbid ids prepended in CLIENT ORDER (matches the live handler).
            for (size_t i = 0; i < clientOutbidIds.size(); ++i)
            {
                AuctionEntry* a = house.GetAuction(clientOutbidIds[i]);
                if (a && a->BuildAuctionInfo(data))
                {
                    ++totalcount;
                    ++count;
                }
            }
            house.BuildListBidderItems(data, player, count, totalcount);
            break;
        }
        case 0: // BROWSE_LIST
        default:
            house.BuildListAuctionItems(data, player, wname, listfrom, levelmin, levelmax,
                usable, invType, itemClass, itemSubClass, quality, count, totalcount);
            break;
    }
}

/**
 * @brief Test seam for the client-outbid-prepend order contract.
 *
 * Records the ids from @p clientIds into @p outOrder in CLIENT ORDER, locking
 * the invariant that BuildListForKind(BIDDER=2) prepends them before the sweep.
 * Called only by the -t ahbrowsehelper smoke test.
 */
void AhAppendClientOutbidsForTest(const std::vector<uint32>& clientIds,
                                  std::vector<uint32>& outOrder)
{
    for (size_t i = 0; i < clientIds.size(); ++i)
    {
        outOrder.push_back(clientIds[i]);
    }
}

/**
 * @brief Custody co-commit mirror of AuctionBidWinning (orchestrator, spec S4).
 *
 * Appends every DB write to the caller's ALREADY-OPEN CharacterDatabase
 * transaction and defers every in-memory effect into @p def; the caller
 * checked-commits then runs @p def. The deferred-effect order matches the legacy
 * resolution: owner-notify -> seller-mail -> bidder-notify -> winner-mail
 * (rendered by the two co-commit mail cores), then -- pushed LAST -- the AH map
 * RemoveAuction + `delete this`, so the in-memory auction survives until the very
 * end of def.run() (earlier closures that read auction fields snapshot by value).
 *
 * The seller payout and winner item delivery always retain legacy behavior.
 * Seller item/deposit rows and the bidder row are terminalized independently,
 * according to the route facts validated by the caller.
 *
 * Gold note: do NOT re-save newbidder's gold here. On a buyout it was already
 * saved by ReserveGold/TopUpBid in UpdateBidCustody; on the expiry path newbidder
 * is NULL.
 *
 * @param newbidder The online winner (buyout) or NULL (expiry).
 * @param def       Ordered deferred-effects queue for this co-commit.
 */
void AhAuctionBidWinningCustody(AuctionEntry& a, Player* newbidder, CustodyDeferred& def,
                                bool usesPlayerSellerCustody,
                                bool hasLiveBidCustody,
                                std::string const& knownBidKey)
{
    // (void) newbidder: its gold is already persisted by the bid seam (buyout) or
    // it is NULL (expiry); the auction UPDATE/DELETE persists the rest.
    (void)newbidder;

    // 1) Seller payout = bid + deposit - cut (single legacy mail; owner-notify
    //    deferred BEFORE the mail push by the co-commit core).
    AhSendAuctionSuccessfulMailInTransaction(&a, def);

    // 2) Net player-seller custody only. The seller mail above already carries
    //    the deposit return and proceeds, so these are ledger-only transitions.
    if (usesPlayerSellerCustody)
    {
        CustodyService::RollbackGoldLedgerOnly("dep:" + std::to_string(a.Id));
    }

    // The caller either validated this existing key before BeginTransaction or
    // created it in this same transaction while processing a buyout.
    if (hasLiveBidCustody)
    {
        CustodyService::CommitGoldLedgerOnly(knownBidKey);
    }

    // 3) Item to winner (receiver-exists owner UPDATE) or destroy (bidder == 0 ->
    //    account lookup fails -> destroy branch). Bidder-notify + RemoveAItem +
    //    (destroy: delete pItem) are deferred by the co-commit core.
    AhSendAuctionWonMailInTransaction(&a, def);

    if (usesPlayerSellerCustody)
    {
        CustodyService::CommitGoldLedgerOnly("item:" + std::to_string(a.Id));
    }

    // 4) Delete the auction row IN-TXN (appends to the caller's open transaction).
    a.DeleteFromDB();

    // 5) Defer the AH-map erase + object delete LAST, so `this` stays valid for
    //    every earlier deferred closure throughout def.run(). Snapshot the map
    //    pointer + Id by value (the closure must not read `this` after delete).
    AuctionHouseObject* houseMap = sAuctionMgr.GetAuctionsMap(a.auctionHouseEntry);
    uint32 const aucId = a.Id;
    AuctionEntry* self = &a;
    def.effects.push_back([houseMap, aucId, self]()
    {
        houseMap->RemoveAuction(aucId);
        delete self;
    });
}

/**
 * @brief Custody co-commit mirror of the unsold-expiry path (spec S6).
 *
 * The no-bidder expiry branch of AuctionHouseObject::Update() routes here when
 * the auction carries live custody rows. Returns the item to the seller by mail
 * (or destroys it if the account is gone), forfeits the deposit to the house,
 * deletes the auction row, and defers every in-memory effect (owner-expired
 * notification, mail push, RemoveAItem, RemoveAuction, delete this) into @p def.
 * Every DB write appends to the caller's ALREADY-OPEN CharacterDatabase
 * transaction; the caller checked-commits it then runs @p def (spec Sec 6 S6).
 * S6 makes NO synchronous in-memory mutation (every effect is deferred), so on
 * rollback there is nothing to restore -- the auction + item survive intact and
 * the next tick re-resolves cleanly.
 */
void AhExpireUnsoldCustody(AuctionEntry& a, CustodyDeferred& def)
{
    // 1) Return the item to the seller (or destroy if the account is gone) +
    //    flip the "item:<Id>" escrow row, all co-committed; the online owner's
    //    expired notification + RemoveAItem are deferred by the co-commit core.
    AhSendAuctionExpiredMailInTransaction(&a, def);

    // 2) Deposit FORFEIT to the house on an unsold expiry (legacy keeps it):
    //    flip "dep:<Id>" -> TERMINAL_OK ledger-only (house sink, no money, no
    //    mail). Mirror S5 cancel's deposit forfeit.
    CustodyService::CommitGoldLedgerOnly("dep:" + std::to_string(a.Id));

    // 3) Delete the auction row IN-TXN (appends to the caller's open transaction).
    a.DeleteFromDB();

    // 4) Defer the AH-map erase + object delete LAST, so `this` stays valid for
    //    every earlier deferred closure throughout def.run(). Snapshot the map
    //    pointer + Id by value (the closure must not read `this` after delete).
    //    Mirror AuctionBidWinningCustody :1373-1380.
    AuctionHouseObject* houseMap = sAuctionMgr.GetAuctionsMap(a.auctionHouseEntry);
    uint32 const aucId = a.Id;
    AuctionEntry* self = &a;
    def.effects.push_back([houseMap, aucId, self]()
    {
        houseMap->RemoveAuction(aucId);
        delete self;
    });
}

void AhPrepareCancelCustody(AuctionEntry& a, Player* seller, CustodyDeferred& def,
                            bool usesPlayerSellerCustody,
                            bool hasLiveBidCustody,
                            std::string const& liveBidKey,
                            uint32 auctionCut)
{
    if (a.bid)
    {
        seller->ModifyMoney(-int32(auctionCut));
    }

    if (a.bidder != 0)
    {
        if (hasLiveBidCustody)
        {
            CustodyService::RollbackGoldLedgerOnly(liveBidKey);
        }
        AhSendAuctionCancelledToBidderMailInTransaction(&a, def);
    }

    if (usesPlayerSellerCustody)
    {
        CustodyService::CommitGoldLedgerOnly("dep:" + std::to_string(a.Id));
    }

    Item* item = sAuctionMgr.GetAItem(a.itemGuidLow);
    MANGOS_ASSERT(item);
    uint32 const savedItemGuidLow = a.itemGuidLow;
    def.effects.push_back([savedItemGuidLow]()
    {
        sAuctionMgr.RemoveAItem(savedItemGuidLow);
    });

    std::ostringstream subject;
    subject << a.itemTemplate << ":" << a.itemRandomPropertyId << ":" << AUCTION_CANCELED;
    MailDraft itemReturn(subject.str(), "");
    itemReturn.AddItem(item);
    if (usesPlayerSellerCustody)
    {
        CustodyService::DeliverItem(def, "item:" + std::to_string(a.Id), itemReturn,
                                    MailReceiver(seller), MailSender(&a),
                                    MAIL_CHECK_MASK_COPIED);
    }
    else
    {
        itemReturn.SendMailToInTransaction(MailReceiver(seller), MailSender(&a),
                                           def.effects, MAIL_CHECK_MASK_COPIED);
    }

    seller->SaveInventoryAndGoldToDB();
    a.DeleteFromDB();
}

/**
 * @brief Custody co-commit mirror of UpdateBid.
 *
 * Reproduces UpdateBid's gold movement EXACTLY but through the custody
 * primitives, appending every DB write to the caller's already-open
 * CharacterDatabase transaction (the caller opens and checked-commits it).
 * Live effects (outbid notify + refund mail push) are queued into @p def and
 * run only after the checked commit succeeds.
 *
 * A buyout (newbid >= buyout) is absorbed here (Task 10): the bid is capped at
 * buyout, reserved/refunded as a normal bid, then the win is resolved on the same
 * open transaction via AuctionBidWinningCustody, which defers the auction delete
 * + cache mutations into @p def. Returns false on a buyout (auction no longer
 * active), true on a normal bid.
 *
 * @param newbid     The new bid amount (capped at buyout here, as in UpdateBid).
 * @param newbidder  The bidding player, or NULL for a generated bot bid.
 * @param def        Ordered deferred-effects queue for this co-commit.
 * @param liveBidKey idem_key of the existing live bid row (validated by the
 *                   handler), empty when the auction has no live bidder.
 * @return true if the auction remains active (normal bid); false on buyout.
 */
bool AhUpdateBidCustody(AuctionEntry& a, uint32 newbid, Player* newbidder, CustodyDeferred& def,
                        bool usesPlayerSellerCustody,
                        bool hadLiveBidCustody,
                        std::string const& liveBidKey)
{
    // Cap the bid at buyout FIRST, mirroring UpdateBid (:1055-1058). A buyout bid
    // (newbid >= buyout) now runs through custody (Task 10): it reserves/refunds
    // exactly like a normal bid and then resolves the win in this same open txn.
    if (a.buyout && newbid > a.buyout)
    {
        newbid = a.buyout;
    }
    bool const isBuyout = (a.buyout != 0 && newbid >= a.buyout);

    // The idem_key of the bid row that becomes the winner's bid on a buyout. On a
    // same-bidder buyout it is the existing (topped-up) live bid row; otherwise it
    // is the freshly reserved row. Passed to AuctionBidWinningCustody so it can
    // commit-net the row WITHOUT a synchronous SELECT (the row is uncommitted in
    // this same open txn). Empty when bidder ends up 0 (no row to commit).
    std::string winningBidKey;

    if (newbidder && newbidder->GetGUIDLow() == a.bidder)
    {
        if (hadLiveBidCustody)
        {
            CustodyService::TopUpBid(liveBidKey, newbid, newbid - a.bid, newbidder);
            winningBidKey = liveBidKey;
        }
        else
        {
            // Seller custody can meet a legacy standing bid. Preserve the
            // legacy delta debit, then establish custody at the full new amount.
            newbidder->ModifyMoney(-int32(newbid - a.bid));
            newbidder->SaveInventoryAndGoldToDB();
            winningBidKey = "bid:" + std::to_string(a.Id) + ":" +
                            std::to_string(CustodyLedger::NextBidSeq(a.Id));
            CustodyService::ReserveGoldAlreadyDebited(
                newbidder->GetGUIDLow(), newbid, winningBidKey, a.Id, ROLE_BID);
        }
    }
    else
    {
        // Refund/displace a REAL prior bidder first (reads the OLD bid), then
        // reserve the new bidder's full amount. A bot-displaced bid
        // (bid>0, bidder==0) carries no custody row: no rollback, no outbid mail
        // (matches UpdateBid's `if (bidder)` skipping the refund -- spec R2).
        if (a.bidder != 0)
        {
            if (hadLiveBidCustody)
            {
                CustodyService::RollbackGoldLedgerOnly(liveBidKey);
            }
            AhSendAuctionOutbiddedMailInTransaction(&a, def);
        }

        // Reserve the new bidder's full bid (debits -newbid + SaveInventory).
        // Mirrors UpdateBid's `if (newbidder) ModifyMoney(-newbid)`.
        // NextBidSeq returns MAX(id) of existing bid rows: monotonic, never
        // decreases after TTL pruning, so the suffix is always strictly greater
        // than every existing row's suffix -- UNIQUE constraint cannot fire.
        if (newbidder)
        {
            std::string newBidKey = "bid:" + std::to_string(a.Id) + ":" +
                                    std::to_string(CustodyLedger::NextBidSeq(a.Id));
            CustodyService::ReserveGold(def, newbidder->GetGUIDLow(),
                                        newbidder, newbid, newBidKey, a.Id, ROLE_BID);
            winningBidKey = newBidKey;
        }
    }

    a.bidder = newbidder ? newbidder->GetGUIDLow() : 0;
    a.bid = newbid;

    if (!isBuyout)                                          // normal a.bid
    {
        // The new bidder's gold was already persisted by ReserveGold/TopUpBid
        // (SaveInventoryAndGoldToDB), so do NOT save again. The auction UPDATE
        // appends to the caller's open transaction.
        CharacterDatabase.PExecute("UPDATE `auction` SET `buyguid` = '%u', `lastbid` = '%u' WHERE `id` = '%u'", a.bidder, a.bid, a.Id);
        return true;
    }

    // Buyout: resolve the win on this same open transaction. The winner's gold is
    // already persisted (ReserveGold/TopUpBid above), so AuctionBidWinningCustody
    // does NOT re-save it. Pass winningBidKey so the bid-row commit-net does not
    // SELECT for the uncommitted row. Generated buyers have no gold reservation.
    // The auction is deleted in a deferred closure run only after the caller's
    // checked commit succeeds.
    AhAuctionBidWinningCustody(a, newbidder, def, usesPlayerSellerCustody,
                             !winningBidKey.empty(), winningBidKey);
    return false;
}

AuctionExpiry AhExpireAuction(AuctionEntry& auction)
{
    // Runtime disable stops config-gated custody entry and maintenance, not
    // settlement of value already represented by durable rows.
    CustodyRouteState const route = CustodyLedger::GetRouteState(auction.Id);
    if (!route.known)
    {
        sLog.outError("custody route unavailable; deferring auction expiry");
        return AUCTION_EXPIRY_STOP;
    }

    ///- perform the transaction if there was bidder
    if (auction.bid)
    {
        // Seller and bid custody are independent. A worker/bot listing can
        // carry only a player bid row; a player listing can still carry a
        // legacy bid with no bid row.
        if (!route.usesPlayerSellerCustody && !route.hasLiveBidCustody)
        {
            return AUCTION_EXPIRY_CORE;
        }

        std::string liveBidKey;
        if (route.hasLiveBidCustody)
        {
            CustodyRow liveRow;
            if (auction.bidder == 0 ||
                !CustodyLedger::GetSingleLiveBidRow(auction.Id, liveRow) ||
                liveRow.ownerGuid != auction.bidder ||
                liveRow.amount != auction.bid)
            {
                sLog.outError("custody S4: live bid row validation failed for auction %u "
                              "(bidder %u, bid %u); failing closed",
                              auction.Id, auction.bidder, auction.bid);
                return AUCTION_EXPIRY_DONE;
            }
            liveBidKey = liveRow.idemKey;
        }

        uint32 const auctionId = auction.Id;
        CustodyDeferred def;
        CharacterDatabase.BeginTransaction();
        AhAuctionBidWinningCustody(auction, NULL, def, route.usesPlayerSellerCustody,
                                   route.hasLiveBidCustody, liveBidKey);
        CustodyService::MaybeCrash("pre-commit");
        if (CharacterDatabase.CommitTransactionChecked())
        {
            CustodyService::MaybeCrash("pre-deferred");
            def.run();
        }
        else
        {
            // Nothing live changed before the commit: the auction and its custody
            // item survive and the next tick settles them again.
            sLog.outError("custody S4: win txn rolled back for auction %u", auctionId);
        }
        return AUCTION_EXPIRY_DONE;
    }

    // Unsold expiry has no bid value to settle, so only player seller custody
    // selects the custody transaction.
    if (!route.usesPlayerSellerCustody)
    {
        return AUCTION_EXPIRY_CORE;
    }

    uint32 const auctionId = auction.Id;
    CustodyDeferred def;
    CharacterDatabase.BeginTransaction();
    AhExpireUnsoldCustody(auction, def);
    CustodyService::MaybeCrash("pre-commit");
    if (CharacterDatabase.CommitTransactionChecked())
    {
        CustodyService::MaybeCrash("pre-deferred");
        def.run();
    }
    else
    {
        sLog.outError("custody S6: expire txn rolled back for auction %u", auctionId);
    }
    return AUCTION_EXPIRY_DONE;
}

bool AhSettleGeneratedBid(AuctionEntry& auction, uint32 newbid, bool& stillActive, bool& applied)
{
    applied = false;
    stillActive = false;

    // Both service intents and the in-process buyer enter here. Preserve player
    // custody when a generated bid displaces its current owner.
    CustodyRouteState const route = CustodyLedger::GetRouteState(auction.Id);
    if (!route.known)
    {
        return true;
    }
    if (!route.usesPlayerSellerCustody && !route.hasLiveBidCustody)
    {
        return false;
    }

    std::string liveBidKey;
    if (route.hasLiveBidCustody)
    {
        CustodyRow row;
        if (auction.bidder == 0u || !CustodyLedger::GetSingleLiveBidRow(auction.Id, row) ||
            row.ownerGuid != auction.bidder || row.amount != auction.bid)
        {
            sLog.outError("custody bot bid validation failed for auction %u", auction.Id);
            return true;
        }
        liveBidKey = row.idemKey;
    }

    uint32 const oldBid = auction.bid;
    uint32 const oldBidder = auction.bidder;
    CustodyDeferred def;
    if (!CharacterDatabase.BeginTransaction())
    {
        return true;
    }
    bool const active = AhUpdateBidCustody(auction, newbid, NULL, def,
        route.usesPlayerSellerCustody, route.hasLiveBidCustody, liveBidKey);
    if (!CustodyService::CommitCheckedOrForcedFail("bot-bid"))
    {
        auction.bid = oldBid;
        auction.bidder = oldBidder;
        return true;
    }
    applied = true;
    stillActive = active;
    def.run(); // A successful buyout deletes this auction last.
    return true;
}

void AhPlaceGeneratedBid(AuctionEntry& auction, uint32 newbid, bool& applied)
{
    bool stillActive = false;
    if (!AhSettleGeneratedBid(auction, newbid, stillActive, applied))
    {
        auction.UpdateBid(newbid);
        applied = true;
    }
}

AuctionEntry* AhAddAuctionInTransaction(AuctionHouseObject& house, AuctionHouseEntry const* auctionHouseEntry,
                                        Item* newItem, uint32 etime, uint32 bid, uint32 buyout,
                                        uint32 deposit, Player* pl)
{
    uint32 auction_time = uint32(etime * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_TIME));

    AuctionEntry* AH = new AuctionEntry;
    AH->Id = sObjectMgr.GenerateAuctionID();
    AH->itemGuidLow = newItem->GetObjectGuid().GetCounter();
    AH->itemTemplate = newItem->GetEntry();
    AH->itemCount = newItem->GetCount();
    AH->itemRandomPropertyId = newItem->GetItemRandomPropertyId();
    AH->owner = pl->GetGUIDLow();
    AH->startbid = bid;
    AH->bidder = 0;
    AH->bid = 0;
    AH->buyout = buyout;
    AH->expireTime = time(NULL) + auction_time;
    AH->deposit = deposit;
    AH->auctionHouseEntry = auctionHouseEntry;

    house.AddAuction(AH);

    sAuctionMgr.AddAItem(newItem);

    pl->MoveItemFromInventory(newItem->GetBagSlot(), newItem->GetSlot(), true);

    newItem->DeleteFromInventoryDB();
    newItem->SaveToDB();
    AH->SaveToDB();
    pl->SaveInventoryAndGoldToDB();

    return AH;
}
