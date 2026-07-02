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

#include "AuctionBook.h"
#include "ServiceDatabase.h"
#include "ItemInstanceFields.h"
#include "PlayerMutations.h"

#include <cstdio>

AuctionBook::AuctionBook(ServiceDatabase* db)
    : m_db(db)
{
}

uint8 AuctionBook::HouseGroup(uint8 houseId)
{
    // Mirrors AuctionHouseMgr::GetAuctionHouseTeam (AuctionHouseMgr.cpp:930-945).
    switch (houseId)
    {
        case 1: case 2: case 3:
            return 0;   // ALLIANCE map
        case 4: case 5: case 6:
            return 1;   // HORDE map
        default:
            return 2;   // NEUTRAL map (houseid 7)
    }
}

uint8 AuctionBook::Admit(uint8 op, BookRow const* row)
{
    if (op == OP_BID || op == OP_BUYOUT)
    {
        // Absent -> the legacy not-found result for CMSG_AUCTION_PLACE_BID is
        // AUCTION_ERR_BID_OWN (AuctionHouseHandler.cpp:738-742). Prepared or
        // resolving rows are rejected as the legacy race-loser (spec 4.3b).
        if (row == NULL || row->state != BOOK_LIVE)
        {
            return BOOK_ERR_BID_OWN;
        }
        return BOOK_ERR_OK;
    }
    if (op == OP_CANCEL_PREPARE)
    {
        // Absent / already prepared / resolving -> the legacy removed-or-
        // missing result AUCTION_ERR_DATABASE (AuctionHouseHandler.cpp:921-925).
        if (row == NULL || row->state != BOOK_LIVE)
        {
            return BOOK_ERR_DATABASE;
        }
        return BOOK_ERR_OK;
    }
    return BOOK_ERR_DATABASE;   // unknown op: fail closed
}

bool AuctionBook::BuildFromRows(std::vector<RawAuctionRow> const& rows,
                                std::vector<AhJournal::JournalRow> const& activeJournal)
{
    m_rows.clear();
    m_orphans.clear();

    for (size_t i = 0; i < rows.size(); ++i)
    {
        RawAuctionRow const& r = rows[i];
        BookRow row = r.row;
        row.state = BOOK_LIVE;

        // Gate A -- the listed item must exist and decode. Legacy DELETEs the
        // auction (AuctionHouseMgr.cpp:823-830; an unloadable item ends the
        // same way via LoadAuctionItems:729-743). The worker REPORTS instead
        // of repairing (spec 5.6): disposal is mangosd's, over the resolve leg.
        if (!r.itemExists || !r.blobValid)
        {
            OrphanRow o;
            o.row  = row;
            o.kind = ORPHAN_MISSING_ITEM;
            m_orphans.push_back(o);
            fprintf(stderr, "ah-service: book load: auction %u has no loadable"
                            " item %u - reported (not repaired)\n",
                    row.id, row.itemGuid);
            continue;
        }

        // Gate B -- the house must resolve. Legacy returns the item + deletes
        // (AuctionHouseMgr.cpp:846-872). The worker REPORTS.
        if (row.houseId < 1 || row.houseId > 7)
        {
            OrphanRow o;
            o.row  = row;
            o.kind = ORPHAN_BAD_HOUSE;
            m_orphans.push_back(o);
            fprintf(stderr, "ah-service: book load: auction %u has invalid"
                            " houseid %u - reported (not repaired)\n",
                    row.id, static_cast<unsigned>(row.houseId));
            continue;
        }

        // Gate C -- adopt the real item data (legacy repair,
        // AuctionHouseMgr.cpp:833-844). The UPDATE is issued by LoadFromDb so
        // this core stays pure.
        if (row.itemTemplate != r.itemEntry ||
            row.itemCount != r.itemStack ||
            row.randomPropertyId != r.itemRandProp)
        {
            row.itemTemplate     = r.itemEntry;
            row.itemCount        = r.itemStack;
            row.randomPropertyId = r.itemRandProp;
        }

        if (m_rows.find(row.id) != m_rows.end())
        {
            fprintf(stderr, "ah-service: book load: duplicate auction id %u -"
                            " refusing to run\n", row.id);
            return false;
        }
        m_rows[row.id] = row;
    }

    // Journal re-mark (spec 4.3 v3 I2/I3) + one-ACTIVE-per-auction invariant.
    std::map<uint32, uint32> activePerAuction;
    for (size_t i = 0; i < activeJournal.size(); ++i)
    {
        AhJournal::JournalRow const& j = activeJournal[i];
        if (j.state != AhJournal::JRN_RESOLVING &&
            j.state != AhJournal::JRN_CANCEL_PREPARED)
        {
            continue;   // JRN_INTENT_PENDING has no book row to mark
        }

        if (++activePerAuction[j.auctionId] > 1u)
        {
            fprintf(stderr, "ah-service: journal invariant violated: auction %u"
                            " has more than one ACTIVE journal row - refusing"
                            " to run\n", j.auctionId);
            return false;
        }

        BookRow* row = Find(j.auctionId);
        if (row == NULL)
        {
            // A terminal resolution committed its book-row DELETE before the
            // ack landed: normal for RESOLVING (the re-send driver finishes
            // it). CANCEL_PREPARED without a row cannot happen by construction
            // (confirm deletes row + retires the journal row in ONE txn) --
            // log it as drift-audit material, do not refuse the boot.
            if (j.state == AhJournal::JRN_CANCEL_PREPARED)
            {
                fprintf(stderr, "ah-service: CANCEL_PREPARED journal row for"
                                " auction %u has no auction row - drift-audit"
                                " material\n", j.auctionId);
            }
            continue;
        }

        if (j.state == AhJournal::JRN_CANCEL_PREPARED)
        {
            row->state = BOOK_CANCEL_PREPARED;
        }
        else if (j.kind == static_cast<uint8>(RESOLVE_WON) ||
                 j.kind == static_cast<uint8>(RESOLVE_EXPIRED_NOBID))
        {
            // Only TERMINAL resolutions (WON, EXPIRED_NOBID) freeze the row
            // RESOLVING while awaiting the ack's terminal apply. NON-terminal
            // kinds (CANCELLED_UNLOCK, REPAIR_RETURN) leave the listing LIVE
            // while resolving (spec 4.3 v3 I2/I3): row.state is already
            // BOOK_LIVE from the load loop above, so there is nothing to do
            // here -- re-marking it RESOLVING would freeze a live listing
            // across a crash/restart (F1).
            row->state = BOOK_RESOLVING;
        }
    }

    return true;
}

bool AuctionBook::LoadFromDb(ServiceDatabase& db,
                             std::vector<AhJournal::JournalRow> const& activeJournal)
{
    //                          0       1           2            3
    QueryResult* result = db.Character().Query(
        "SELECT a.`id`, a.`houseid`, a.`itemguid`, a.`item_template`,"
        //  4               5                         6              7
        " a.`item_count`, a.`item_randompropertyid`, a.`itemowner`, a.`buyoutprice`,"
        //  8          9            10           11            12           13
        " a.`time`, a.`buyguid`, a.`lastbid`, a.`startbid`, a.`deposit`, ii.`guid`,"
        //  14
        " ii.`data`"
        " FROM `auction` a LEFT JOIN `item_instance` ii ON a.`itemguid` = ii.`guid`");

    std::vector<RawAuctionRow> rows;
    if (result != NULL)
    {
        do
        {
            Field* fields = result->Fetch();
            RawAuctionRow r;
            r.row.id               = fields[0].GetUInt32();
            r.row.houseId          = static_cast<uint8>(fields[1].GetUInt32());
            r.row.itemGuid         = fields[2].GetUInt32();
            r.row.itemTemplate     = fields[3].GetUInt32();
            r.row.itemCount        = fields[4].GetUInt32();
            r.row.randomPropertyId = fields[5].GetInt32();
            r.row.owner            = fields[6].GetUInt32();
            r.row.buyout           = fields[7].GetUInt32();
            r.row.expireTime       = fields[8].GetUInt64();
            r.row.bidder           = fields[9].GetUInt32();
            r.row.bid              = fields[10].GetUInt32();
            r.row.startbid         = fields[11].GetUInt32();
            r.row.deposit          = fields[12].GetUInt32();
            r.row.state            = BOOK_LIVE;
            r.itemExists           = !fields[13].IsNULL();
            r.blobValid            = false;
            r.itemEntry            = 0u;
            r.itemStack            = 0u;
            r.itemRandProp         = 0;
            if (r.itemExists)
            {
                ItemInstanceFields f = AhItemBlob::Decode(fields[14].GetCppString());
                r.blobValid    = f.valid;
                r.itemEntry    = f.entry;
                r.itemStack    = f.stackCount;
                r.itemRandProp = f.randomPropId;
            }
            rows.push_back(r);
        }
        while (result->NextRow());
        delete result;
    }

    if (!BuildFromRows(rows, activeJournal))
    {
        return false;
    }

    // Boot repair pass: persist gate-C adoption (legacy repair UPDATE,
    // AuctionHouseMgr.cpp:842-843, worker-owned under WriteAuthority per 5.7).
    // Autocommit is fine here: boot-time, idempotent, matches legacy.
    for (size_t i = 0; i < rows.size(); ++i)
    {
        BookRow* loaded = Find(rows[i].row.id);
        if (loaded == NULL)
        {
            continue;   // orphan: reported, never repaired
        }
        if (loaded->itemTemplate != rows[i].row.itemTemplate ||
            loaded->itemCount != rows[i].row.itemCount ||
            loaded->randomPropertyId != rows[i].row.randomPropertyId)
        {
            db.Character().PExecute(
                "UPDATE `auction` SET `item_template` = %u, `item_count` = %u,"
                " `item_randompropertyid` = %i WHERE `itemguid` = %u",
                loaded->itemTemplate, loaded->itemCount,
                loaded->randomPropertyId, loaded->itemGuid);
        }
    }

    printf("ah-service: book loaded: %u live listing(s), %u orphan(s) reported\n",
           static_cast<unsigned>(m_rows.size()),
           static_cast<unsigned>(m_orphans.size()));
    return true;
}

BookRow* AuctionBook::Find(uint32 auctionId)
{
    BookMap::iterator it = m_rows.find(auctionId);
    if (it == m_rows.end())
    {
        return NULL;
    }
    return &it->second;
}

void AuctionBook::Insert(BookRow const& row)
{
    m_rows[row.id] = row;
    if (m_db != NULL)
    {
        // Mirrors AuctionEntry::SaveToDB (AuctionHouseMgr.cpp:1524-1530).
        m_db->Character().PExecute(
            "INSERT INTO `auction` (`id`,`houseid`,`itemguid`,`item_template`,"
            "`item_count`,`item_randompropertyid`,`itemowner`,`buyoutprice`,"
            "`time`,`buyguid`,`lastbid`,`startbid`,`deposit`) "
            "VALUES ('%u', '%u', '%u', '%u', '%u', '%i', '%u', '%u', '" UI64FMTD "',"
            " '%u', '%u', '%u', '%u')",
            row.id, static_cast<uint32>(row.houseId), row.itemGuid,
            row.itemTemplate, row.itemCount, row.randomPropertyId, row.owner,
            row.buyout, row.expireTime, row.bidder, row.bid, row.startbid,
            row.deposit);
    }
}

void AuctionBook::UpdateBid(uint32 auctionId, uint32 bidder, uint32 bid)
{
    BookRow* row = Find(auctionId);
    if (row == NULL)
    {
        return;
    }
    row->bidder = bidder;
    row->bid    = bid;
    if (m_db != NULL)
    {
        // Mirrors the UpdateBid persist (AuctionHouseMgr.cpp:1738).
        m_db->Character().PExecute(
            "UPDATE `auction` SET `buyguid` = '%u', `lastbid` = '%u'"
            " WHERE `id` = '%u'",
            bidder, bid, auctionId);
    }
}

void AuctionBook::Remove(uint32 auctionId)
{
    m_rows.erase(auctionId);
    if (m_db != NULL)
    {
        // Mirrors AuctionEntry::DeleteFromDB (AuctionHouseMgr.cpp:1515-1519).
        m_db->Character().PExecute("DELETE FROM `auction` WHERE `id` = '%u'",
                                   auctionId);
    }
}

void AuctionBook::RollbackInsert(uint32 auctionId)
{
    m_rows.erase(auctionId);
}

void AuctionBook::RollbackUpdateBid(uint32 auctionId, uint32 prevBidder, uint32 prevBid)
{
    BookRow* row = Find(auctionId);
    if (row != NULL)
    {
        row->bidder = prevBidder;
        row->bid    = prevBid;
    }
}

void AuctionBook::RollbackRemove(BookRow const& row)
{
    m_rows[row.id] = row;
}

void AuctionBook::RemoveMemoryOnly(uint32 auctionId)
{
    m_rows.erase(auctionId);
}

uint32 AuctionBook::CountOwned(uint32 ownerGuid, uint8 houseId) const
{
    uint8 const group = HouseGroup(houseId);
    uint32 count = 0;
    for (BookMap::const_iterator it = m_rows.begin(); it != m_rows.end(); ++it)
    {
        if (it->second.owner == ownerGuid &&
            HouseGroup(it->second.houseId) == group)
        {
            ++count;
        }
    }
    return count;
}

void AuctionBook::VisitExpired(uint64 now, std::vector<uint32>& outIds) const
{
    outIds.clear();
    BookMap::const_iterator it = m_rows.begin();
    for (; it != m_rows.end(); ++it)
    {
        if (it->second.state != static_cast<uint8>(BOOK_LIVE))
        {
            continue;
        }
        if (it->second.expireTime > now)
        {
            continue;
        }
        outIds.push_back(it->first);
    }
}

void AuctionBook::TestSeedRow(BookRow const& row)
{
    m_rows[row.id] = row;
}
