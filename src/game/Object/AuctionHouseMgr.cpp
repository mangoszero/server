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

#include "AuctionHouseMgr.h"
#include "Database/DatabaseEnv.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "ProgressBar.h"

#include "AccountMgr.h"
#include "Item.h"
#include "Language.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Mail.h"
#include "AuctionHouseBot/CustodyService.h"
#include "AuctionHouseBot/CustodyLedger.h"
#include "AuctionHouseBot/CustodyDeferred.h"

#include "Policies/Singleton.h"

#include <string>

/** \addtogroup auctionhouse
 * @{
 * \file
 */

INSTANTIATE_SINGLETON_1(AuctionHouseMgr);

/**
 * @brief Initializes the auction house manager.
 */
AuctionHouseMgr::AuctionHouseMgr()
{
}

/**
 * @brief Destroys the auction house manager and frees cached auction items.
 */
AuctionHouseMgr::~AuctionHouseMgr()
{
    for (ItemMap::const_iterator itr = mAitems.begin(); itr != mAitems.end(); ++itr)
    {
        delete itr->second;
    }
}

/**
 * @brief Retrieves the auction collection for a specific auction house entry.
 *
 * @param house The auction house entry.
 * @return Pointer to the auction collection serving that house.
 */
AuctionHouseObject* AuctionHouseMgr::GetAuctionsMap(AuctionHouseEntry const* house)
{
    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
    {
        return &mAuctions[AUCTION_HOUSE_NEUTRAL];
    }

    // team have linked auction houses
    switch (GetAuctionHouseTeam(house))
    {
        case ALLIANCE: return &mAuctions[AUCTION_HOUSE_ALLIANCE];
        case HORDE:    return &mAuctions[AUCTION_HOUSE_HORDE];
        default:       return &mAuctions[AUCTION_HOUSE_NEUTRAL];
    }
}

/**
 * @brief Calculates the auction deposit required for an item listing.
 *
 * @param entry The auction house entry containing deposit settings.
 * @param time The auction duration.
 * @param pItem The item being listed.
 * @return The required deposit amount.
 */
uint32 AuctionHouseMgr::GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item* pItem)
{
    float deposit = float(pItem->GetProto()->SellPrice * pItem->GetCount() * (time / MIN_AUCTION_TIME));

    deposit = deposit * entry->depositPercent / 100.0f;

    float min_deposit = float(sWorld.getConfig(CONFIG_UINT32_AUCTION_DEPOSIT_MIN));

    if (deposit < min_deposit)
    {
        deposit = min_deposit;
    }

    return uint32(deposit * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_DEPOSIT));
}

// does not clear ram

/**
 * @brief Sends the winning bidder mail for a completed auction.
 *
 * @param auction The completed auction entry.
 */
void AuctionHouseMgr::SendAuctionWonMail(AuctionEntry* auction)
{
    Item* pItem = GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        return;
    }

    ObjectGuid bidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* bidder = sObjectMgr.GetPlayer(bidder_guid);

    uint32 bidder_accId = 0;

    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, auction->owner);
    // data for gm.log
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
        // owner in `data` will set at mail receive and item extracting
        CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", auction->bidder, auction->itemGuidLow);

        if (bidder)
        {
            bidder->GetSession()->SendAuctionBidderNotification(auction, true);
        }

        RemoveAItem(auction->itemGuidLow);                  // we have to remove the item, before we delete it !!
        auction->itemGuidLow = 0;                           // pending list will not use guid data

        // will delete item or place to receiver mail list
        MailDraft(msgAuctionWonSubject.str(), msgAuctionWonBody.str())
            .AddItem(pItem)
            .SendMailTo(MailReceiver(bidder, bidder_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
    // receiver not exist
    else
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", auction->itemGuidLow);
        RemoveAItem(auction->itemGuidLow);                  // we have to remove the item, before we delete it !!
        auction->itemGuidLow = 0;
        delete pItem;
    }
}

// call this method to send mail to auction owner, when auction is successful, it does not clear ram

/**
 * @brief Sends the successful sale mail to the auction owner.
 *
 * @param auction The completed auction entry.
 */
void AuctionHouseMgr::SendAuctionSuccessfulMail(AuctionEntry* auction)
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

        if (owner)
        {
            // send auction owner notification, bidder must be current!
            owner->GetSession()->SendAuctionOwnerNotification(auction, true);
        }

        MailDraft(msgAuctionSuccessfulSubject.str(), auctionSuccessfulBody.str())
            .SetMoney(profit)
            .SendMailTo(MailReceiver(owner, owner_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
}

// does not clear ram

/**
 * @brief Sends the expired auction mail returning the item to the owner.
 *
 * @param auction The expired auction entry.
 */
void AuctionHouseMgr::SendAuctionExpiredMail(AuctionEntry* auction)
{
    // return an item in auction to its owner by mail
    Item* pItem = GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        sLog.outError("Auction item (GUID: %u) not found, and lost.", auction->itemGuidLow);
        return;
    }

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
        std::ostringstream subject;
        subject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_EXPIRED;

        if (owner)
        {
            owner->GetSession()->SendAuctionOwnerNotification(auction, false);
        }

        RemoveAItem(auction->itemGuidLow);                  // we have to remove the item, before we delete it !!
        auction->itemGuidLow = 0;

        // will delete item or place to receiver mail list
        MailDraft(subject.str(),"")
            .AddItem(pItem)
            .SendMailTo(MailReceiver(owner, owner_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
    // owner not found
    else
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", auction->itemGuidLow);
        RemoveAItem(auction->itemGuidLow);                  // we have to remove the item, before we delete it !!
        auction->itemGuidLow = 0;
        delete pItem;
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
void AuctionHouseMgr::SendAuctionSuccessfulMailInTransaction(AuctionEntry* auction, CustodyDeferred& def)
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
                    p->GetSession()->SendAuctionOwnerNotificationData(houseId, aucId, bidValue, outbid, bidder, itemTpl, itemRand, true);
                }
            });
        }

        MailDraft(msgAuctionSuccessfulSubject.str(), auctionSuccessfulBody.str())
            .SetMoney(profit)
            .SendMailToInTransaction(MailReceiver(owner, owner_guid), MailSender(auction), def, MAIL_CHECK_MASK_COPIED);
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
void AuctionHouseMgr::SendAuctionWonMailInTransaction(AuctionEntry* auction, CustodyDeferred& def)
{
    Item* pItem = GetAItem(auction->itemGuidLow);
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
    // closure must use the original value (auction->itemGuidLow is zeroed below).
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
                    p->GetSession()->SendAuctionBidderNotificationData(houseId, aucId, bidderGuidLow, bidValue, outbid, itemTpl, itemRand, true);
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
            .SendMailToInTransaction(MailReceiver(bidder, bidder_guid), MailSender(auction), def, MAIL_CHECK_MASK_COPIED);
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
 * @brief Loads auction items from the database into memory.
 */
void AuctionHouseMgr::LoadAuctionItems()
{
    // data needs to be at first place for Item::LoadFromDB 0  1        2
    QueryResult* result = CharacterDatabase.Query("SELECT `data`,`itemguid`,`item_template` FROM `auction` JOIN `item_instance` ON `itemguid` = `guid`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 auction items");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    Field* fields;
    do
    {
        bar.step();

        fields = result->Fetch();
        uint32 item_guid        = fields[1].GetUInt32();
        uint32 item_template    = fields[2].GetUInt32();

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_template);

        if (!proto)
        {
            sLog.outError("AuctionHouseMgr::LoadAuctionItems: Unknown item (GUID: %u id: #%u) in auction, skipped.", item_guid, item_template);
            continue;
        }

        Item* item = NewItemOrBag(proto);

        if (!item->LoadFromDB(item_guid, fields))
        {
            delete item;
            continue;
        }
        AddAItem(item);

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u auction items", count);
    sLog.outString();
}

/**
 * @brief Loads auction listings from the database.
 */
void AuctionHouseMgr::LoadAuctions()
{
    QueryResult* result = CharacterDatabase.Query("SELECT COUNT(*) FROM `auction`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded 0 auctions. DB table `auction` is empty.");
        return;
    }

    Field* fields = result->Fetch();
    uint32 AuctionCount = fields[0].GetUInt32();
    delete result;

    if (!AuctionCount)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded 0 auctions. DB table `auction` is empty.");
        return;
    }

    //                                       0  1       2        3             4          5                     6         7           8    9       10      11       12
    result = CharacterDatabase.Query("SELECT `id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,`lastbid`,`startbid`,`deposit` FROM `auction`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> Loaded 0 auctions. DB table `auction` is empty.");
        return;
    }

    BarGoLink bar(AuctionCount);

    do
    {
        fields = result->Fetch();

        bar.step();

        AuctionEntry* auction = new AuctionEntry;
        auction->Id = fields[0].GetUInt32();
        uint32 houseid  = fields[1].GetUInt32();
        auction->itemGuidLow = fields[2].GetUInt32();
        auction->itemTemplate = fields[3].GetUInt32();
        auction->itemCount = fields[4].GetUInt32();
        auction->itemRandomPropertyId = fields[5].GetUInt32();
        auction->owner = fields[6].GetUInt32();
        auction->buyout = fields[7].GetUInt32();
        auction->expireTime = time_t(fields[8].GetUInt64());
        auction->bidder = fields[9].GetUInt32();
        auction->bid = fields[10].GetUInt32();
        auction->startbid = fields[11].GetUInt32();
        auction->deposit = fields[12].GetUInt32();

        auction->auctionHouseEntry = NULL;                  // init later

        // check if sold item exists for guid
        // and item_template in fact (GetAItem will fail if problematic in result check in AuctionHouseMgr::LoadAuctionItems)
        Item* pItem = GetAItem(auction->itemGuidLow);
        if (!pItem)
        {
            auction->DeleteFromDB();
            sLog.outError("Auction %u has not a existing item : %u, deleted", auction->Id, auction->itemGuidLow);
            delete auction;
            continue;
        }

        // overwrite by real item data
        if ((auction->itemTemplate != pItem->GetEntry()) ||
            (auction->itemCount != pItem->GetCount()) ||
            (auction->itemRandomPropertyId != pItem->GetItemRandomPropertyId()))
        {
            auction->itemTemplate = pItem->GetEntry();
            auction->itemCount    = pItem->GetCount();
            auction->itemRandomPropertyId = pItem->GetItemRandomPropertyId();

            // No SQL injection (no strings)
            CharacterDatabase.PExecute("UPDATE `auction` SET `item_template` = %u, `item_count` = %u, `item_randompropertyid` = %i WHERE `itemguid` = %u",
                auction->itemTemplate, auction->itemCount, auction->itemRandomPropertyId, auction->itemGuidLow);
        }

        auction->auctionHouseEntry = sAuctionHouseStore.LookupEntry(houseid);

        if (!auction->auctionHouseEntry)
        {
            // need for send mail, use goblin auctionhouse
            auction->auctionHouseEntry = sAuctionHouseStore.LookupEntry(7);

            // Attempt send item back to owner
            std::ostringstream msgAuctionCanceledOwner;
            msgAuctionCanceledOwner << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_CANCELED;

            if (auction->itemGuidLow)
            {
                RemoveAItem(auction->itemGuidLow);
                auction->itemGuidLow = 0;

                // item will deleted or added to received mail list
                MailDraft(msgAuctionCanceledOwner.str(), "")// TODO: fix body
                    .AddItem(pItem)
                    .SendMailTo(MailReceiver(ObjectGuid(HIGHGUID_PLAYER, auction->owner)), auction, MAIL_CHECK_MASK_COPIED);
            }

            auction->DeleteFromDB();
            delete auction;

            continue;
        }

        GetAuctionsMap(auction->auctionHouseEntry)->AddAuction(auction);
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u auctions", AuctionCount);
    sLog.outString();
}

/**
 * @brief Adds an auction item to the in-memory item cache.
 *
 * @param it The item to cache.
 */
void AuctionHouseMgr::AddAItem(Item* it)
{
    MANGOS_ASSERT(it);
    MANGOS_ASSERT(mAitems.find(it->GetGUIDLow()) == mAitems.end());
    mAitems[it->GetGUIDLow()] = it;
}

/**
 * @brief Removes an auction item from the in-memory item cache.
 *
 * @param id The low GUID of the cached item.
 * @return true if the item was removed; otherwise, false.
 */
bool AuctionHouseMgr::RemoveAItem(uint32 id)
{
    ItemMap::iterator i = mAitems.find(id);
    if (i == mAitems.end())
    {
        return false;
    }
    mAitems.erase(i);
    return true;
}

/**
 * @brief Updates all auction house collections.
 */
void AuctionHouseMgr::Update()
{
    for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        mAuctions[i].Update();
    }
}

/**
 * @brief Resolves the team associated with an auction house entry.
 *
 * @param house The auction house entry.
 * @return The team identifier, or 0 for neutral houses.
 */
uint32 AuctionHouseMgr::GetAuctionHouseTeam(AuctionHouseEntry const* house)
{
    // auction houses have faction field pointing to PLAYER,* factions,
    // but player factions not have filled team field, and hard go from faction value to faction_template value,
    // so more easy just sort by auction house ids
    switch (house->houseId)
    {
        case 1: case 2: case 3:
            return ALLIANCE;
        case 4: case 5: case 6:
            return HORDE;
        case 7:
        default:
            return 0;                                       // neutral
    }
}

/**
 * @brief Determines which auction house entry a unit should access.
 *
 * @param unit The interacting unit.
 * @return Pointer to the selected auction house entry.
 */
AuctionHouseEntry const* AuctionHouseMgr::GetAuctionHouseEntry(Unit* unit)
{
    uint32 houseid = 1;                                     // dwarf auction house (used for normal cut/etc percents)

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
    {
        if (unit->GetTypeId() == TYPEID_UNIT)
        {
            // FIXME: found way for proper auctionhouse selection by another way
            // AuctionHouse.dbc have faction field with _player_ factions associated with auction house races.
            // but no easy way convert creature faction to player race faction for specific city
            uint32 factionTemplateId = unit->getFaction();
            switch (factionTemplateId)
            {
                case   12: houseid = 1; break;              // human
                case   29: houseid = 6; break;              // orc, and generic for horde
                case   55: houseid = 2; break;              // dwarf/gnome, and generic for alliance
                case   68: houseid = 4; break;              // undead
                case   80: houseid = 3; break;              // n-elf
                case  104: houseid = 5; break;              // trolls
                case  120: houseid = 7; break;              // booty bay, neutral
                case  474: houseid = 7; break;              // gadgetzan, neutral
                case  534: houseid = 2; break;              // Alliance Generic
                case  855: houseid = 7; break;              // everlook, neutral
                default:                                    // for unknown case
                {
                    FactionTemplateEntry const* u_entry = sFactionTemplateStore.LookupEntry(factionTemplateId);
                    if (!u_entry)
                    {
                        houseid = 7; // goblin auction house
                    }
                    else if (u_entry->ourMask & FACTION_MASK_ALLIANCE)
                    {
                        houseid = 1; // human auction house
                    }
                    else if (u_entry->ourMask & FACTION_MASK_HORDE)
                    {
                        houseid = 6; // orc auction house
                    }
                    else
                    {
                        houseid = 7; // goblin auction house
                    }
                    break;
                }
            }
        }
        else
        {
            Player* player = (Player*)unit;
            if (player->GetAuctionAccessMode() > 0)
            {
                houseid = 7;
            }
            else
            {
                switch (((Player*)unit)->GetTeam())
                {
                    case ALLIANCE: houseid = player->GetAuctionAccessMode() == 0 ? 1 : 6; break;
                    case HORDE:    houseid = player->GetAuctionAccessMode() == 0 ? 6 : 1; break;
                    default: break;
                }
            }
        }
    }

    return sAuctionHouseStore.LookupEntry(houseid);
}

/**
 * @brief Updates auction entries and expires finished auctions.
 */
void AuctionHouseObject::Update()
{
    time_t curTime = sWorld.GetGameTime();
    ///- Handle expired auctions
    for (AuctionEntryMap::iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end();)
    {
        AuctionEntryMap::iterator old = itr++;
        if (curTime > old->second->expireTime)
        {
            ///- perform the transaction if there was bidder
            if (old->second->bid)
            {
                // Custody co-commit path (per-auction drain, X3): only auctions
                // carrying live custody rows resolve through the ledger; legacy
                // (pre-gate / bot-created) auctions fall through unchanged.
                // `old = itr++` already advanced the iterator, so the deferred
                // RemoveAuction(Id) erase of `old`'s slot does NOT invalidate itr.
                if (sWorld.IsAhCustodyEnabled() && CustodyLedger::HasRows(old->second->Id))
                {
                    CustodyDeferred def;
                    CharacterDatabase.BeginTransaction();
                    old->second->AuctionBidWinningCustody(NULL, def);
                    if (CharacterDatabase.CommitTransactionChecked())
                    {
                        def.run();
                    }
                    else
                    {
                        // S4 mutates NO live state before the checked commit (every
                        // in-memory effect is deferred and does not run on rollback),
                        // so there is nothing to restore: the auction/object survive
                        // intact, the custody item stays in mAitems, and the next
                        // tick re-resolves cleanly with a valid GetAItem pointer.
                        sLog.outError("custody S4: win txn rolled back for auction %u", old->second->Id);
                    }
                }
                else
                {
                    old->second->AuctionBidWinning();
                }
            }
            ///- cancel the auction if there was no bidder and clear the auction
            else
            {
                sAuctionMgr.SendAuctionExpiredMail(old->second);

                old->second->DeleteFromDB();
                sAuctionMgr.RemoveAItem(old->second->itemGuidLow);
                delete old->second;
                AuctionsMap.erase(old);
                continue;
            }
        }
    }
}

/**
 * @brief Builds the list of auctions the player is currently bidding on.
 *
 * @param data The packet buffer to append to.
 * @param player The player requesting the list.
 * @param count Receives the number of appended entries.
 * @param totalcount Receives the total number of matching entries.
 */
void AuctionHouseObject::BuildListBidderItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount)
{
    for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        if (Aentry->bidder == player->GetGUIDLow())
        {
            if (itr->second->BuildAuctionInfo(data))
            {
                ++count;
            }
            ++totalcount;
        }
    }
}

/**
 * @brief Builds the list of auctions owned by the player.
 *
 * @param data The packet buffer to append to.
 * @param player The player requesting the list.
 * @param count Receives the number of appended entries.
 * @param totalcount Receives the total number of matching entries.
 */
void AuctionHouseObject::BuildListOwnerItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount)
{
    for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        if (Aentry->owner == player->GetGUIDLow())
        {
            if (Aentry->BuildAuctionInfo(data))
            {
                ++count;
            }
            ++totalcount;
        }
    }
}

/**
 * @brief Builds the filtered public auction browse list.
 *
 * @param data The packet buffer to append to.
 * @param player The player requesting the list.
 * @param wsearchedname The search string in wide-character form.
 * @param listfrom The starting result offset.
 * @param levelmin The minimum required item level filter.
 * @param levelmax The maximum required item level filter.
 * @param usable Whether only usable items should be listed.
 * @param inventoryType The inventory type filter.
 * @param itemClass The item class filter.
 * @param itemSubClass The item subclass filter.
 * @param quality The minimum quality filter.
 * @param count Receives the number of appended entries.
 * @param totalcount Receives the total number of matching entries.
 */
void AuctionHouseObject::BuildListAuctionItems(WorldPacket& data, Player* player,
    std::wstring const& wsearchedname, uint32 listfrom, uint32 levelmin, uint32 levelmax, uint32 usable,
    uint32 inventoryType, uint32 itemClass, uint32 itemSubClass, uint32 quality,
    uint32& count, uint32& totalcount)
{
    int loc_idx = player->GetSession()->GetSessionDbLocaleIndex();

    for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        Item* item = sAuctionMgr.GetAItem(Aentry->itemGuidLow);
        if (!item)
        {
            continue;
        }

        {
            ItemPrototype const* proto = item->GetProto();

            if (itemClass != 0xffffffff && proto->Class != itemClass)
            {
                continue;
            }

            if (itemSubClass != 0xffffffff && proto->SubClass != itemSubClass)
            {
                continue;
            }

            if (inventoryType != 0xffffffff && proto->InventoryType != inventoryType)
            {
                continue;
            }

            if (quality != 0xffffffff && proto->Quality < quality)
            {
                continue;
            }

            if (levelmin != 0x00 && (proto->RequiredLevel < levelmin || (levelmax != 0x00 && proto->RequiredLevel > levelmax)))
            {
                continue;
            }

            if (usable != 0x00)
            {
                if (player->CanUseItem(item) != EQUIP_ERR_OK)
                {
                    continue;
                }

                if (proto->Class == ITEM_CLASS_RECIPE)
                {
                    if (SpellEntry const* spell = sSpellStore.LookupEntry(proto->Spells[0].SpellId))
                    {
                        if (player->HasSpell(spell->EffectTriggerSpell[EFFECT_INDEX_0]))
                        {
                            continue;
                        }
                    }
                }
            }

            std::string name = proto->Name1;
            sObjectMgr.GetItemLocaleStrings(proto->ItemId, loc_idx, &name);

            if (!wsearchedname.empty() && !Utf8FitTo(name, wsearchedname))
            {
                continue;
            }

            if (count < 50 && totalcount >= listfrom)
            {
                ++count;
                Aentry->BuildAuctionInfo(data);
            }
        }

        ++totalcount;
    }
}

/**
 * @brief Creates and stores a new auction from a player's item.
 *
 * @param auctionHouseEntry The target auction house entry.
 * @param newItem The item being auctioned.
 * @param etime The requested auction duration.
 * @param bid The starting bid.
 * @param buyout The buyout price.
 * @param deposit The deposit amount.
 * @param pl The player creating the auction.
 * @param ownTransaction When true (default, all legacy callers) the four DB
 *        writes are wrapped in this method's own Begin/CommitTransaction.  When
 *        false the caller has already opened a transaction and the writes
 *        auto-append to it (custody co-commit seam, spec S1); the in-memory
 *        parts run identically in both cases.
 * @return Pointer to the created auction entry.
 */
AuctionEntry* AuctionHouseObject::AddAuction(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint32 bid, uint32 buyout, uint32 deposit, Player* pl /*= NULL*/, bool ownTransaction /*= true*/)
{
    uint32 auction_time = uint32(etime * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_TIME));

    AuctionEntry* AH = new AuctionEntry;
    AH->Id = sObjectMgr.GenerateAuctionID();
    AH->itemGuidLow = newItem->GetObjectGuid().GetCounter();
    AH->itemTemplate = newItem->GetEntry();
    AH->itemCount = newItem->GetCount();
    AH->itemRandomPropertyId = newItem->GetItemRandomPropertyId();
    AH->owner = pl ? pl->GetGUIDLow() : 0;
    AH->startbid = bid;
    AH->bidder = 0;
    AH->bid = 0;
    AH->buyout = buyout;
    AH->expireTime = time(NULL) + auction_time;
    AH->deposit = deposit;
    AH->auctionHouseEntry = auctionHouseEntry;

    AddAuction(AH);

    sAuctionMgr.AddAItem(newItem);

    if (pl)
    {
        pl->MoveItemFromInventory(newItem->GetBagSlot(), newItem->GetSlot(), true);
    }

    if (ownTransaction)
    {
        CharacterDatabase.BeginTransaction();
    }

    if (pl)
    {
        newItem->DeleteFromInventoryDB();
    }

    newItem->SaveToDB();
    AH->SaveToDB();

    if (pl)
    {
        pl->SaveInventoryAndGoldToDB();
    }

    if (ownTransaction)
    {
        CharacterDatabase.CommitTransaction();
    }

    return AH;
}

/**
 * @brief Creates and stores a new auction for an explicit owner GUID.
 *
 * @param auctionHouseEntry The target auction house entry.
 * @param newItem The item being auctioned.
 * @param etime The requested auction duration.
 * @param bid The starting bid.
 * @param buyout The buyout price.
 * @param lowguid The low GUID of the auction owner.
 * @return Pointer to the created auction entry.
 */
AuctionEntry* AuctionHouseObject::AddAuctionByGuid(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint32 bid, uint32 buyout, uint32 lowguid)
{
    uint32 auction_time = uint32(etime * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_TIME));

    AuctionEntry* AH = new AuctionEntry;
    AH->Id = sObjectMgr.GenerateAuctionID();
    AH->itemGuidLow = newItem->GetObjectGuid().GetCounter();
    AH->itemTemplate = newItem->GetEntry();
    AH->itemCount = newItem->GetCount();
    AH->itemRandomPropertyId = newItem->GetItemRandomPropertyId();
    AH->owner = lowguid;
    AH->startbid = bid;
    AH->bidder = 0;
    AH->bid = 0;
    AH->buyout = buyout;
    AH->expireTime = time(NULL) + auction_time;
    AH->deposit = 0;
    AH->auctionHouseEntry = auctionHouseEntry;

    AddAuction(AH);

    sAuctionMgr.AddAItem(newItem);

    CharacterDatabase.BeginTransaction();

    newItem->SaveToDB();
    AH->SaveToDB();

    CharacterDatabase.CommitTransaction();

    return AH;
}

// this function inserts to WorldPacket auction's data

/**
 * @brief Serializes auction information into a packet.
 *
 * @param data The packet buffer to append to.
 * @return true if the auction item data was available; otherwise, false.
 */
bool AuctionEntry::BuildAuctionInfo(WorldPacket& data) const
{
    Item* pItem = sAuctionMgr.GetAItem(itemGuidLow);
    if (!pItem)
    {
        sLog.outError("auction to item, that doesn't exist !!!!");
        return false;
    }
    data << uint32(Id);
    data << uint32(pItem->GetEntry());

    // [-ZERO] no other infos about enchantment in 1.12 [?]
    data << uint32(pItem->GetEnchantmentId(EnchantmentSlot(PERM_ENCHANTMENT_SLOT)));

    data << uint32(pItem->GetItemRandomPropertyId());       // random item property id
    data << uint32(pItem->GetItemSuffixFactor());           // SuffixFactor
    data << uint32(pItem->GetCount());                      // item->count
    data << uint32(pItem->GetSpellCharges());               // item->charge FFFFFFF
    data << ObjectGuid(HIGHGUID_PLAYER, owner);             // Auction->owner
    data << uint32(startbid);                               // Auction->startbid (not sure if useful)
    data << uint32(bid ? GetAuctionOutBid() : 0);           // minimal outbid
    data << uint32(buyout);                                 // auction->buyout
    data << uint32((expireTime - time(NULL))*IN_MILLISECONDS); // time left
    data << ObjectGuid(HIGHGUID_PLAYER, bidder);            // auction->bidder current
    data << uint32(bid && startbid > bid ? startbid : bid); // current bid

    return true;
}

/**
 * @brief Computes the auction house cut for the final bid.
 *
 * @return The auction house cut amount.
 */
uint32 AuctionEntry::GetAuctionCut() const
{
    return uint32(auctionHouseEntry->cutPercent * bid * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_CUT) / 100.0f);
}

/// the sum of outbid is (1% from current bid)*5, if bid is very small, it is 1c
uint32 AuctionEntry::GetAuctionOutBid() const
{
    uint32 outbid = (bid / 100) * 5;
    if (!outbid)
    {
        outbid = 1;
    }
    return outbid;
}

/**
 * @brief Deletes this auction entry from the character database.
 */
void AuctionEntry::DeleteFromDB() const
{
    // No SQL injection (Id is integer)
    CharacterDatabase.PExecute("DELETE FROM `auction` WHERE `id` = '%u'", Id);
}

/**
 * @brief Saves this auction entry to the character database.
 */
void AuctionEntry::SaveToDB() const
{
    // No SQL injection (no strings)
    CharacterDatabase.PExecute("INSERT INTO `auction` (`id`,`houseid`,`itemguid`,`item_template`,`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,`lastbid`,`startbid`,`deposit`) "
        "VALUES ('%u', '%u', '%u', '%u', '%u', '%i', '%u', '%u', '" UI64FMTD "', '%u', '%u', '%u', '%u')",
        Id, auctionHouseEntry->houseId, itemGuidLow, itemTemplate, itemCount, itemRandomPropertyId, owner, buyout, (uint64)expireTime, bidder, bid, startbid, deposit);
}

/**
 * @brief Finalizes a winning auction bid and completes the sale.
 *
 * @param newbidder The winning bidder, if online.
 */
void AuctionEntry::AuctionBidWinning(Player* newbidder)
{
    sAuctionMgr.SendAuctionSuccessfulMail(this);
    sAuctionMgr.SendAuctionWonMail(this);

    sAuctionMgr.RemoveAItem(this->itemGuidLow);
    sAuctionMgr.GetAuctionsMap(this->auctionHouseEntry)->RemoveAuction(this->Id);

    CharacterDatabase.BeginTransaction();
    this->DeleteFromDB();
    if (newbidder)
    {
        newbidder->SaveInventoryAndGoldToDB();
    }
    CharacterDatabase.CommitTransaction();

    delete this;
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
 * Netting (Sec 5.4): the seller is paid bid + deposit - cut by the single legacy
 * seller mail (step 1); the deposit + bid ledger rows are flipped LEDGER-ONLY
 * (no second mail, no released coin). The deposit row "dep:<Id>" returns; the live
 * bid row commits -- UNLESS bidder == 0 (bot-displaced win), where no bid row
 * exists so the commit is skipped and the item destroys (winner guid 0).
 *
 * Gold note: do NOT re-save newbidder's gold here. On a buyout it was already
 * saved by ReserveGold/TopUpBid in UpdateBidCustody; on the expiry path newbidder
 * is NULL.
 *
 * @param newbidder The online winner (buyout) or NULL (expiry).
 * @param def       Ordered deferred-effects queue for this co-commit.
 */
void AuctionEntry::AuctionBidWinningCustody(Player* newbidder, CustodyDeferred& def,
                                           std::string const& knownBidKey)
{
    // (void) newbidder: its gold is already persisted by the bid seam (buyout) or
    // it is NULL (expiry); the auction UPDATE/DELETE persists the rest.
    (void)newbidder;

    // 1) Seller payout = bid + deposit - cut (single legacy mail; owner-notify
    //    deferred BEFORE the mail push by the co-commit core).
    sAuctionMgr.SendAuctionSuccessfulMailInTransaction(this, def);

    // 2) Netting (Sec 5.4, ledger-only -- the seller mail above already carries
    //    both the deposit return and the proceeds, so flip the rows WITHOUT mail
    //    or coin to avoid double-crediting).
    CustodyService::RollbackGoldLedgerOnly("dep:" + std::to_string(Id));
    if (bidder != 0)
    {
        // Commit the live bid row. A bot-displaced win (bidder == 0) carried no bid
        // custody row -> skip (spec R2/X3).
        if (!knownBidKey.empty())
        {
            // Buyout path: the bid row was RESERVED in this same still-open txn, so
            // a synchronous SELECT cannot see it yet -- use the key the bid seam
            // just reserved.
            CustodyService::CommitGoldLedgerOnly(knownBidKey);
        }
        else
        {
            // Expiry path: the bid row is committed -> fetch + validate it.
            CustodyRow liveBidRow;
            if (CustodyLedger::GetSingleLiveBidRow(Id, liveBidRow))
            {
                CustodyService::CommitGoldLedgerOnly(liveBidRow.idemKey);
            }
            else
            {
                // Fail-soft: a real bidder with no single live bid row is a custody
                // drift (logged for ah repair). The seller is still paid and the
                // item still delivers; only the bid row's terminal flip is skipped.
                sLog.outError("custody S4: no single live bid row for auction %u (bidder %u); "
                              "skipping bid commit", Id, bidder);
            }
        }
    }

    // 3) Item to winner (receiver-exists owner UPDATE) or destroy (bidder == 0 ->
    //    account lookup fails -> destroy branch). Bidder-notify + RemoveAItem +
    //    (destroy: delete pItem) are deferred by the co-commit core.
    sAuctionMgr.SendAuctionWonMailInTransaction(this, def);

    // Terminalize the item escrow row: on a win the item always resolves
    // (delivered to the winner or destroyed by SendAuctionWonMailInTransaction
    // above), so flip "item:<Id>" -> TERMINAL_OK ledger-only. In-txn, so on
    // rollback the flip rolls back with everything else (no orphan). Without
    // this the "item:" row stays CST_RESERVED after the auction row is deleted
    // -> orphaned non-terminal row (breaks reconciliation / ah repair).
    CustodyService::CommitGoldLedgerOnly("item:" + std::to_string(Id));

    // 4) Delete the auction row IN-TXN (appends to the caller's open transaction).
    this->DeleteFromDB();

    // 5) Defer the AH-map erase + object delete LAST, so `this` stays valid for
    //    every earlier deferred closure throughout def.run(). Snapshot the map
    //    pointer + Id by value (the closure must not read `this` after delete).
    AuctionHouseObject* houseMap = sAuctionMgr.GetAuctionsMap(this->auctionHouseEntry);
    uint32 const aucId = this->Id;
    AuctionEntry* self = this;
    def.effects.push_back([houseMap, aucId, self]()
    {
        houseMap->RemoveAuction(aucId);
        delete self;
    });
}

/**
 * @brief Updates the current bid and handles buyout completion if reached.
 *
 * @param newbid The new bid amount.
 * @param newbidder The player placing the bid.
 * @return true if the auction remains active after the update; otherwise, false.
 */
bool AuctionEntry::UpdateBid(uint32 newbid, Player* newbidder /*=NULL*/)
{
    Player* auction_owner = owner ? sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, owner)) : NULL;

    // bid can't be greater buyout
    if (buyout && newbid > buyout)
    {
        newbid = buyout;
    }

    if (newbidder && newbidder->GetGUIDLow() == bidder)
    {
        newbidder->ModifyMoney(-int32(newbid - bid));
    }
    else
    {
        if (newbidder)
        {
            newbidder->ModifyMoney(-int32(newbid));
        }

        if (bidder)                                     // return money to old bidder if present
        {
            WorldSession::SendAuctionOutbiddedMail(this);
        }
    }

    bidder = newbidder ? newbidder->GetGUIDLow() : 0;
    bid = newbid;

    if ((newbid < buyout) || (buyout == 0))                 // bid
    {
        // after this update we should save player's money ...
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("UPDATE `auction` SET `buyguid` = '%u', `lastbid` = '%u' WHERE `id` = '%u'", bidder, bid, Id);
        if (newbidder)
        {
            newbidder->SaveInventoryAndGoldToDB();
        }
        CharacterDatabase.CommitTransaction();
        return true;
    }
    else                                                    // buyout
    {
        AuctionBidWinning(newbidder);
        return false;
    }
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
 * @param newbidder  The player placing the bid (always non-NULL for the player seam).
 * @param def        Ordered deferred-effects queue for this co-commit.
 * @param liveBidKey idem_key of the existing live bid row (validated by the
 *                   handler), empty when the auction has no live bidder.
 * @return true if the auction remains active (normal bid); false on buyout.
 */
bool AuctionEntry::UpdateBidCustody(uint32 newbid, Player* newbidder, CustodyDeferred& def,
                                   std::string const& liveBidKey)
{
    // Cap the bid at buyout FIRST, mirroring UpdateBid (:1055-1058). A buyout bid
    // (newbid >= buyout) now runs through custody (Task 10): it reserves/refunds
    // exactly like a normal bid and then resolves the win in this same open txn.
    if (buyout && newbid > buyout)
    {
        newbid = buyout;
    }
    bool const isBuyout = (buyout != 0 && newbid >= buyout);

    // The idem_key of the bid row that becomes the winner's bid on a buyout. On a
    // same-bidder buyout it is the existing (topped-up) live bid row; otherwise it
    // is the freshly reserved row. Passed to AuctionBidWinningCustody so it can
    // commit-net the row WITHOUT a synchronous SELECT (the row is uncommitted in
    // this same open txn). Empty when bidder ends up 0 (no row to commit).
    std::string winningBidKey;

    if (newbidder && newbidder->GetGUIDLow() == bidder)
    {
        // same-bidder raise: debit the DELTA and bump the live bid row amount.
        // The full-price affordability guard in the handler already gated this
        // (spec I1). Mirrors UpdateBid's ModifyMoney(-(newbid - bid)). The live
        // bid key was pre-fetched and VALIDATED by the handler (spec I1).
        CustodyService::TopUpBid(liveBidKey, newbid, newbid - bid, newbidder);
        winningBidKey = liveBidKey;
    }
    else
    {
        // Refund/displace a REAL prior bidder first (reads the OLD bid), then
        // reserve the new bidder's full amount. A bot-displaced bid
        // (bid>0, bidder==0) carries no custody row: no rollback, no outbid mail
        // (matches UpdateBid's `if (bidder)` skipping the refund -- spec R2).
        if (bidder != 0)
        {
            // liveBidKey was pre-fetched and VALIDATED by the handler (owner_guid
            // == bidder, amount == bid, exactly one live row) before the txn
            // opened (spec I1), so terminalize exactly that verified row.
            CustodyService::RollbackGoldLedgerOnly(liveBidKey);
            WorldSession::SendAuctionOutbiddedMailInTransaction(this, def);
        }

        // Reserve the new bidder's full bid (debits -newbid + SaveInventory).
        // Mirrors UpdateBid's `if (newbidder) ModifyMoney(-newbid)`.
        // NextBidSeq returns MAX(id) of existing bid rows: monotonic, never
        // decreases after TTL pruning, so the suffix is always strictly greater
        // than every existing row's suffix -- UNIQUE constraint cannot fire.
        std::string newBidKey = "bid:" + std::to_string(Id) + ":" +
                                std::to_string(CustodyLedger::NextBidSeq(Id));
        CustodyService::ReserveGold(def, newbidder ? newbidder->GetGUIDLow() : 0,
                                    newbidder, newbid, newBidKey, Id, ROLE_BID);
        winningBidKey = newBidKey;
    }

    bidder = newbidder ? newbidder->GetGUIDLow() : 0;
    bid = newbid;

    if (!isBuyout)                                          // normal bid
    {
        // The new bidder's gold was already persisted by ReserveGold/TopUpBid
        // (SaveInventoryAndGoldToDB), so do NOT save again. The auction UPDATE
        // appends to the caller's open transaction.
        CharacterDatabase.PExecute("UPDATE `auction` SET `buyguid` = '%u', `lastbid` = '%u' WHERE `id` = '%u'", bidder, bid, Id);
        return true;
    }

    // Buyout: resolve the win on this same open transaction. The winner's gold is
    // already persisted (ReserveGold/TopUpBid above), so AuctionBidWinningCustody
    // does NOT re-save it. Pass winningBidKey so the bid-row commit-net does not
    // SELECT for the uncommitted row (bidder==0 cannot happen here: a player buyout
    // always has a live newbidder). The auction is deleted in a deferred closure
    // run only after the caller's checked commit succeeds.
    AuctionBidWinningCustody(newbidder, def, winningBidKey);
    return false;
}

/** @} */
