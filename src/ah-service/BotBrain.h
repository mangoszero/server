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

#ifndef AH_SERVICE_BOT_BRAIN_H
#define AH_SERVICE_BOT_BRAIN_H

#include "Common.h"
#include "AhBotDefines.h"
#include "ServiceConfig.h"
#include "ItemPool.h"
#include "MarketSnapshot.h"
#include "AuctionIntents.h"

#include <ctime>
#include <map>
#include <vector>

/**
 * @file BotBrain.h
 * @brief Child-owned port of the AuctionHouseBot decision logic.
 *
 * BotBrain is a faithful reimplementation of the in-process
 * @c AuctionHouseBot seller + buyer agents
 * (src/game/AuctionHouseBot/AuctionHouseBot.cpp). It rotates through
 * 3 houses x {seller, buyer} one operation per @c RunOneOperation() call,
 * exactly like @c AuctionHouseBot::Update()'s @c m_OperationSelector, and
 * produces auction INTENTS instead of mutating the live auction house.
 *
 * The price arithmetic and candidate-selection predicates
 * (@c IsBuyableEntry, @c IsBidableEntry, @c SetPricesOfItem,
 * @c getRandomArray) are ported verbatim. @c SetStat and
 * @c GetBuyableEntry are reimplemented to read the @c MarketSnapshot
 * (grouped by house) instead of the live @c sAuctionMgr map, preserving
 * the same counting / candidate predicates.
 *
 * BotBrain never touches the database (the snapshot and pool are passed in)
 * and never performs IPC: it appends the intents it decides to a caller-owned
 * vector, and the caller logs (dry-run) or sends them.
 */

/// One intent produced by a BotBrain tick (tagged union over the 3 types).
struct EmittedIntent
{
    /// Which of the three intent kinds this carries.
    enum Kind
    {
        KIND_SELL,
        KIND_BID,
        KIND_BUYOUT
    };

    Kind         kind;    ///< Discriminator for the union members below.
    SellIntent   sell;    ///< Valid when kind == KIND_SELL.
    BidIntent    bid;     ///< Valid when kind == KIND_BID.
    BuyoutIntent buyout;  ///< Valid when kind == KIND_BUYOUT.
};

/// Random selection entry (mirror of the source's @c RandomArrayEntry).
struct RandomArrayEntry
{
    uint32 color;      ///< Item quality (AuctionQuality index).
    uint32 itemclass;  ///< Item class (ItemClass index).
};

typedef std::vector<RandomArrayEntry> RandomArray;

/**
 * @brief Per-house seller state (mirror of @c AHB_Seller_Config).
 *
 * Holds the desired item amounts (per quality and per [quality][class]),
 * the resulting "missed item" counts filled by @c SetStat, the price ratio
 * per quality, and the min/max auction durations.
 */
struct SellerHouseConfig
{
    SellerHouseConfig();

    uint8  houseType;                 ///< AhAuctionHouseType for this house.
    uint32 lastMissedItem;            ///< Total missed items from last SetStat.
    uint32 minTime;                   ///< Min auction duration (hours).
    uint32 maxTime;                   ///< Max auction duration (hours).

    /// Desired item count per quality (mirror SellerItemInfo.AmountOfItems).
    uint32 amountPerQuality[AH_MAX_AUCTION_QUALITY];
    /// Price ratio per quality (mirror SellerItemInfo.PriceRatio).
    uint32 priceRatioPerQuality[AH_MAX_AUCTION_QUALITY];
    /// Per-stack quantity per [quality][class] (mirror Quantity).
    uint32 quantity[AH_MAX_AUCTION_QUALITY][AH_MAX_ITEM_CLASS];
    /// Desired item count per [quality][class] (mirror AmountOfItems).
    uint32 amountPerClass[AH_MAX_AUCTION_QUALITY][AH_MAX_ITEM_CLASS];
    /// Missed item count per [quality][class] (mirror MissItems).
    uint32 missItems[AH_MAX_AUCTION_QUALITY][AH_MAX_ITEM_CLASS];

    /// @brief Effective min time (>= 1, capped at max), as the source does.
    uint32 GetMinTime() const;
};

/**
 * @brief Per-house buyer state (mirror of @c AHB_Buyer_Config).
 */
struct BuyerHouseConfig
{
    BuyerHouseConfig();

    uint8  houseType;       ///< AhAuctionHouseType for this house.
    bool   buyerEnabled;    ///< Whether buyer is enabled for this house.
    uint32 buyerPriceRatio; ///< Price ratio for buying decisions.
    uint32 factionChance;   ///< 5000 * configured faction chance.
};

/**
 * @brief The AH bot decision engine for the ah-service child.
 */
class BotBrain
{
    public:
        /**
         * @brief Construct against the loaded config, pool and snapshot.
         *
         * @param config   Loaded AH bot config (tuning + toggles).
         * @param pool      Built seller item pool (per [quality][class]).
         * @param snapshot  Live market snapshot (per house).
         * @param botGuid   Resolved bot low GUID (0 = unresolved: no selling).
         * @param runId     Per-spawn run-id (uuid high 32 bits).
         */
        BotBrain(const ServiceConfig& config, const ItemPool& pool,
                 const MarketSnapshot& snapshot, uint32 botGuid,
                 uint32 runId);

        /**
         * @brief Build the seller/buyer per-house config tables.
         *
         * Mirrors @c AuctionBotSeller::LoadConfig + @c LoadItemsQuantity and
         * @c AuctionBotBuyer::LoadConfig + @c LoadBuyerValues. Must be called
         * once before @c RunOneOperation.
         */
        void Initialize();

        /**
         * @brief Run ONE rotated operation (mirror AuctionHouseBot::Update).
         *
         * Rotates @c m_operationSelector over 0..2*MAX_HOUSE-1 doing one
         * seller- or buyer-house operation, stopping at the first that did
         * work. Intents the operation decides are appended to @p out, each
         * already uuid-stamped.
         *
         * @param out Caller-owned sink for the produced intents.
         */
        void RunOneOperation(std::vector<EmittedIntent>& out);

        /// @brief True if the seller is enabled (config + bot guid resolved).
        bool SellerEnabled() const { return m_sellerEnabled; }
        /// @brief True if any buyer house is enabled.
        bool BuyerEnabled() const { return m_buyerEnabled; }

    private:
        // --- Orchestration ------------------------------------------------
        bool SellerUpdate(uint8 houseType, std::vector<EmittedIntent>& out);
        bool BuyerUpdate(uint8 houseType, std::vector<EmittedIntent>& out);

        // --- Seller (SetStat reimplemented; rest verbatim) ----------------
        uint32 SetStat(SellerHouseConfig& cfg);
        bool getRandomArray(const SellerHouseConfig& cfg, RandomArray& ra,
                            const std::vector<std::vector<uint32> >& added)
            const;
        void SetPricesOfItem(const SellerHouseConfig& cfg, uint32& buyp,
                             uint32& bidp, uint32 stackcnt,
                             uint32 itemQuality) const;
        void addNewAuctions(SellerHouseConfig& cfg,
                            std::vector<EmittedIntent>& out);

        // --- Buyer (GetBuyableEntry reimplemented; rest verbatim) ---------
        bool IsBuyableEntry(uint32 buyoutPrice, double inGameBuyPrice,
                            double maxBuyablePrice, uint32 minBuyPrice,
                            uint32 maxChance, uint32 chanceRatio) const;
        bool IsBidableEntry(uint32 bidPrice, double inGameBuyPrice,
                            double maxBidablePrice, uint32 minBidPrice,
                            uint32 maxChance, uint32 chanceRatio) const;
        void addNewAuctionBuyerBotBid(BuyerHouseConfig& cfg,
                                      std::vector<EmittedIntent>& out);

        // --- uuid stamping ------------------------------------------------
        uint64 NextUuid();

        // --- Per-house config loaders -------------------------------------
        void LoadSellerHouse(SellerHouseConfig& cfg);
        void LoadBuyerHouse(BuyerHouseConfig& cfg);
        uint32 GetItemAmountRatio(uint8 houseType) const;
        bool GetBuyerEnabledFor(uint8 houseType) const;

        const ServiceConfig&  m_config;
        const ItemPool&       m_pool;
        const MarketSnapshot& m_snapshot;
        uint32                m_botGuid;
        uint32                m_runId;
        uint32                m_seq;               ///< uuid low-32 counter.
        uint32                m_operationSelector; ///< 0..2*MAX_HOUSE-1.
        bool                  m_sellerEnabled;
        bool                  m_buyerEnabled;

        SellerHouseConfig     m_sellerHouse[AH_MAX_AUCTION_HOUSE_TYPE];
        BuyerHouseConfig      m_buyerHouse[AH_MAX_AUCTION_HOUSE_TYPE];

        /**
         * @brief Cross-tick recheck throttle for each buyer house.
         *
         * Maps auctionId -> wall-clock time of the last decision attempt.
         * Mirrors the in-process @c BuyerAuctionEval::LastChecked map
         * (AuctionHouseBot.cpp:969 / cpp:1337).  Keyed per house index
         * (0=Alliance, 1=Horde, 2=Neutral).  Survives across ticks so that
         * any given auction is evaluated at most once per
         * @c AHB_BUYER_RECHECK_INTERVAL_SECONDS (1200 s).
         */
        std::map<uint32, time_t>
            m_buyerLastChecked[AH_MAX_AUCTION_HOUSE_TYPE];

        // Non-copyable.
        BotBrain(const BotBrain&);
        BotBrain& operator=(const BotBrain&);
};

#endif // AH_SERVICE_BOT_BRAIN_H
