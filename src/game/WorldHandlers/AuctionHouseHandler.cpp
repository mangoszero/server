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
#include "AuctionHouseBot/MutationPending.h"
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
#include "PlayerMutations.h"
#include "WorkerSupervisor.h"
#include <string>
#include <vector>
#include <map>
#include <list>
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

// SP-1 coordinator: the player-facing "AH temporarily unavailable" signal -- a
// transient center-screen flash (SMSG_NOTIFICATION) PLUS a persistent red system
// chat line. Used wherever a worker is the AH authority but cannot serve: the
// window-open gate (SendAuctionHello) and the three browse read paths
// (AhSendBrowseUnavailable, also from the World.cpp reply branch + TTL sweep).
static void AhSendUnavailableMessage(WorldSession* session)
{
    session->SendNotification("The Auction House is temporarily unavailable.");
    ChatHandler(session).SendSysMessage("|cffff0000The Auction House is temporarily unavailable.|r");
}

// SP-2 Task 10: bid-forward classifier. See MutationPending.h for the full
// contract. Kept as a free function so `mangosd -t ahforwardreserve` can
// exercise the real classification logic against seeded ledger rows without a
// live Player or worker.
uint16 AhClassifyBidForward(uint32 auctionId, uint32 bidderGuidLow, uint32 price,
                            uint32& reserveAmount)
{
    reserveAmount = price;

    CustodyRow liveRow;
    if (!CustodyLedger::GetSingleLiveBidRow(auctionId, liveRow) ||
        liveRow.ownerGuid != bidderGuidLow)
    {
        // Not provably this player's live bid (no row, ambiguous rows, or
        // another player's row): reserve the full submitted price as maxPrice
        // and let the worker adjudicate against the book (spec I6 / 4.1).
        return uint16(IPC_PLAYER_BUYOUT);
    }

    if (price <= liveRow.amount)
    {
        // Raising to at-or-below the player's own live bid: legacy rejects
        // inline (`price <= auction->bid` -> AUCTION_ERR_HIGHER_BID).
        return 0u;
    }

    // Same-bidder top-up (spec I9): only the delta is reserved; the finalize
    // applies the top-up. Route it as IPC_PLAYER_BUYOUT instead of BID because
    // mangosd does not own the book's buyout price under WriteAuthority. The
    // worker treats a below-buyout 0x42 as a normal bid at maxPrice and an
    // at/over-buyout 0x42 as the win, so the same delta reserve covers both
    // raise and current-high-bidder-buyout without an out-of-band book read.
    reserveAmount = price - liveRow.amount;
    return uint16(IPC_PLAYER_BUYOUT);
}

bool AhBuildCancelPrepareForward(MutationPendingMap& pending, uint32 playerGuidLow,
                                 uint32 auctionId, uint64 uuid, uint32 sentSec,
                                 IpcMessage& out)
{
    if (!pending.CanRegister(playerGuidLow))
    {
        return false;
    }

    PendingMutation pm;
    pm.uuid           = uuid;
    pm.playerGuidLow  = playerGuidLow;
    pm.op             = uint16(IPC_PLAYER_CANCEL);
    pm.auctionId      = auctionId;
    pm.state          = uint8(PMUT_AWAIT_RESULT);
    pm.sentSec        = sentSec;
    pm.reservedAmount = 0u;
    pm.reserveKey.clear();
    pm.itemKey.clear();
    pm.depKey.clear();
    pending.Register(pm);

    PlayerCancelPrepare prep;
    prep.uuid       = uuid;
    prep.auctionId  = auctionId;
    prep.sellerGuid = playerGuidLow;

    out = IpcMessage();
    out.op = IPC_PLAYER_CANCEL;
    prep.Encode(out.body);
    return true;
}

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
    // SP-1 coordinator: if the worker is the CONFIGURED AH authority but not
    // currently active (never started, or down), do NOT open the auction window
    // at all -- the AH is unavailable; tell the player (center flash + chat) and
    // return. The authority test is IsAhServiceConfigured(), NOT the supervisor
    // pointer, so a configured-but-failed-start (sv == NULL) is still caught here
    // rather than silently opening the window. When the worker is not configured,
    // open normally (legacy single-process). This is the single chokepoint for
    // every open path (auctioneer click, gossip option, .auction GM commands).
    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (sWorld.IsAhServiceConfigured() && !(sv && sv->ServiceActive()))
    {
        AhSendUnavailableMessage(this);
        return;
    }

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

    // ----- SP-2 write-authority forward path: reserve -> forward (spec 4.1) -----
    // Placed BEFORE the 50-owned-auctions loop: that guard walks the local
    // AuctionsMap, which is dead under WriteAuthority -- the worker owns the
    // book and enforces the 50-cap itself (spec I6). Eluna OnAdd fires at
    // finalize time from the worker facts (spec 5.8, Task 11), not here.
    if (sWorld.IsAhWriteAuthority())
    {
        // No in-process fallback of any kind (spec 5.5). Worker configured but
        // down => the existing unavailable path (center flash + red chat line)
        // plus an ERR_DATABASE command result so the client UI unlocks
        // (the same result class as the M2 in-doubt tombstone).
        WorkerSupervisor* sv = sWorld.GetAhSupervisor();
        if (!sv || !sv->ServiceActive())
        {
            AhSendUnavailableMessage(this);
            SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_DATABASE);
            return;
        }

        // Cap check BEFORE any reserve (spec 5.5).
        if (!sWorld.GetMutationPending().CanRegister(pl->GetGUIDLow()))
        {
            SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_DATABASE);
            return;
        }

        // mangosd is the sole auction-ID allocator (spec decision 8): minted
        // here and carried in the intent, so every custody idem key exists at
        // reserve time.
        uint32 const auctionId     = sObjectMgr.GenerateAuctionID();
        uint64 const uuid          = AhMintMutationUuid();
        std::string const aucIdStr = std::to_string(auctionId);
        uint32 const itemGuidLow   = itemGuid.GetCounter();

        // In-memory escrow exactly as the custody AddAuction path does (deposit
        // debit at the legacy :605 position, then mAitems + out of the bags);
        // the durable writes co-commit below. mAitems stays in mangosd (I7).
        pl->ModifyMoney(-int32(deposit));
        sAuctionMgr.AddAItem(it);
        pl->MoveItemFromInventory(it->GetBagSlot(), it->GetSlot(), true);

        // ONE checked txn (spec decision 9): item ownership move + escrow row +
        // deposit reserve row. NO `auction` row -- the worker is the sole book
        // writer and inserts it at commit (spec 4.1 step 3).
        CharacterDatabase.BeginTransaction();
        it->DeleteFromInventoryDB();
        it->SaveToDB();
        pl->SaveInventoryAndGoldToDB();
        CustodyService::EscrowItem(pl->GetGUIDLow(), itemGuidLow,
            "item:" + aucIdStr, auctionId);
        CustodyService::ReserveGoldAlreadyDebited(pl->GetGUIDLow(), deposit,
            "dep:" + aucIdStr, auctionId, ROLE_DEPOSIT);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            // Durable rollback -> restore live state (mirrors the custody S1
            // rollback below, minus the phantom AuctionEntry: none exists here).
            sAuctionMgr.RemoveAItem(itemGuidLow);
            ItemPosCountVec dest;
            if (pl->CanStoreItem(NULL_BAG, NULL_SLOT, dest, it, false) == EQUIP_ERR_OK)
            {
                pl->MoveItemToInventory(dest, it, true, true);
            }
            else
            {
                sLog.outError("SP-2 sell: seller %u could not re-store item %u on reserve rollback of "
                              "auction %u; item remains durably the seller's on disk (reloads on relog)",
                              pl->GetGUIDLow(), itemGuidLow, auctionId);
            }
            pl->ModifyMoney(int32(deposit));
            SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_DATABASE);
            sLog.outError("SP-2 sell: reserve txn rolled back for auction %u; live state restored", auctionId);
            return;
        }

        // [SP-2 Task 15] crash seam: the escrow+deposit reserve is durably
        // committed but the intent has NOT been forwarded. Reconcile (Task 12)
        // must release the reserved-but-un-forwarded row. Inert on a live realm.
        CustodyService::MaybeCrash("post-reserve-pre-forward");

        // Register BEFORE the send so a fast reply can never race an
        // unregistered uuid.
        PendingMutation pm;
        pm.uuid           = uuid;
        pm.playerGuidLow  = pl->GetGUIDLow();
        pm.op             = uint16(IPC_PLAYER_SELL);
        pm.auctionId      = auctionId;
        pm.state          = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec        = uint32(time(NULL));
        pm.reservedAmount = 0u;                 // deposit rides depKey, no bid reserve
        pm.reserveKey.clear();
        pm.itemKey        = "item:" + aucIdStr;
        pm.depKey         = "dep:" + aucIdStr;
        sWorld.GetMutationPending().Register(pm);

        PlayerSellIntent psi;
        psi.uuid             = uuid;
        psi.auctionId        = auctionId;
        psi.sellerGuid       = pl->GetGUIDLow();
        psi.house            = uint8(auctionHouseEntry->houseId);
        psi.itemGuid         = itemGuidLow;
        psi.itemTemplate     = it->GetEntry();
        psi.itemCount        = it->GetCount();
        psi.randomPropertyId = it->GetItemRandomPropertyId();
        psi.startbid         = bid;
        psi.buyout           = buyout;
        psi.deposit          = deposit;         // mangosd-computed, worker persists verbatim (spec 4.1)
        psi.expireTime       = uint32(time(NULL) + uint32(etime * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_TIME)));

        IpcMessage m;
        m.op = IPC_PLAYER_SELL;
        psi.Encode(m.body);
        if (!sv->Channel().SendFrame(m))
        {
            // The reserve is durable and is NEVER unilaterally rolled back
            // (decision 10): tombstone as in-doubt; reconcile-on-reconnect
            // resolves it against the worker journal (spec 8).
            sWorld.GetMutationPending().Tombstone(uuid);
            SendAuctionCommandResult(NULL, AUCTION_STARTED, AUCTION_ERR_DATABASE);
            sLog.outError("SP-2 sell: forward send failed for auction %u uuid " UI64FMTD "; reservation in doubt",
                          auctionId, uuid);
        }
        return;
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

    // ----- SP-2 write-authority forward path: reserve -> forward (spec 4.1) -----
    // Placed BEFORE the local-map GetAuction: mangosd's AuctionsMap is dead
    // under WriteAuthority (spec 5.7). Existence/state/min-increment/bid-own/
    // same-account guards move to the worker (spec I6); mangosd validates only
    // what it owns -- session, gold, and its own ledger (spec 4.1 step 1).
    if (sWorld.IsAhWriteAuthority())
    {
        Player* pl = GetPlayer();

        WorkerSupervisor* sv = sWorld.GetAhSupervisor();
        if (!sv || !sv->ServiceActive())
        {
            AhSendUnavailableMessage(this);
            SendAuctionCommandResult(NULL, AUCTION_BID_PLACED, AUCTION_ERR_DATABASE);
            return;
        }

        // Cap check BEFORE any reserve (spec 5.5).
        if (!sWorld.GetMutationPending().CanRegister(pl->GetGUIDLow()))
        {
            SendAuctionCommandResult(NULL, AUCTION_BID_PLACED, AUCTION_ERR_DATABASE);
            return;
        }

        // Full-price affordability guard at legacy parity (silent return, no
        // result) -- legacy gates the FULL price even for a same-bidder raise,
        // so a raise legacy accepts is never rejected here (spec I9).
        if (price > pl->GetMoney())
        {
            return;
        }

        // Classify against mangosd's OWN ledger: same-bidder top-up ->
        // IPC_PLAYER_BUYOUT with a delta reserve; everything else ->
        // IPC_PLAYER_BUYOUT with the full price reserved as maxPrice (worker:
        // effectiveBid = min(maxPrice, buyout), spec 4.1); 0 -> inline legacy
        // ERR_HIGHER_BID.
        uint32 reserveAmount = 0u;
        uint16 const fwdOp = AhClassifyBidForward(auctionId, pl->GetGUIDLow(), price, reserveAmount);
        if (fwdOp == 0u)
        {
            SendAuctionCommandResultData(auctionId, AUCTION_BID_PLACED, AUCTION_ERR_HIGHER_BID, EQUIP_ERR_OK, 0);
            return;
        }

        uint64 const uuid = AhMintMutationUuid();
        std::string const reserveKey = "bid:" + std::to_string(auctionId) + ":" +
                                       std::to_string(CustodyLedger::NextBidSeq(auctionId));

        // Durable reserve in ONE checked txn (spec decision 9). ReserveGold
        // debits the wallet + saves gold inline (legacy debit timing) and
        // appends the CST_RESERVED row to this open transaction.
        CustodyDeferred def;
        CharacterDatabase.BeginTransaction();
        CustodyService::ReserveGold(def, pl->GetGUIDLow(), pl, reserveAmount, reserveKey, auctionId, ROLE_BID);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            // DB rolled back -> restore ReserveGold's in-memory debit.
            pl->ModifyMoney(int32(reserveAmount));
            SendAuctionCommandResultData(auctionId, AUCTION_BID_PLACED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
            sLog.outError("SP-2 bid: reserve txn rolled back for auction %u", auctionId);
            return;
        }

        // [SP-2 Task 15] crash seam: the bid reserve is durably committed but the
        // intent has NOT been forwarded. Reconcile (Task 12) must release the
        // reserved-but-un-forwarded row. Inert on a live realm.
        CustodyService::MaybeCrash("post-reserve-pre-forward");

        // Register BEFORE the send (reply can never race an unknown uuid).
        PendingMutation pm;
        pm.uuid           = uuid;
        pm.playerGuidLow  = pl->GetGUIDLow();
        pm.op             = fwdOp;
        pm.auctionId      = auctionId;
        pm.state          = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec        = uint32(time(NULL));
        pm.reservedAmount = reserveAmount;
        pm.reserveKey     = reserveKey;
        pm.itemKey.clear();
        pm.depKey.clear();
        sWorld.GetMutationPending().Register(pm);

        IpcMessage m;
        if (fwdOp == uint16(IPC_PLAYER_BID))
        {
            PlayerBidIntent pbi;
            pbi.uuid       = uuid;
            pbi.auctionId  = auctionId;
            pbi.bidderGuid = pl->GetGUIDLow();
            pbi.bidAmount  = price;    // full new price; the finalize tops up by the delta
            m.op = IPC_PLAYER_BID;
            pbi.Encode(m.body);
        }
        else
        {
            PlayerBuyoutIntent pbo;
            pbo.uuid       = uuid;
            pbo.auctionId  = auctionId;
            pbo.bidderGuid = pl->GetGUIDLow();
            pbo.maxPrice   = price;    // worker: effectiveBid = min(maxPrice, buyout) (spec 4.1)
            m.op = IPC_PLAYER_BUYOUT;
            pbo.Encode(m.body);
        }

        if (!sv->Channel().SendFrame(m))
        {
            // Never roll a durable reserve back unilaterally (decision 10):
            // tombstone as in-doubt; reconcile resolves it via the journal.
            sWorld.GetMutationPending().Tombstone(uuid);
            SendAuctionCommandResultData(auctionId, AUCTION_BID_PLACED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
            sLog.outError("SP-2 bid: forward send failed for auction %u uuid " UI64FMTD "; reservation in doubt",
                          auctionId, uuid);
        }
        return;
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

    // ----- SP-2 write-authority cancel path: prepare -> worker -> phase 2 -----
    // Placed BEFORE the local-map GetAuction: under WriteAuthority the worker
    // owns the book, so mangosd's legacy AuctionsMap lookup is empty/stale.
    // Ownership/existence checks move to the worker; mangosd owns only the
    // pending slot, later cut reserve, and value finalize.
    if (sWorld.IsAhWriteAuthority())
    {
        Player* pl = GetPlayer();

        WorkerSupervisor* sv = sWorld.GetAhSupervisor();
        if (!sv || !sv->ServiceActive())
        {
            AhSendUnavailableMessage(this);
            SendAuctionCommandResultData(auctionId, AUCTION_REMOVED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
            return;
        }

        uint64 const uuid = AhMintMutationUuid();
        IpcMessage m;
        if (!AhBuildCancelPrepareForward(sWorld.GetMutationPending(), pl->GetGUIDLow(),
                                         auctionId, uuid, uint32(time(NULL)), m))
        {
            SendAuctionCommandResultData(auctionId, AUCTION_REMOVED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
            return;
        }

        if (!sv->Channel().SendFrame(m))
        {
            // No unilateral rollback: the frame may have reached the worker.
            // Reconcile-on-reconnect resolves by the shared worker journal.
            sWorld.GetMutationPending().Tombstone(uuid);
            SendAuctionCommandResultData(auctionId, AUCTION_REMOVED, AUCTION_ERR_DATABASE, EQUIP_ERR_OK, 0);
            sLog.outError("SP-2 cancel: forward send failed for auction %u uuid " UI64FMTD
                          "; cancellation in doubt", auctionId, uuid);
        }
        return;
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
                                    MailReceiver(pl), MailSender(auction),
                                    MAIL_CHECK_MASK_COPIED);

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

// Coordinator model: the worker is the AH read authority. When it cannot serve
// (down, a transient register/encode/send failure, queue-full, oversize, or a
// reply timeout) mangosd serves NOTHING in-process. It sends an empty list so
// the client does not hang, plus the unavailable message (center flash + red
// chat line via AhSendUnavailableMessage). Non-static: the world thread calls it
// from the IPC_BROWSE_RESULT reply branch and the TTL sweep. (kind is a
// BrowseKind; only the opcode depends on it.)
void AhSendBrowseUnavailable(WorldSession* session, uint8 kind)
{
    uint16 opcode = (kind == uint8(BROWSE_OWNER)) ? SMSG_AUCTION_OWNER_LIST_RESULT
                  : (kind == uint8(BROWSE_BIDDER)) ? SMSG_AUCTION_BIDDER_LIST_RESULT
                  : SMSG_AUCTION_LIST_RESULT;
    WorldPacket data(opcode, 4 + 4);
    data << uint32(0);   // count
    data << uint32(0);   // totalcount
    session->SendPacket(&data);
    AhSendUnavailableMessage(session);
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
            AhSendBrowseUnavailable(this, pb.kind);
            return;
        }

        IpcMessage qm; qm.op = IPC_BROWSE_QUERY; q.Encode(qm.body);
        if (qm.body.size() > BrowseQuery::MAX_WIRE)
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhSendBrowseUnavailable(this, pb.kind);
            return;
        }
        if (!sv->Channel().SendFrame(qm))
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhSendBrowseUnavailable(this, pb.kind);
        }
        return;
    }

    // Worker configured but DOWN (or failed to start) -> coordinator: no
    // in-process serving. Key off IsAhServiceConfigured(), not the pointer, so a
    // failed Start() (sv == NULL) still returns "unavailable" here.
    if (sWorld.IsAhServiceConfigured())
    {
        AhSendBrowseUnavailable(this, uint8(BROWSE_BIDDER));
        return;
    }

    // --- no worker configured (legacy single-process): in-process path ---
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
            AhSendBrowseUnavailable(this, pb.kind);
            return;
        }

        IpcMessage qm; qm.op = IPC_BROWSE_QUERY; q.Encode(qm.body);
        if (qm.body.size() > BrowseQuery::MAX_WIRE)
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhSendBrowseUnavailable(this, pb.kind);
            return;
        }
        if (!sv->Channel().SendFrame(qm))
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhSendBrowseUnavailable(this, pb.kind);
        }
        return;
    }

    // Worker configured but DOWN (or failed to start) -> coordinator: no
    // in-process serving. Key off IsAhServiceConfigured(), not the pointer, so a
    // failed Start() (sv == NULL) still returns "unavailable" here.
    if (sWorld.IsAhServiceConfigured())
    {
        AhSendBrowseUnavailable(this, uint8(BROWSE_OWNER));
        return;
    }

    // --- no worker configured (legacy single-process): in-process path ---
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
            // F5: malformed UTF-8 in the search name. The worker is the authority
            // and this request never leaves -- send the terminal "unavailable"
            // response (mirroring the sibling early-rejects below) so the client's
            // browse panel does not hang waiting for a reply that never comes.
            AhSendBrowseUnavailable(this, uint8(BROWSE_LIST));
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
            AhSendBrowseUnavailable(this, pb.kind);
            return;
        }

        IpcMessage qm; qm.op = IPC_BROWSE_QUERY; q.Encode(qm.body);
        // open-c: oversize profile fallback. If encoding exceeds the cap, drop to
        // in-process immediately (rare; a huge spellbook/rep set).
        if (qm.body.size() > BrowseQuery::MAX_WIRE)
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhSendBrowseUnavailable(this, pb.kind);
            return;
        }
        // C1: if the send itself fails, fall back in-process now and drop pending.
        if (!sv->Channel().SendFrame(qm))
        {
            PendingBrowse tmp; (void)sWorld.GetBrowsePending().Take(q.queryId, tmp);
            AhSendBrowseUnavailable(this, pb.kind);
        }
        return;   // async (or fell back synchronously above)
    }

    // Worker configured but DOWN (or failed to start) -> coordinator: no
    // in-process serving. Key off IsAhServiceConfigured(), not the pointer, so a
    // failed Start() (sv == NULL) still returns "unavailable" here.
    if (sWorld.IsAhServiceConfigured())
    {
        AhSendBrowseUnavailable(this, uint8(BROWSE_LIST));
        return;
    }

    // --- no worker configured (legacy single-process): in-process path ---
    WorldPacket data(SMSG_AUCTION_LIST_RESULT, (4 + 4));
    uint32 count = 0;
    uint32 totalcount = 0;
    data << uint32(0);

    // converting string that we try to find to lower case
    std::wstring wsearchedname;
    if (!Utf8toWStr(searchedname, wsearchedname))
    {
        // F5: malformed UTF-8 -> send a well-formed empty list result (the AH is
        // available on this legacy path) rather than a bare return that leaves the
        // client's browse panel hung. count/totalcount are still 0 here.
        data.put<uint32>(0, count);
        data << uint32(totalcount);
        SendPacket(&data);
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

// ===========================================================================
// SP-2 write-authority: player-mutation finalize + cancel phase-2 driver.
// The worker is the book authority; mangosd applies VALUE ONLY, from the
// wire facts, cross-checked fail-closed against its own custody ledger
// (spec section 3). No AuctionEntry / AuctionsMap access on any path here.
// ===========================================================================

// [SP-2] outbid increment from a bid value (mirrors AuctionEntry::GetAuctionOutBid,
// AuctionHouseMgr.cpp GetAuctionOutBid).
static uint32 AhOutbidFor(uint32 bid)
{
    uint32 outbid = (bid / 100) * 5;
    if (!outbid)
    {
        outbid = 1;
    }
    return outbid;
}

// [SP-2] auction-house cut from wire facts (mirrors AuctionEntry::GetAuctionCut,
// keyed by houseId -- no AuctionEntry exists on the finalize path). NULL house
// entry -> 0 (also the -t harness case, where DBC data is not loaded).
static uint32 AhCutFor(uint8 houseId, uint32 bid)
{
    AuctionHouseEntry const* he = sAuctionHouseStore.LookupEntry(uint32(houseId));
    if (!he)
    {
        return 0;
    }
    return uint32(he->cutPercent * bid * sWorld.getConfig(CONFIG_FLOAT_RATE_AUCTION_CUT) / 100.0f);
}

// [SP-2] PlayerMutationResult.op (low byte of the IpcOpcode) -> AuctionAction.
static AuctionAction AhActionForOp(uint8 op)
{
    switch (op)
    {
        case uint8(IPC_PLAYER_SELL & 0xFFu):
            return AUCTION_STARTED;
        case uint8(IPC_PLAYER_CANCEL & 0xFFu):
        case uint8(IPC_PLAYER_CANCEL_CONFIRM & 0xFFu):
            return AUCTION_REMOVED;
        default:
            return AUCTION_BID_PLACED;   // IPC_PLAYER_BID / IPC_PLAYER_BUYOUT
    }
}

// [SP-2] GUID-re-resolve SMSG emitter (the SP-1 reply-branch pattern): never
// hold a WorldSession* across ticks; re-resolve by low GUID at send time and
// skip silently when offline (the durable mail rows are authoritative).
static void AhSendCommandResultTo(uint32 playerGuidLow, uint32 aucId,
                                  AuctionAction action, AuctionError err,
                                  uint32 newOutbid)
{
    Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, playerGuidLow));
    if (p && p->GetSession())
    {
        p->GetSession()->SendAuctionCommandResultData(aucId, action, err, EQUIP_ERR_OK, newOutbid);
    }
}

// [SP-2] AUCTION_ERR_HIGHER_BID carries the current bidder/bid (the inline
// branch of SendAuctionCommandResult); rebuilt from wire facts.
static void AhSendHigherBidResultTo(uint32 playerGuidLow, uint8 op, MutationFacts const& f)
{
    Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, playerGuidLow));
    if (!p || !p->GetSession())
    {
        return;
    }
    WorldPacket data(SMSG_AUCTION_COMMAND_RESULT, 16);
    data << uint32(f.auctionId);
    data << uint32(AhActionForOp(op));
    data << uint32(AUCTION_ERR_HIGHER_BID);
    data << ObjectGuid(HIGHGUID_PLAYER, f.curBidderGuid);
    data << uint32(f.curBid);
    data << uint32(AhOutbidFor(f.curBid));
    p->GetSession()->SendPacket(&data);
}

// [SP-2] "<itemTemplate>:<rand>:<response>" mail subject (the legacy pattern
// used by every AH mail).
static std::string AhMailSubject(uint32 itemTemplate, int32 itemRand, uint32 response)
{
    std::ostringstream s;
    s << itemTemplate << ":" << itemRand << ":" << response;
    return s.str();
}

// [SP-2] every live (CST_RESERVED CUSTODY_GOLD ROLE_BID) row for an auction.
// LoadNonTerminal + filter: no new CustodyLedger read API; the non-terminal
// set is TTL-bounded.
static void AhLoadLiveBidRows(uint32 auctionId, std::vector<CustodyRow>& out)
{
    std::vector<CustodyRow> rows;
    CustodyLedger::LoadNonTerminal(rows);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i].auctionId == auctionId && rows[i].kind == CUSTODY_GOLD &&
            rows[i].role == ROLE_BID && rows[i].state == CST_RESERVED)
        {
            out.push_back(rows[i]);
        }
    }
}

// [SP-2] the prior/current bidder's live bid row: EXACTLY ONE live bid row for
// the auction that is not `excludeKey` and matches the reported owner+amount.
// Any other shape returns false -> fail closed (spec section 3 matrix).
static bool AhFindPriorBidRow(uint32 auctionId, uint32 bidderGuid, uint32 amount,
                              std::string const& excludeKey, CustodyRow& out)
{
    std::vector<CustodyRow> rows;
    AhLoadLiveBidRows(auctionId, rows);
    bool found = false;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (!excludeKey.empty() && rows[i].idemKey == excludeKey)
        {
            continue;
        }
        if (found)
        {
            return false;   // ambiguous -> fail closed
        }
        out = rows[i];
        found = true;
    }
    if (!found)
    {
        return false;
    }
    return out.ownerGuid == bidderGuid && out.amount == amount;
}

// [SP-2] in-txn wallet re-credit WITHOUT a ledger-row flip (the buyout
// remainder: the row shrinks via SetAmount and then commits). Mirrors
// ReleaseGoldToWallet's wallet leg. The caller MUST undo the online in-memory
// credit if its checked commit fails (forward-only redrive, X6 idiom).
static void AhCreditWalletInTxn(uint32 guidLow, uint32 amount)
{
    if (amount == 0)
    {
        return;
    }
    Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, guidLow));
    if (p)
    {
        p->ModifyMoney(int32(amount));
        p->SaveInventoryAndGoldToDB();
    }
    else
    {
        CharacterDatabase.PExecute("UPDATE `characters` SET `money` = `money` + '%u' "
            "WHERE `guid` = '%u'", amount, guidLow);
    }
}

// ---------------------------------------------------------------------------
// Redrive queue (spec 4.1 step 4, "failed finalize"): forward-only re-attempts
// of finalizes whose checked commit returned false. NEVER rolls a committed
// book fact back. In-memory only (a mangosd crash re-enters via reconcile).
// ---------------------------------------------------------------------------
struct AhRedriveEntry
{
    PlayerMutationResult res;
    PendingMutation pm;
    uint32 nextRetrySec;
    uint32 attempts;
};
static std::list<AhRedriveEntry> s_ahRedrive;

// In-doubt sweep TTL (spec 4.1 step 5). A player mutation un-answered by the
// worker for this long is tombstoned and the player gets the legacy DB error;
// the reservation stays non-terminal (decision 10) and a late reply still
// applies via Take. Generous vs the worker round-trip; default-off in prod.
static uint32 const AH_MUTATION_INDOUBT_TTL_SEC = 30u;

// forward declarations (bodies below)
static bool AhFinalizeOkDispatch(PlayerMutationResult const& res, PendingMutation const& pm);
static void AhHandleCancelPrepared(PlayerMutationResult const& res);

// [SP-2] replay of SendAuctionSuccessfulMailInTransaction from wire facts:
// owner-exists guard, sold-notify deferred BEFORE the mail push, profit =
// effectiveBid + deposit - cut, COPIED mask.
static void AhSellerPayoutFromFacts(MutationFacts const& f, CustodyDeferred& def)
{
    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, f.sellerGuid);
    Player* owner = sObjectMgr.GetPlayer(ownerGuid);
    uint32 ownerAccId = 0;
    if (!owner)
    {
        ownerAccId = sObjectMgr.GetPlayerAccountIdByGUID(ownerGuid);
    }
    if (!owner && !ownerAccId)
    {
        return;
    }

    uint32 const cut = AhCutFor(f.houseId, f.effectiveBid);
    uint32 const profit = f.effectiveBid + f.deposit - cut;

    std::ostringstream body;
    body.width(16);
    body << std::right << std::hex << f.curBidderGuid;
    body << std::dec << ":" << f.effectiveBid << ":" << f.buyout;
    body << ":" << f.deposit << ":" << cut;

    if (owner)
    {
        uint32 const ownerGuidLow = f.sellerGuid;
        uint32 const houseId  = f.houseId;
        uint32 const aucId    = f.auctionId;
        uint32 const bidValue = f.effectiveBid;
        uint32 const outbid   = AhOutbidFor(f.effectiveBid);
        uint32 const bidder   = f.curBidderGuid;
        uint32 const itemTpl  = f.itemTemplate;
        int32  const itemRand = f.randomPropertyId;
        def.effects.push_back([ownerGuidLow, houseId, aucId, bidValue, outbid, bidder, itemTpl, itemRand]()
        {
            Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, ownerGuidLow));
            if (p)
            {
            p->GetSession()->SendAuctionOwnerNotificationData(houseId, aucId,
                bidValue, outbid, bidder, itemTpl, itemRand, true);
            }
        });
    }

    MailDraft(AhMailSubject(f.itemTemplate, f.randomPropertyId, AUCTION_SUCCESSFUL), body.str())
        .SetMoney(profit)
        .SendMailToInTransaction(MailReceiver(owner, ownerGuid),
                                 MailSender(MAIL_AUCTION, uint32(f.houseId), MAIL_STATIONERY_AUCTION),
                                 def, MAIL_CHECK_MASK_COPIED);
}

// [SP-2] replay of SendAuctionWonMailInTransaction from wire facts, minus the
// GM-log block (server-side-only trace). A bot winner (curBidderGuid==0)
// resolves to "no receiver" and follows the legacy destroy branch (spec
// section 3). Escrow-cache miss -> loud log, no item mail (the caller still
// terminalizes "item:<id>").
static void AhItemToWinnerFromFacts(MutationFacts const& f, CustodyDeferred& def)
{
    Item* pItem = sAuctionMgr.GetAItem(f.itemGuid);
    if (!pItem)
    {
        sLog.outError("[AHMut] auction %u won-item %u missing from escrow cache; no item mail",
                      f.auctionId, f.itemGuid);
        return;
    }

    ObjectGuid bidderGuid = ObjectGuid(HIGHGUID_PLAYER, f.curBidderGuid);
    Player* bidder = sObjectMgr.GetPlayer(bidderGuid);
    uint32 bidderAccId = 0;
    if (!bidder)
    {
        bidderAccId = sObjectMgr.GetPlayerAccountIdByGUID(bidderGuid);
    }

    uint32 const savedItemGuidLow = f.itemGuid;

    if (bidder || bidderAccId)
    {
        std::ostringstream body;
        body.width(16);
        body << std::right << std::hex << f.sellerGuid;
        body << std::dec << ":" << f.effectiveBid << ":" << f.buyout;

        CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'",
                                   f.curBidderGuid, savedItemGuidLow);

        if (bidder)
        {
            uint32 const bidderGuidLow = f.curBidderGuid;
            uint32 const houseId  = f.houseId;
            uint32 const aucId    = f.auctionId;
            uint32 const bidValue = f.effectiveBid;
            uint32 const outbid   = AhOutbidFor(f.effectiveBid);
            uint32 const itemTpl  = f.itemTemplate;
            int32  const itemRand = f.randomPropertyId;
            def.effects.push_back([bidderGuidLow, houseId, aucId, bidValue, outbid, itemTpl, itemRand]()
            {
                Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, bidderGuidLow));
                if (p)
                {
                    p->GetSession()->SendAuctionBidderNotificationData(houseId,
                        aucId, bidderGuidLow, bidValue, outbid, itemTpl,
                        itemRand, true);
                }
            });
        }

        def.effects.push_back([savedItemGuidLow]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
        });

        MailDraft(AhMailSubject(f.itemTemplate, f.randomPropertyId, AUCTION_WON), body.str())
            .AddItem(pItem)
            .SendMailToInTransaction(MailReceiver(bidder, bidderGuid),
                                     MailSender(MAIL_AUCTION, uint32(f.houseId), MAIL_STATIONERY_AUCTION),
                                     def, MAIL_CHECK_MASK_COPIED);
    }
    else
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", savedItemGuidLow);
        def.effects.push_back([savedItemGuidLow, pItem]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
            delete pItem;
        });
    }
}

// [SP-2] replay of the S2 outbid displacement (UpdateBidCustody +
// SendAuctionOutbiddedMailInTransaction): terminalize the prior live bid row
// ledger-only, outbid-notify deferred BEFORE the refund-mail push, refund
// money = the prior bid, COPIED mask.
static void AhRefundPriorBidderFromFacts(MutationFacts const& f, std::string const& priorKey, CustodyDeferred& def)
{
    CustodyService::RollbackGoldLedgerOnly(priorKey);

    ObjectGuid priorGuid = ObjectGuid(HIGHGUID_PLAYER, f.priorBidderGuid);
    Player* prior = sObjectMgr.GetPlayer(priorGuid);
    uint32 priorAccId = 0;
    if (!prior)
    {
        priorAccId = sObjectMgr.GetPlayerAccountIdByGUID(priorGuid);
    }
    if (!prior && !priorAccId)
    {
        return;   // legacy sends nothing when the old bidder is gone
    }

    if (prior)
    {
        uint32 const priorGuidLow = f.priorBidderGuid;
        uint32 const houseId  = f.houseId;
        uint32 const aucId    = f.auctionId;
        uint32 const bidValue = f.priorBidAmount;
        uint32 const outbid   = AhOutbidFor(f.priorBidAmount);
        uint32 const itemTpl  = f.itemTemplate;
        int32  const itemRand = f.randomPropertyId;
        def.effects.push_back([priorGuidLow, houseId, aucId, bidValue, outbid, itemTpl, itemRand]()
        {
            Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, priorGuidLow));
            if (p)
            {
                p->GetSession()->SendAuctionBidderNotificationData(houseId,
                    aucId, priorGuidLow, bidValue, outbid, itemTpl, itemRand,
                    false);
            }
        });
    }

    MailDraft(AhMailSubject(f.itemTemplate, f.randomPropertyId, AUCTION_OUTBIDDED), "")
        .SetMoney(f.priorBidAmount)
        .SendMailToInTransaction(MailReceiver(prior, priorGuid),
                                 MailSender(MAIL_AUCTION, uint32(f.houseId), MAIL_STATIONERY_AUCTION),
                                 def, MAIL_CHECK_MASK_COPIED);
}

// [SP-2] replay of the S5 bidder-refund leg
// (SendAuctionCancelledToBidderMailInTransaction): removed-notify deferred
// BEFORE the refund-mail push, money = the standing bid.
static void AhRefundCancelledBidderFromFacts(MutationFacts const& f, std::string const& bidKey, CustodyDeferred& def)
{
    CustodyService::RollbackGoldLedgerOnly(bidKey);

    ObjectGuid bidderGuid = ObjectGuid(HIGHGUID_PLAYER, f.curBidderGuid);
    Player* bidder = sObjectMgr.GetPlayer(bidderGuid);
    uint32 accId = 0;
    if (!bidder)
    {
        accId = sObjectMgr.GetPlayerAccountIdByGUID(bidderGuid);
    }
    if (!bidder && !accId)
    {
        return;
    }

    if (bidder)
    {
        uint32 const bidderGuidLow = f.curBidderGuid;
        uint32 const aucId    = f.auctionId;
        uint32 const itemTpl  = f.itemTemplate;
        int32  const itemRand = f.randomPropertyId;
        def.effects.push_back([bidderGuidLow, aucId, itemTpl, itemRand]()
        {
            Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, bidderGuidLow));
            if (p)
            {
                p->GetSession()->SendAuctionRemovedNotificationData(aucId, itemTpl, itemRand);
            }
        });
    }

    MailDraft(AhMailSubject(f.itemTemplate, f.randomPropertyId, AUCTION_CANCELLED_TO_BIDDER), "")
        .SetMoney(f.curBid)
        .SendMailToInTransaction(MailReceiver(bidder, bidderGuid),
                                 MailSender(MAIL_AUCTION, uint32(f.houseId), MAIL_STATIONERY_AUCTION),
                                 def, MAIL_CHECK_MASK_COPIED);
}

// [SP-2] item return to the seller (cancel / expiry / repair), replaying
// SendAuctionExpiredMailInTransaction / the S5 return: expired owner-notify
// (EXPIRED response only), RemoveAItem deferred FIRST, DeliverItem flips
// "item:<id>" -> TERMINAL_OK and co-commits the mail; destroy branch when the
// account is gone; ledger-only flip when the escrow cache lost the Item*.
static void AhReturnItemToSellerFromFacts(MutationFacts const& f, uint32 mailResponse, CustodyDeferred& def)
{
    std::string const itemKey = "item:" + std::to_string(f.auctionId);
    Item* pItem = sAuctionMgr.GetAItem(f.itemGuid);
    if (!pItem)
    {
        sLog.outError("[AHMut] auction %u return-item %u missing from escrow cache; ledger-only flip",
                      f.auctionId, f.itemGuid);
        CustodyService::CommitGoldLedgerOnly(itemKey);
        return;
    }

    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, f.sellerGuid);
    Player* owner = sObjectMgr.GetPlayer(ownerGuid);
    uint32 accId = 0;
    if (!owner)
    {
        accId = sObjectMgr.GetPlayerAccountIdByGUID(ownerGuid);
    }

    uint32 const savedItemGuidLow = f.itemGuid;

    if (owner || accId)
    {
        if (owner && mailResponse == uint32(AUCTION_EXPIRED))
        {
            uint32 const ownerGuidLow = f.sellerGuid;
            uint32 const houseId  = f.houseId;
            uint32 const aucId    = f.auctionId;
            uint32 const itemTpl  = f.itemTemplate;
            int32  const itemRand = f.randomPropertyId;
            def.effects.push_back([ownerGuidLow, houseId, aucId, itemTpl, itemRand]()
            {
                Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, ownerGuidLow));
                if (p)
                {
                    p->GetSession()->SendAuctionOwnerNotificationData(houseId,
                        aucId, 0, 0, 0, itemTpl, itemRand, false);
                }
            });
        }

        def.effects.push_back([savedItemGuidLow]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
        });

        MailDraft itemReturn(AhMailSubject(f.itemTemplate, f.randomPropertyId, mailResponse), "");
        itemReturn.AddItem(pItem);
        CustodyService::DeliverItem(def, itemKey, itemReturn,
                                    MailReceiver(owner, ownerGuid),
                                    MailSender(MAIL_AUCTION, uint32(f.houseId), MAIL_STATIONERY_AUCTION),
                                    MAIL_CHECK_MASK_COPIED);
    }
    else
    {
        CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid`='%u'", savedItemGuidLow);
        CustodyService::CommitGoldLedgerOnly(itemKey);
        def.effects.push_back([savedItemGuidLow, pItem]()
        {
            sAuctionMgr.RemoveAItem(savedItemGuidLow);
            delete pItem;
        });
    }
}

#ifdef ENABLE_ELUNA
// [SP-2 spec 5.8] Under WriteAuthority the worker owns the book, so the legacy
// in-map AuctionEntry that fed the Eluna AH hooks no longer lives in mangosd.
// Synthesize a transient (stack-only, never map-inserted) AuctionEntry from the
// wire facts and fire the SAME OnAdd/OnRemove the legacy custody path invokes,
// so Lua AH scripts still observe listing/finalize events. The hooks self-guard
// on owner-online + escrow-item-present (GetAItem), so fire them while the
// escrow item is still cached -- i.e. BEFORE the finalize's deferred RemoveAItem
// runs. startbid is not carried in MutationFacts (0 here); every other field is
// exact.
static void AhElunaFillEntry(AuctionEntry& e, MutationFacts const& f)
{
    e.Id                   = f.auctionId;
    e.itemGuidLow          = f.itemGuid;
    e.itemTemplate         = f.itemTemplate;
    e.itemCount            = f.itemCount;
    e.itemRandomPropertyId = f.randomPropertyId;
    e.owner                = f.sellerGuid;
    e.startbid             = 0;
    e.bid                  = f.curBid;
    e.buyout               = f.buyout;
    e.expireTime           = 0;
    e.bidder               = f.curBidderGuid;
    e.deposit              = f.deposit;
    e.auctionHouseEntry    = sAuctionHouseStore.LookupEntry(uint32(f.houseId));
}

static void AhElunaOnAddFromFacts(MutationFacts const& f)
{
    Eluna* e = sWorld.GetEluna();
    if (!e)
    {
        return;
    }
    AuctionEntry tmp;
    AhElunaFillEntry(tmp, f);
    if (!tmp.auctionHouseEntry)
    {
        return;
    }
    e->OnAdd(sAuctionMgr.GetAuctionsMap(tmp.auctionHouseEntry), &tmp);
}

static void AhElunaOnRemoveFromFacts(MutationFacts const& f)
{
    Eluna* e = sWorld.GetEluna();
    if (!e)
    {
        return;
    }
    AuctionEntry tmp;
    AhElunaFillEntry(tmp, f);
    if (!tmp.auctionHouseEntry)
    {
        return;
    }
    e->OnRemove(sAuctionMgr.GetAuctionsMap(tmp.auctionHouseEntry), &tmp);
}
#endif /* ENABLE_ELUNA */

// [SP-2] MUT_OK sell (spec 4.1 step 4): NO value moves -- the deposit stays
// reserved and the item stays escrowed until resolution. Cross-check + the
// legacy AUCTION_STARTED/AUCTION_OK result only.
static bool AhFinalizeSellOk(PlayerMutationResult const& res, PendingMutation const& pm)
{
    MutationFacts const& f = res.facts;
    CustodyRow depRow;
    CustodyRow itemRow;
    if (!CustodyLedger::Get(pm.depKey, depRow) || depRow.state != CST_RESERVED ||
        depRow.ownerGuid != pm.playerGuidLow || depRow.amount != f.deposit ||
        !CustodyLedger::Get(pm.itemKey, itemRow) || itemRow.state != CST_RESERVED ||
        itemRow.itemGuid != f.itemGuid ||
        f.sellerGuid != pm.playerGuidLow || f.auctionId != pm.auctionId)
    {
        sLog.outError("[AHMut] PROTOCOL FAULT: sell facts mismatch ledger for uuid " UI64FMTD
                      " (auction %u); finalize refused", res.uuid, f.auctionId);
        return true;   // faults are terminal, never redriven
    }
    AhSendCommandResultTo(pm.playerGuidLow, f.auctionId, AUCTION_STARTED, AUCTION_OK, 0);
#ifdef ENABLE_ELUNA
    // [SP-2 spec 5.8] listing committed to the worker's book -> Eluna OnAdd.
    AhElunaOnAddFromFacts(f);
#endif
    return true;
}

// [SP-2] MUT_OK bid/buyout (spec 4.1 step 4 + I4 value math): section 4.4
// order = command-result FIRST, then outbid-notify, then mails.
//
// [SP-2 reconciliation] op=0x42 (from an IPC_PLAYER_BUYOUT intent) is a buyout
// WIN only when the worker actually removed the row at the buyout price; a
// below-buyout price is committed by the worker as a NORMAL bid and the row
// stays LIVE (HEAD 16e22c4e). Distinguish by facts: WIN <=> buyout != 0 &&
// effectiveBid == buyout. A BID sub-case falls through the standing-bid path
// (release = reservedAmount - effectiveBid == 0; no seller payout, no item
// delivery, just the displaced prior-bidder refund).
static bool AhFinalizeBidOk(PlayerMutationResult const& res, PendingMutation const& pm)
{
    MutationFacts const& f = res.facts;
    bool const isBuyoutWin = (res.op == uint8(IPC_PLAYER_BUYOUT & 0xFFu)
                              && f.buyout != 0 && f.effectiveBid == f.buyout);
    bool const sameBidderRaise = (f.priorBidderGuid != 0 && f.priorBidderGuid == pm.playerGuidLow);

    // ---- fail-closed cross-check vs our OWN ledger (spec section 3) ----
    CustodyRow ownRow;
    if (!CustodyLedger::Get(pm.reserveKey, ownRow) || ownRow.state != CST_RESERVED ||
        ownRow.ownerGuid != pm.playerGuidLow || ownRow.auctionId != f.auctionId ||
        ownRow.amount != pm.reservedAmount ||
        f.curBidderGuid != pm.playerGuidLow || f.effectiveBid != f.curBid)
    {
        sLog.outError("[AHMut] PROTOCOL FAULT: bid facts mismatch ledger for uuid " UI64FMTD
                      " (auction %u); finalize refused", res.uuid, f.auctionId);
        return true;
    }
    if (sameBidderRaise && f.effectiveBid < f.priorBidAmount)
    {
        sLog.outError("[AHMut] PROTOCOL FAULT: raise below prior bid for uuid " UI64FMTD, res.uuid);
        return true;
    }
    uint32 const needed = sameBidderRaise ? (f.effectiveBid - f.priorBidAmount) : f.effectiveBid;
    if (needed > pm.reservedAmount)
    {
        sLog.outError("[AHMut] PROTOCOL FAULT: needed %u exceeds reserved %u for uuid " UI64FMTD,
                      needed, pm.reservedAmount, res.uuid);
        return true;
    }
    uint32 const remainder = pm.reservedAmount - needed;

    CustodyRow liveRow;
    std::string priorKey;
    if (f.priorBidderGuid != 0)
    {
        if (!AhFindPriorBidRow(f.auctionId, f.priorBidderGuid, f.priorBidAmount, pm.reserveKey, liveRow))
        {
            sLog.outError("[AHMut] PROTOCOL FAULT: prior live bid row mismatch for auction %u (uuid " UI64FMTD ")",
                          f.auctionId, res.uuid);
            return true;
        }
        priorKey = liveRow.idemKey;
    }

    CustodyDeferred def;

    // 4.4: command-result FIRST (the S2 seam pushes it before UpdateBid).
    {
        uint32 const bidderGuidLow = pm.playerGuidLow;
        uint32 const aucId = f.auctionId;
        uint32 const newOutbid = AhOutbidFor(f.effectiveBid);
        def.effects.push_back([bidderGuidLow, aucId, newOutbid]()
        {
            AhSendCommandResultTo(bidderGuidLow, aucId, AUCTION_BID_PLACED, AUCTION_OK, newOutbid);
        });
    }

    CharacterDatabase.BeginTransaction();

    if (sameBidderRaise)
    {
        // Merge the delta row into the live bid row. The delta gold was
        // debited at reserve time -> NULL bidder = ledger-only, no 2nd debit.
        CustodyService::TopUpBid(priorKey, f.effectiveBid, 0, NULL);
        if (remainder > 0)
        {
            CustodyLedger::SetAmount(pm.reserveKey, needed);
        }
        CustodyService::CommitGoldLedgerOnly(pm.reserveKey);
    }
    else
    {
        if (f.priorBidderGuid != 0)
        {
            AhRefundPriorBidderFromFacts(f, priorKey, def);
        }
        if (remainder > 0)
        {
            CustodyLedger::SetAmount(pm.reserveKey, needed);
        }
        // Our (possibly shrunk) reserved row IS the live bid row: stays RESERVED.
    }

    AhCreditWalletInTxn(pm.playerGuidLow, remainder);   // I4: buyout remainder (0 on a bid)

    if (isBuyoutWin)
    {
        // S4 replay MINUS the auction-row delete + AuctionsMap ops: the worker
        // owns the book under WriteAuthority.
        AhSellerPayoutFromFacts(f, def);
        CustodyService::RollbackGoldLedgerOnly("dep:" + std::to_string(f.auctionId));
        CustodyService::CommitGoldLedgerOnly(sameBidderRaise ? priorKey : pm.reserveKey);
        AhItemToWinnerFromFacts(f, def);
        CustodyService::CommitGoldLedgerOnly("item:" + std::to_string(f.auctionId));
    }

    // [SP-2 Task 15] finalize-fail crash seam: CommitCheckedOrForcedFail forces
    // this checked commit to roll back and report false ONCE when
    // AH.Service.CustodyFailCommitAt == "finalize-fail", exercising the
    // redrive-without-rollback path below. Inert (a plain checked commit) on a
    // live realm.
    if (!CustodyService::CommitCheckedOrForcedFail("finalize-fail"))
    {
        if (remainder > 0)
        {
            Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow));
            if (p)
            {
                p->ModifyMoney(-int32(remainder));   // X6: undo the in-memory credit
            }
        }
        return false;   // -> redrive, never rollback (I10)
    }
#ifdef ENABLE_ELUNA
    if (isBuyoutWin)
    {
        // [SP-2 spec 5.8] buyout win removed the listing -> Eluna OnRemove
        // (fired while the escrow item is still cached, before def.run()).
        AhElunaOnRemoveFromFacts(f);
    }
#endif
    def.run();
    return true;
}

// [SP-2] MUT_OK cancel (after CONFIRM; spec 4.2 step 3): S5 replay minus the
// auction delete/map ops; 4.4 order = notifications and mails first,
// command-result LAST.
static bool AhFinalizeCancelOk(PlayerMutationResult const& res, PendingMutation const& pm)
{
    MutationFacts const& f = res.facts;

    uint32 const cut = f.curBid ? AhCutFor(f.houseId, f.curBid) : 0;
    if (cut)
    {
        CustodyRow cutRow;
        if (pm.reserveKey.empty() || !CustodyLedger::Get(pm.reserveKey, cutRow) ||
            cutRow.state != CST_RESERVED || cutRow.ownerGuid != pm.playerGuidLow ||
            cutRow.amount != cut || cutRow.auctionId != f.auctionId)
        {
            sLog.outError("[AHMut] PROTOCOL FAULT: cancel cut reservation mismatch for uuid " UI64FMTD
                          " (auction %u)", res.uuid, f.auctionId);
            return true;
        }
    }
    std::string bidKey;
    if (f.curBid && f.curBidderGuid != 0)
    {
        CustodyRow liveRow;
        if (!AhFindPriorBidRow(f.auctionId, f.curBidderGuid, f.curBid, pm.reserveKey, liveRow))
        {
            sLog.outError("[AHMut] PROTOCOL FAULT: cancel live bid row mismatch for auction %u (uuid " UI64FMTD ")",
                          f.auctionId, res.uuid);
            return true;
        }
        bidKey = liveRow.idemKey;
    }

    CustodyDeferred def;
    CharacterDatabase.BeginTransaction();

    if (!bidKey.empty())
    {
        AhRefundCancelledBidderFromFacts(f, bidKey, def);
    }
    CustodyService::CommitGoldLedgerOnly("dep:" + std::to_string(f.auctionId));   // forfeit (S5)
    AhReturnItemToSellerFromFacts(f, uint32(AUCTION_CANCELED), def);
    if (cut)
    {
        CustodyService::CommitGoldLedgerOnly(pm.reserveKey);                      // cut -> house sink
    }
    {
        uint32 const sellerGuidLow = pm.playerGuidLow;
        uint32 const aucId = f.auctionId;
        def.effects.push_back([sellerGuidLow, aucId]()
        {
            AhSendCommandResultTo(sellerGuidLow, aucId, AUCTION_REMOVED, AUCTION_OK, 0);
        });
    }

    if (!CharacterDatabase.CommitTransactionChecked())
    {
        return false;
    }
#ifdef ENABLE_ELUNA
    // [SP-2 spec 5.8] cancel removed the listing -> Eluna OnRemove (fired while
    // the escrow item is still cached, before def.run()).
    AhElunaOnRemoveFromFacts(f);
#endif
    def.run();
    return true;
}

static bool AhFinalizeOkDispatch(PlayerMutationResult const& res, PendingMutation const& pm)
{
    switch (res.op)
    {
        case uint8(IPC_PLAYER_SELL & 0xFFu):
            return AhFinalizeSellOk(res, pm);
        case uint8(IPC_PLAYER_BID & 0xFFu):
        case uint8(IPC_PLAYER_BUYOUT & 0xFFu):
            return AhFinalizeBidOk(res, pm);
        case uint8(IPC_PLAYER_CANCEL & 0xFFu):
        case uint8(IPC_PLAYER_CANCEL_CONFIRM & 0xFFu):
            return AhFinalizeCancelOk(res, pm);
        default:
            sLog.outError("[AHMut] PROTOCOL FAULT: MUT_OK with unknown op 0x%02X (uuid " UI64FMTD ")",
                          uint32(res.op), res.uuid);
            return true;
    }
}

// [SP-2] MUT_REJECTED (spec 4.1 step 4): silent wallet re-credit of the
// reservation (I1: NO mail for the gold) + the legacy error result. For a
// rejected SELL the escrowed item also leaves custody: returned by MAIL.
static bool AhFinalizeRejected(PlayerMutationResult const& res, PendingMutation const& pm)
{
    MutationFacts const& f = res.facts;
    Player* online = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow));
    uint32 onlineCredit = 0;

    CustodyDeferred def;
    CharacterDatabase.BeginTransaction();

    if (res.op == uint8(IPC_PLAYER_SELL & 0xFFu))
    {
        CustodyService::ReleaseGoldToWallet(def, pm.playerGuidLow, online, pm.reservedAmount, pm.depKey);
        if (online)
        {
            onlineCredit += pm.reservedAmount;
        }

        CustodyRow itemRow;
        if (!pm.itemKey.empty() && CustodyLedger::Get(pm.itemKey, itemRow) && itemRow.state == CST_RESERVED)
        {
            Item* pItem = sAuctionMgr.GetAItem(itemRow.itemGuid);
            CustodyLedger::SetState(pm.itemKey, CST_TERMINAL_BACK, static_cast<uint64>(time(NULL)));
            if (pItem)
            {
                uint32 const savedItemGuidLow = itemRow.itemGuid;
                def.effects.push_back([savedItemGuidLow]()
                {
                    sAuctionMgr.RemoveAItem(savedItemGuidLow);
                });
                MailDraft ret(AhMailSubject(pItem->GetEntry(), pItem->GetItemRandomPropertyId(), AUCTION_CANCELED), "");
                ret.AddItem(pItem);
                ret.SendMailToInTransaction(MailReceiver(online, ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow)),
                                            MailSender(MAIL_AUCTION, uint32(f.houseId), MAIL_STATIONERY_AUCTION),
                                            def, MAIL_CHECK_MASK_COPIED);
            }
            else
            {
                sLog.outError("[AHMut] rejected sell %u: escrow item %u missing; ledger-only return",
                              pm.auctionId, itemRow.itemGuid);
            }
        }
    }
    else if (!pm.reserveKey.empty() && pm.reservedAmount > 0)
    {
        // bid / buyout (and, defensively, a post-CONFIRM cancel reject).
        CustodyService::ReleaseGoldToWallet(def, pm.playerGuidLow, online, pm.reservedAmount, pm.reserveKey);
        if (online)
        {
            onlineCredit += pm.reservedAmount;
        }
    }

    // Legacy error result, deferred so it fires only when the release landed.
    {
        uint8 const op = res.op;
        uint8 const reason = res.reason;
        uint32 const guidLow = pm.playerGuidLow;
        uint32 const aucId = pm.auctionId;
        MutationFacts const facts = f;
        def.effects.push_back([op, reason, guidLow, aucId, facts]()
        {
            if (reason == uint8(AUCTION_ERR_HIGHER_BID))
            {
                AhSendHigherBidResultTo(guidLow, op, facts);
            }
            else if (reason != 0)
            {
                AhSendCommandResultTo(guidLow, aucId, AhActionForOp(op), AuctionError(reason), 0);
            }
            // reason == 0 -> silent (spec: legacy emits nothing).
        });
    }

    if (!CharacterDatabase.CommitTransactionChecked())
    {
        if (onlineCredit > 0 && online)
        {
            online->ModifyMoney(-int32(onlineCredit));   // X6: undo the in-memory credit
        }
        return false;
    }
    def.run();
    return true;
}

// [SP-2] MUT_REJECTED_STALE (spec 4.2 step 4): the cancel CONFIRM raced the
// worker's unlock. Release the cut reservation (if one was made) and resolve
// the tombstone. No SMSG: the auction is untouched (divergence 12.5).
static bool AhFinalizeStale(PlayerMutationResult const& res, PendingMutation const& pm)
{
    if (pm.reserveKey.empty() || pm.reservedAmount == 0)
    {
        DETAIL_LOG("[AHMut] REJECTED_STALE for uuid " UI64FMTD " with no reservation", res.uuid);
        return true;
    }
    Player* online = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow));
    CustodyDeferred def;
    CharacterDatabase.BeginTransaction();
    CustodyService::ReleaseGoldToWallet(def, pm.playerGuidLow, online, pm.reservedAmount, pm.reserveKey);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        if (online)
        {
            online->ModifyMoney(-int32(pm.reservedAmount));
        }
        return false;
    }
    def.run();
    return true;
}

void AhNotifyMutationInDoubt(PendingMutation const& pm)
{
    // M2: the timed-out mutation reports the legacy DB error; the reservation
    // stays non-terminal (decision 10 -- no unilateral rollback) and a late
    // reply still applies through the tombstone.
    AhSendCommandResultTo(pm.playerGuidLow, pm.auctionId,
                          AhActionForOp(uint8(pm.op & 0xFFu)), AUCTION_ERR_DATABASE, 0);
}

void AhHandlePlayerMutationResult(PlayerMutationResult const& res)
{
    if (res.status == uint8(MUT_PREPARED))
    {
        AhHandleCancelPrepared(res);
        return;
    }

    MutationPendingMap& pend = sWorld.GetMutationPending();
    PendingMutation pm;
    if (!pend.Take(res.uuid, pm))
    {
        // Late reply on a terminal/unknown row: loud protocol fault, never a
        // silent drop or a double apply (decision 10).
        sLog.outError("[AHMut] PROTOCOL FAULT: result status %u for unknown or already-consumed uuid " UI64FMTD,
                      uint32(res.status), res.uuid);
        return;
    }

    bool ok = true;
    switch (res.status)
    {
        case uint8(MUT_OK):
            ok = AhFinalizeOkDispatch(res, pm);
            break;
        case uint8(MUT_REJECTED):
            ok = AhFinalizeRejected(res, pm);
            break;
        case uint8(MUT_REJECTED_STALE):
            ok = AhFinalizeStale(res, pm);
            break;
        default:
            sLog.outError("[AHMut] PROTOCOL FAULT: unknown status %u for uuid " UI64FMTD,
                          uint32(res.status), res.uuid);
            break;
    }

    if (!ok)
    {
        AhRedriveEntry e;
        e.res = res;
        e.pm = pm;
        e.attempts = 1;
        e.nextRetrySec = uint32(time(NULL)) + 5;
        s_ahRedrive.push_back(e);
        sLog.outError("[AHMut] finalize checked-commit FAILED for uuid " UI64FMTD "; queued for redrive", res.uuid);
    }
}

// [SP-2] MUT_PREPARED -- the cancel phase-2 driver (spec 4.2 step 2 + [v3]).
static void AhHandleCancelPrepared(PlayerMutationResult const& res)
{
    MutationPendingMap& pend = sWorld.GetMutationPending();
    WorkerSupervisor* sv = sWorld.GetAhSupervisor();

    PendingMutation pm;
    if (!pend.Peek(res.uuid, pm))
    {
        sLog.outError("[AHMut] PROTOCOL FAULT: PREPARED for unknown uuid " UI64FMTD, res.uuid);
        return;
    }
    if (pm.op != uint16(IPC_PLAYER_CANCEL))
    {
        sLog.outError("[AHMut] PROTOCOL FAULT: PREPARED for non-cancel op 0x%04X (uuid " UI64FMTD ")",
                      uint32(pm.op), res.uuid);
        return;
    }

    // A PREPARE reply landing after the tombstone -> mangosd answers ABORT
    // (spec 4.2 [v3]); the slot resolves.
    if (pm.state == uint8(PMUT_TOMBSTONE))
    {
        if (sv && sv->ServiceActive())
        {
            PlayerCancelDecide d;
            d.uuid = res.uuid;
            d.auctionId = pm.auctionId;
            IpcMessage m;
            m.op = IPC_PLAYER_CANCEL_ABORT;
            d.Encode(m.body);
            sv->Channel().SendFrame(m);
        }
        PendingMutation consumed;
        pend.Take(res.uuid, consumed);
        return;
    }

    // [FIX D] Duplicate-PREPARED guard: only a slot still in AWAIT_RESULT is
    // awaiting its first PREPARE reply. A slot already advanced past it (CONFIRM
    // sent -> AWAIT_CONFIRM) that receives a second PREPARE frame must be ignored:
    // re-running the affordability gate would re-reserve the cut on an already-
    // confirmed slot. (TOMBSTONE is handled above.)
    if (pm.state != uint8(PMUT_AWAIT_RESULT))
    {
        sLog.outError("[AHMut] duplicate PREPARED for uuid " UI64FMTD " in state %u"
                      " (not AWAIT_RESULT); ignoring", res.uuid, uint32(pm.state));
        return;
    }

    MutationFacts const& f = res.facts;
    uint32 const cut = f.curBid ? AhCutFor(f.houseId, f.curBid) : 0;
    Player* pl = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow));

    // Affordability gate (legacy silent return). A standing bid (curBid != 0)
    // means a cut is owed on cancel: an OFFLINE seller cannot be debited -> ABORT
    // (the auction is untouched; the seller can retry -- byte-parity: legacy
    // emits nothing here). Gated on curBid, not the computed cut, so the offline
    // -> ABORT decision is independent of whether DBC cut data is loaded (the -t
    // harness has none; AhCutFor would return 0 and mask the gate).
    bool abort = false;
    if (f.curBid != 0)
    {
        if (!pl)
        {
            abort = true;
        }
        else if (pl->GetMoney() < cut)
        {
            abort = true;
        }
    }

    if (!abort && cut)
    {
        // Durable cut reserve = its own checked commit (decision 9). The key
        // is uuid-salted so a retry after a stale/aborted cancel of the same
        // auction can never collide on uk_idem.
        std::string const cutKey = "cut:" + std::to_string(pm.auctionId) + ":" + std::to_string(res.uuid);
        CustodyDeferred def;
        CharacterDatabase.BeginTransaction();
        CustodyService::ReserveGold(def, pm.playerGuidLow, pl, cut, cutKey, pm.auctionId, ROLE_PROCEEDS);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            if (pl)
            {
                pl->ModifyMoney(int32(cut));   // X6: undo the in-memory debit
            }
            sLog.outError("[AHMut] cancel cut reserve commit FAILED for uuid " UI64FMTD "; aborting cancel", res.uuid);
            abort = true;
        }
        else
        {
            def.run();
            pend.SetReserve(res.uuid, cut, cutKey);
        }
    }

    PlayerCancelDecide d;
    d.uuid = res.uuid;
    d.auctionId = pm.auctionId;
    IpcMessage m;
    m.op = abort ? IPC_PLAYER_CANCEL_ABORT : IPC_PLAYER_CANCEL_CONFIRM;
    d.Encode(m.body);
    if (sv && sv->ServiceActive())
    {
        sv->Channel().SendFrame(m);
    }

    if (abort)
    {
        PendingMutation consumed;
        pend.Take(res.uuid, consumed);       // legacy silent-return parity: no SMSG
    }
    else
    {
        pend.RearmConfirm(res.uuid, uint32(time(NULL)));
    }
}

void AhProcessRedriveQueue(uint32 nowSec)
{
    // (1) Forward-only re-attempt of finalizes whose checked commit failed.
    for (std::list<AhRedriveEntry>::iterator it = s_ahRedrive.begin(); it != s_ahRedrive.end();)
    {
        if (nowSec < it->nextRetrySec)
        {
            ++it;
            continue;
        }
        bool ok = true;
        switch (it->res.status)
        {
            case uint8(MUT_OK):
                ok = AhFinalizeOkDispatch(it->res, it->pm);
                break;
            case uint8(MUT_REJECTED):
                ok = AhFinalizeRejected(it->res, it->pm);
                break;
            case uint8(MUT_REJECTED_STALE):
                ok = AhFinalizeStale(it->res, it->pm);
                break;
            default:
                break;
        }
        if (ok)
        {
            it = s_ahRedrive.erase(it);
        }
        else
        {
            ++it->attempts;
            it->nextRetrySec = nowSec + 5;
            if (it->attempts % 12 == 0)
            {
                sLog.outError("[AHMut] redrive STUCK: uuid " UI64FMTD " still failing after %u attempts",
                              it->res.uuid, it->attempts);
            }
            ++it;
        }
    }

    // (2) Age un-answered player mutations into in-doubt tombstones and emit the
    // one-time legacy AUCTION_ERR_DATABASE result for each (spec 4.1 step 5).
    // Cheap no-op when the pending map is empty (the default-off case).
    std::vector<uint64> newlyInDoubt;
    MutationPendingMap& pend = sWorld.GetMutationPending();
    pend.SweepToTombstones(nowSec, AH_MUTATION_INDOUBT_TTL_SEC, newlyInDoubt);
    for (size_t i = 0; i < newlyInDoubt.size(); ++i)
    {
        PendingMutation pm;
        if (pend.Peek(newlyInDoubt[i], pm))
        {
            AhNotifyMutationInDoubt(pm);
        }
    }
}

// ===========================================================================
// SP-2 Task 12: worker-initiated resolutions (IPC_RESOLVE_APPLY) + reconcile.
// ===========================================================================

// [SP-2] the single live (CST_RESERVED ROLE_PROCEEDS CUSTODY_GOLD) cut
// reservation for an auction. The cancel-CONFIRM path salts the cut idem key
// with the CANCEL uuid ("cut:<auc>:<uuid>"), but a RESOLVE_CANCELLED_UNLOCK
// arrives with its OWN (worker-minted) uuid, so the cut cannot be found by a
// deterministic point key -- it is located by auction + role instead (mirrors
// AhLoadLiveBidRows). Returns the row's real idemKey for the release.
static bool AhFindLiveCutRow(uint32 auctionId, CustodyRow& out)
{
    std::vector<CustodyRow> rows;
    CustodyLedger::LoadNonTerminal(rows);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i].auctionId == auctionId && rows[i].kind == CUSTODY_GOLD &&
            rows[i].role == ROLE_PROCEEDS && rows[i].state == CST_RESERVED)
        {
            out = rows[i];
            return true;
        }
    }
    return false;
}

// [SP-2] Apply one worker-initiated resolution (spec 4.3). ALL per-kind value
// effects + the resolve:<uuid> applied-record commit in ONE checked txn, so
// DUPLICATE == APPLIED and a commit failure keeps the worker's row RESOLVING
// (RES_FAILED, never a book rollback -- decision 10). Reuses Task 11's
// from-facts value helpers verbatim. An unknown kind is an unrecoverable
// protocol fault -> AH_RESOLVE_NO_ACK (the pump sends no ack; never retried).
uint8 AhHandleResolveApply(ResolveApply const& ra)
{
    // Idempotency: DUPLICATE == APPLIED (spec 4.3-C1). Answer before mutating.
    if (CustodyService::ResolutionApplied(ra.uuid))
    {
        return uint8(RES_DUPLICATE);
    }

    MutationFacts const& f = ra.facts;
    CustodyDeferred def;
    // [F1] A RESOLVE_CANCELLED_UNLOCK release credits an ONLINE owner's wallet
    // immediately (in-memory ModifyMoney, non-transactional); only the ledger
    // flip is in the checked txn. Capture that credit so a commit failure can
    // undo it (X6) before RES_FAILED -- mirrors the five sibling finalize sites.
    Player* releasedCutOnline = NULL;
    uint32  releasedCutAmount = 0u;
    CharacterDatabase.BeginTransaction();

    switch (ra.kind)
    {
        case RESOLVE_WON:
        {
            // Consume the winning bid row + the seller deposit, pay the seller
            // (profit = effectiveBid + deposit - cut), deliver the item, and flip
            // the escrow item row terminal. curBidderGuid == 0 => bot win (the
            // destroy branch lives inside AhItemToWinnerFromFacts). The explicit
            // item-row flip mirrors the landed buyout-win path in AhFinalizeBidOk
            // (AhItemToWinnerFromFacts does NOT touch "item:<id>" itself).
            std::string const depKey  = "dep:" + std::to_string(f.auctionId);
            std::string const itemKey = "item:" + std::to_string(f.auctionId);
            CustodyRow bidRow;
            bool const haveBidRow = CustodyLedger::GetSingleLiveBidRow(f.auctionId, bidRow);
            // [F2] A REAL winner (curBidderGuid != 0) MUST have exactly one live
            // bid reservation. If it is missing/ambiguous, fail-closed (mirror
            // AhFinalizeBidOk): pay/deliver NOTHING and roll back, so the winner's
            // RESERVED bid never leaks and the resolution re-drives. A BOT win has
            // curBidderGuid == 0 and correctly holds NO bid row -> it proceeds.
            if (f.curBidderGuid != 0u && !haveBidRow)
            {
                CharacterDatabase.RollbackTransaction();
                sLog.outError("[AHMut] PROTOCOL FAULT: RESOLVE_WON winner bid row"
                              " missing/ambiguous for auction %u - RES_FAILED", f.auctionId);
                return uint8(RES_FAILED);
            }
            if (haveBidRow)
            {
                CustodyService::CommitGoldLedgerOnly(bidRow.idemKey);
            }
            CustodyService::CommitGoldLedgerOnly(depKey);
            AhSellerPayoutFromFacts(f, def);
            AhItemToWinnerFromFacts(f, def);
            CustodyService::CommitGoldLedgerOnly(itemKey);
            break;
        }
        case RESOLVE_EXPIRED_NOBID:
        {
            // No-bid expiry: the deposit is FORFEIT (legacy: not returned) and
            // the item goes back to the seller. AhReturnItemToSellerFromFacts
            // flips "item:<id>" terminal internally (DeliverItem / ledger-only).
            std::string const depKey = "dep:" + std::to_string(f.auctionId);
            CustodyService::CommitGoldLedgerOnly(depKey);
            AhReturnItemToSellerFromFacts(f, uint32(AUCTION_EXPIRED), def);
            break;
        }
        case RESOLVE_CANCELLED_UNLOCK:
        {
            // Worker timed out a cancel PREPARE and unlocked the row: release any
            // cut reservation we hold for this auction back to the seller. No
            // item/bid movement -- the auction persists.
            CustodyRow cutRow;
            if (AhFindLiveCutRow(f.auctionId, cutRow))
            {
                // [F3] Release to the guid actually debited at reserve time (the
                // reserved row's authoritative owner), NOT the worker-supplied
                // seller facts -- used for both the release owner and the lookup.
                Player* owner = sObjectMgr.GetPlayer(
                    ObjectGuid(HIGHGUID_PLAYER, cutRow.ownerGuid));
                CustodyService::ReleaseGoldToWallet(def, cutRow.ownerGuid, owner,
                                                    cutRow.amount, cutRow.idemKey);
                // [F1] An online owner was credited in-memory now; remember it so
                // a checked-commit failure can undo the credit (offline owners are
                // an in-txn UPDATE that rolls back on failure -- nothing to undo).
                if (owner)
                {
                    releasedCutOnline = owner;
                    releasedCutAmount = cutRow.amount;
                }
            }
            break;
        }
        case RESOLVE_REPAIR_RETURN:
        {
            if (f.curBidderGuid == 0u && f.priorBidderGuid != 0u)
            {
                // A bot displaced a real player bidder: refund the prior bidder;
                // the listing persists (non-terminal). The prior bid key is the
                // single live bid row for this auction.
                CustodyRow priorRow;
                if (CustodyLedger::GetSingleLiveBidRow(f.auctionId, priorRow))
                {
                    AhRefundPriorBidderFromFacts(f, priorRow.idemKey, def);
                }
            }
            else
            {
                // Item repair / no displaced player: deposit forfeit + item back.
                std::string const depKey = "dep:" + std::to_string(f.auctionId);
                CustodyService::CommitGoldLedgerOnly(depKey);
                AhReturnItemToSellerFromFacts(f, uint32(AUCTION_EXPIRED), def);
            }
            break;
        }
        default:
        {
            CharacterDatabase.RollbackTransaction();
            sLog.outError("[AHMut] AhHandleResolveApply unknown kind %u (auction %u)"
                          " - protocol fault, no ack", uint32(ra.kind), f.auctionId);
            return AH_RESOLVE_NO_ACK;
        }
    }

    // The applied-record commits atomically with the value effects.
    CustodyService::WriteResolutionApplied(f.auctionId, ra.uuid);

    // [SP-2 Task 15] crash seam: the applied-record is queued but NOT yet
    // committed. On restart the worker re-sends RESOLVE_APPLY and the
    // ResolutionApplied guard above must answer DUPLICATE == APPLIED. Inert on a
    // live realm.
    CustodyService::MaybeCrash("resolving-pre-apply");

    if (!CharacterDatabase.CommitTransactionChecked())
    {
        // Keep the worker's row RESOLVING: it re-sends on its cadence and we
        // DUPLICATE-answer once it finally applies (spec 4.3; never roll the
        // worker book back on a mangosd-side failure).
        // [F1] Undo the online owner's in-memory cut credit (the ledger flip +
        // any offline UPDATE already rolled back with the txn). Mirrors the X6
        // sibling undo: ModifyMoney only, since the DB write was transactional.
        if (releasedCutOnline && releasedCutAmount > 0u)
        {
            releasedCutOnline->ModifyMoney(-int32(releasedCutAmount));   // X6: undo the in-memory credit
        }
        sLog.outError("[AHMut] resolve kind %u (auction %u) commit failed - RES_FAILED",
                      uint32(ra.kind), f.auctionId);
        return uint8(RES_FAILED);
    }
#ifdef ENABLE_ELUNA
    // [SP-2 spec 5.8] fire Eluna OnRemove for worker-initiated resolutions that
    // terminally remove the listing from the book: won, no-bid expiry, and the
    // repair item-return branch. Cancel-unlock and the repair bidder-refund
    // sub-case keep the listing live, so they do not fire. Fired while the
    // escrow item is still cached, before def.run().
    if (ra.kind == RESOLVE_WON || ra.kind == RESOLVE_EXPIRED_NOBID ||
        (ra.kind == RESOLVE_REPAIR_RETURN &&
         !(f.curBidderGuid == 0u && f.priorBidderGuid != 0u)))
    {
        AhElunaOnRemoveFromFacts(f);
    }
#endif
    def.run();
    return uint8(RES_APPLIED);
}

// ---------------------------------------------------------------------------
// Reconcile-on-reconnect (spec 8). The worker journal (ah_worker_journal) is a
// Character-DB table shared with the worker; mangosd has no ServiceDatabase, so
// it reads the table directly with its own connection. Journal states mirror
// AhJournal::JournalState: COMMITTED=1, RESOLVING=2, APPLIED=3,
// CANCEL_PREPARED=4, INTENT_PENDING=5.
// ---------------------------------------------------------------------------

static int AhHexNibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

// One peeked ah_worker_journal row (state + kind + decoded facts).
struct AhJournalPeek
{
    uint8         state;
    uint8         kind;
    MutationFacts facts;
    bool          factsOk;
};

// [FIX C.2] Tri-state result of a journal peek. mangos `PQuery` returns NULL for
// BOTH "no rows" and "query could not be executed" (table missing / transient DB
// error), so a bare bool conflates a genuine row-absent (release the reservation)
// with a query failure (must NOT release -- the worker may have committed).
enum AhJournalRead
{
    AHJRN_FOUND        = 0,  ///< row present; @p out is populated
    AHJRN_ABSENT       = 1,  ///< row genuinely absent (table present) -> release
    AHJRN_QUERY_FAILED = 2   ///< table missing / DB error -> in-doubt, do NOT release
};

// Read one journal row by uuid directly from the shared Character DB. On a NULL
// row query, a `SHOW TABLES` probe distinguishes a genuine absent row (table
// present -> AHJRN_ABSENT, the "worker never committed" release signal, spec 8)
// from a query that could not run (table missing or transient DB error, both
// NULL -> AHJRN_QUERY_FAILED, which the caller leaves in-doubt rather than
// releasing a possibly-committed reservation). The facts BLOB is stored as ASCII
// hex (NUL-safe); decode it in place (mirrors AhJournal::HexDecode +
// MutationFacts::Decode).
static AhJournalRead AhReadWorkerJournal(uint64 uuid, AhJournalPeek& out)
{
    QueryResult* q = CharacterDatabase.PQuery(
        "SELECT `state`, `kind`, `facts` FROM `ah_worker_journal` WHERE `uuid` = %llu",
        static_cast<unsigned long long>(uuid));
    if (q == NULL)
    {
        // Row query returned NULL: probe whether the table exists at all. If the
        // probe finds the table, the row is genuinely absent (release). If the
        // probe ALSO returns NULL -- table missing OR the DB is unreachable (a
        // transient error hits both queries) -- the peek could not be executed;
        // report QUERY_FAILED so the caller keeps the pending in-doubt.
        QueryResult* probe = CharacterDatabase.Query("SHOW TABLES LIKE 'ah_worker_journal'");
        if (probe == NULL)
        {
            return AHJRN_QUERY_FAILED;
        }
        delete probe;
        return AHJRN_ABSENT;
    }
    Field* fld = q->Fetch();
    out.state = static_cast<uint8>(fld[0].GetUInt32());
    out.kind  = static_cast<uint8>(fld[1].GetUInt32());
    std::string const hex = fld[2].GetCppString();
    delete q;

    out.factsOk = false;
    if ((hex.size() % 2u) == 0u && !hex.empty())
    {
        std::string bin;
        bin.reserve(hex.size() / 2u);
        bool ok = true;
        for (size_t i = 0; i + 1u < hex.size(); i += 2u)
        {
            int const hi = AhHexNibble(hex[i]);
            int const lo = AhHexNibble(hex[i + 1u]);
            if (hi < 0 || lo < 0)
            {
                ok = false;
                break;
            }
            bin.push_back(static_cast<char>((hi << 4) | lo));
        }
        if (ok && !bin.empty())
        {
            ByteBuffer bb;
            bb.append(reinterpret_cast<uint8 const*>(bin.data()), bin.size());
            out.factsOk = out.facts.Decode(bb);
        }
    }
    return AHJRN_FOUND;
}

// [SP-2] Release whatever reservations a pending player mutation still holds
// back to the wallet (forward-only; the absent-journal case, spec 8). Mirrors
// AhFinalizeRejected's release core, but keyed off the pending (no worker
// facts). Accumulates the online in-memory credit into @p onlineCredit so the
// caller can undo it if its checked commit fails (X6).
static void AhReleasePendingReservations(PendingMutation const& pm,
                                         CustodyDeferred& def, uint32& onlineCredit)
{
    Player* online = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow));

    if (!pm.depKey.empty())
    {
        CustodyRow depRow;
        if (CustodyLedger::Get(pm.depKey, depRow) && depRow.state == CST_RESERVED)
        {
            CustodyService::ReleaseGoldToWallet(def, pm.playerGuidLow, online, depRow.amount, pm.depKey);
            if (online)
            {
                onlineCredit += depRow.amount;
            }
        }
    }
    if (!pm.reserveKey.empty())
    {
        CustodyRow rsvRow;
        if (CustodyLedger::Get(pm.reserveKey, rsvRow) && rsvRow.state == CST_RESERVED)
        {
            CustodyService::ReleaseGoldToWallet(def, pm.playerGuidLow, online, rsvRow.amount, pm.reserveKey);
            if (online)
            {
                onlineCredit += rsvRow.amount;
            }
        }
    }
    if (!pm.itemKey.empty())
    {
        CustodyRow itemRow;
        if (CustodyLedger::Get(pm.itemKey, itemRow) && itemRow.state == CST_RESERVED)
        {
            Item* pItem = sAuctionMgr.GetAItem(itemRow.itemGuid);
            CustodyLedger::SetState(pm.itemKey, CST_TERMINAL_BACK, static_cast<uint64>(time(NULL)));
            if (pItem)
            {
                uint32 const savedItemGuidLow = itemRow.itemGuid;
                def.effects.push_back([savedItemGuidLow]()
                {
                    sAuctionMgr.RemoveAItem(savedItemGuidLow);
                });
                MailDraft ret(AhMailSubject(pItem->GetEntry(), pItem->GetItemRandomPropertyId(), AUCTION_CANCELED), "");
                ret.AddItem(pItem);
                ret.SendMailToInTransaction(MailReceiver(online, ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow)),
                                            MailSender(MAIL_AUCTION, 0u, MAIL_STATIONERY_AUCTION),
                                            def, MAIL_CHECK_MASK_COPIED);
            }
            else
            {
                sLog.outError("[AHMut] reconcile release: escrow item %u missing for uuid " UI64FMTD
                              "; ledger-only return", itemRow.itemGuid, pm.uuid);
            }
        }
    }
}

// [SP-2] Absent-journal (or anomalous-state) disposition: release the pending's
// reservations, write the applied-record, and consume the slot. One checked txn.
static void AhReconcileReleaseAndConsume(PendingMutation const& pm)
{
    CustodyDeferred def;
    uint32 onlineCredit = 0u;
    CharacterDatabase.BeginTransaction();
    AhReleasePendingReservations(pm, def, onlineCredit);
    CustodyService::WriteResolutionApplied(pm.auctionId, pm.uuid);
    if (CharacterDatabase.CommitTransactionChecked())
    {
        def.run();
    }
    else
    {
        if (onlineCredit > 0u)
        {
            Player* p = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow));
            if (p)
            {
                p->ModifyMoney(-int32(onlineCredit));   // X6: undo the in-memory credit
            }
        }
        sLog.outError("[AHMut] reconcile release commit FAILED for uuid " UI64FMTD, pm.uuid);
    }
    PendingMutation consumed;
    sWorld.GetMutationPending().Take(pm.uuid, consumed);
}

// [SP-2] COMMITTED/APPLIED journal disposition: the worker committed the book
// but the IPC_PLAYER_RESULT frame was lost. Re-drive the value finalize from
// the journal facts. AhHandlePlayerMutationResult is fail-closed against the
// custody ledger (a reserve row already flipped terminal by an earlier finalize
// makes the cross-check refuse), so a partially/fully-applied finalize re-driven
// here can never double-move value or double-mail; it also consumes the pending.
static void AhResolveForwardFromJournal(PendingMutation const& pm, AhJournalPeek const& jp)
{
    if (!jp.factsOk)
    {
        sLog.outError("[AHMut] reconcile uuid " UI64FMTD ": journal committed but facts"
                      " undecodable; releasing reservation instead", pm.uuid);
        AhReconcileReleaseAndConsume(pm);
        return;
    }
    PlayerMutationResult res;
    res.uuid   = pm.uuid;
    res.op     = jp.kind;               // journal kind == originating opcode low byte
    res.status = uint8(MUT_OK);
    res.reason = 0;
    res.facts  = jp.facts;
    AhHandlePlayerMutationResult(res);  // Takes the pending + fail-closed finalize
}

// [SP-2] CANCEL_PREPARED journal disposition (spec 8): a cancel PREPARE lock we
// hold with no CONFIRM -> release any cut reservation + tell the worker to ABORT
// (unlock the book row), then consume the slot. Mirrors AhFinalizeStale + the
// AhHandleCancelPrepared abort frame.
static void AhAbortAndRelease(PendingMutation const& pm)
{
    if (!pm.reserveKey.empty())
    {
        CustodyRow cutRow;
        if (CustodyLedger::Get(pm.reserveKey, cutRow) && cutRow.state == CST_RESERVED)
        {
            Player* online = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, pm.playerGuidLow));
            CustodyDeferred def;
            CharacterDatabase.BeginTransaction();
            CustodyService::ReleaseGoldToWallet(def, pm.playerGuidLow, online, cutRow.amount, pm.reserveKey);
            if (CharacterDatabase.CommitTransactionChecked())
            {
                def.run();
            }
            else
            {
                if (online)
                {
                    online->ModifyMoney(-int32(cutRow.amount));   // X6: undo in-memory credit
                }
                sLog.outError("[AHMut] reconcile cut release commit FAILED for uuid " UI64FMTD, pm.uuid);
            }
        }
    }

    WorkerSupervisor* const sv = sWorld.GetAhSupervisor();
    if (sv != NULL && sv->ServiceActive())
    {
        PlayerCancelDecide d;
        d.uuid      = pm.uuid;
        d.auctionId = pm.auctionId;
        IpcMessage m;
        m.op = IPC_PLAYER_CANCEL_ABORT;
        d.Encode(m.body);
        sv->Channel().SendFrame(m);
    }

    PendingMutation consumed;
    sWorld.GetMutationPending().Take(pm.uuid, consumed);
}

// [SP-2] Walk every in-flight pending against the shared worker journal on the
// service-just-became-active edge (spec 8). Per uuid: COMMITTED/APPLIED =>
// finalize-forward; CANCEL_PREPARED => abort + release; absent (or an anomalous
// present state) => release the reservation. Each disposition consumes its slot.
void AhReconcileOnReconnect()
{
    MutationPendingMap& pend = sWorld.GetMutationPending();
    std::vector<PendingMutation> inflight;
    pend.SnapshotInflight(inflight);
    if (inflight.empty())
    {
        return;
    }
    sLog.outString("[AHMut] reconcile-on-reconnect: %u in-flight pending mutation(s)",
                   uint32(inflight.size()));

    for (size_t i = 0; i < inflight.size(); ++i)
    {
        PendingMutation const& pm = inflight[i];
        AhJournalPeek jp;
        AhJournalRead const rd = AhReadWorkerJournal(pm.uuid, jp);

        // [FIX C.2] The journal peek could not be executed (table missing or a
        // transient DB error): do NOT release -- the worker may have committed
        // this mutation. Leave the pending in-doubt (tombstone, reservation held)
        // so a later reconcile or an operator resolves it. Consumes no slot.
        if (rd == AHJRN_QUERY_FAILED)
        {
            sLog.outError("[AHMut] reconcile uuid " UI64FMTD ": journal peek FAILED"
                          " (table missing / DB error); holding reservation in-doubt",
                          pm.uuid);
            sWorld.GetMutationPending().Tombstone(pm.uuid);
            continue;
        }

        if (rd == AHJRN_FOUND)
        {
            // COMMITTED(1) / APPLIED(3): the worker committed the book -> value forward.
            if (jp.state == 1u || jp.state == 3u)
            {
                AhResolveForwardFromJournal(pm, jp);
                continue;
            }
            // CANCEL_PREPARED(4): a stuck cancel lock -> abort + release the cut.
            if (jp.state == 4u)
            {
                AhAbortAndRelease(pm);
                continue;
            }
            // Any other present state for a mangosd pending is anomalous (the
            // mangosd/worker uuid spaces are disjoint) -> release, never leak gold.
            sLog.outError("[AHMut] reconcile uuid " UI64FMTD ": unexpected journal state %u;"
                          " releasing reservation", pm.uuid, uint32(jp.state));
        }
        // Row genuinely absent (AHJRN_ABSENT), or an anomalous present state: the
        // worker never committed -> release the reservation (forward-only, spec 8).
        AhReconcileReleaseAndConsume(pm);
    }
}

/** @} */
