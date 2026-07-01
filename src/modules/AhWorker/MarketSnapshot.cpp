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
      m_consecutiveFailures(0),
      m_hasSucceeded(false)
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

    // A NULL result means EITHER the auction table is empty OR the query
    // itself failed (e.g. a misconfigured cross-DB JOIN: a wrong
    // WorldDatabaseInfo db name, or item_template not reachable from the
    // character connection). Disambiguate with a COUNT(*) on the same-DB,
    // non-JOIN auction table and READ its value: only a confirmed 0 rows is
    // "empty"; a positive count with a NULL JOIN means the JOIN failed while
    // live auctions exist - a real failure that must NOT be mistaken for an
    // empty market (which would make the seller flood a full AH).
    QueryResult* result = m_db.Character().Query(query.c_str());

    if (!result)
    {
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

        const uint32 auctionRows = probe->Fetch()[0].GetUInt32();
        delete probe;

        if (auctionRows != 0)
        {
            // auction has rows but the market JOIN returned nothing -> the
            // JOIN query failed (cross-DB / config / permission). Fail closed
            // (preserve the previous snapshot, trip the failure counter);
            // do NOT clear the houses or treat the market as empty.
            ++m_consecutiveFailures;
            sLog.outError(
                "MarketSnapshot::Refresh: market JOIN returned no rows but"
                " auction has %u row(s) - the cross-DB JOIN failed (check the"
                " WorldDatabaseInfo db name / item_template access)"
                " (consecutive failures: %u)",
                static_cast<unsigned>(auctionRows),
                static_cast<unsigned>(m_consecutiveFailures));
            return;     // stale-fallback
        }

        // Auction table is genuinely empty: snapshot is valid (all houses 0).
        for (int h = 0; h < AH_MAX_AUCTION_HOUSE_TYPE; ++h)
        {
            m_houses[h].clear();
        }
        m_consecutiveFailures = 0;
        m_hasSucceeded = true;
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
    m_hasSucceeded = true;
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
    // Not healthy until at least one refresh has SUCCEEDED (populated or a
    // confirmed-empty auction table). This prevents the seller from acting
    // on a never-populated (default-empty) snapshot when the very first
    // refresh fails - e.g. a misconfigured cross-DB JOIN at startup, which
    // would otherwise look like an empty market and trigger a flood.
    return m_hasSucceeded && m_consecutiveFailures <= k_maxConsecFailures;
}

uint32 MarketSnapshot::ConsecutiveFailures() const
{
    return m_consecutiveFailures;
}
