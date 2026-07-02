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

#ifndef AH_WORKER_AUCTION_BOOK_H
#define AH_WORKER_AUCTION_BOOK_H

#include "Common.h"
#include "Journal.h"

#include <map>
#include <string>
#include <vector>

class ServiceDatabase;

/**
 * @file AuctionBook.h
 * @brief SP-2 authoritative in-memory auction book (spec v3 sections 3 / 4.3b / 5.6).
 *
 * Owned by the MAIN service-loop thread ONLY (the serializer). Browse stays
 * SQL-backed on its own thread; this book is the VALIDATOR's state, never a
 * browse source. Mutating methods with a DB side effect append their SQL to
 * the CALLER's open transaction on the worker's own character-DB connection
 * (callers own the txn). Constructed with db == NULL the book runs memory-only
 * (--selftest mode: no SQL is ever issued).
 *
 * The worker builds WITHOUT game headers, so the AuctionError wire values it
 * must emit are pinned here as BOOK_ERR_* (source of truth:
 * src/game/Object/AuctionHouseMgr.h:64-75). mangosd casts
 * PlayerMutationResult::reason straight back to AuctionError.
 */

/// Book row lock states (interface contract section 4 -- pinned).
enum BookRowState : uint8
{
    BOOK_LIVE            = 0,
    BOOK_CANCEL_PREPARED = 1,
    BOOK_RESOLVING       = 2
};

/// One authoritative auction row (interface contract section 4 -- pinned).
struct BookRow
{
    uint32 id;
    uint8  houseId;
    uint32 itemGuid;
    uint32 itemTemplate;
    uint32 itemCount;
    int32  randomPropertyId;
    uint32 owner;
    uint32 buyout;
    uint64 expireTime;
    uint32 bidder;
    uint32 bid;
    uint32 startbid;
    uint32 deposit;
    uint8  state;
};

// AuctionError low bytes (AuctionHouseMgr.h:64-75). BOOK_ERR_OK as the reason
// on a MUT_REJECTED result means "silent legacy reject": legacy sent NO packet
// for this rejection; mangosd releases the reservation and emits nothing.
const uint8 BOOK_ERR_OK            = 0;    ///< AUCTION_OK
const uint8 BOOK_ERR_DATABASE      = 2;    ///< AUCTION_ERR_DATABASE
const uint8 BOOK_ERR_HIGHER_BID    = 5;    ///< AUCTION_ERR_HIGHER_BID
const uint8 BOOK_ERR_BID_INCREMENT = 7;    ///< AUCTION_ERR_BID_INCREMENT
const uint8 BOOK_ERR_BID_OWN       = 10;   ///< AUCTION_ERR_BID_OWN

/// Ops admitted against a row state (spec 4.3b admission table).
enum BookOp : uint8
{
    OP_BID            = 0,
    OP_BUYOUT         = 1,
    OP_CANCEL_PREPARE = 2
};

/// Orphan classes REPORTED, never repaired (spec 5.6: legacy deleted/returned;
/// the worker reports them for mangosd to dispose over the resolve leg).
enum OrphanKind : uint8
{
    ORPHAN_MISSING_ITEM = 0,   ///< no item_instance row, or undecodable data blob
    ORPHAN_BAD_HOUSE    = 1    ///< houseid outside 1..7
};

struct OrphanRow
{
    BookRow row;
    uint8   kind;   ///< OrphanKind
};

/// One decoded `auction` LEFT JOIN `item_instance` result row: the
/// pure-testable input unit of BuildFromRows (LoadFromDb renders SQL rows
/// into these; the selftest builds them directly).
struct RawAuctionRow
{
    BookRow row;           ///< auction columns (state forced BOOK_LIVE)
    bool    itemExists;    ///< item_instance JOIN hit
    bool    blobValid;     ///< item_instance.data decoded (ItemInstanceFields.valid)
    uint32  itemEntry;     ///< blob word 3  (OBJECT_FIELD_ENTRY)
    uint32  itemStack;     ///< blob word 14 (stack count)
    int32   itemRandProp;  ///< blob word 44 (random property id)
};

class AuctionBook
{
    public:
        /// db == NULL -> memory-only selftest mode (no SQL side effects).
        explicit AuctionBook(ServiceDatabase* db);

        /**
         * @brief SELECT auction LEFT JOIN item_instance, decode, BuildFromRows,
         *        then persist any gate-C adoption (the legacy repair UPDATE,
         *        AuctionHouseMgr.cpp:833-844, worker-owned under WriteAuthority
         *        per spec 5.7).
         *
         * @return false on a journal-invariant violation or duplicate auction
         *         id (corruption): the caller MUST exit non-zero.
         */
        bool LoadFromDb(ServiceDatabase& db,
                        std::vector<AhJournal::JournalRow> const& activeJournal);

        /// Pure core: load gates + journal re-mark + one-ACTIVE invariant.
        bool BuildFromRows(std::vector<RawAuctionRow> const& rows,
                           std::vector<AhJournal::JournalRow> const& activeJournal);

        /// @return the live row, or NULL. Pointer valid until the next mutation.
        BookRow* Find(uint32 auctionId);

        /// Memory insert + INSERT INTO `auction` on the caller's open txn
        /// (mirrors AuctionEntry::SaveToDB, AuctionHouseMgr.cpp:1524-1530).
        void Insert(BookRow const& row);

        /// Memory bid update + UPDATE `auction` on the caller's open txn
        /// (mirrors the UpdateBid persist, AuctionHouseMgr.cpp:1738).
        void UpdateBid(uint32 auctionId, uint32 bidder, uint32 bid);

        /// Memory erase + DELETE FROM `auction` on the caller's open txn
        /// (mirrors AuctionEntry::DeleteFromDB, AuctionHouseMgr.cpp:1515-1519).
        void Remove(uint32 auctionId);

        /**
         * @brief Memory-only erase, no DB side effect (SP-2 Task 7).
         *
         * Used after a caller-owned DELETE has ALREADY committed in its own
         * checked transaction (MutationHandler::CommitTerminalApply) -- unlike
         * Remove(), which issues its own DELETE for callers that rely on it
         * joining an already-open BeginCommit()/FinishCommit() transaction.
         */
        void RemoveMemoryOnly(uint32 auctionId);

        // Memory-only compensation when CommitTransactionChecked fails: the DB
        // rolled back, so the in-memory image must be restored to match.
        void RollbackInsert(uint32 auctionId);
        void RollbackUpdateBid(uint32 auctionId, uint32 prevBidder, uint32 prevBid);
        void RollbackRemove(BookRow const& row);

        /**
         * @brief Owned-listing count in @p houseId's house GROUP.
         *
         * Legacy caps at 50 per AuctionHouseObject map
         * (AuctionHouseHandler.cpp:579-601); the maps group houses per
         * GetAuctionHouseTeam (AuctionHouseMgr.cpp:930-945). Divergence note:
         * with CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION=1 legacy
         * collapses ALL houses into one neutral map; the worker cannot see
         * mangosd's config and always counts per group. The default config
         * matches legacy exactly.
         */
        uint32 CountOwned(uint32 ownerGuid, uint8 houseId) const;

        std::vector<OrphanRow> const& Orphans() const
        {
            return m_orphans;
        }

        size_t Size() const
        {
            return m_rows.size();
        }

        /// houseid -> map group: 1-3 alliance(0), 4-6 horde(1), else neutral(2).
        static uint8 HouseGroup(uint8 houseId);

        /**
         * @brief Spec 4.3b admission cell for @p op vs @p row.
         *
         * @return BOOK_ERR_OK to admit, else the legacy AuctionError low byte
         *         the race-loser would have received:
         *         - bid/buyout vs absent/prepared/resolving -> BOOK_ERR_BID_OWN
         *           (the legacy not-found result for a bid,
         *           AuctionHouseHandler.cpp:738-742; a prepared/resolving row
         *           is rejected exactly as if the cancel/resolution had won
         *           the legacy world-thread race)
         *         - cancel-prepare vs absent/prepared/resolving ->
         *           BOOK_ERR_DATABASE (AuctionHouseHandler.cpp:921-925)
         */
        static uint8 Admit(uint8 op, BookRow const* row);

        /**
         * @brief Collect ids of LIVE rows whose expireTime has passed.
         *
         * 4.3b: CANCEL_PREPARED and RESOLVING rows are skipped by state.
         * Ascending id (deterministic mint order for the tick budget).
         */
        void VisitExpired(uint64 now, std::vector<uint32>& outIds) const;

        /**
         * @brief SELFTEST-ONLY: seed a row in memory with no auction INSERT
         *        and no journal write. Production inserts go through Insert()
         *        inside a caller-owned transaction.
         */
        void TestSeedRow(BookRow const& row);

    private:
        typedef std::map<uint32, BookRow> BookMap;

        BookMap                m_rows;
        std::vector<OrphanRow> m_orphans;
        ServiceDatabase*       m_db;

        // Non-copyable: single-owner main-thread state.
        AuctionBook(const AuctionBook&);
        AuctionBook& operator=(const AuctionBook&);
};

#endif // AH_WORKER_AUCTION_BOOK_H
