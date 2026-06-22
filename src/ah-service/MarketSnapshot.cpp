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

#include "MarketSnapshot.h"
#include "ServiceDatabase.h"
#include "Database/DatabaseEnv.h"
#include "Log/Log.h"

static const uint32 k_maxConsecFailures = 5;

// ---------------------------------------------------------------------------
// houseid -> AhAuctionHouseType
//
// Derived from AuctionHouseMgr::GetAuctionHouseTeam()
// (src/game/Object/AuctionHouseMgr.cpp:555-564):
//   houseid 1,2,3 -> ALLIANCE team -> AH_AUCTION_HOUSE_ALLIANCE (0)
//   houseid 4,5,6 -> HORDE team    -> AH_AUCTION_HOUSE_HORDE    (1)
//   houseid 7     -> neutral        -> AH_AUCTION_HOUSE_NEUTRAL  (2)
//   all others    -> neutral (safe default)
// ---------------------------------------------------------------------------

uint8 MarketSnapshot::HouseIdToType(uint32 houseid)
{
    switch (houseid)
    {
        case 1: case 2: case 3:
            return static_cast<uint8>(AH_AUCTION_HOUSE_ALLIANCE);
        case 4: case 5: case 6:
            return static_cast<uint8>(AH_AUCTION_HOUSE_HORDE);
        case 7:
        default:
            return static_cast<uint8>(AH_AUCTION_HOUSE_NEUTRAL);
    }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MarketSnapshot::MarketSnapshot(ServiceDatabase& db)
    : m_db(db),
      m_consecutiveFailures(0)
{
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void MarketSnapshot::Refresh()
{
    // auction lives in the character database; item_template lives in
    // the world database. Build a cross-database JOIN using the world
    // DB name retrieved from ServiceDatabase so both tables are
    // reachable via the character DB connection.
    std::string worldDb = m_db.WorldDbName();

    std::string query =
        "SELECT a.id, a.houseid, a.item_template, a.item_count,"
        "       a.itemowner, a.buyoutprice, a.lastbid, a.startbid,"
        "       a.time, a.buyguid,"
        "       it.Quality, it.class, it.BuyPrice, it.SellPrice"
        " FROM auction a JOIN ";

    if (!worldDb.empty())
    {
        query += "`";
        query += worldDb;
        query += "`.item_template it ON a.item_template = it.entry";
    }
    else
    {
        // Fallback: assume item_template is accessible without qualifier.
        query += "item_template it ON a.item_template = it.entry";
    }

    // Use PQuery to surface MySQL errors to log; NULL means 0 rows OR error.
    // Distinguish the two by running a COUNT(*) probe when the JOIN returns
    // NULL, so an empty auction house does not trip the failure counter.
    QueryResult* result = m_db.Character().Query(query.c_str());

    if (!result)
    {
        // Check whether auction is simply empty (vs a real query error).
        QueryResult* probe = m_db.Character().Query(
            "SELECT COUNT(*) FROM auction");
        if (!probe)
        {
            // Real failure: character DB unreachable.
            ++m_consecutiveFailures;
            sLog.outError(
                "MarketSnapshot::Refresh: character DB unreachable"
                " (consecutive failures: %u)",
                static_cast<unsigned>(m_consecutiveFailures));
            return;     // stale-fallback
        }
        delete probe;

        // Auction table is empty: snapshot is valid (all houses have 0).
        for (int h = 0; h < AH_MAX_AUCTION_HOUSE_TYPE; ++h)
        {
            m_houses[h].clear();
        }
        m_consecutiveFailures = 0;
        sLog.outDetail("MarketSnapshot::Refresh: auction table empty"
                       " (0 auctions)");
        return;
    }

    // Success: replace snapshot.
    for (int h = 0; h < AH_MAX_AUCTION_HOUSE_TYPE; ++h)
    {
        m_houses[h].clear();
    }

    do
    {
        Field* f = result->Fetch();

        AuctionRecord rec;
        rec.id             = f[0].GetUInt32();
        const uint32 houseid = f[1].GetUInt32();
        rec.houseType      = HouseIdToType(houseid);
        rec.itemId         = f[2].GetUInt32();
        rec.itemCount      = f[3].GetUInt32();
        rec.ownerGuid      = f[4].GetUInt32();
        rec.buyout         = f[5].GetUInt32();
        rec.curBid         = f[6].GetUInt32();
        rec.startBid       = f[7].GetUInt32();
        rec.expireTime     = f[8].GetUInt32();
        rec.bidderGuid     = f[9].GetUInt32();
        rec.quality        = f[10].GetUInt32();
        rec.itemClass      = f[11].GetUInt32();
        rec.vendorBuyPrice = f[12].GetUInt32();
        rec.vendorSellPrice = f[13].GetUInt32();

        if (rec.houseType < AH_MAX_AUCTION_HOUSE_TYPE)
        {
            m_houses[rec.houseType].push_back(rec);
        }
    }
    while (result->NextRow());

    delete result;

    m_consecutiveFailures = 0;
    sLog.outDetail(
        "MarketSnapshot::Refresh: OK"
        " (alliance=%u horde=%u neutral=%u)",
        static_cast<unsigned>(
            m_houses[AH_AUCTION_HOUSE_ALLIANCE].size()),
        static_cast<unsigned>(
            m_houses[AH_AUCTION_HOUSE_HORDE].size()),
        static_cast<unsigned>(
            m_houses[AH_AUCTION_HOUSE_NEUTRAL].size()));
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const std::vector<AuctionRecord>&
MarketSnapshot::GetHouse(uint8 houseType) const
{
    if (houseType < AH_MAX_AUCTION_HOUSE_TYPE)
    {
        return m_houses[houseType];
    }
    // Return alliance as a safe empty-or-valid fallback.
    return m_houses[AH_AUCTION_HOUSE_ALLIANCE];
}

uint32 MarketSnapshot::TotalCount() const
{
    uint32 total = 0;
    for (int h = 0; h < AH_MAX_AUCTION_HOUSE_TYPE; ++h)
    {
        total += static_cast<uint32>(m_houses[h].size());
    }
    return total;
}

bool MarketSnapshot::Healthy() const
{
    return m_consecutiveFailures <= k_maxConsecFailures;
}

uint32 MarketSnapshot::ConsecutiveFailures() const
{
    return m_consecutiveFailures;
}
