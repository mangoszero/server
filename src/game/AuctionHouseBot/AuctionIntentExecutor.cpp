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
#include "ItemPrototype.h"
#include "Item.h"
#include "DBCStores.h"
#include "Config/Config.h"
#include "Log.h"
#include "World.h"

#include <algorithm>  // std::max

/// Sentinel meaning the executor produced no reply frame for the caller.
static const IpcOpcode IPC_OP_NONE = IpcOpcode(0);

/**
 * @brief Upper-bound multiple for a competitive bid, relative to the
 *        auction's own live values.
 *
 * The child only ADVISES; mangosd is the authority and must never trust the
 * child's bid amount as an absolute. A stale / corrupt / garbage
 * @c bidAmount (up to ~UINT32_MAX) must not flow into UpdateBid(), because on
 * a bid-only listing (buyout == 0) that would set bid=huge / bidder=0 and, at
 * expiry, AuctionBidWinning(NULL) would credit the REAL seller phantom gold
 * (capped only by the player money ceiling, ~214748 g) AND destroy the item
 * via the receiver-not-exist branch.
 *
 * A legitimate competitive bot bid is at most a few outbid increments over the
 * current bid (the in-process buyer raises by GetAuctionOutBid()), never a
 * 100x jump over the auction's own live floor. We therefore reject any bid
 * exceeding @c max(startbid, live bid, 1) * AH_BID_SANITY_MULTIPLE. 100x is
 * generous enough to never reject a real bid yet rejects every absurd/garbage
 * amount: e.g. a 1g floor caps the accepted bid at 100g, far below the ~214k g
 * phantom-credit a UINT32-range amount would inject.
 */
static const uint32 AH_BID_SANITY_MULTIPLE = 100u;

/**
 * @brief Single-item buyability boost, mirrored from the in-process buyer.
 *
 * AuctionHouseBot.cpp (addNewAuctionBuyerBotBid): when only ONE auction of an
 * item exists, the buyer multiplies its MaxBuyablePrice by 5
 * (@c MaxBuyablePrice = MaxBuyablePrice * 5). Because the executor cannot know
 * how many copies of the item are currently listed (and the hostile child must
 * not be trusted to tell us), the ceiling assumes the most permissive case:
 * the single-item boost always applies. This only widens the cap, so it never
 * false-rejects a legitimate buy.
 */
static const uint64 AH_VALUE_SINGLE_ITEM_BOOST = 5u;

/**
 * @brief Upper price/MaxBuyablePrice ratio at which the buyer still buys.
 *
 * AuctionHouseBot.cpp (IsBuyableEntry): once the per-item price exceeds
 * MaxBuyablePrice the buy chance decays linearly while @c ratio < 10, and only
 * collapses to "never" (Chance = 0) at @c ratio >= 10. So the in-process buyer
 * can, with low probability, still buy at up to 10x its MaxBuyablePrice. To
 * never reject a buy the in-process bot itself could legitimately make, the
 * authoritative ceiling allows the full @c ratio < 10 band.
 */
static const uint64 AH_VALUE_RATIO_CEILING = 10u;

/**
 * @brief Compute the authoritative spend ceiling for an auctioned item.
 *
 * This reproduces the in-process buyer's own valuation (AuctionHouseBot.cpp
 * addNewAuctionBuyerBotBid / IsBuyableEntry) so the cap matches what the
 * in-process bot would actually be willing to pay -- independent of the
 * PLAYER-controlled startbid / buyout on the live auction. The ceiling is the
 * total (whole-stack) gold the bot could ever spend on this listing:
 *
 *   BasePrice        = (BuyPrice.Buyer ? proto->BuyPrice : proto->SellPrice)
 *                      * itemCount
 *   MaxBuyablePrice  = BasePrice * BuyerPriceRatio / 100
 *   ceiling          = MaxBuyablePrice
 *                      * AH_VALUE_SINGLE_ITEM_BOOST     (single-copy boost)
 *                      * AH_VALUE_RATIO_CEILING         (ratio<10 buy band)
 *
 * @c BuyerPriceRatio is the house price ratio + 50 (LoadBuyerValues). The
 * executor does not stamp a house on bid/buyout intents and scans all three
 * houses for the auction, so the MAX of the three configured house price
 * ratios is used: that yields the most permissive (largest) ceiling and so can
 * never false-reject a legitimate buy in whichever house the auction lives.
 *
 * All arithmetic is 64-bit so the chained multiplies cannot wrap (proto prices
 * and itemCount are uint32; the product stays well within uint64).
 *
 * @param proto     Item prototype of the live auction's item.
 * @param itemCount Stack count of the live auction (auction->itemCount).
 * @return Maximum total copper the bot may spend on the listing. 0 only if
 *         both intrinsic prices are 0 (vendor-trash with no value), which the
 *         callers treat as "reject any non-trivial bid/buyout".
 */
static uint64 ComputeBotValueCeiling(ItemPrototype const* proto,
                                     uint32 itemCount)
{
    const bool useBuyPrice =
        sAuctionBotConfig.getConfig(CONFIG_BOOL_AHBOT_BUYPRICE_BUYER);
    const uint64 unitPrice = useBuyPrice ? proto->BuyPrice : proto->SellPrice;

    const uint64 count = itemCount ? uint64(itemCount) : 1u;
    const uint64 basePrice = unitPrice * count;

    // BuyerPriceRatio = (house price ratio) + 50; use the most permissive
    // (largest) of the three houses since bid/buyout intents are house-blind.
    uint32 priceRatio = sAuctionBotConfig.getConfig(
        CONFIG_UINT32_AHBOT_ALLIANCE_PRICE_RATIO);
    priceRatio = std::max<uint32>(priceRatio,
        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_HORDE_PRICE_RATIO));
    priceRatio = std::max<uint32>(priceRatio,
        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_NEUTRAL_PRICE_RATIO));
    const uint64 buyerPriceRatio = uint64(priceRatio) + 50u;

    const uint64 maxBuyablePrice = (basePrice * buyerPriceRatio) / 100u;

    return maxBuyablePrice * AH_VALUE_SINGLE_ITEM_BOOST
           * AH_VALUE_RATIO_CEILING;
}

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
    //
    // Clamp to [60, 86400] so the dedup window is always a meaningful positive
    // duration. Zero gives same-tick expiry (dedup never holds -> double-apply
    // on redelivery); negative values wrap to a huge uint32 (never expire).
    static const uint32 TTL_MIN = 60u;
    static const uint32 TTL_MAX = 86400u;

    const int32 raw = sConfig.GetIntDefault("AH.Service.IntentTtlSec", 900);
    uint32 ttl;
    if (raw < static_cast<int32>(TTL_MIN))
    {
        sLog.outError("[AHExecutor] AH.Service.IntentTtlSec=%d is below "
                      "minimum (%u); clamping to %u", raw, TTL_MIN, TTL_MIN);
        ttl = TTL_MIN;
    }
    else if (static_cast<uint32>(raw) > TTL_MAX)
    {
        sLog.outError("[AHExecutor] AH.Service.IntentTtlSec=%d exceeds "
                      "maximum (%u); clamping to %u", raw, TTL_MAX, TTL_MAX);
        ttl = TTL_MAX;
    }
    else
    {
        ttl = static_cast<uint32>(raw);
    }
    return ttl;
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

    // Idempotency FIRST: a redelivered uuid must never double-apply. Refresh
    // the dedup expiry on every hit so an actively-redelivering uuid (whose
    // INTENT_RESULT reply was lost) keeps its sliding window open and never
    // ages out into a second apply.
    if (IsDuplicate(s.uuid))
    {
        ++m_duplicate;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_DUPLICATE, REASON_NONE);
        return;
    }

    // botGuid guard: the listing owner must be the real bot low-GUID. When
    // GetAHBotId() returns 0 (unresolvable bot name) a botGuid-0 intent would
    // otherwise pass the bare mismatch test (0 != 0 is false) and mutate real
    // players' auctions as owner 0; reject outright in that misconfig case.
    if (sAuctionBotConfig.GetAHBotId() == 0 ||
        s.botGuid != sAuctionBotConfig.GetAHBotId())
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

    // Reject a malformed listing the child should never have sent:
    //   - stack == 0: CreateItem would fail anyway, but reject explicitly so
    //     the outcome is an honest REJECTED rather than a silent BAD_ITEM.
    //   - buyout != 0 && bid > buyout: a "dead" unbuyable listing (the start
    //     bid already exceeds the instant-buy price, so it can never sell).
    // mangosd is the authority; do not post a listing the wire claims is sane
    // but is internally contradictory.
    if (s.stack == 0 || (s.buyout != 0 && s.bid > s.buyout))
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // Re-validate the item template against live data.
    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(s.itemId);
    if (!proto)
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // SELL POLICY (authority-side, mirrors the in-process seller so a hostile
    // child cannot mint out-of-policy listings the bot would never create).
    // The in-process seller (AuctionBotSeller::addNewAuctions) only ever posts
    // listings that satisfy ALL of the following; we enforce the same bounds:
    //
    //   1. duration in [MinTime, MaxTime] hours
    //        (urand(GetMinTime(), GetMaxTime()) * HOUR). Rejects zero / absurd
    //        / huge durations.
    //   2. stack in [1, proto->GetMaxStackSize()]
    //        (urand(1, GetMaxStackSize())). Rejects 0 / over-stack.
    //   3. buyout within the item's intrinsic value ceiling
    //        (reuses the A1 ComputeBotValueCeiling so an absurd buyout vs the
    //        item's vendor value is rejected). bid <= buyout and stack > 0 are
    //        already enforced above.
    //   4. item quality in the bot's configured auction-quality range, and a
    //        quality the seller is configured to list at all
    //        (getConfigItemQualityAmount > 0). Rejects qualities the bot would
    //        never auction.
    //
    // NOTE (Phase 2): the strong form is a full host-rebuilt pool allowlist
    // (the exact item-id set the seller would pick from its m_ItemPool, gated
    // by the Items.* / Class.* / item-level / req-level filters). That is
    // heavy; this policy enforces the value + duration + stack + quality
    // envelope now and defers the per-item-id allowlist.

    // (1) Duration must be within the seller's configured listing window.
    const uint32 minHrs =
        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_MINTIME);
    const uint32 maxHrs =
        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_MAXTIME);
    // Mirror AHB_Seller_Config::GetMinTime(): clamp the effective floor to at
    // least 1 hour, and never above the configured max.
    uint32 effMinHrs = minHrs < 1u ? 1u : minHrs;
    if (maxHrs != 0 && effMinHrs > maxHrs)
    {
        effMinHrs = maxHrs;
    }
    if (s.durationHrs < effMinHrs || s.durationHrs > maxHrs)
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // (2) Stack must be within [1, prototype max]. stack == 0 already rejected.
    if (s.stack > proto->GetMaxStackSize())
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // (3) Buyout must be within the item's intrinsic value ceiling. A SELL with
    // buyout == 0 is bid-only and legitimate, so only cap a non-zero buyout.
    if (s.buyout != 0)
    {
        const uint64 valueCeiling = ComputeBotValueCeiling(proto, s.stack);
        if (uint64(s.buyout) > valueCeiling)
        {
            ++m_rejected;
            Remember(s.uuid, now);
            MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
            return;
        }
    }

    // (4) Quality must be within the bot's auction-quality range AND be a
    // quality the seller is configured to list (amount > 0). proto->Quality
    // indexes AuctionQuality 1:1 (AUCTION_QUALITY_* == ITEM_QUALITY_*).
    if (proto->Quality >= MAX_AUCTION_QUALITY ||
        sAuctionBotConfig.getConfigItemQualityAmount(
            AuctionQuality(proto->Quality)) == 0)
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

    // Refresh the dedup expiry on every duplicate hit (sliding window) so a
    // redelivered uuid never ages out into a second UpdateBid.
    if (IsDuplicate(b.uuid))
    {
        ++m_duplicate;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_DUPLICATE, REASON_NONE);
        return;
    }

    // Reject when the bot GUID is unresolvable (GetAHBotId()==0) as well as on
    // a genuine mismatch, so a botGuid-0 intent can never bid as bidder 0.
    if (sAuctionBotConfig.GetAHBotId() == 0 ||
        b.botGuid != sAuctionBotConfig.GetAHBotId())
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

    // Enforce the startbid floor the canonical authority requires
    // (AuctionHouseHandler.cpp: "if (price < auction->startbid) return;").
    // The min-increment check below clamps to 1 on a fresh auction (bid==0),
    // so without this a 1-copper first bid would be accepted; UpdateBid would
    // then set bid=1/bidder=0 and at win-time AuctionBidWinning would pay the
    // real seller ~1c and destroy the item via the receiver-not-exist branch.
    if (b.bidAmount < auction->startbid)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_STALE_BID);
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

    // UPPER bound (anti gold-injection): mangosd is the authority and must not
    // trust the child's bid amount. Without an upper clamp a stale / corrupt /
    // huge bidAmount flows unclamped into UpdateBid() on a bid-only listing
    // (buyout == 0, so the buyout-reached reject above never fires), setting a
    // huge live bid with bidder=0; at expiry the real seller is paid phantom
    // gold and the item is destroyed. Reject anything above the auction's own
    // live floor times AH_BID_SANITY_MULTIPLE. Computed in 64-bit so the
    // multiply cannot wrap. (Bids at/above a non-zero buyout are already
    // rejected as REASON_BOUGHT_OUT above, so the buyout itself is the cap for
    // buyout-able listings; this clamp is the guard for bid-only listings.)
    const uint64 floor =
        std::max<uint64>(std::max<uint32>(auction->startbid, auction->bid), 1u);
    const uint64 sanityCap = floor * AH_BID_SANITY_MULTIPLE;
    if (uint64(b.bidAmount) > sanityCap)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_STALE_BID);
        return;
    }

    // AUTHORITATIVE VALUE CEILING (anti gold-injection): the floor above is
    // derived from the PLAYER-controlled startbid/bid, so an accomplice seller
    // can inflate it to lift the AH_BID_SANITY_MULTIPLE cap arbitrarily. Cap
    // the bid by the item's INTRINSIC value instead -- the same MaxBuyablePrice
    // band the in-process buyer would itself pay (see ComputeBotValueCeiling).
    // A guid-0 / missing prototype is a reject: never bid on an item we cannot
    // value.
    ItemPrototype const* bidProto =
        ObjectMgr::GetItemPrototype(auction->itemTemplate);
    if (!bidProto)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    const uint64 valueCeiling =
        ComputeBotValueCeiling(bidProto, auction->itemCount);
    if (uint64(b.bidAmount) > valueCeiling)
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

    // Refresh the dedup expiry on every duplicate hit (sliding window) so a
    // redelivered uuid never ages out into a second buyout.
    if (IsDuplicate(b.uuid))
    {
        ++m_duplicate;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_DUPLICATE, REASON_NONE);
        return;
    }

    // Reject when the bot GUID is unresolvable (GetAHBotId()==0) as well as on
    // a genuine mismatch, so a botGuid-0 intent can never buy out as buyer 0.
    if (sAuctionBotConfig.GetAHBotId() == 0 ||
        b.botGuid != sAuctionBotConfig.GetAHBotId())
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

    // AUTHORITATIVE VALUE CEILING (anti gold-injection): the buyout currently
    // flows UNCAPPED into UpdateBid(auction->buyout, NULL), which pays the real
    // seller auction->buyout while debiting the bot nothing. auction->buyout is
    // PLAYER-controlled, so an accomplice seller can list vendor-trash at an
    // arbitrary buyout to inject gold. Cap the buyout by the item's intrinsic
    // value -- the same MaxBuyablePrice band the in-process buyer would itself
    // pay (see ComputeBotValueCeiling). A guid-0 / missing prototype is a
    // reject: never buy out an item we cannot value.
    ItemPrototype const* buyProto =
        ObjectMgr::GetItemPrototype(auction->itemTemplate);
    if (!buyProto)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    const uint64 valueCeiling =
        ComputeBotValueCeiling(buyProto, auction->itemCount);
    if (uint64(auction->buyout) > valueCeiling)
    {
        ++m_rejected;
        Remember(b.uuid, now);
        MakeResult(resultOut, b.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
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
