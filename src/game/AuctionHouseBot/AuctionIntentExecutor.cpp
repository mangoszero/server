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
#include "CustodyLedger.h"
#include "ObjectMgr.h"
#include "ItemPrototype.h"
#include "Item.h"
#include "DBCStores.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "World.h"

#include <ctime>    // time

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
    res.itemGuid = 0u;    // [SP-2] no materialization on this legacy reply path
    res.auctionId = 0u;   // [SP-2] (Task 13 owns the materialization producer)

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

void AuctionIntentExecutor::RefreshTtl()
{
    // Read + clamp ONCE here (lazily on first use, then once/second from
    // PurgeExpiredUuids) instead of on the per-intent hot path. A hostile child
    // cannot make this run per intent, and a live config reload is still picked
    // up within ~1 second.
    //
    // Clamp to [60, 86400] so the dedup window is always a meaningful positive
    // duration. Zero would give same-tick expiry (dedup never holds ->
    // double-apply on redelivery); negative values wrap to a huge uint32
    // (never expire).
    static const uint32 TTL_MIN = 60u;
    static const uint32 TTL_MAX = 86400u;

    const int32 raw = sConfig.GetIntDefault("AH.Service.IntentTtlSec", 900);
    uint32 ttl;
    if (raw < static_cast<int32>(TTL_MIN))
    {
        ttl = TTL_MIN;
    }
    else if (static_cast<uint32>(raw) > TTL_MAX)
    {
        ttl = TTL_MAX;
    }
    else
    {
        ttl = static_cast<uint32>(raw);
    }

    // Log a clamp only when the effective TTL actually changes, so a misconfig
    // emits at most one line per change (not once per intent / per purge).
    if (ttl != m_ttlSec)
    {
        if (raw < static_cast<int32>(TTL_MIN))
        {
            sLog.outError("[AHExecutor] AH.Service.IntentTtlSec=%d is below "
                          "minimum (%u); clamping to %u",
                          raw, TTL_MIN, TTL_MIN);
        }
        else if (static_cast<uint32>(raw) > TTL_MAX)
        {
            sLog.outError("[AHExecutor] AH.Service.IntentTtlSec=%d exceeds "
                          "maximum (%u); clamping to %u",
                          raw, TTL_MAX, TTL_MAX);
        }
    }

    m_ttlSec = ttl;
}

void AuctionIntentExecutor::LogMalformed(const char* kind, size_t bodySize)
{
    ++m_malformed;

    // Rate-limit: a hostile child can resend malformed frames every drained
    // frame (up to 256/tick). Emit at most once per MALFORMED_LOG_INTERVAL
    // seconds and fold the suppressed count into the next emitted line, mirror-
    // ing the rate-limited overflow warnings in World::Update.
    const uint32 now = uint32(sWorld.GetGameTime());
    if (m_lastMalformedLog != 0 &&
        now - m_lastMalformedLog < MALFORMED_LOG_INTERVAL)
    {
        ++m_malformedSuppressed;
        return;
    }

    sLog.outError("[AHExecutor] malformed %s (body=%u bytes); %u similar "
                  "suppressed since last log",
                  kind, static_cast<unsigned>(bodySize),
                  static_cast<unsigned>(m_malformedSuppressed));
    m_lastMalformedLog = now;
    m_malformedSuppressed = 0;
}

void AuctionIntentExecutor::DedupCache::EraseIndex(uint64 uuid, uint32 expiry)
{
    // Erase the single (expiry -> uuid) index pair matching this uuid. Several
    // uuids can share an expiry second, so scan that bucket for the exact uuid.
    std::pair<std::multimap<uint32, uint64>::iterator,
              std::multimap<uint32, uint64>::iterator> range =
        m_byExpiry.equal_range(expiry);
    for (std::multimap<uint32, uint64>::iterator itr = range.first;
         itr != range.second; ++itr)
    {
        if (itr->second == uuid)
        {
            m_byExpiry.erase(itr);
            return;
        }
    }
}

uint32 AuctionIntentExecutor::DedupCache::Remember(uint64 uuid, uint32 expiry)
{
    // Insert-or-refresh. On refresh, MOVE the uuid to its new (later) expiry
    // position: erase the stale index pair first, then re-insert, so the
    // sliding window holds -- a redelivered uuid's LATEST expiry is the only
    // one in the index, and purge/eviction key off that. The unordered_map
    // value is then updated (or inserted) to the new expiry.
    std::unordered_map<uint64, uint32>::iterator found = m_byUuid.find(uuid);
    if (found != m_byUuid.end())
    {
        EraseIndex(uuid, found->second);
        found->second = expiry;
        m_byExpiry.insert(std::make_pair(expiry, uuid));
        return 0; // refresh of an existing entry never grows the cache
    }

    uint32 evicted = 0;
    // Hard cap: if at capacity, evict the OLDEST-expiry entry (front of the
    // ordered index) before inserting. cap >> legitimate working set, so this
    // can only fire under a unique-uuid flood and only drops attacker entries
    // closest to expiring anyway.
    if (m_byUuid.size() >= MAX_ENTRIES)
    {
        std::multimap<uint32, uint64>::iterator oldest = m_byExpiry.begin();
        if (oldest != m_byExpiry.end())
        {
            m_byUuid.erase(oldest->second);
            m_byExpiry.erase(oldest);
            evicted = 1;
        }
    }

    m_byUuid.insert(std::make_pair(uuid, expiry));
    m_byExpiry.insert(std::make_pair(expiry, uuid));
    return evicted;
}

void AuctionIntentExecutor::DedupCache::Purge(uint32 now)
{
    // Pop only the expired prefix: the index is expiry-ordered, so iterate from
    // the front and stop at the first entry that has not yet expired. O(k) in
    // the number of entries actually expiring, not O(n) over the whole cache.
    std::multimap<uint32, uint64>::iterator itr = m_byExpiry.begin();
    while (itr != m_byExpiry.end() && itr->first <= now)
    {
        m_byUuid.erase(itr->second);
        itr = m_byExpiry.erase(itr);
    }
}

void AuctionIntentExecutor::PurgeExpiredUuids(uint32 now)
{
    // Refresh the cached TTL here (cheap, once/second) so a live config reload
    // is honoured without touching the per-intent hot path, then drop the
    // expired prefix in O(k).
    RefreshTtl();
    m_dedup.Purge(now);
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
            // [SP-2] Under WriteAuthority the worker owns bids (it applies the
            // value effect in-process, bidder=0) and no longer sends
            // IPC_INTENT_BID. A frame arriving here is stray/stale: log+ignore
            // (no reply) rather than mutate a live auction from a stale snapshot.
            if (sWorld.IsAhWriteAuthority())
            {
                sLog.outError("[AHExecutor] IPC_INTENT_BID under WriteAuthority"
                              " (worker owns bids) - ignored");
                break;
            }
            ApplyBid(in, resultOut, now);
            break;
        }
        case IPC_INTENT_BUYOUT:
        {
            // [SP-2] Same as IPC_INTENT_BID: buyouts are worker-owned under
            // WriteAuthority. A stray frame is logged and ignored.
            if (sWorld.IsAhWriteAuthority())
            {
                sLog.outError("[AHExecutor] IPC_INTENT_BUYOUT under"
                              " WriteAuthority (worker owns buyouts) - ignored");
                break;
            }
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
        LogMalformed("SellIntent", in.body.size());
        return; // no reply: caller leaves resultOut empty
    }

    // Idempotency FIRST: a redelivered uuid must never double-apply. Refresh
    // the dedup expiry on every hit so an actively-redelivering uuid (whose
    // INTENT_RESULT reply was lost) keeps its sliding window open and never
    // ages out into a second apply.
    //
    // [SP-2] EXCEPTION under WriteAuthority: a sell's idempotency is DURABLE
    // (the botlist:<uuid> row) and a redelivered *materialized* sell must replay
    // the REAL minted ids so the worker can finally write the book (its first
    // IPC_INTENT_RESULT was lost). The volatile dedup would instead answer
    // INTENT_DUPLICATE carrying zero ids, which the worker's OnBotSellResult
    // treats as a rejection and drops -- stranding the minted item for the
    // orphan sweep. So skip the in-memory dedup here under authority and let
    // MaterializeSell's durable replay own idempotency (it never double-mints:
    // Get("botlist:<uuid>") replays the recorded ids on a redelivery).
    if (!sWorld.IsAhWriteAuthority() && IsDuplicate(s.uuid))
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

    // (3) The listing price must be within the item's intrinsic value ceiling.
    // For a buyout-able listing cap the buyout; for a bid-only listing
    // (buyout == 0) the buyout cap never fires, so cap the START BID instead --
    // otherwise a hostile child could mint a bot-owned listing with an absurd
    // start bid (up to UINT32_MAX) that a real buyer would then have to pay.
    // The harm is bounded (the bot owns the listing) but close it anyway.
    const uint64 valueCeiling = ComputeBotValueCeiling(proto, s.stack);
    const uint64 priceToCap = s.buyout != 0 ? uint64(s.buyout) : uint64(s.bid);
    if (priceToCap > valueCeiling)
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
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

    // [SP-2] WriteAuthority: mangosd no longer owns the book. The worker is the
    // sole `auction`-table writer; mangosd's job is to mint+persist+escrow the
    // item, allocate the id and record the durable idempotency row, then reply
    // the ids for the worker to write the book. All validations above still ran.
    if (sWorld.IsAhWriteAuthority())
    {
        MaterializeSell(s, resultOut, now);
        return;
    }

    // --- legacy (WriteAuthority off): mangosd owns the book -----------------
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

void AuctionIntentExecutor::MaterializeSell(SellIntent const& s,
                                            IpcMessage& resultOut, uint32 now)
{
    // Durable id-replay: a redelivered uuid returns the SAME ids, never a
    // second item. This survives a restart (the in-memory dedup window does
    // not), so a lost IPC_INTENT_RESULT can never cause a double-materialize.
    std::string const key = "botlist:" + std::to_string(s.uuid);
    CustodyRow prior;
    if (CustodyLedger::Get(key, prior))
    {
        IntentResult res;
        res.uuid = s.uuid;
        res.status = INTENT_OK;
        res.reason = REASON_NONE;
        res.itemGuid = prior.itemGuid;
        res.auctionId = prior.auctionId;
        resultOut.op = IPC_INTENT_RESULT;
        resultOut.body.clear();
        res.Encode(resultOut.body);
        return;
    }

    // Mint the listing item (owner = bot lister). CreateItem clamps stack to
    // the prototype max and returns NULL for a bad item / zero count.
    Item* it = Item::CreateItem(s.itemId, s.stack);
    if (!it)
    {
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }
    // Stamp the bot as the on-disk owner so the orphan sweep can safely tell a
    // still-stranded mint (owner == bot) from an item that reached the book and
    // was later delivered to a buyer (owner changed) -- see
    // SweepOrphanMaterializations.
    it->SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, s.botGuid));

    uint32 const auctionId   = sObjectMgr.GenerateAuctionID();
    uint32 const itemGuidLow = it->GetGUIDLow();

    // FAIL-SAFE: never reply INTENT_OK carrying a zero id. A zero item guid or
    // auction id would have the worker write a book row keyed on 0. If the id
    // allocator cannot produce nonzero ids, reject (the worker re-sends).
    if (auctionId == 0u || itemGuidLow == 0u)
    {
        sLog.outError("[AHExecutor] materialize produced a zero id"
                      " (item %u auc %u) uuid %llu - rejecting",
                      itemGuidLow, auctionId,
                      static_cast<unsigned long long>(s.uuid));
        delete it;
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    // In-memory escrow BEFORE the txn (mirrors the custody player-sell path):
    // the worker's win / return leg finds the item via sAuctionMgr.GetAItem.
    sAuctionMgr.AddAItem(it);

    // ONE checked txn: persist the item_instance row + the durable botlist
    // idempotency row. NO `auction` row -- the worker inserts the book at its
    // own commit.
    CharacterDatabase.BeginTransaction();
    it->SaveToDB();                       // item_instance (owner = botGuid)
    CustodyRow r;
    r.id = 0;
    r.idemKey = key;
    r.kind = CUSTODY_ITEM;
    r.role = ROLE_RESOLUTION;
    r.state = CST_RESERVED;
    r.ownerGuid = s.botGuid;
    r.beneficiaryGuid = 0;
    r.amount = 0;
    r.itemGuid = itemGuidLow;
    r.auctionId = auctionId;
    r.createdTime = static_cast<uint64>(time(NULL));
    r.resolvedTime = 0;
    CustodyLedger::Insert(r);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        // Nothing persisted -> undo the in-memory escrow, drop the minted item
        // and reject. The worker re-sends its still-PENDING intent.
        sLog.outError("[AHExecutor] materialize commit failed (uuid %llu)",
                      static_cast<unsigned long long>(s.uuid));
        sAuctionMgr.RemoveAItem(itemGuidLow);
        delete it;
        ++m_rejected;
        Remember(s.uuid, now);
        MakeResult(resultOut, s.uuid, INTENT_REJECTED, REASON_BAD_ITEM);
        return;
    }

    ++m_applied;
    Remember(s.uuid, now);
    IntentResult res;
    res.uuid = s.uuid;
    res.status = INTENT_OK;
    res.reason = REASON_NONE;
    res.itemGuid = itemGuidLow;
    res.auctionId = auctionId;
    resultOut.op = IPC_INTENT_RESULT;
    resultOut.body.clear();
    res.Encode(resultOut.body);
}

void AuctionIntentExecutor::TestMaterializeSell(SellIntent const& s,
                                                IpcMessage& resultOut,
                                                uint32 now)
{
    // Test-only seam (see header): drive the durable leg directly, bypassing
    // the ApplySell re-validation chain that needs a fully loaded world.
    MaterializeSell(s, resultOut, now);
}

void AuctionIntentExecutor::SweepOrphanMaterializations(uint32 nowSec)
{
    // Grace window: only rows older than T are candidates, so a materialize
    // whose book-commit is still in flight on the worker is never reaped.
    static const uint32 ORPHAN_GRACE_SEC = 300u;
    uint64 const cutoff = (nowSec > ORPHAN_GRACE_SEC)
                              ? uint64(nowSec) - ORPHAN_GRACE_SEC
                              : 0u;

    // Candidates: durable botlist rows, past the grace window, whose auction id
    // is absent from the shared `auction` table (worker never wrote / already
    // removed the book row).
    QueryResult* q = CharacterDatabase.PQuery(
        "SELECT `idem_key`, `item_guid`, `auction_id`, `owner_guid` "
        "FROM `custody_ledger` "
        "WHERE `idem_key` LIKE 'botlist:%%' AND `created_time` < " UI64FMTD " "
        "AND `auction_id` NOT IN (SELECT `id` FROM `auction`)",
        cutoff);
    if (q == NULL)
    {
        return;
    }

    do
    {
        Field* f = q->Fetch();
        std::string idemKey    = f[0].GetCppString();
        uint32 const itemGuid  = f[1].GetUInt32();
        uint32 const ownerGuid = f[3].GetUInt32();
        CharacterDatabase.escape_string(idemKey);

        // Delete the minted item ONLY while it is still the bot's AND is not
        // attached to any mail. This is the safety that distinguishes a
        // genuinely stranded mint (crashed before book-commit: still bot-owned,
        // never mailed) from a listing that DID reach the book and later
        // sold/returned -- whose item is now the buyer's (owner changed) or is
        // sitting in the bot's return mail (mail_items ref). The stale botlist
        // row itself is always removed, bounding custody_ledger growth for
        // resolved listings too.
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "DELETE FROM `item_instance` WHERE `guid` = %u "
            "AND `owner_guid` = %u "
            "AND `guid` NOT IN (SELECT `item_guid` FROM `mail_items`)",
            itemGuid, ownerGuid);
        CharacterDatabase.PExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key` = '%s'",
            idemKey.c_str());
        CharacterDatabase.CommitTransactionChecked();

        // Drop the in-memory escrow (harmless no-op after a restart, where the
        // orphaned item was never re-loaded into mAitems).
        sAuctionMgr.RemoveAItem(itemGuid);
        sLog.outString("[AHExecutor] swept orphan materialization %s (item %u)",
                       f[0].GetCppString().c_str(), itemGuid);
    }
    while (q->NextRow());

    delete q;
}

void AuctionIntentExecutor::ApplyBid(const IpcMessage& in,
                                     IpcMessage& resultOut, uint32 now)
{
    ByteBuffer body(in.body);
    BidIntent b;
    if (!b.Decode(body))
    {
        LogMalformed("BidIntent", in.body.size());
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
        LogMalformed("BuyoutIntent", in.body.size());
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
