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

#ifndef AH_SERVICE_MARKET_SNAPSHOT_H
#define AH_SERVICE_MARKET_SNAPSHOT_H

#include "Common.h"
#include "AhBotDefines.h"

#include <vector>

class ServiceDatabase;

/**
 * @brief A single auction entry from the live auction table.
 *
 * Fields are populated by MarketSnapshot::Refresh() from a JOIN of
 * @c auction and @c item_template.  All monetary values are in copper.
 */
struct AuctionRecord
{
    uint32 id;              ///< Auction row id (auction.id).
    uint8  houseType;       ///< AhAuctionHouseType (0=Alliance,1=Horde,2=Neutral).
    uint32 itemId;          ///< item_template.entry
    uint32 itemCount;       ///< Number of items in the stack.
    uint32 ownerGuid;       ///< Seller low-guid.
    uint32 buyout;          ///< Buyout price in copper (0 = no buyout).
    uint32 curBid;          ///< Current bid (auction.lastbid; 0 = none).
    uint32 startBid;        ///< Starting bid.
    uint32 expireTime;      ///< Unix timestamp when the auction expires.
    uint32 bidderGuid;      ///< Current bidder low-guid (0 = none).
    uint32 quality;         ///< item_template.Quality.
    uint32 itemClass;       ///< item_template.class.
    uint32 vendorBuyPrice;  ///< item_template.BuyPrice (vendor buy, copper).
    uint32 vendorSellPrice; ///< item_template.SellPrice (vendor sell, copper).
};

/**
 * @brief Read-only snapshot of the live auction tables.
 *
 * Refresh() issues one SELECT joining @c auction and @c item_template for
 * all three houses in a single query, then groups the results by house into
 * three vectors.  On failure the previous snapshot is preserved (stale
 * fallback); after more than five consecutive failures Healthy() returns
 * false so 8c can stop emitting while the DB is unreachable.
 *
 * The houseid->houseType mapping is derived from
 * AuctionHouseMgr::GetAuctionHouseTeam() (AuctionHouseMgr.cpp:555-564):
 *   houseid 1,2,3 -> ALLIANCE (AH_AUCTION_HOUSE_ALLIANCE = 0)
 *   houseid 4,5,6 -> HORDE    (AH_AUCTION_HOUSE_HORDE    = 1)
 *   houseid 7     -> NEUTRAL  (AH_AUCTION_HOUSE_NEUTRAL  = 2)
 *   all others    -> NEUTRAL  (safe default)
 */
class MarketSnapshot
{
    public:
        /**
         * @param db ServiceDatabase whose Character() connection is used
         *           for all queries (READ-ONLY).
         */
        explicit MarketSnapshot(ServiceDatabase& db);

        /**
         * @brief Execute the auction JOIN query and refresh the snapshot.
         *
         * On query failure the previous snapshot is preserved.  Successive
         * failures increment @c m_consecutiveFailures; > 5 trips Healthy().
         */
        void Refresh();

        /**
         * @brief Auction list for one house.
         *
         * @param houseType AhAuctionHouseType (0=Alliance, 1=Horde, 2=Neutral).
         * @return Const reference to the current snapshot for that house.
         */
        const std::vector<AuctionRecord>& GetHouse(uint8 houseType) const;

        /**
         * @brief Total number of auction records across all houses.
         */
        uint32 TotalCount() const;

        /**
         * @brief True if the DB is reachable (fewer than 6 consecutive
         *        query failures).
         */
        bool Healthy() const;

        /**
         * @brief Consecutive query failure count since last success.
         */
        uint32 ConsecutiveFailures() const;

    private:
        /// Convert a DB houseid to AhAuctionHouseType.
        static uint8 HouseIdToType(uint32 houseid);

        ServiceDatabase&                 m_db;
        std::vector<AuctionRecord>       m_houses[AH_MAX_AUCTION_HOUSE_TYPE];
        uint32                           m_consecutiveFailures;

        // Non-copyable.
        MarketSnapshot(const MarketSnapshot&);
        MarketSnapshot& operator=(const MarketSnapshot&);
};

#endif // AH_SERVICE_MARKET_SNAPSHOT_H
