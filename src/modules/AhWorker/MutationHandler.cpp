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

#include "MutationHandler.h"
#include "ServiceDatabase.h"

#include <cstdio>
#include <ctime>

MutationHandler::MutationHandler(AuctionBook& book, ServiceDatabase* db, uint32 runId)
    : m_book(book), m_db(db), m_runId(runId), m_nextSeq(0x80000000u)
{
}

uint32 MutationHandler::OutBidAmount(uint32 bid)
{
    // AuctionEntry::GetAuctionOutBid (AuctionHouseMgr.cpp:1502-1510):
    // (1% of current bid) * 5, floor 1 copper.
    uint32 outbid = (bid / 100u) * 5u;
    if (outbid == 0u)
    {
        outbid = 1u;
    }
    return outbid;
}

void MutationHandler::ClearFacts(MutationFacts& out)
{
    out.auctionId = 0u;
    out.houseId = 0u;
    out.itemGuid = 0u;
    out.itemTemplate = 0u;
    out.randomPropertyId = 0;
    out.itemCount = 0u;
    out.sellerGuid = 0u;
    out.deposit = 0u;
    out.effectiveBid = 0u;
    out.priorBidderGuid = 0u;
    out.priorBidAmount = 0u;
    out.curBidderGuid = 0u;
    out.curBid = 0u;
    out.buyout = 0u;
}

void MutationHandler::FillFacts(BookRow const& row, MutationFacts& out)
{
    out.auctionId        = row.id;
    out.houseId          = row.houseId;
    out.itemGuid         = row.itemGuid;
    out.itemTemplate     = row.itemTemplate;
    out.randomPropertyId = row.randomPropertyId;
    out.itemCount        = row.itemCount;
    out.sellerGuid       = row.owner;
    out.deposit          = row.deposit;
    out.effectiveBid     = 0u;
    out.priorBidderGuid  = row.bidder;
    out.priorBidAmount   = row.bid;
    out.curBidderGuid    = row.bidder;
    out.curBid           = row.bid;
    out.buyout           = row.buyout;
}

PlayerMutationResult MutationHandler::MakeResult(uint64 uuid, uint8 op,
                                                 uint8 status, uint8 reason) const
{
    PlayerMutationResult res;
    res.uuid   = uuid;
    res.op     = op;
    res.status = status;
    res.reason = reason;
    ClearFacts(res.facts);
    return res;
}

uint32 MutationHandler::LookupAccount(uint32 guidLow) const
{
    if (m_db == NULL || guidLow == 0u)
    {
        return 0u;
    }
    QueryResult* result = m_db->Character().PQuery(
        "SELECT `account` FROM `characters` WHERE `guid` = %u", guidLow);
    if (result == NULL)
    {
        return 0u;
    }
    uint32 const account = result->Fetch()[0].GetUInt32();
    delete result;
    return account;
}

bool MutationHandler::BeginCommit()
{
    if (m_db == NULL)
    {
        return true;   // selftest mode
    }
    return m_db->Character().BeginTransaction();
}

bool MutationHandler::FinishCommit()
{
    if (m_db == NULL)
    {
        return true;   // selftest mode
    }
    return m_db->Character().CommitTransactionChecked();
}

void MutationHandler::JournalCommitted(uint64 uuid, uint32 auctionId, uint8 kind,
                                       PlayerMutationResult const& res)
{
    if (m_db == NULL)
    {
        return;   // selftest mode
    }
    ByteBuffer bb;
    res.Encode(bb);
    AhJournal::JournalRow row;
    row.uuid         = uuid;
    row.auctionId    = auctionId;
    row.kind         = kind;
    row.state        = AhJournal::JRN_COMMITTED;
    row.facts        = std::string(reinterpret_cast<const char*>(bb.contents()),
                                   bb.size());
    row.createdTime  = static_cast<uint64>(time(NULL));
    row.resolvedTime = 0u;
    AhJournal::Insert(*m_db, row);
}

ValidationOutcome MutationHandler::ValidateBid(BookRow const* row, uint32 bidderGuid,
                                               uint32 bidAmount, uint32 ownerAccount,
                                               uint32 bidderAccount, uint8& reasonOut)
{
    // (1) Existence / lock admission (spec 4.3b).
    reasonOut = AuctionBook::Admit(OP_BID, row);
    if (reasonOut != BOOK_ERR_OK)
    {
        return VALIDATE_REJECT;
    }

    // (2) Bid-own (:738-743) + same-account (:747-754). Two online characters
    //     on one account are impossible, so an unconditional account compare
    //     is equivalent to legacy's offline-owner-only check. Account 0 means
    //     unknown -> no account reject.
    if (row->owner == bidderGuid ||
        (ownerAccount != 0u && ownerAccount == bidderAccount))
    {
        reasonOut = BOOK_ERR_BID_OWN;
        return VALIDATE_REJECT;
    }

    // (3) Must beat the current bid (:756-762).
    if (bidAmount <= row->bid)
    {
        reasonOut = BOOK_ERR_HIGHER_BID;
        return VALIDATE_REJECT;
    }

    // (4) Minimum increment when not a buyout (:764-771).
    if ((bidAmount < row->buyout || row->buyout == 0u) &&
        bidAmount < row->bid + OutBidAmount(row->bid))
    {
        reasonOut = BOOK_ERR_BID_INCREMENT;
        return VALIDATE_REJECT;
    }

    // (5) Below the start bid: legacy returns with NO result packet (:780-784).
    if (bidAmount < row->startbid)
    {
        reasonOut = BOOK_ERR_OK;
        return VALIDATE_REJECT_SILENT;
    }

    // (6) A bid at/over buyout must ride IPC_PLAYER_BUYOUT (mangosd routes by
    //     price >= buyout). Reaching here is a forwarder defect: silent reject,
    //     the caller logs the protocol fault.
    if (row->buyout != 0u && bidAmount >= row->buyout)
    {
        reasonOut = BOOK_ERR_OK;
        return VALIDATE_REJECT_SILENT;
    }

    reasonOut = BOOK_ERR_OK;
    return VALIDATE_ADMIT;
}

ValidationOutcome MutationHandler::ValidateBuyout(BookRow const* row, uint32 bidderGuid,
                                                  uint32 maxPrice, uint32 ownerAccount,
                                                  uint32 bidderAccount, uint8& reasonOut)
{
    reasonOut = AuctionBook::Admit(OP_BUYOUT, row);
    if (reasonOut != BOOK_ERR_OK)
    {
        return VALIDATE_REJECT;
    }

    if (row->owner == bidderGuid ||
        (ownerAccount != 0u && ownerAccount == bidderAccount))
    {
        reasonOut = BOOK_ERR_BID_OWN;
        return VALIDATE_REJECT;
    }

    // Legacy price<=bid guard (:756-762) applies to the buyout path too.
    if (maxPrice <= row->bid)
    {
        reasonOut = BOOK_ERR_HIGHER_BID;
        return VALIDATE_REJECT;
    }

    // No buyout on the row, or price below it: mangosd should have routed this
    // as a plain bid. Forwarder defect -> silent reject (caller logs).
    if (row->buyout == 0u || maxPrice < row->buyout)
    {
        reasonOut = BOOK_ERR_OK;
        return VALIDATE_REJECT_SILENT;
    }

    reasonOut = BOOK_ERR_OK;
    return VALIDATE_ADMIT;
}

PlayerMutationResult MutationHandler::OnSell(PlayerSellIntent const& in)
{
    PlayerMutationResult res = MakeResult(in.uuid, 0x40u, MUT_REJECTED,
                                          BOOK_ERR_DATABASE);

    // mangosd is the sole auction-ID allocator (spec decision 8): a colliding
    // ID is a protocol fault, rejected with the legacy internal-error shape.
    if (m_book.Find(in.auctionId) != NULL)
    {
        fprintf(stderr, "ah-service: protocol fault: IPC_PLAYER_SELL duplicate"
                        " auction id %u\n", in.auctionId);
        return res;
    }

    if (in.house < 1u || in.house > 7u)
    {
        fprintf(stderr, "ah-service: protocol fault: IPC_PLAYER_SELL invalid"
                        " houseid %u\n", static_cast<unsigned>(in.house));
        return res;
    }

    // 50-owned-listings cap, moved worker-side (spec I6). Legacy replies
    // AUCTION_ERR_DATABASE on AUCTION_STARTED (AuctionHouseHandler.cpp:579-601).
    if (m_book.CountOwned(in.sellerGuid, in.house) >= 50u)
    {
        return res;
    }

    BookRow row;
    row.id               = in.auctionId;
    row.houseId          = in.house;
    row.itemGuid         = in.itemGuid;
    row.itemTemplate     = in.itemTemplate;
    row.itemCount        = in.itemCount;
    row.randomPropertyId = in.randomPropertyId;
    row.owner            = in.sellerGuid;
    row.buyout           = in.buyout;
    row.expireTime       = static_cast<uint64>(in.expireTime);
    row.bidder           = 0u;
    row.bid              = 0u;
    row.startbid         = in.startbid;
    // Deposit computed by mangosd at reserve time; persisted verbatim (spec 4.1).
    row.deposit          = in.deposit;
    row.state            = BOOK_LIVE;

    if (!BeginCommit())
    {
        return res;
    }
    m_book.Insert(row);

    res.status = MUT_OK;
    res.reason = BOOK_ERR_OK;
    FillFacts(row, res.facts);

    JournalCommitted(in.uuid, in.auctionId, 0x40u, res);
    if (!FinishCommit())
    {
        m_book.RollbackInsert(in.auctionId);
        res.status = MUT_REJECTED;
        res.reason = BOOK_ERR_DATABASE;
        ClearFacts(res.facts);
        fprintf(stderr, "ah-service: sell commit failed for auction %u -"
                        " REJECTED err-database\n", in.auctionId);
        return res;
    }

    return res;
}

PlayerMutationResult MutationHandler::OnBid(PlayerBidIntent const& in)
{
    PlayerMutationResult res = MakeResult(in.uuid, 0x41u, MUT_REJECTED,
                                          BOOK_ERR_DATABASE);

    BookRow* row = m_book.Find(in.auctionId);
    if (row != NULL)
    {
        // REJECTED results still carry the AUCTION_ERR_HIGHER_BID data (spec 4.5).
        FillFacts(*row, res.facts);
    }

    uint32 const ownerAccount  = (row != NULL) ? LookupAccount(row->owner) : 0u;
    uint32 const bidderAccount = LookupAccount(in.bidderGuid);

    uint8 reason = BOOK_ERR_OK;
    ValidationOutcome const v = ValidateBid(row, in.bidderGuid, in.bidAmount,
                                            ownerAccount, bidderAccount, reason);
    if (v != VALIDATE_ADMIT)
    {
        if (v == VALIDATE_REJECT_SILENT && row != NULL &&
            row->buyout != 0u && in.bidAmount >= row->buyout)
        {
            fprintf(stderr, "ah-service: protocol fault: IPC_PLAYER_BID at/over"
                            " buyout (auction %u) - mangosd must route buyouts"
                            " on IPC_PLAYER_BUYOUT\n", in.auctionId);
        }
        res.reason = reason;   // 0 == silent legacy reject
        return res;
    }

    // Commit: auction UPDATE + journal COMMITTED, one checked txn (spec 4.1
    // step 3). Snapshot the prior bidder for the outbid-refund facts FIRST.
    uint32 const prevBidder = row->bidder;
    uint32 const prevBid    = row->bid;

    if (!BeginCommit())
    {
        return res;
    }
    m_book.UpdateBid(in.auctionId, in.bidderGuid, in.bidAmount);

    res.status = MUT_OK;
    res.reason = BOOK_ERR_OK;
    FillFacts(*m_book.Find(in.auctionId), res.facts);
    res.facts.priorBidderGuid = prevBidder;
    res.facts.priorBidAmount  = prevBid;
    res.facts.effectiveBid    = in.bidAmount;

    JournalCommitted(in.uuid, in.auctionId, 0x41u, res);
    if (!FinishCommit())
    {
        m_book.RollbackUpdateBid(in.auctionId, prevBidder, prevBid);
        res.status = MUT_REJECTED;
        res.reason = BOOK_ERR_DATABASE;
        FillFacts(*m_book.Find(in.auctionId), res.facts);
        fprintf(stderr, "ah-service: bid commit failed for auction %u -"
                        " REJECTED err-database\n", in.auctionId);
        return res;
    }

    return res;
}

PlayerMutationResult MutationHandler::OnBuyout(PlayerBuyoutIntent const& in)
{
    PlayerMutationResult res = MakeResult(in.uuid, 0x42u, MUT_REJECTED,
                                          BOOK_ERR_DATABASE);

    BookRow* row = m_book.Find(in.auctionId);
    if (row != NULL)
    {
        FillFacts(*row, res.facts);
    }

    uint32 const ownerAccount  = (row != NULL) ? LookupAccount(row->owner) : 0u;
    uint32 const bidderAccount = LookupAccount(in.bidderGuid);

    uint8 reason = BOOK_ERR_OK;
    ValidationOutcome const v = ValidateBuyout(row, in.bidderGuid, in.maxPrice,
                                               ownerAccount, bidderAccount, reason);
    if (v != VALIDATE_ADMIT)
    {
        if (v == VALIDATE_REJECT_SILENT)
        {
            fprintf(stderr, "ah-service: protocol fault: IPC_PLAYER_BUYOUT below"
                            " buyout or buyout-less (auction %u)\n", in.auctionId);
        }
        res.reason = reason;
        return res;
    }

    // The sold row leaves the book: DELETE + journal COMMITTED, one checked
    // txn. effectiveBid = min(maxPrice, buyout) -- the spec 4.1 I4 formula
    // verbatim (validation guarantees maxPrice >= buyout, so it equals buyout).
    BookRow const sold = *row;
    uint32 const effectiveBid = (in.maxPrice < sold.buyout) ? in.maxPrice
                                                            : sold.buyout;

    if (!BeginCommit())
    {
        return res;
    }
    m_book.Remove(in.auctionId);

    res.status = MUT_OK;
    res.reason = BOOK_ERR_OK;
    FillFacts(sold, res.facts);
    res.facts.effectiveBid  = effectiveBid;
    res.facts.curBidderGuid = in.bidderGuid;
    res.facts.curBid        = effectiveBid;
    // priorBidderGuid / priorBidAmount already carry the outbid-refund leg.

    JournalCommitted(in.uuid, in.auctionId, 0x42u, res);
    if (!FinishCommit())
    {
        m_book.RollbackRemove(sold);
        res.status = MUT_REJECTED;
        res.reason = BOOK_ERR_DATABASE;
        FillFacts(sold, res.facts);
        fprintf(stderr, "ah-service: buyout commit failed for auction %u -"
                        " REJECTED err-database\n", in.auctionId);
        return res;
    }

    return res;
}

uint64 MutationHandler::NextWorkerUuid()
{
    return (static_cast<uint64>(m_runId) << 32) | static_cast<uint64>(m_nextSeq++);
}

PlayerMutationResult MutationHandler::OnCancelPrepare(PlayerCancelPrepare const& in)
{
    PlayerMutationResult res = MakeResult(in.uuid, 0x43u, MUT_REJECTED,
                                          BOOK_ERR_DATABASE);

    BookRow* row = m_book.Find(in.auctionId);

    uint8 const admit = AuctionBook::Admit(OP_CANCEL_PREPARE, row);
    if (admit != BOOK_ERR_OK)
    {
        res.reason = admit;
        return res;
    }

    // Ownership: legacy replies AUCTION_ERR_DATABASE + a cheater log
    // (AuctionHouseHandler.cpp:921-926).
    if (row->owner != in.sellerGuid)
    {
        fprintf(stderr, "ah-service: CHEATER? guid %u tried to cancel"
                        " auction %u owned by %u\n",
                in.sellerGuid, in.auctionId, row->owner);
        return res;
    }

    // Build the MUT_PREPARED reply first: its encoded form is the journal
    // facts blob, so reconcile reads the exact snapshot mangosd saw.
    res.status = MUT_PREPARED;
    res.reason = BOOK_ERR_OK;
    FillFacts(*row, res.facts);   // curBid / curBidderGuid / deposit ride here

    // Lock DURABLY before replying (standalone checked commit -- plain
    // PExecute without a txn is async and would not be durable-before-reply).
    if (m_db != NULL)
    {
        ByteBuffer bb;
        res.Encode(bb);
        AhJournal::JournalRow j;
        j.uuid         = in.uuid;
        j.auctionId    = in.auctionId;
        j.kind         = 0x43u;
        j.state        = AhJournal::JRN_CANCEL_PREPARED;
        j.facts        = std::string(reinterpret_cast<const char*>(bb.contents()),
                                     bb.size());
        j.createdTime  = static_cast<uint64>(time(NULL));
        j.resolvedTime = 0u;

        if (!m_db->Character().BeginTransaction())
        {
            return MakeResult(in.uuid, 0x43u, MUT_REJECTED, BOOK_ERR_DATABASE);
        }
        AhJournal::Insert(*m_db, j);
        if (!m_db->Character().CommitTransactionChecked())
        {
            fprintf(stderr, "ah-service: cancel-prepare journal write failed"
                            " for auction %u - REJECTED err-database\n",
                    in.auctionId);
            return MakeResult(in.uuid, 0x43u, MUT_REJECTED, BOOK_ERR_DATABASE);
        }
    }

    row->state = BOOK_CANCEL_PREPARED;
    PrepareEntry pe;
    pe.auctionId  = in.auctionId;
    pe.sellerGuid = in.sellerGuid;
    pe.preparedAt = static_cast<uint64>(time(NULL));
    m_prepares[in.uuid] = pe;

    return res;
}

PlayerMutationResult MutationHandler::OnCancelDecide(uint64 uuid, uint32 auctionId,
                                                     bool confirm)
{
    uint8 const op = confirm ? 0x47u : 0x48u;

    PrepareMap::iterator it = m_prepares.find(uuid);
    if (it == m_prepares.end())
    {
        // Post-unlock CONFIRM/ABORT or unknown uuid: answer explicitly, never
        // silence (spec 4.2 v3 -- the worker answers EVERY confirm).
        return MakeResult(uuid, op, MUT_REJECTED_STALE, BOOK_ERR_OK);
    }

    if (it->second.auctionId != auctionId)
    {
        fprintf(stderr, "ah-service: protocol fault: cancel decide uuid/auction"
                        " mismatch (locked auction %u, frame auction %u)\n",
                it->second.auctionId, auctionId);
        return MakeResult(uuid, op, MUT_REJECTED_STALE, BOOK_ERR_OK);
    }

    BookRow* row = m_book.Find(auctionId);
    if (row == NULL || row->state != BOOK_CANCEL_PREPARED)
    {
        fprintf(stderr, "ah-service: cancel decide: auction %u not in prepared"
                        " state - dropping the stale lock entry\n", auctionId);
        m_prepares.erase(it);
        return MakeResult(uuid, op, MUT_REJECTED_STALE, BOOK_ERR_OK);
    }

    if (confirm)
    {
        // Commit the removal: DELETE + journal COMMITTED, one checked txn
        // (spec 4.2 step 3). Snapshot the row for the facts + rollback.
        BookRow const removed = *row;

        PlayerMutationResult res = MakeResult(uuid, op, MUT_OK, BOOK_ERR_OK);
        FillFacts(removed, res.facts);

        if (!BeginCommit())
        {
            return MakeResult(uuid, op, MUT_REJECTED, BOOK_ERR_DATABASE);
        }
        m_book.Remove(auctionId);
        if (m_db != NULL)
        {
            AhJournal::SetState(*m_db, uuid, AhJournal::JRN_COMMITTED,
                                static_cast<uint64>(time(NULL)));
        }
        if (!FinishCommit())
        {
            // DB rolled back: restore the prepared row in memory. The lock
            // stays armed and the timeout sweep recovers it; mangosd releases
            // its cut reservation on the err-database reply.
            m_book.RollbackRemove(removed);
            fprintf(stderr, "ah-service: cancel confirm commit failed for"
                            " auction %u - row stays prepared until timeout\n",
                    auctionId);
            return MakeResult(uuid, op, MUT_REJECTED, BOOK_ERR_DATABASE);
        }

        m_prepares.erase(it);
        return res;
    }

    // ABORT: retire the journal row durably, then unlock in memory. The
    // auction is untouched; the seller gets the legacy silent-return behavior
    // (spec 4.2 step 2 -- mangosd emits nothing on an ABORT it initiated).
    if (m_db != NULL)
    {
        if (!m_db->Character().BeginTransaction())
        {
            return MakeResult(uuid, op, MUT_REJECTED, BOOK_ERR_DATABASE);
        }
        AhJournal::SetState(*m_db, uuid, AhJournal::JRN_APPLIED,
                            static_cast<uint64>(time(NULL)));
        if (!m_db->Character().CommitTransactionChecked())
        {
            // Journal still says CANCEL_PREPARED: keep the memory lock so
            // journal and book agree; the timeout sweep unlocks it via the
            // journal-anchored resolution.
            fprintf(stderr, "ah-service: cancel abort journal write failed for"
                            " auction %u - lock kept until timeout\n",
                    auctionId);
            return MakeResult(uuid, op, MUT_REJECTED, BOOK_ERR_DATABASE);
        }
    }
    row->state = BOOK_LIVE;
    m_prepares.erase(it);
    return MakeResult(uuid, op, MUT_OK, BOOK_ERR_OK);
}

void MutationHandler::AdoptActiveJournal(std::vector<AhJournal::JournalRow> const& rows)
{
    for (size_t i = 0; i < rows.size(); ++i)
    {
        AhJournal::JournalRow const& j = rows[i];
        if (j.state != AhJournal::JRN_CANCEL_PREPARED)
        {
            continue;
        }
        BookRow* row = m_book.Find(j.auctionId);
        if (row == NULL)
        {
            continue;   // already logged by the book load
        }
        PrepareEntry pe;
        pe.auctionId  = j.auctionId;
        pe.sellerGuid = row->owner;
        pe.preparedAt = j.createdTime;
        m_prepares[j.uuid] = pe;
    }
}

void MutationHandler::CheckPrepareTimeouts(uint64 nowSecs)
{
    std::vector<uint64> expired;
    for (PrepareMap::const_iterator it = m_prepares.begin();
         it != m_prepares.end(); ++it)
    {
        if (it->second.preparedAt + CANCEL_PREPARE_TIMEOUT_SECS <= nowSecs)
        {
            expired.push_back(it->first);
        }
    }

    for (size_t i = 0; i < expired.size(); ++i)
    {
        uint64 const prepareUuid = expired[i];
        PrepareMap::iterator pit = m_prepares.find(prepareUuid);
        if (pit == m_prepares.end())
        {
            continue;
        }
        PrepareEntry const pe = pit->second;

        BookRow* row = m_book.Find(pe.auctionId);
        if (row == NULL || row->state != BOOK_CANCEL_PREPARED)
        {
            m_prepares.erase(pit);
            continue;
        }

        // Journal-anchored unlock (spec 4.2 v3): retire the prepare row and
        // insert the RESOLVE_CANCELLED_UNLOCK resolution in ONE txn. The
        // one-ACTIVE-per-auction invariant holds throughout, and a late
        // CONFIRM now finds no armed lock -> MUT_REJECTED_STALE.
        ResolveApply ra;
        ra.uuid = NextWorkerUuid();
        ra.kind = RESOLVE_CANCELLED_UNLOCK;
        FillFacts(*row, ra.facts);

        if (m_db != NULL)
        {
            ByteBuffer bb;
            ra.Encode(bb);
            AhJournal::JournalRow j;
            j.uuid         = ra.uuid;
            j.auctionId    = pe.auctionId;
            j.kind         = RESOLVE_CANCELLED_UNLOCK;
            j.state        = AhJournal::JRN_RESOLVING;
            j.facts        = std::string(reinterpret_cast<const char*>(bb.contents()),
                                         bb.size());
            j.createdTime  = nowSecs;
            j.resolvedTime = 0u;

            if (!m_db->Character().BeginTransaction())
            {
                continue;   // retry next sweep
            }
            AhJournal::SetState(*m_db, prepareUuid, AhJournal::JRN_APPLIED,
                                nowSecs);
            AhJournal::Insert(*m_db, j);
            if (!m_db->Character().CommitTransactionChecked())
            {
                fprintf(stderr, "ah-service: prepare-timeout unlock txn failed"
                                " for auction %u - retrying next sweep\n",
                        pe.auctionId);
                continue;
            }
        }

        row->state = BOOK_LIVE;
        m_prepares.erase(pit);
        m_resolveQueue.push_back(ra);
        printf("ah-service: cancel prepare timed out for auction %u -"
               " unlocked via RESOLVE_CANCELLED_UNLOCK\n", pe.auctionId);
    }
}

bool MutationHandler::PopQueuedResolve(ResolveApply& out)
{
    if (m_resolveQueue.empty())
    {
        return false;
    }
    out = m_resolveQueue.front();
    m_resolveQueue.pop_front();
    return true;
}
