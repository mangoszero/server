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
 */

#include "AuctionIntentExecutor.h"

#include "AuctionIntents.h"
#include "AuctionHouseMgr.h"
#include "AuctionHouseBot.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "DBCStores.h"
#include "Config/Config.h"
#include "Log.h"
#include "World.h"

/// Sentinel meaning the executor produced no reply frame for the caller.
static const IpcOpcode IPC_OP_NONE = IpcOpcode(0);

/**
 * @brief Build an IPC_INTENT_RESULT frame into @p out.
 *
 * @param out    Destination frame (op + encoded body).
 * @param uuid   Echoed intent uuid.
 * @param status IntentStatus value.
 * @param reason IntentReason value.
 */
static void MakeResult(IpcMessage& out, uint64 uuid, uint8 status,
                       uint8 reason)
{
    IntentResult res;
    res.uuid = uuid;
    res.status = status;
    res.reason = reason;

    out.op = IPC_INTENT_RESULT;
    out.body.clear();
    res.Encode(out.body);
}

/**
 * @brief Resolve the AuctionHouseEntry for a wire house index [0,2].
 *
 * Reuses the exact mapping used by the in-process seller
 * (AuctionBotSeller::addNewAuctions): house type -> houseId ->
 * sAuctionHouseStore.LookupEntry().
 *
 * @param house Wire house index (0=Alliance,1=Horde,2=Neutral).
 * @return The matching AuctionHouseEntry, or NULL if @p house is invalid.
 */
static AuctionHouseEntry const* ResolveHouseEntry(uint8 house)
{
    uint32 houseId;
    switch (house)
    {
        case AUCTION_HOUSE_ALLIANCE: houseId = 1; break;
        case AUCTION_HOUSE_HORDE:    houseId = 6; break;
        case AUCTION_HOUSE_NEUTRAL:  houseId = 7; break;
        default:                     return NULL;
    }
    return sAuctionHouseStore.LookupEntry(houseId);
}

uint32 AuctionIntentExecutor::IntentTtlSec() const
{
    // Read on every call; Config access is a cheap map lookup and this keeps
    // the executor stateless w.r.t. config reloads.
    return sConfig.GetIntDefault("AH.Service.IntentTtlSec", 900);
}

void AuctionIntentExecutor::PurgeExpiredUuids(uint32 now)
{
    for (std::unordered_map<uint64, uint32>::iterator itr =
             m_appliedUuid.begin(); itr != m_appliedUuid.end();)
    {
        if (itr->second <= now)
        {
            itr = m_appliedUuid.erase(itr);
        }
        else
        {
            ++itr;
        }
    }
}

void AuctionIntentExecutor::Apply(const IpcMessage& in, IpcMessage& resultOut)
{
    resultOut.op = IPC_OP_NONE;
    resultOut.body.clear();

    const uint32 now = uint32(sWorld.GetGameTime());

    switch (in.op)
    {
        case IPC_INTENT_SELL:
        {
            ApplySell(in, resultOut, now);
            break;
        }
        case IPC_INTENT_BID:
        {
            ApplyBid(in, resultOut, now);
            break;
        }
        case IPC_INTENT_BUYOUT:
        {
            ApplyBuyout(in, resultOut, now);
            break;
        }
        case IPC_INTENT_RESULT:
        {
            // mangosd -> child direction; must never arrive here.
            sLog.outError("[AHExecutor] unexpected IPC_INTENT_RESULT inbound;"
                          " ignoring");
            break;
        }
        default:
        {
            sLog.outError("[AHExecutor] unroutable opcode 0x%04X; ignoring",
                          static_cast<unsigned>(in.op));
            break;
        }
    }
}

void AuctionIntentExecutor::ApplySell(const IpcMessage& in,
                                      IpcMessage& resultOut, uint32 now)
{
    // Decode a private copy: ByteBuffer read advances rpos, so never mutate
    // the caller's frame.
    ByteBuffer body(in.body);
    SellIntent s;
    if (!s.Decode(body))
    {
        ++m_malformed;
        sLog.outError("[AHExecutor] malformed SellIntent (body=%u bytes)",
                      static_cast<unsigned>(in.body.size()));
        return; // no reply: caller leaves resultOut empty
    }

    // Idempotency FIRST: a redelivered uuid must never double-apply.
    if (IsDuplicate(s.uuid))
    {
        ++m_duplicate;
        MakeResult(resultOut, s.uuid, INTENT_DUPLICATE, REASON_NONE);
        return;
    }

    // botGuid guard: the listing owner must be the real bot low-GUID.
    if (s.botGuid != sAuctionBotConfig.GetAHBotId())
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_GUID_MISMATCH);
        return;
    }

    // Re-validate house against live DBC store.
    AuctionHouseEntry const* ahEntry = ResolveHouseEntry(s.house);
    if (!ahEntry)
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // Re-validate the item template against live data.
    if (!ObjectMgr::GetItemPrototype(s.itemId))
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // Materialise the listing item. CreateItem also clamps stack to the
    // prototype max and returns NULL for a bad item/zero count.
    Item* it = Item::CreateItem(s.itemId, s.stack);
    if (!it)
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // From here on, AddAuctionByGuid takes ownership of `it` on the success
    // path (it registers the item in sAuctionMgr and persists it). There is
    // no remaining reject branch below, so no `delete it` is required after
    // this point: every code path hands `it` to AddAuctionByGuid.
    AuctionHouseObject* house =
        sAuctionMgr.GetAuctionsMap(AuctionHouseType(s.house));
    house->AddAuctionByGuid(ahEntry, it, s.durationHrs * HOUR,
                            s.bid, s.buyout, s.botGuid);

    ++m_applied;
    Remember(s.uuid, now);
    MakeResult(resultOut, s.uuid, INTENT_OK, REASON_NONE);
}

void AuctionIntentExecutor::ApplyBid(const IpcMessage& in,
                                     IpcMessage& resultOut, uint32 now)
{
    ByteBuffer body(in.body);
    BidIntent b;
    if (!b.Decode(body))
    {
        ++m_malformed;
        sLog.outError("[AHExecutor] malformed BidIntent (body=%u bytes)",
                      static_cast<unsigned>(in.body.size()));
        return;
    }

    if (IsDuplicate(b.uuid))
    {
        ++m_duplicate;
        MakeResult(resultOut, b.uuid, INTENT_DUPLICATE, REASON_NONE);
        return;
    }

    if (b.botGuid != sAuctionBotConfig.GetAHBotId())
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_GUID_MISMATCH);
        return;
    }

    // The child does not stamp a house on bid/buyout intents, so scan all
    // three houses for the live auction id (the source buyer looks up by id
    // within a known house via GetAuction(); here we locate the house first).
    AuctionEntry* auction = NULL;
    for (int h = 0; h < MAX_AUCTION_HOUSE_TYPE && !auction; ++h)
    {
        auction =
            sAuctionMgr.GetAuctionsMap(AuctionHouseType(h))->GetAuction(
                b.auctionId);
    }

    if (!auction)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_NOT_FOUND);
        return;
    }

    // Re-validate against the LIVE auction (the child snapshot is stale).
    if (auction->expireTime <= time_t(now))
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_EXPIRED);
        return;
    }

    // Never bid on the bot's own listing.
    if (auction->owner == sAuctionBotConfig.GetAHBotId())
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_GUID_MISMATCH);
        return;
    }

    // A non-zero buyout already at/over the bid means the listing is, in
    // effect, sittable; reject a "bid" that meets or exceeds buyout so the
    // child re-issues it as an explicit BUYOUT instead of silently buying.
    if (auction->buyout != 0 && b.bidAmount >= auction->buyout)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_BOUGHT_OUT);
        return;
    }

    // Enforce the same minimum increment the in-process buyer respects: the
    // new bid must clear the current bid plus GetAuctionOutBid().
    uint32 minBid = auction->bid + auction->GetAuctionOutBid();
    if (b.bidAmount < minBid)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_STALE_BID);
        return;
    }

    // Apply. With newbidder == NULL the bot pays nothing (no ModifyMoney);
    // UpdateBid returns true for a normal bid. It can only return false if
    // newbid reaches buyout, which we excluded above, so for a pure bid this
    // is the OK path regardless of return value.
    auction->UpdateBid(b.bidAmount, NULL);

    ++m_applied;
    Remember(b.uuid, now);
    MakeResult(resultOut, b.uuid, INTENT_OK, REASON_NONE);
}

void AuctionIntentExecutor::ApplyBuyout(const IpcMessage& in,
                                        IpcMessage& resultOut, uint32 now)
{
    ByteBuffer body(in.body);
    BuyoutIntent b;
    if (!b.Decode(body))
    {
        ++m_malformed;
        sLog.outError("[AHExecutor] malformed BuyoutIntent (body=%u bytes)",
                      static_cast<unsigned>(in.body.size()));
        return;
    }

    if (IsDuplicate(b.uuid))
    {
        ++m_duplicate;
        MakeResult(resultOut, b.uuid, INTENT_DUPLICATE, REASON_NONE);
        return;
    }

    if (b.botGuid != sAuctionBotConfig.GetAHBotId())
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_GUID_MISMATCH);
        return;
    }

    AuctionEntry* auction = NULL;
    for (int h = 0; h < MAX_AUCTION_HOUSE_TYPE && !auction; ++h)
    {
        auction =
            sAuctionMgr.GetAuctionsMap(AuctionHouseType(h))->GetAuction(
                b.auctionId);
    }

    if (!auction)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_NOT_FOUND);
        return;
    }

    if (auction->expireTime <= time_t(now))
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_EXPIRED);
        return;
    }

    // A buyout of 0 means the listing is bid-only; it cannot be bought out.
    if (auction->buyout == 0)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_BOUGHT_OUT);
        return;
    }

    // Never buy out the bot's own listing.
    if (auction->owner == sAuctionBotConfig.GetAHBotId())
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_GUID_MISMATCH);
        return;
    }

    // Drive the buyout/AuctionBidWinning path. With newbidder == NULL the bot
    // pays nothing; UpdateBid returns false here (buyout reached) and deletes
    // the auction internally -- false is the SUCCESS path for buyout, so we
    // must NOT touch `auction` afterwards.
    auction->UpdateBid(auction->buyout, NULL);

    ++m_applied;
    Remember(b.uuid, now);
    MakeResult(resultOut, b.uuid, INTENT_OK, REASON_NONE);
}
