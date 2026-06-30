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

/**
 * @file AuctionHouseHandler.cpp
 * @brief Auction house opcode handlers
 *
 * This file handles auction house-related opcodes including:
 * - CMSG_AUCTION_HELLO: Open auction house interface
 * - CMSG_AUCTION_LIST_ITEMS: List auction items
 * - CMSG_AUCTION_SELL_ITEM: Sell item on auction
 * - CMSG_AUCTION_BID: Bid on auction
 * - CMSG_AUCTION_REMOVE_ITEM: Cancel auction
 *
 * The auction house allows players to buy and sell items
 * with other players using the in-game currency.
 */

#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "AuctionHouseMgr.h"
#include "AuctionHouseBot/CustodyService.h"
#include "AuctionHouseBot/CustodyLedger.h"
#include "AuctionHouseBot/BrowsePending.h"
#include "Mail.h"
#include "Util.h"
#include "Chat.h"
#include "ReputationMgr.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "SpellMgr.h"
#include "BrowseMessages.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "WorkerSupervisor.h"
#include <string>
#include <vector>
#include <map>
#include <ctime>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/** \addtogroup auctionhouse
 * @{
 * \file
 */

// please DO NOT use iterator++, because it is slower than ++iterator!!!
// post-incrementation is always slower than pre-incrementation !

// void called when player click on auctioneer npc
void WorldSession::HandleAuctionHelloOpcode(WorldPacket& recv_data)
{
    ObjectGuid auctioneerGuid;                              // NPC guid
    recv_data >> auctioneerGuid;

    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(auctioneerGuid, UNIT_NPC_FLAG_AUCTIONEER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleAuctionHelloOpcode - %s not found or you can't interact with him.", auctioneerGuid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    SendAuctionHello(unit);
}

// this void causes that auction window is opened
void WorldSession::SendAuctionHello(Unit* unit)
{
    // always return pointer
    AuctionHouseEntry const* ahEntry = AuctionHouseMgr::GetAuctionHouseEntry(unit);

    WorldPacket data(MSG_AUCTION_HELLO, 12);
    data << unit->GetObjectGuid();
    data << uint32(ahEntry->houseId);
    SendPacket(&data);
}

// call this method when player bids, creates, or deletes auction
void WorldSession::SendAuctionCommandResult(AuctionEntry* auc, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError, uint32 newOutbid /*= 0*/)
{
    // The AUCTION_ERR_HIGHER_BID branch dereferences auc (bidder/bid); it is
    // only ever sent inline on a guard-reject (never deferred), so it stays
    // here. All other branches are value-only and delegate to the by-value
    // variant below. The bid-OK increment is resolved here while auc is live.
    if (ErrorCode == AUCTION_ERR_HIGHER_BID)
    {
        WorldPacket data(SMSG_AUCTION_COMMAND_RESULT, 16);
        data << uint32(auc ? auc->Id : 0);
        data << uint32(Action);
        data << uint32(ErrorCode);
        data << ObjectGuid(HIGHGUID_PLAYER, auc->bidder);   // new bidder guid
        data << uint32(auc->bid);                           // new bid
        data << uint32(auc->GetAuctionOutBid());            // new AuctionOutBid?
        SendPacket(&data);
        return;
    }

    uint32 outbid = newOutbid;
    if (ErrorCode == AUCTION_OK && Action == AUCTION_BID_PLACED && !outbid)
    {
        outbid = auc->GetAuctionOutBid();                   // resolve while auc is live
    }

    SendAuctionCommandResultData(auc ? auc->Id : 0, Action, ErrorCode, invError, outbid);
}

// by-value variant: builds SMSG_AUCTION_COMMAND_RESULT from raw values so a
// deferred custody closure can fire it after the AuctionEntry is gone (spec I5)
void WorldSession::SendAuctionCommandResultData(uint32 aucId, AuctionAction Action, AuctionError ErrorCode, InventoryResult invError, uint32 newOutbid)
{
    WorldPacket data(SMSG_AUCTION_COMMAND_RESULT, 16);
    data << uint32(aucId);
    data << uint32(Action);
    data << uint32(ErrorCode);

    switch (ErrorCode)
    {
        case AUCTION_OK:
            if (Action == AUCTION_BID_PLACED)
            {
                data << uint32(newOutbid);                  // outbid increment (pre-resolved)
            }
            break;
        case AUCTION_ERR_INVENTORY:
            data << uint32(invError);
            break;
        default:
            break;
    }

    SendPacket(&data);
}

// this function sends notification, if bidder is online
void WorldSession::SendAuctionBidderNotification(AuctionEntry* auction, bool won)
{
    SendAuctionBidderNotificationData(auction->GetHouseId(), auction->Id, auction->bidder,
                                      auction->bid, auction->GetAuctionOutBid(),
                                      auction->itemTemplate, auction->itemRandomPropertyId, won);
}

// by-value variant of SendAuctionBidderNotification (spec I5)
void WorldSession::SendAuctionBidderNotificationData(uint32 houseId, uint32 id, uint32 bidder, uint32 bid, uint32 outbid, uint32 itemTemplate, int32 itemRand, bool won)
{
    WorldPacket data(SMSG_AUCTION_BIDDER_NOTIFICATION, (8 * 4));
    data << uint32(houseId);
    data << uint32(id);
    data << ObjectGuid(HIGHGUID_PLAYER, bidder);

    // if 0, client shows ERR_AUCTION_WON_S, else ERR_AUCTION_OUTBID_S
    data << uint32(won ? 0 : bid);
    data << uint32(outbid);                                 // AuctionOutBid?
    data << uint32(itemTemplate);
    data << int32(itemRand);

    SendPacket(&data);
}

// this void causes on client to display: "Your auction sold"
void WorldSession::SendAuctionOwnerNotification(AuctionEntry* auction, bool sold)
{
    SendAuctionOwnerNotificationData(auction->GetHouseId(), auction->Id, auction->bid,
                                     auction->GetAuctionOutBid(), auction->bidder,
                                     auction->itemTemplate, auction->itemRandomPropertyId, sold);
}

// by-value variant of SendAuctionOwnerNotification (spec I5)
void WorldSession::SendAuctionOwnerNotificationData(uint32 houseId, uint32 id, uint32 bid, uint32 outbid, uint32 bidderGuidLow, uint32 itemTemplate, int32 itemRand, bool sold)
{
    // SMSG_AUCTION_OWNER_NOTIFICATION carries no houseId field (unlike the bidder
    // notification); it is accepted for signature symmetry with the other by-value
    // builders and to snapshot it alongside the other scalars.
    (void)houseId;

    WorldPacket data(SMSG_AUCTION_OWNER_NOTIFICATION, (7 * 4));
    data << uint32(id);
    data << uint32(bid);                                    // if 0, client shows ERR_AUCTION_EXPIRED_S, else ERR_AUCTION_SOLD_S (works only when guid==0)
    data << uint32(outbid);                                 // AuctionOutBid?

    ObjectGuid bidder_guid = ObjectGuid();
    if (!sold)                                              // not sold yet
    {
        bidder_guid = ObjectGuid(HIGHGUID_PLAYER, bidderGuidLow);
    }

    // bidder==0 and moneyDeliveryTime==0 for expired auctions, and client shows error messages as described above
    // if bidder!=0 client updates auctions with new bid, outbid and bidderGuid
    data << bidder_guid;                                    // bidder guid
    data << uint32(itemTemplate);                           // item entry
    data << int32(itemRand);

    SendPacket(&data);
}

// shows ERR_AUCTION_REMOVED_S
void WorldSession::SendAuctionRemovedNotification(AuctionEntry* auction)
{
    SendAuctionRemovedNotificationData(auction->Id, auction->itemTemplate, auction->itemRandomPropertyId);
}

// by-value variant of SendAuctionRemovedNotification (spec I5)
void WorldSession::SendAuctionRemovedNotificationData(uint32 id, uint32 itemTemplate, int32 itemRand)
{
    WorldPacket data(SMSG_AUCTION_REMOVED_NOTIFICATION, (3 * 4));
    data << uint32(id);
    data << uint32(itemTemplate);
    data << uint32(itemRand);

    SendPacket(&data);
}

// this function sends mail to old bidder
void WorldSession::SendAuctionOutbiddedMail(AuctionEntry* auction)
{
    ObjectGuid oldBidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* oldBidder = sObjectMgr.GetPlayer(oldBidder_guid);

    uint32 oldBidder_accId = 0;
    if (!oldBidder)
    {
        oldBidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(oldBidder_guid);
    }

    // old bidder exist
    if (oldBidder || oldBidder_accId)
    {
        std::ostringstream msgAuctionOutbiddedSubject;
        msgAuctionOutbiddedSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_OUTBIDDED;

        if (oldBidder)
        {
            oldBidder->GetSession()->SendAuctionBidderNotification(auction, false);
        }

        MailDraft(msgAuctionOutbiddedSubject.str(),"")
            .SetMoney(auction->bid)
            .SendMailTo(MailReceiver(oldBidder, oldBidder_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
}

// Custody co-commit variant of SendAuctionOutbiddedMail. Same old-bidder guard;
// it (a) defers the online bidder notification snapshotted BY VALUE so it fires
// in legacy order (notify before mail), then (b) co-commits the refund mail into
// the caller's open transaction (its own online push is appended AFTER the notify
// by SendMailToInTransaction). Reads the OLD auction->bid, so the caller MUST
// call this before mutating bid (spec B / S2).
void WorldSession::SendAuctionOutbiddedMailInTransaction(AuctionEntry* auction, CustodyDeferred& def)
{
    ObjectGuid oldBidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* oldBidder = sObjectMgr.GetPlayer(oldBidder_guid);

    uint32 oldBidder_accId = 0;
    if (!oldBidder)
    {
        oldBidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(oldBidder_guid);
    }

    // old bidder exist
    if (oldBidder || oldBidder_accId)
    {
        std::ostringstream msgAuctionOutbiddedSubject;
        msgAuctionOutbiddedSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_OUTBIDDED;

        // Defer the online bidder notification, snapshotting every field BY
        // VALUE (the buyout path may delete the AuctionEntry before run()).
        // Appended BEFORE the refund mail so packet order = notify-then-mail.
        // The closure captures the old bidder's low GUID + packet scalars only
        // (no live WorldSession*/Player*/AuctionEntry*) and RE-RESOLVES the player
        // by GUID at run time, skipping the live packet if offline -- the durable
        // mail row is authoritative (spec I2).
        if (oldBidder)
        {
            uint32 oldBidderGuidLow = auction->bidder;
            uint32 houseId  = auction->GetHouseId();
            uint32 aucId    = auction->Id;
            uint32 bidder   = auction->bidder;
            uint32 bidValue = auction->bid;
            uint32 outbid   = auction->GetAuctionOutBid();
            uint32 itemTpl  = auction->itemTemplate;
            int32  itemRand = auction->itemRandomPropertyId;
            def.effects.push_back([oldBidderGuidLow, houseId, aucId, bidder, bidValue, outbid, itemTpl, itemRand]()
            {
                Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, oldBidderGuidLow));
                if (p)
                {
                    p->GetSession()->SendAuctionBidderNotificationData(houseId, aucId, bidder, bidValue, outbid, itemTpl, itemRand, false);
                }
            });
        }

        MailDraft(msgAuctionOutbiddedSubject.str(),"")
            .SetMoney(auction->bid)
            .SendMailToInTransaction(MailReceiver(oldBidder, oldBidder_guid), MailSender(auction), def, MAIL_CHECK_MASK_COPIED);
    }
}

// this function sends mail, when auction is cancelled to old bidder
void WorldSession::SendAuctionCancelledToBidderMail(AuctionEntry* auction)
{
    ObjectGuid bidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* bidder = sObjectMgr.GetPlayer(bidder_guid);

    uint32 bidder_accId = 0;
    if (!bidder)
    {
        bidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(bidder_guid);
    }

    // bidder exist
    if (bidder || bidder_accId)
    {
        std::ostringstream msgAuctionCancelledSubject;
        msgAuctionCancelledSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_CANCELLED_TO_BIDDER;

        if (bidder)
        {
            bidder->GetSession()->SendAuctionRemovedNotification(auction);
        }

        MailDraft(msgAuctionCancelledSubject.str(),"")
            .SetMoney(auction->bid)
            .SendMailTo(MailReceiver(bidder, bidder_guid), auction, MAIL_CHECK_MASK_COPIED);
    }
}

// Custody co-commit variant of SendAuctionCancelledToBidderMail. Same old-bidder
// guard; it (a) defers the online bidder's DISTINCT SMSG_AUCTION_REMOVED_NOTIFICATION
// snapshotted BY VALUE so it fires in legacy order (notify before mail), then
// (b) co-commits the refund mail (money = auction->bid) into the caller's open
// transaction (its own online push is appended AFTER the notify by
// SendMailToInTransaction). Reads auction->bid, so the caller MUST call this
// before any bid mutation (there is none in S5; cancel deletes the auction).
// Note: unlike the outbid path this emits SendAuctionRemovedNotification (a third,
// distinct opcode), NOT SendAuctionBidderNotification (spec B / S5).
void WorldSession::SendAuctionCancelledToBidderMailInTransaction(AuctionEntry* auction, CustodyDeferred& def)
{
    ObjectGuid bidder_guid = ObjectGuid(HIGHGUID_PLAYER, auction->bidder);
    Player* bidder = sObjectMgr.GetPlayer(bidder_guid);

    uint32 bidder_accId = 0;
    if (!bidder)
    {
        bidder_accId = sObjectMgr.GetPlayerAccountIdByGUID(bidder_guid);
    }

    // bidder exist
    if (bidder || bidder_accId)
    {
        std::ostringstream msgAuctionCancelledSubject;
        msgAuctionCancelledSubject << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_CANCELLED_TO_BIDDER;

        // Defer the online bidder's removed-notification, snapshotting every field
        // BY VALUE (the cancel deletes the AuctionEntry before run()). Appended
        // BEFORE the refund mail so packet order = notify-then-mail (legacy :334).
        // The closure captures the bidder's low GUID + packet scalars only (no live
        // WorldSession*/Player*/AuctionEntry*) and RE-RESOLVES the player by GUID at
        // run time, skipping the live packet if offline -- the durable mail row is
        // authoritative (spec I2).
        if (bidder)
        {
            uint32 bidderGuidLow = auction->bidder;
            uint32 aucId    = auction->Id;
            uint32 itemTpl  = auction->itemTemplate;
            int32  itemRand = auction->itemRandomPropertyId;
            def.effects.push_back([bidderGuidLow, aucId, itemTpl, itemRand]()
            {
                Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, bidderGuidLow));
                if (p)
                {
                    p->GetSession()->SendAuctionRemovedNotificationData(aucId, itemTpl, itemRand);
                }
            });
        }

        MailDraft(msgAuctionCancelledSubject.str(),"")
            .SetMoney(auction->bid)
            .SendMailToInTransaction(MailReceiver(bidder, bidder_guid), MailSender(auction), def, MAIL_CHECK_MASK_COPIED);
    }
}

/**
 * @brief Resolves the auction house entry for a player or auctioneer interaction.
 *
 * @param guid The guid of the player or auctioneer being used.
 * @return AuctionHouseEntry const* The resolved auction house entry, or NULL when access is invalid.
 */
AuctionHouseEntry const* WorldSession::GetCheckedAuctionHouseForAuctioneer(ObjectGuid guid)
{
    Unit* auctioneer;

    // GM case
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        // using special access modes (1,-1) done at mode set in command, so not need recheck
        if (GetPlayer()->GetAuctionAccessMode() == 0 && !ChatHandler(GetPlayer()).FindCommand("auction"))
        {
            DEBUG_LOG("%s attempt open auction in cheating way.", guid.GetString().c_str());
            return NULL;
        }

        auctioneer = GetPlayer();
    }
    // auctioneer case
    else
    {
        auctioneer = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_AUCTIONEER);
        if (!auctioneer)
        {
            DEBUG_LOG("Auctioneer %s accessed in cheating way.", guid.GetString().c_str());
            return NULL;
        }
    }

    // always return pointer
    return AuctionHouseMgr::GetAuctionHouseEntry(auctioneer);
}

// this void creates new auction and adds auction to some auctionhouse
void WorldSession::HandleAuctionSellItem(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionSellItem");

    ObjectGuid auctioneerGuid;
    ObjectGuid itemGuid;
    uint32 etime, bid, buyout;

    recv_data >> auctioneerGuid;
    recv_data >> itemGuid;
    recv_data >> bid;
    recv_data >> buyout;
    recv_data >> etime;

    if (!bid || !etime)
    {
        return;                                              // check for cheaters
    }

    Player* pl = GetPlayer();

    AuctionHouseEntry const* auctionHouseEntry = GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    // always return pointer
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    // client send time in minutes, convert to common used sec time
    etime *= MINUTE;

    // client understand only 3 auction time
    switch (etime)
    {
        case 1*MIN_AUCTION_TIME:
        case 4*MIN_AUCTION_TIME:
        case 12*MIN_AUCTION_TIME:
            break;
        default:
            return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    if (!itemGuid)
    {
        return;
    }

    Item* it = pl->GetItemByGuid(itemGuid);

    // do not allow to sell already auctioned items
    if (sAuctionMgr.GetAItem(itemGuid.GetCounter()))
    {
        sLog.outError("AuctionError, %s is sending %s, but item is already in another auction", pl->GetGuidStr().c_str(), itemGuid.GetString().c_str());
        SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    // prevent sending bag with items (cheat: can be placed in bag after adding equipped empty bag to auction)
    if (!it)
    {
        SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    if (!it->CanBeTraded())
    {
        SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    if ((it->GetProto()->Flags & ITEM_FLAG_CONJURED) || it->GetUInt32Value(ITEM_FIELD_DURATION))
    {
        SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    // check money for deposit
    uint32 deposit = AuctionHouseMgr::GetAuctionDeposit(auctionHouseEntry, etime, it);
    if (pl->GetMoney() < deposit)
    {
        SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    if (GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        sLog.outCommand(GetAccountId(), "GM %s (Account: %u) create auction: %s (Entry: %u Count: %u)",
            GetPlayerName(), GetAccountId(), it->GetProto()->Name1, it->GetEntry(), it->GetCount());
    }

    /* The client limits owned auctions to 50: */
    /* Make sure we do not go over this limit, or the client will crash */
    char numTotalOwned = 0;
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionHouse->GetAuctions().begin(); itr != auctionHouse->GetAuctions().end(); ++itr)
    {
        AuctionEntry* Aentry = itr->second;
        if (Aentry->owner == pl->GetGUIDLow())
        {
            Item *pItem = sAuctionMgr.GetAItem(Aentry->itemGuidLow);
            if (!pItem)
            {
                sLog.outError("%s:%d:\tItem %id doesn't exist!", __FILE__, __LINE__, Aentry->itemGuidLow);
            }
            else
            {
                numTotalOwned++;
                if (numTotalOwned == 50)
                {
                    /* Player already listed 50 auctions; */
                    /* Send an internal error result back down to the client... */
                    return SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK);
                }
            }
        }
    }

    pl->ModifyMoney(-int32(deposit));

    AuctionEntry* AH;
    if (sWorld.IsAhCustodyEnabled())
    {
        CharacterDatabase.BeginTransaction();
        AH = auctionHouse->AddAuction(auctionHouseEntry, it, etime, bid, buyout, deposit, pl, false);
        std::string aucId = std::to_string(AH->Id);
        CustodyService::EscrowItem(AH->owner, AH->itemGuidLow, "item:" + aucId, AH->Id);
        CustodyService::ReserveGoldAlreadyDebited(AH->owner, deposit, "dep:" + aucId, AH->Id, ROLE_DEPOSIT);
        CustodyService::MaybeCrash("pre-commit");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            // FIX X6: durable rollback -> UNDO the in-memory mutations
            // AddAuction(ownTransaction=false) applied before the (now reverted)
            // DB writes. The checked commit failed, so CharacterDatabase rolled
            // back to the pre-seam DB state (gold intact, character_inventory link
            // intact, item_instance row intact, no custody_ledger rows). Live
            // memory, however, still holds: the debited deposit, the item moved out
            // of the seller's bag + into the AH cache, and a PHANTOM AuctionEntry in
            // the in-memory map (biddable, with no DB row). Restore live state to
            // exactly match the rolled-back DB, then report the DB error.
            uint32 const phantomId = AH->Id;
            uint32 const itemGuidLow = AH->itemGuidLow;

            // (1) Drop the phantom auction from the in-memory map and free it.
            //     RemoveAuction only erases the map slot (legacy cancel deletes the
            //     entry itself), so delete AH after.
            auctionHouse->RemoveAuction(phantomId);

            // (2) Drop the item from the AH item cache (mAitems). The Item* is NOT
            //     freed by RemoveAItem -- it is the same live object AddAuction moved
            //     out of the bag, so it survives for return-to-inventory below.
            sAuctionMgr.RemoveAItem(itemGuidLow);

            // (3) Return the item to the seller's inventory -- the canonical inverse
            //     of MoveItemFromInventory (see HandleMailTakeItem / TradeHandler
            //     item-return): resolve a destination with CanStoreItem, then
            //     MoveItemToInventory with in_characterInventoryDB=true (sets the
            //     live item ITEM_CHANGED). The item was never deleted and ownership
            //     was not cleared on removal, so this re-attaches the same object.
            //     No re-persist to DB is needed: the failed commit rolled the txn
            //     back, so the durable character_inventory link + item_instance row
            //     are STILL the seller's on disk (the success path's
            //     SaveInventoryAndGoldToDB DELETE of the inventory link never
            //     committed). This step only re-syncs in-memory state.
            ItemPosCountVec dest;
            if (pl->CanStoreItem(NULL_BAG, NULL_SLOT, dest, it, false) == EQUIP_ERR_OK)
            {
                pl->MoveItemToInventory(dest, it, true, true);
            }
            else
            {
                // Should be unreachable: we are returning the item to the very
                // inventory it left in this same synchronous handler (no other
                // packet ran in between to consume the slot). Even so, there is NO
                // durable item loss -- the rolled-back DB still holds the item as
                // the seller's, so it reloads into the bag on next login. Log loudly
                // rather than risk a duplicate by force-storing. (TradeHandler's
                // item-return fallback takes the same log-only stance.)
                sLog.outError("custody S1: seller %u could not re-store item %u on rollback of "
                              "auction %u; item remains durably the seller's on disk (reloads on relog)",
                              pl->GetGUIDLow(), itemGuidLow, phantomId);
            }

            // (4) Refund the in-memory deposit debit applied at :493. The DB gold
            //     already rolled back; this re-syncs memory (no re-save, mirroring
            //     the S2 bid-rollback refund).
            pl->ModifyMoney(int32(deposit));

            // (5) Free the phantom entry and report the failure to the client in
            //     place of the AUCTION_OK that the success path would have sent.
            delete AH;
            SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_DATABASE);
            sLog.outError("custody S1: create txn rolled back for auction %u; live state restored", phantomId);
            return;
        }

        CustodyService::MaybeCrash("pre-deferred");
    }
    else
    {
        AH = auctionHouse->AddAuction(auctionHouseEntry, it, etime, bid, buyout, deposit, pl);
    }

    DETAIL_LOG("selling %s to auctioneer %s with initial bid %u with buyout %u and with time %u (in sec) in auctionhouse %u",
        itemGuid.GetString().c_str(), auctioneerGuid.GetString().c_str(), bid, buyout, etime, auctionHouseEntry->houseId);

    SendAuctionCommandResult(AH, AUCTION_STARTED, AUCTION_OK);

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnAdd(auctionHouse, AH);
    }
#endif /* ENABLE_ELUNA */
}

// this function is called when client bids or buys out auction
void WorldSession::HandleAuctionPlaceBid(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionPlaceBid");

    ObjectGuid auctioneerGuid;
    uint32 auctionId;
    uint32 price;
    recv_data >> auctioneerGuid;
    recv_data >> auctionId >> price;

    if (!auctionId || !price)
    {
        return;                                              // check for cheaters
    }

    AuctionHouseEntry const* auctionHouseEntry = GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    // always return pointer
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    AuctionEntry* auction = auctionHouse->GetAuction(auctionId);
    Player* pl = GetPlayer();

    if (!auction || auction->owner == pl->GetGUIDLow())
    {
        // you can not bid your own auction:
        SendAuctionCommandResult(NULL, AUCTION_BID_PLACED, AUCTION_ERR_BID_OWN);
        return;
    }

    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, auction->owner);

    // impossible have online own another character (use this for speedup check in case online owner)
    Player* auction_owner = sObjectMgr.GetPlayer(ownerGuid);
    if (!auction_owner && sObjectMgr.GetPlayerAccountIdByGUID(ownerGuid) == pl->GetSession()->GetAccountId())
    {
        // you can not bid your another character auction:
        SendAuctionCommandResult(NULL, AUCTION_BID_PLACED, AUCTION_ERR_BID_OWN);
        return;
    }

    // cheating or client lags
    if (price <= auction->bid)
    {
        // client test but possible in result lags
        SendAuctionCommandResult(auction, AUCTION_BID_PLACED, AUCTION_ERR_HIGHER_BID);
        return;
    }

    // price too low for next bid if not buyout
    if ((price < auction->buyout || auction->buyout == 0) &&
        price < auction->bid + auction->GetAuctionOutBid())
    {
        // client test but possible in result lags
        SendAuctionCommandResult(auction, AUCTION_BID_PLACED, AUCTION_ERR_BID_INCREMENT);
        return;
    }

    if (price > pl->GetMoney())
    {
        // you don't have enough money!, client tests!
        // SendAuctionCommandResult(auction->auctionId, AUCTION_ERR_INVENTORY, EQUIP_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    // cheating
    if (price < auction->startbid)
    {
        return;
    }

    uint32 newOutbid = (price / 100) * 5;
    if (!newOutbid)
    {
        newOutbid = 1;
    }

    // Buyout now goes through custody too (Task 10): UpdateBidCustody caps the
    // bid at buyout, reserves/refunds as a normal bid, then routes to
    // AuctionBidWinningCustody, all on the handler's single open transaction.
    if (sWorld.IsAhCustodyEnabled() && CustodyLedger::HasRows(auction->Id))
    {
        // Custody co-commit path. The success-path SendAuctionCommandResult is
        // deferred and appended FIRST (its legacy position :507 precedes
        // UpdateBid), snapshotting auction->Id + newOutbid BY VALUE since the
        // buyout route deletes the AuctionEntry in the same deferred run (I5).
        uint32 const capId = auction->Id;

        // FIX I1: validated, fail-closed live-bid lookup BEFORE the txn (it is a
        // read; no live mutation has happened yet). When the auction already has
        // a bidder (same-bidder raise OR outbid), there MUST be exactly one live
        // bid row matching the current auction state; otherwise fail closed.
        std::string liveBidKey;
        if (auction->bidder != 0)
        {
            CustodyRow liveRow;
            if (!CustodyLedger::GetSingleLiveBidRow(capId, liveRow) ||
                liveRow.ownerGuid != auction->bidder ||
                liveRow.amount != auction->bid)
            {
                // No single matching live bid row -> abort before any mutation.
                sLog.outError("custody S2: live bid row validation failed for auction %u "
                              "(bidder %u, bid %u); failing closed", capId, auction->bidder, auction->bid);
                SendAuctionCommandResultData(capId, AUCTION_BID_PLACED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
                return;
            }
            liveBidKey = liveRow.idemKey;
        }

        // FIX C1: snapshot the in-memory state mutated by UpdateBidCustody so a
        // checked-commit rollback can restore it. effectiveBid mirrors
        // UpdateBidCustody's buyout cap (newbid = buyout when price > buyout) so the
        // X6 refund matches the actual debit on a buyout. Only the NEW bidder's
        // wallet is debited in memory; the old bidder's refund is mail-based and
        // rolls back with the txn, so no old-bidder restore is needed.
        uint32 const oldBid = auction->bid;
        uint32 const oldBidder = auction->bidder;
        uint32 const effectiveBid = (auction->buyout && price > auction->buyout) ? auction->buyout : price;
        uint32 const bidderDebit = (oldBidder == pl->GetGUIDLow()) ? (effectiveBid - oldBid) : effectiveBid;

        // Capture bidder by low GUID so the deferred closure holds only uint32
        // scalars; re-resolving at run-time avoids a dangling WorldSession* if
        // the player logs out between commit and def.run().
        uint32 const bidderGuidLow = pl->GetGUIDLow();

        CustodyDeferred def;
        def.effects.push_back([bidderGuidLow, capId, newOutbid]()
        {
            Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, bidderGuidLow));
            if (p)
            {
                p->GetSession()->SendAuctionCommandResultData(capId, AUCTION_BID_PLACED, AUCTION_OK, EQUIP_ERR_OK, newOutbid);
            }
        });

        CharacterDatabase.BeginTransaction();
        // UpdateBidCustody mutates only deferred/DB state and returns whether the
        // auction stays active: true = normal bid, false = BUYOUT (the win was
        // resolved on this same open txn; the auction delete + cache mutations are
        // deferred into def and run only on commit success). Both are success
        // paths -> proceed to the checked commit. On the buyout path the auction is
        // NOT deleted until def.run(), so the X6 restore below is still safe to
        // reference auction on a commit FAILURE (def.run() did not execute).
        bool const stillActive = auction->UpdateBidCustody(price, pl, def, liveBidKey);
        (void)stillActive;

        CustodyService::MaybeCrash("pre-commit");
        if (CharacterDatabase.CommitTransactionChecked())
        {
            CustodyService::MaybeCrash("pre-deferred");
            def.run();          // ordered live effects (command-result, notify/mail pushes, buyout win + delete)
        }
        else
        {
            // FIX C1/X6: durable rollback -> UNDO the in-memory mutations
            // UpdateBidCustody applied, refund the in-memory debit, and report the
            // DB error. The auction was NOT deleted (def.run() did not execute), so
            // restoring its bid/bidder is safe for both the normal-bid and buyout
            // paths. Otherwise the DB reverts but live memory keeps the bid/gold
            // and the next bid reads corrupted state. No items are freed here: the
            // deferred effects did not run, so every custody item survives in
            // mAitems and the buyout auction re-resolves on the next tick.
            auction->bid = oldBid;
            auction->bidder = oldBidder;
            pl->ModifyMoney(int32(bidderDebit));    // refund the debit we applied in memory
            SendAuctionCommandResultData(capId, AUCTION_BID_PLACED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
            sLog.outError("custody S2: bid txn rolled back for auction %u; live state restored", capId);
        }
    }
    else
    {
        SendAuctionCommandResult(auction, AUCTION_BID_PLACED, AUCTION_OK, EQUIP_ERR_OK, newOutbid);

        auction->UpdateBid(price, pl);
    }
}

// this void is called when auction_owner cancels his auction
void WorldSession::HandleAuctionRemoveItem(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionRemoveItem");

    ObjectGuid auctioneerGuid;
    uint32 auctionId;
    recv_data >> auctioneerGuid;
    recv_data >> auctionId;
    // DEBUG_LOG("Cancel AUCTION AuctionID: %u", auctionId);

    AuctionHouseEntry const* auctionHouseEntry = GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    // always return pointer
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    AuctionEntry* auction = auctionHouse->GetAuction(auctionId);
    Player* pl = GetPlayer();

    if (!auction || auction->owner != pl->GetGUIDLow())
    {
        SendAuctionCommandResult(NULL, AUCTION_REMOVED, AUCTION_ERR_DATABASE);
        sLog.outError("CHEATER : %u, he tried to cancel auction (id: %u) of another player, or auction is NULL", pl->GetGUIDLow(), auctionId);
        return;
    }

    Item* pItem = sAuctionMgr.GetAItem(auction->itemGuidLow);
    if (!pItem)
    {
        sLog.outError("Auction id: %u has nonexistent item (item guid : %u)!!!", auction->Id, auction->itemGuidLow);
        SendAuctionCommandResult(NULL, AUCTION_REMOVED, AUCTION_ERR_INVENTORY, EQUIP_ERR_ITEM_NOT_FOUND);
        return;
    }

    if (sWorld.IsAhCustodyEnabled() && CustodyLedger::HasRows(auction->Id))
    {
        // -------------------------------------------------------------------
        // Custody co-commit path (per-auction drain, X3). One checked txn:
        // cut debit + bidder refund (ledger + mail) + deposit FORFEIT (ledger
        // only) + item return to the seller, all co-committed; every live
        // effect deferred and run only on commit success (spec B/C, S5).
        // -------------------------------------------------------------------

        // (1) GUARD FIRST -- before any mutation or txn. Matches legacy :835-838
        //     (silent return, no command-result, no txn) so a too-poor seller
        //     cancelling a bid auction is byte-identical to today.
        uint32 const auctionCut = auction->bid ? auction->GetAuctionCut() : 0;
        if (auction->bid && pl->GetMoney() < auctionCut)
        {
            return;
        }

        uint32 const capId = auction->Id;
        uint32 const itemGuidLow = auction->itemGuidLow;

        // (2) I1 fail-closed live-bid lookup BEFORE the txn (it is a read; no
        //     mutation yet). When the auction has a real bidder there MUST be
        //     exactly one live bid row matching the current auction state;
        //     otherwise fail closed (no txn, no mutation).
        std::string liveBidKey;
        if (auction->bidder != 0)
        {
            CustodyRow liveRow;
            if (!CustodyLedger::GetSingleLiveBidRow(capId, liveRow) ||
                liveRow.ownerGuid != auction->bidder ||
                liveRow.amount != auction->bid)
            {
                sLog.outError("custody S5: live bid row validation failed for auction %u "
                              "(bidder %u, bid %u); failing closed", capId, auction->bidder, auction->bid);
                SendAuctionCommandResultData(capId, AUCTION_REMOVED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
                return;
            }
            liveBidKey = liveRow.idemKey;
        }

        // (3) X6 snapshot. The ONLY synchronous in-memory mutation S5 makes is
        //     the cut debit; capture it for restore-on-failure. (cutDebited == 0
        //     when there is no bid -- nothing to restore in that case.)
        uint32 const cutDebited = auctionCut;

        // Capture the seller's low GUID so the deferred command-result closure
        // holds only uint32 scalars; re-resolving at run-time avoids a dangling
        // WorldSession* if the seller logs out between commit and def.run().
        uint32 const sellerGuidLow = pl->GetGUIDLow();

        CustodyDeferred def;

        // -- in-memory mutation BEFORE the txn (SaveInventoryAndGoldToDB persists
        //    current memory) --
        if (auction->bid)
        {
            pl->ModifyMoney(-int32(auctionCut));
        }

        CharacterDatabase.BeginTransaction();

        // (a) Bidder refund: pushes the removed-notify THEN the refund-mail push
        //     into def (legacy notify-before-mail, :334-339). Only for a REAL
        //     bidder; a bot-bid auction (bidder==0) charges the cut but sends no
        //     refund (spec R2). Terminalize the validated live bid row (ledger
        //     only -- the refund coin rides the mail above).
        if (auction->bid && auction->bidder != 0)
        {
            CustodyService::RollbackGoldLedgerOnly(liveBidKey);
            SendAuctionCancelledToBidderMailInTransaction(auction, def);
        }

        // Deposit FORFEIT to the house on cancel (spec 4.2 / S5): flip the
        // deposit row to TERMINAL_OK ledger-only -- house sink, no money, no mail.
        CustodyService::CommitGoldLedgerOnly("dep:" + std::to_string(capId));

        // (b) Seller item-return. Build the return mail exactly like legacy
        //     (:848-854) and co-commit it. Push the seam RemoveAItem FIRST so it
        //     runs BEFORE the item-mail's disposal closure (RemoveAItem-first,
        //     the corrected lifecycle); DeliverItem appends the mail push (and the
        //     online seller's AddMItem disposal) AFTER. On rollback neither runs
        //     and the item survives in mAitems for re-resolution.
        std::ostringstream msgAuctionCanceledOwner;
        msgAuctionCanceledOwner << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_CANCELED;

        def.effects.push_back([itemGuidLow]()
        {
            sAuctionMgr.RemoveAItem(itemGuidLow);
        });

        MailDraft itemReturn(msgAuctionCanceledOwner.str(), "");
        itemReturn.AddItem(pItem);
        CustodyService::DeliverItem(def, "item:" + std::to_string(capId), itemReturn,
                                    MailReceiver(pl), MailSender(auction));

        // (c) Command-result to the SELLER, deferred LAST (legacy :857 fires it
        //     after the item mail). Scalar-only closure: re-resolve the seller by
        //     GUID, snapshot capId by value (the auction is deleted in this same
        //     deferred run).
        def.effects.push_back([sellerGuidLow, capId]()
        {
            Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, sellerGuidLow));
            if (p)
            {
                p->GetSession()->SendAuctionCommandResultData(capId, AUCTION_REMOVED, AUCTION_OK, EQUIP_ERR_OK, 0);
            }
        });

        // Persist the cut debit + delete the auction row, both IN-TXN.
        pl->SaveInventoryAndGoldToDB();
        auction->DeleteFromDB();

        // Defer the AH-map erase + Eluna OnRemove hook + object delete LAST, in
        // the exact legacy order of the non-custody branch (RemoveAuction
        // out-of-map -> OnRemove -> delete), so OnRemove fires ONLY on a
        // successful checked commit AND sees the same map-state legacy does (the
        // auction already removed from the map, the object not yet deleted).
        // `self`/auctionHouse stay valid until the delete at the end of this
        // closure.
        AuctionEntry* self = auction;
        def.effects.push_back([auctionHouse, capId, self]()
        {
            auctionHouse->RemoveAuction(capId);
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                e->OnRemove(auctionHouse, self);
            }
#endif /* ENABLE_ELUNA */
            delete self;
        });

        CustodyService::MaybeCrash("pre-commit");
        if (CharacterDatabase.CommitTransactionChecked())
        {
            CustodyService::MaybeCrash("pre-deferred");
            def.run();          // ordered live effects (refund notify/mail, item return, command-result, OnRemove, delete)
        }
        else
        {
            // X6: durable rollback -> UNDO the only in-memory mutation (the cut
            // debit). The item + auction survive in memory (no deferred effect
            // ran), so there is nothing else to restore; the auction re-lists on
            // the next observer and the seller can retry the cancel. No item
            // discard -- items survive rollback in mAitems (the corrected
            // lifecycle).
            pl->ModifyMoney(int32(cutDebited));
            SendAuctionCommandResultData(capId, AUCTION_REMOVED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
            sLog.outError("custody S5: cancel txn rolled back for auction %u; cut restored", capId);
        }
    }
    else
    {
        if (auction->bid)                                   // If we have a bid, we have to send him the money he paid
        {
            uint32 auctionCut = auction->GetAuctionCut();
            if (pl->GetMoney() < auctionCut)                // player doesn't have enough money, maybe message needed
            {
                return;
            }

            if (auction->bidder)                            // if auction have real existed bidder send mail
            {
                SendAuctionCancelledToBidderMail(auction);
            }

            pl->ModifyMoney(-int32(auctionCut));
        }
        // Return the item by mail
        std::ostringstream msgAuctionCanceledOwner;
        msgAuctionCanceledOwner << auction->itemTemplate << ":" << auction->itemRandomPropertyId << ":" << AUCTION_CANCELED;

        // item will deleted or added to received mail list
        MailDraft(msgAuctionCanceledOwner.str(),"")
            .AddItem(pItem)
            .SendMailTo(pl, auction, MAIL_CHECK_MASK_COPIED);

        // inform player, that auction is removed
        SendAuctionCommandResult(auction, AUCTION_REMOVED, AUCTION_OK);
        // Now remove the auction
        CharacterDatabase.BeginTransaction();
        auction->DeleteFromDB();
        pl->SaveInventoryAndGoldToDB();
        CharacterDatabase.CommitTransaction();
        sAuctionMgr.RemoveAItem(auction->itemGuidLow);
        auctionHouse->RemoveAuction(auction->Id);

        // Used by Eluna
#ifdef ENABLE_ELUNA
        if (Eluna* e = sWorld.GetEluna())
        {
            e->OnRemove(auctionHouse, auction);
        }
#endif /* ENABLE_ELUNA */
        delete auction;
    }
}

// ===========================================================================
// SP-1 async browse proxy: profile/recipe/house builders + in-process fallback
// ===========================================================================

// Map an AuctionHouseEntry to the BrowseQuery house code (0/1/2). The houseId
// ranges mirror the live faction split (1..3 = Alliance, 4..6 = Horde, else
// Neutral).
static uint8 AhHouseToType(AuctionHouseEntry const* e)
{
    uint32 id = e->houseId;
    if (id >= 1u && id <= 3u)
    {
        return 0u;   // ALLIANCE
    }
    if (id >= 4u && id <= 6u)
    {
        return 1u;   // HORDE
    }
    return 2u;       // NEUTRAL
}

// Snapshot the browsing player's usability inputs into the wire profile.
static void AhBuildBrowseQueryProfile(Player* player, PlayerProfile& out)
{
    out.classId   = player->getClass();
    out.raceId    = player->getRace();
    out.level     = uint8(player->getLevel());
    out.honorRank = player->GetHonorHighestRankInfo().rank;   // direct_action=true uses highest
    // Known spells (open-c: full set; vanilla spellbooks are small).
    PlayerSpellMap const& spells = player->GetSpellMap();
    for (PlayerSpellMap::const_iterator it = spells.begin(); it != spells.end(); ++it)
    {
        if (it->second.state != PLAYERSPELL_REMOVED && !it->second.disabled)
        {
            out.knownSpells.push_back(it->first);
        }
    }
    // Skills (D6): emit (skillId, GetSkillValue) for every known skill line by
    // walking the PLAYER_SKILL_INFO update fields (3 uint32 per slot; the skill
    // id is the low 16 bits of word 0). The worker gates BOTH RequiredSkill AND
    // item proficiency against these.
    for (uint32 i = 0; i < PLAYER_MAX_SKILLS; ++i)
    {
        uint32 w0 = player->GetUInt32Value(PLAYER_SKILL_INFO_1_1 + i * 3);
        uint16 skillId = uint16(w0 & 0x0000FFFFu);
        if (skillId == 0u)
        {
            continue;
        }
        SkillRank sr;
        sr.skillId = skillId;
        sr.rank    = player->GetSkillValue(skillId);
        out.skills.push_back(sr);
    }
    // Reputations: emit (factionId, rank) for each faction the player has state.
    FactionStateList const& reps = player->GetReputationMgr().GetStateList();
    for (FactionStateList::const_iterator it = reps.begin(); it != reps.end(); ++it)
    {
        FactionEntry const* fe = sFactionStore.LookupEntry(it->second.ID);
        if (!fe)
        {
            continue;
        }
        RepStanding rs;
        rs.factionId = uint16(it->second.ID);
        rs.rank = uint8(player->GetReputationMgr().GetRank(fe));
        out.reps.push_back(rs);
    }
}

// C2b/D9: sRecipeCastToTaught maps each recipe item's CAST spell (spellid_1) to
// the spell it TEACHES (EffectTriggerSpell[0]). Built ONCE at world load.
typedef std::map<uint32, uint32> RecipeCastMap;
static RecipeCastMap sRecipeCastToTaught;
static bool          sRecipeCastBuilt = false;

// Build the recipe cast->taught map. Declared in AuctionHouseBot/BrowsePending.h
// (V6). Call from the TOP of AuctionHouseMgr::LoadAuctionItems, BEFORE its
// empty-AH early return (V6), at world load (single-threaded). Idempotent.
void AhEnsureRecipeCastMap()
{
    if (sRecipeCastBuilt)
    {
        return;
    }
    sRecipeCastBuilt = true;
    sRecipeCastToTaught.clear();
    // Scan recipe-class item_template once; resolve spellid_1 -> trigger spell.
    for (uint32 entry = 0; entry < sItemStorage.GetMaxEntry(); ++entry)
    {
        ItemPrototype const* proto = sItemStorage.LookupEntry<ItemPrototype>(entry);
        if (!proto || proto->Class != ITEM_CLASS_RECIPE)
        {
            continue;
        }
        uint32 castSpell = proto->Spells[0].SpellId;
        if (castSpell == 0u)
        {
            continue;
        }
        SpellEntry const* se = sSpellStore.LookupEntry(castSpell);
        if (!se)
        {
            continue;
        }
        uint32 taught = se->EffectTriggerSpell[EFFECT_INDEX_0];
        if (taught == 0u)
        {
            continue;   // no taught spell -> can't ever be "known"; skip
        }
        sRecipeCastToTaught[castSpell] = taught;
    }
}

// The set of recipe CAST spell ids (spellid_1) whose TAUGHT spell the player
// already knows. The worker matches each recipe item's spellid_1 against it.
static void AhBuildKnownRecipeCastSpells(Player* player, std::vector<uint32>& out)
{
    for (RecipeCastMap::const_iterator it = sRecipeCastToTaught.begin();
         it != sRecipeCastToTaught.end(); ++it)
    {
        if (player->HasSpell(it->second))
        {
            out.push_back(it->first);   // the player knows what this recipe teaches
        }
    }
}

// V8: re-resolve the AuctionHouseObject for a stored pending request, mirroring
// GetAuctionsMap by team. pb.house: 0=ALLIANCE,1=HORDE,2=NEUTRAL (== the
// AuctionHouseType values); allHouses => the neutral house. Non-static so the
// world thread's IPC_BROWSE_RESULT reply branch can use it for BIDDER prepends.
AuctionHouseObject* AhResolveHouse(const PendingBrowse& pb)
{
    AuctionHouseType t = AUCTION_HOUSE_NEUTRAL;
    if (pb.allHouses == 0u)
    {
        t = (pb.house == 0u) ? AUCTION_HOUSE_ALLIANCE
          : (pb.house == 1u) ? AUCTION_HOUSE_HORDE
          :                    AUCTION_HOUSE_NEUTRAL;
    }
    return sAuctionMgr.GetAuctionsMap(t);
}

// In-process fallback for a stored pending request (C1). Serves the browse
// exactly as the handler would via the Task-9 BuildListForKind dispatcher.
// Non-static so the world thread (World.cpp) can call it from the reply branch
// and the TTL sweep.
void AhServeBrowseInProcess(WorldSession* session, const PendingBrowse& pb)
{
    Player* player = session->GetPlayer();
    if (!player)
    {
        return;
    }
    AuctionHouseObject* house = AhResolveHouse(pb);
    if (!house)
    {
        return;
    }
    uint16 opcode = (pb.kind == uint8(BROWSE_OWNER)) ? SMSG_AUCTION_OWNER_LIST_RESULT
                  : (pb.kind == uint8(BROWSE_BIDDER)) ? SMSG_AUCTION_BIDDER_LIST_RESULT
                  : SMSG_AUCTION_LIST_RESULT;
    WorldPacket data(opcode, 4 + 4);
    uint32 count = 0, totalcount = 0;
    data << uint32(0);
    house->BuildListForKind(pb.kind, data, player, pb.wname, pb.listfrom,
        pb.levelmin, pb.levelmax, pb.usable, pb.inventoryType, pb.itemClass,
        pb.itemSubClass, pb.quality, pb.clientOutbidIds, count, totalcount);
    data.put<uint32>(0, count);
    data << uint32(totalcount);
    session->SendPacket(&data);
}

// called when player lists his bids
void WorldSession::HandleAuctionListBidderItems(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionListBidderItems");

    ObjectGuid auctioneerGuid;                              // NPC guid
    uint32 listfrom;                                        // page of auctions
    uint32 outbiddedCount;                                  // count of outbidded auctions

    recv_data >> auctioneerGuid;
    recv_data >> listfrom;                                  // not used in fact (this list not have page control in client)
    recv_data >> outbiddedCount;
    if (recv_data.size() != (16 + outbiddedCount * 4))
    {
        sLog.outError("Client sent bad opcode!!! with count: %u and size : %u (must be: %u)", outbiddedCount, (uint32)recv_data.size(), (16 + outbiddedCount * 4));
        outbiddedCount = 0;
    }

    AuctionHouseEntry const* auctionHouseEntry = GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    // always return pointer
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    Player* pl = GetPlayer();

    // Read the client-supplied outbid ids in CLIENT ORDER (same loop as the
    // legacy in-process path, after the size validation above).
    std::vector<uint32> clientOutbidIds;
    while (outbiddedCount > 0)
    {
        --outbiddedCount;
        uint32 outbiddedAuctionId;
        recv_data >> outbiddedAuctionId;
        clientOutbidIds.push_back(outbiddedAuctionId);
    }

    // ----- SP-1 async proxy: hand the browse to the worker when active -----
    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (sv && sv->ServiceActive())
    {
        PendingBrowse pb;
        pb.accountId      = GetAccountId();
        pb.playerGuidLow  = pl->GetGUIDLow();
        pb.kind           = uint8(BROWSE_BIDDER);
        pb.house          = AhHouseToType(auctionHouseEntry);
        pb.allHouses      = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION) ? 1u : 0u;
        pb.itemClass      = 0xFFFFFFFFu;
        pb.itemSubClass   = 0xFFFFFFFFu;
        pb.inventoryType  = 0xFFFFFFFFu;
        pb.quality        = 0xFFFFFFFFu;
        pb.levelmin       = 0u;
        pb.levelmax       = 0u;
        pb.usable         = 0u;
        pb.deferEluna     = 0u;
        pb.listfrom       = listfrom;
        pb.wname.clear();
        pb.clientOutbidIds = clientOutbidIds;
        pb.seq            = sWorld.GetBrowsePending().NextSeqFor(pb.playerGuidLow, pb.kind);

        BrowseQuery q;
        q.queryId          = 0;
        q.kind             = pb.kind;
        q.house            = pb.house;
        q.allHouses        = pb.allHouses;
        q.itemClass        = pb.itemClass;
        q.itemSubClass     = pb.itemSubClass;
        q.inventoryType    = pb.inventoryType;
        q.quality          = pb.quality;
        q.levelmin         = pb.levelmin;
        q.levelmax         = pb.levelmax;
        q.usable           = pb.usable;
        q.deferEluna       = pb.deferEluna;
        q.listfrom         = pb.listfrom;
        {
            int dbIdx = GetSessionDbLocaleIndex();
            LocaleConstant lc = (dbIdx < 0) ? LOCALE_enUS
                                            : sObjectMgr.GetLocaleForIndex(dbIdx);
            q.localeIndex = int8(lc);
        }
        q.requesterGuidLow = pb.playerGuidLow;
        q.minMountLevel    = sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL);
        q.minEpicMountLevel= sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL);
        q.searchedName.clear();
        q.outbidIds        = clientOutbidIds;   // client order, mirrored on the wire
        AhBuildBrowseQueryProfile(pl, q.profile);
        AhBuildKnownRecipeCastSpells(pl, q.knownRecipeCastSpells);

        uint32 nowSec = uint32(time(NULL));
        q.queryId = sWorld.GetBrowsePending().Register(pb, nowSec);
        if (q.queryId == 0u)
        {
            AhServeBrowseInProcess(this, pb);
            return;
        }

        IpcMessage qm; qm.op = IPC_BROWSE_QUERY; q.Encode(qm.body);
        if (qm.body.size() > BrowseQuery::MAX_WIRE)
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhServeBrowseInProcess(this, pb);
            return;
        }
        if (!sv->Channel().SendFrame(qm))
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhServeBrowseInProcess(this, pb);
        }
        return;
    }

    // --- worker down: in-process path (unchanged) ---
    WorldPacket data(SMSG_AUCTION_BIDDER_LIST_RESULT, (4 + 4 + 4));
    data << uint32(0);                                      // add 0 as count
    uint32 count = 0;
    uint32 totalcount = 0;
    for (size_t i = 0; i < clientOutbidIds.size(); ++i)    // add all data, which client requires
    {
        AuctionEntry* auction = auctionHouse->GetAuction(clientOutbidIds[i]);
        if (auction && auction->BuildAuctionInfo(data))
        {
            ++totalcount;
            ++count;
        }
    }

    auctionHouse->BuildListBidderItems(data, pl, count, totalcount);
    data.put<uint32>(0, count);                             // add count to placeholder
    data << uint32(totalcount);
    SendPacket(&data);
}

// this void sends player info about his auctions
void WorldSession::HandleAuctionListOwnerItems(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionListOwnerItems");

    ObjectGuid auctioneerGuid;
    uint32 listfrom;

    recv_data >> auctioneerGuid;
    recv_data >> listfrom;                                  // not used in fact (this list not have page control in client)

    AuctionHouseEntry const* auctionHouseEntry = GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    // always return pointer
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // ----- SP-1 async proxy: hand the browse to the worker when active -----
    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (sv && sv->ServiceActive())
    {
        PendingBrowse pb;
        pb.accountId     = GetAccountId();
        pb.playerGuidLow = GetPlayer()->GetGUIDLow();
        pb.kind          = uint8(BROWSE_OWNER);
        pb.house         = AhHouseToType(auctionHouseEntry);
        pb.allHouses     = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION) ? 1u : 0u;
        pb.itemClass     = 0xFFFFFFFFu;
        pb.itemSubClass  = 0xFFFFFFFFu;
        pb.inventoryType = 0xFFFFFFFFu;
        pb.quality       = 0xFFFFFFFFu;
        pb.levelmin      = 0u;
        pb.levelmax      = 0u;
        pb.usable        = 0u;
        pb.deferEluna    = 0u;
        pb.listfrom      = listfrom;
        pb.wname.clear();
        pb.seq           = sWorld.GetBrowsePending().NextSeqFor(pb.playerGuidLow, pb.kind);

        BrowseQuery q;
        q.queryId          = 0;
        q.kind             = pb.kind;
        q.house            = pb.house;
        q.allHouses        = pb.allHouses;
        q.itemClass        = pb.itemClass;
        q.itemSubClass     = pb.itemSubClass;
        q.inventoryType    = pb.inventoryType;
        q.quality          = pb.quality;
        q.levelmin         = pb.levelmin;
        q.levelmax         = pb.levelmax;
        q.usable           = pb.usable;
        q.deferEluna       = pb.deferEluna;
        q.listfrom         = pb.listfrom;
        {
            int dbIdx = GetSessionDbLocaleIndex();
            LocaleConstant lc = (dbIdx < 0) ? LOCALE_enUS
                                            : sObjectMgr.GetLocaleForIndex(dbIdx);
            q.localeIndex = int8(lc);
        }
        q.requesterGuidLow = pb.playerGuidLow;
        q.minMountLevel    = sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL);
        q.minEpicMountLevel= sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL);
        q.searchedName.clear();
        AhBuildBrowseQueryProfile(GetPlayer(), q.profile);
        AhBuildKnownRecipeCastSpells(GetPlayer(), q.knownRecipeCastSpells);

        uint32 nowSec = uint32(time(NULL));
        q.queryId = sWorld.GetBrowsePending().Register(pb, nowSec);
        if (q.queryId == 0u)
        {
            AhServeBrowseInProcess(this, pb);
            return;
        }

        IpcMessage qm; qm.op = IPC_BROWSE_QUERY; q.Encode(qm.body);
        if (qm.body.size() > BrowseQuery::MAX_WIRE)
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhServeBrowseInProcess(this, pb);
            return;
        }
        if (!sv->Channel().SendFrame(qm))
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhServeBrowseInProcess(this, pb);
        }
        return;
    }

    // --- worker down: in-process path (unchanged) ---
    WorldPacket data(SMSG_AUCTION_OWNER_LIST_RESULT, (4 + 4));
    data << (uint32) 0;                                     // amount place holder

    uint32 count = 0;
    uint32 totalcount = 0;

    auctionHouse->BuildListOwnerItems(data, _player, count, totalcount);
    data.put<uint32>(0, count);
    data << uint32(totalcount);
    SendPacket(&data);
}

// this void is called when player clicks on search button
void WorldSession::HandleAuctionListItems(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: HandleAuctionListItems");

    ObjectGuid auctioneerGuid;
    std::string searchedname;
    uint8 levelmin, levelmax, usable;
    uint32 listfrom, auctionSlotID, auctionMainCategory, auctionSubCategory, quality;

    recv_data >> auctioneerGuid;
    recv_data >> listfrom;                                  // start, used for page control listing by 50 elements
    recv_data >> searchedname;

    recv_data >> levelmin >> levelmax;
    recv_data >> auctionSlotID >> auctionMainCategory >> auctionSubCategory >> quality;
    recv_data >> usable;

    AuctionHouseEntry const* auctionHouseEntry = GetCheckedAuctionHouseForAuctioneer(auctioneerGuid);
    if (!auctionHouseEntry)
    {
        return;
    }

    // always return pointer
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // DEBUG_LOG("Auctionhouse search %s list from: %u, searchedname: %s, levelmin: %u, levelmax: %u, auctionSlotID: %u, auctionMainCategory: %u, auctionSubCategory: %u, quality: %u, usable: %u",
    //  auctioneerGuid.GetString().c_str(), listfrom, searchedname.c_str(), levelmin, levelmax, auctionSlotID, auctionMainCategory, auctionSubCategory, quality, usable);

    // ----- SP-1 async proxy: hand the browse to the worker when active -----
    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (sv && sv->ServiceActive())
    {
        std::wstring wsearchedname;
        if (!Utf8toWStr(searchedname, wsearchedname))
        {
            return;
        }
        wstrToLower(wsearchedname);
        std::string nameLower;
        WStrToUtf8(wsearchedname, nameLower);

        PendingBrowse pb;
        pb.accountId     = GetAccountId();
        pb.playerGuidLow = GetPlayer()->GetGUIDLow();
        pb.kind          = uint8(BROWSE_LIST);
        pb.house         = AhHouseToType(auctionHouseEntry);
        pb.allHouses     = sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION) ? 1u : 0u;
        pb.itemClass     = auctionMainCategory;
        pb.itemSubClass  = auctionSubCategory;
        pb.inventoryType = auctionSlotID;
        pb.quality       = quality;
        pb.levelmin      = levelmin;
        pb.levelmax      = levelmax;
        pb.usable        = usable;
        pb.deferEluna    = 0u;
#ifdef ENABLE_ELUNA
        if (usable != 0u)
        {
            pb.deferEluna = 1u;   // defer OnCanUseItem to the reply handler
        }
#endif
        pb.listfrom      = listfrom;
        pb.wname         = wsearchedname;
        pb.seq           = sWorld.GetBrowsePending().NextSeqFor(pb.playerGuidLow, pb.kind);

        BrowseQuery q;
        q.queryId          = 0;
        q.kind             = pb.kind;
        q.house            = pb.house;
        q.allHouses        = pb.allHouses;
        q.itemClass        = pb.itemClass;
        q.itemSubClass     = pb.itemSubClass;
        q.inventoryType    = pb.inventoryType;
        q.quality          = pb.quality;
        q.levelmin         = pb.levelmin;
        q.levelmax         = pb.levelmax;
        q.usable           = pb.usable;
        q.deferEluna       = pb.deferEluna;
        q.listfrom         = pb.listfrom;
        // V3: send the LocaleConstant (NOT the internal session index).
        {
            int dbIdx = GetSessionDbLocaleIndex();   // -1 for enUS / default
            LocaleConstant lc = (dbIdx < 0) ? LOCALE_enUS
                                            : sObjectMgr.GetLocaleForIndex(dbIdx);
            q.localeIndex = int8(lc);                // LOCALE_enUS(0) => no overlay
        }
        q.requesterGuidLow = pb.playerGuidLow;
        q.minMountLevel    = sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL);
        q.minEpicMountLevel= sWorld.getConfig(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL);
        q.searchedName     = nameLower;
        AhBuildBrowseQueryProfile(GetPlayer(), q.profile);
        AhBuildKnownRecipeCastSpells(GetPlayer(), q.knownRecipeCastSpells);

        uint32 nowSec = uint32(time(NULL));
        q.queryId = sWorld.GetBrowsePending().Register(pb, nowSec);
        if (q.queryId == 0u)
        {
            // D3: pending-map at MAX_PENDING (browse flood). Serve in-process now.
            AhServeBrowseInProcess(this, pb);
            return;
        }

        IpcMessage qm; qm.op = IPC_BROWSE_QUERY; q.Encode(qm.body);
        // open-c: oversize profile fallback. If encoding exceeds the cap, drop to
        // in-process immediately (rare; a huge spellbook/rep set).
        if (qm.body.size() > BrowseQuery::MAX_WIRE)
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhServeBrowseInProcess(this, pb);
            return;
        }
        // C1: if the send itself fails, fall back in-process now and drop pending.
        if (!sv->Channel().SendFrame(qm))
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhServeBrowseInProcess(this, pb);
        }
        return;   // async (or fell back synchronously above)
    }

    // --- worker down: in-process path (unchanged) ---
    WorldPacket data(SMSG_AUCTION_LIST_RESULT, (4 + 4));
    uint32 count = 0;
    uint32 totalcount = 0;
    data << uint32(0);

    // converting string that we try to find to lower case
    std::wstring wsearchedname;
    if (!Utf8toWStr(searchedname, wsearchedname))
    {
        return;
    }

    wstrToLower(wsearchedname);

    auctionHouse->BuildListAuctionItems(data, _player,
        wsearchedname, listfrom, levelmin, levelmax, usable,
        auctionSlotID, auctionMainCategory, auctionSubCategory, quality,
        count, totalcount);

    data.put<uint32>(0, count);
    data << uint32(totalcount);
    SendPacket(&data);
}

/** @} */
