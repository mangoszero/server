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
#include "AuctionIntents.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{
    // [SP-2 Task 15] worker-side crash-injection seam. The worker has no game
    // headers (no CustodyService), so this is a self-contained equivalent of
    // CustodyService::MaybeCrash: if getenv("AH_WORKER_CRASH_AT") == @p phase,
    // flush stdio and _exit(3) to simulate worker death at that transition.
    // Inert when the env var is unset (the live default), so it never fires on a
    // real realm.
    void AhWorkerMaybeCrash(const char* phase)
    {
        const char* armed = getenv("AH_WORKER_CRASH_AT");
        if (armed != NULL && phase != NULL && strcmp(armed, phase) == 0)
        {
            fprintf(stderr, "ah-service: worker crash-injection: _exit(3) "
                "at phase '%s' (TEST ONLY)\n", phase);
            fflush(NULL);   // flush ALL stdio streams -- _exit skips cleanup
            _exit(3);
        }
    }
}

MutationHandler::MutationHandler(AuctionBook& book, ServiceDatabase* db, uint32 runId)
    : m_book(book), m_db(db), m_runId(runId), m_nextSeq(0x80000000u),
      m_gameTimeNow(0)
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
    bool const committed = m_db->Character().CommitTransactionChecked();
    if (committed)
    {
        // [SP-2 Task 15] "worker-committed-pre-reply": the book + journal COMMITTED
        // row are durable, but the dispatch loop has NOT yet sent IPC_PLAYER_RESULT.
        // Reconcile must finalize-forward from the journal COMMITTED row on the
        // mangosd side. Inert unless AH_WORKER_CRASH_AT is set.
        AhWorkerMaybeCrash("worker-committed-pre-reply");
    }
    return committed;
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

    // Legacy price<=bid guard (:756-762) applies to both the win and the
    // below-buyout bid leg.
    if (maxPrice <= row->bid)
    {
        reasonOut = BOOK_ERR_HIGHER_BID;
        return VALIDATE_REJECT;
    }

    // At/over buyout on a row that HAS a buyout -> BUYOUT WIN: admit; the caller
    // removes the row and reports effectiveBid == buyout.
    if (row->buyout != 0u && maxPrice >= row->buyout)
    {
        reasonOut = BOOK_ERR_OK;
        return VALIDATE_ADMIT;
    }

    // Below buyout (or buyout-less): mangosd fully reserved maxPrice and the
    // worker adjudicates it as a NORMAL BID at maxPrice (spec 4.1: "buy out if
    // maxPrice >= buyout, else place a normal bid at maxPrice"). Apply the
    // remaining legacy bid checks to maxPrice; the caller commits it as a bid.

    // Minimum increment (:764-771) -- unconditional here (always below buyout).
    if (maxPrice < row->bid + OutBidAmount(row->bid))
    {
        reasonOut = BOOK_ERR_BID_INCREMENT;
        return VALIDATE_REJECT;
    }

    // Below the start bid: legacy returns with NO result packet (:780-784).
    if (maxPrice < row->startbid)
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

    // Commit the bid (shared with the below-buyout IPC_PLAYER_BUYOUT leg).
    return CommitBidAt(in.auctionId, in.bidderGuid, in.bidAmount, 0x41u, in.uuid);
}

PlayerMutationResult MutationHandler::CommitBidAt(uint32 auctionId, uint32 bidder,
                                                  uint32 amount, uint8 op, uint64 uuid)
{
    PlayerMutationResult res = MakeResult(uuid, op, MUT_REJECTED, BOOK_ERR_DATABASE);

    // Caller guarantees the row is present and LIVE (validation ran to ADMIT).
    BookRow* row = m_book.Find(auctionId);
    if (row != NULL)
    {
        FillFacts(*row, res.facts);
    }

    // Commit: auction UPDATE + journal COMMITTED, one checked txn (spec 4.1
    // step 3). Snapshot the prior bidder for the outbid-refund facts FIRST.
    uint32 const prevBidder = (row != NULL) ? row->bidder : 0u;
    uint32 const prevBid    = (row != NULL) ? row->bid : 0u;

    if (!BeginCommit())
    {
        return res;
    }
    m_book.UpdateBid(auctionId, bidder, amount);

    res.status = MUT_OK;
    res.reason = BOOK_ERR_OK;
    FillFacts(*m_book.Find(auctionId), res.facts);
    res.facts.priorBidderGuid = prevBidder;
    res.facts.priorBidAmount  = prevBid;
    res.facts.effectiveBid    = amount;

    JournalCommitted(uuid, auctionId, op, res);
    if (!FinishCommit())
    {
        m_book.RollbackUpdateBid(auctionId, prevBidder, prevBid);
        res.status = MUT_REJECTED;
        res.reason = BOOK_ERR_DATABASE;
        FillFacts(*m_book.Find(auctionId), res.facts);
        fprintf(stderr, "ah-service: bid commit failed for auction %u -"
                        " REJECTED err-database\n", auctionId);
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
        // A silent reject here is a genuine below-startbid on the bid leg, not a
        // forwarder defect: a below-buyout IPC_PLAYER_BUYOUT is a normal bid
        // (spec 4.1), and only its below-startbid case is legacy-silent.
        res.reason = reason;
        return res;
    }

    // spec 4.1: IPC_PLAYER_BUYOUT means "buy out if maxPrice >= buyout, else
    // place a normal BID at maxPrice". Below buyout (or buyout-less) commits as
    // a bid at maxPrice; only a genuine at/over-buyout is the removing WIN.
    // effectiveBid < buyout (or buyout == 0) is the signal mangosd reads as
    // "bid, not win"; effectiveBid == buyout != 0 means WIN.
    bool const isWin = (row->buyout != 0u && in.maxPrice >= row->buyout);
    if (!isWin)
    {
        // Below-buyout normal bid at maxPrice, op stays 0x42 (originating
        // opcode). Same UpdateBid + JRN_COMMITTED path as OnBid, prior* =
        // displaced bidder, effectiveBid = maxPrice, row stays LIVE.
        return CommitBidAt(in.auctionId, in.bidderGuid, in.maxPrice, 0x42u, in.uuid);
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

// ---------------------------------------------------------------------------
// SP-2 Task 7: expiry/win tick + resolve outbox
// ---------------------------------------------------------------------------

uint64 MutationHandler::NextUuid()
{
    // Shares NextWorkerUuid()'s counter (m_nextSeq, already based at
    // 0x80000000u): both mint uuids in the same worker-minted half of this
    // run-id's sequence space, so they must never diverge into two counters.
    return NextWorkerUuid();
}

void MutationHandler::SetGameTime(uint64 now)
{
    m_gameTimeNow = now;
}

uint64 MutationHandler::GameTime() const
{
    return m_gameTimeNow;
}

size_t MutationHandler::ResolvingCount() const
{
    return m_resolving.size();
}

bool MutationHandler::HasActiveResolution(uint32 auctionId) const
{
    return m_resolvingAuctions.find(auctionId) != m_resolvingAuctions.end();
}

bool MutationHandler::IsTerminalResolveKind(uint8 kind)
{
    return kind == static_cast<uint8>(RESOLVE_WON)
        || kind == static_cast<uint8>(RESOLVE_EXPIRED_NOBID);
}

MutationFacts MutationHandler::FactsFromRow(BookRow const& row)
{
    MutationFacts f;
    f.auctionId        = row.id;
    f.houseId          = row.houseId;
    f.itemGuid         = row.itemGuid;
    f.itemTemplate     = row.itemTemplate;
    f.randomPropertyId = row.randomPropertyId;
    f.itemCount        = row.itemCount;
    f.sellerGuid       = row.owner;
    f.deposit          = row.deposit;
    f.effectiveBid     = row.bid;
    f.priorBidderGuid  = 0;
    f.priorBidAmount   = 0;
    f.curBidderGuid    = row.bidder;
    f.curBid           = row.bid;
    f.buyout           = row.buyout;
    return f;
}

void MutationHandler::QueueSend(IpcMessage const& msg)
{
    m_outbound.push_back(msg);
}

void MutationHandler::TakeOutbound(std::vector<IpcMessage>& out)
{
    out.insert(out.end(), m_outbound.begin(), m_outbound.end());
    m_outbound.clear();
}

bool MutationHandler::JournalInsertResolving(AhJournal::JournalRow const& row)
{
    if (m_db == NULL)
    {
        return true;   // selftest mode (TestMutationHandler overrides this)
    }

    // AhJournal::Insert() only APPENDS a PExecute to whatever transaction is
    // currently open on this connection (Journal.h contract); with no caller
    // transaction that PExecute is delay-thread ASYNC, not durable. Wrap it in
    // its own checked commit -- the OnCancelPrepare standalone-durable idiom
    // -- so the RESOLVING mark is truly on disk BEFORE any send (M1).
    if (!m_db->Character().BeginTransaction())
    {
        return false;
    }
    AhJournal::Insert(*m_db, row);
    return m_db->Character().CommitTransactionChecked();
}

bool MutationHandler::CommitTerminalApply(uint64 uuid, uint32 auctionId)
{
    if (m_db == NULL)
    {
        return true;   // selftest mode (TestMutationHandler overrides this)
    }

    DatabaseType& db = m_db->Character();
    if (!db.BeginTransaction())
    {
        return false;
    }
    db.PExecute("DELETE FROM `auction` WHERE `id` = '%u'", auctionId);
    AhJournal::SetState(*m_db, uuid,
                        static_cast<uint8>(AhJournal::JRN_APPLIED),
                        m_gameTimeNow);
    return db.CommitTransactionChecked();
}

bool MutationHandler::JournalMarkApplied(uint64 uuid)
{
    if (m_db == NULL)
    {
        return true;   // selftest mode (TestMutationHandler overrides this)
    }

    if (!m_db->Character().BeginTransaction())
    {
        return false;
    }
    AhJournal::SetState(*m_db, uuid,
                        static_cast<uint8>(AhJournal::JRN_APPLIED),
                        m_gameTimeNow);
    return m_db->Character().CommitTransactionChecked();
}

void MutationHandler::TrackAndSend(uint64 uuid, uint32 auctionId, uint8 kind,
                                   std::string const& wire, uint64 now)
{
    ResolvingEntry e;
    e.auctionId   = auctionId;
    e.kind        = kind;
    e.attempts    = 1u;
    e.lastSentSec = now;
    e.wire        = wire;
    m_resolving[uuid] = e;
    m_resolvingAuctions.insert(auctionId);

    IpcMessage msg;
    msg.op = IPC_RESOLVE_APPLY;
    msg.body.append(reinterpret_cast<const uint8*>(wire.data()), wire.size());
    QueueSend(msg);
}

uint64 MutationHandler::QueueResolution(uint32 auctionId, uint8 kind,
                                        MutationFacts const& facts, uint64 now)
{
    if (HasActiveResolution(auctionId))
    {
        return 0;   // invariant: at most one ACTIVE journal row per auction
    }
    if (m_resolving.size() >= RESOLVE_WINDOW)
    {
        return 0;   // window closed (decision-10 flow control)
    }

    ResolveApply ra;
    ra.uuid  = NextUuid();
    ra.kind  = kind;
    ra.facts = facts;

    ByteBuffer wireBuf;
    ra.Encode(wireBuf);
    std::string wire(reinterpret_cast<const char*>(wireBuf.contents()),
                     wireBuf.size());

    AhJournal::JournalRow jr;
    jr.uuid         = ra.uuid;
    jr.auctionId    = auctionId;
    jr.kind         = kind;
    jr.state        = static_cast<uint8>(AhJournal::JRN_RESOLVING);
    jr.facts        = wire;
    jr.createdTime  = now;
    jr.resolvedTime = 0;

    // M1: the RESOLVING mark is a STANDALONE durable write BEFORE the send.
    if (!JournalInsertResolving(jr))
    {
        return 0;
    }

    if (IsTerminalResolveKind(kind))
    {
        BookRow* row = m_book.Find(auctionId);
        if (row != nullptr)
        {
            row->state = static_cast<uint8>(BOOK_RESOLVING);
        }
    }

    TrackAndSend(ra.uuid, auctionId, kind, wire, now);
    return ra.uuid;
}

void MutationHandler::Tick(uint64 gameTimeNow)
{
    SetGameTime(gameTimeNow);

    // Task 6/7 handoff: prepare-timeout unlocks (RESOLVE_CANCELLED_UNLOCK)
    // are minted -- and already journaled JRN_RESOLVING, inside
    // CheckPrepareTimeouts's own txn -- via the m_resolveQueue seam. Hand
    // each to the SAME outbox tracking so OnResolveAck can retire it exactly
    // like a tick-minted resolution; no re-journal (already durable).
    ResolveApply queued;
    while (PopQueuedResolve(queued))
    {
        ByteBuffer wireBuf;
        queued.Encode(wireBuf);
        std::string wire(reinterpret_cast<const char*>(wireBuf.contents()),
                         wireBuf.size());
        TrackAndSend(queued.uuid, queued.facts.auctionId, queued.kind, wire,
                    gameTimeNow);
    }

    std::vector<uint32> expired;
    m_book.VisitExpired(gameTimeNow, expired);

    uint32 minted = 0;
    for (size_t i = 0; i < expired.size(); ++i)
    {
        if (minted >= RESOLVE_BUDGET_PER_TICK)
        {
            break;   // per-tick resolution budget (decision 10)
        }
        if (m_resolving.size() >= RESOLVE_WINDOW)
        {
            break;   // un-acked window closed; a later tick resumes
        }

        BookRow* row = m_book.Find(expired[i]);
        if (row == nullptr || row->state != static_cast<uint8>(BOOK_LIVE))
        {
            continue;
        }
        if (HasActiveResolution(row->id))
        {
            continue;   // e.g. an un-acked bot-outbid refund on a LIVE row
        }

        // bidder==0 with bid!=0 is a BOT-held bid (contract 3): still WON;
        // mangosd's finalize takes the legacy destroy branch for it.
        MutationFacts facts = FactsFromRow(*row);
        const uint8 kind = (row->bid != 0u)
            ? static_cast<uint8>(RESOLVE_WON)
            : static_cast<uint8>(RESOLVE_EXPIRED_NOBID);

        if (QueueResolution(row->id, kind, facts, gameTimeNow) != 0u)
        {
            ++minted;
        }
    }
}

void MutationHandler::OnResolveAck(ResolveAck const& ack)
{
    std::map<uint64, ResolvingEntry>::iterator it = m_resolving.find(ack.uuid);
    if (it == m_resolving.end())
    {
        fprintf(stderr, "ah-service: PROTOCOL FAULT - IPC_RESOLVE_ACK for"
                        " unknown uuid=%08x:%08x (status=%u) - ignored\n",
                static_cast<unsigned>(ack.uuid >> 32),
                static_cast<unsigned>(ack.uuid & 0xFFFFFFFFu),
                static_cast<unsigned>(ack.status));
        return;
    }

    ResolvingEntry& e = it->second;

    if (ack.status == static_cast<uint8>(RES_FAILED))
    {
        // mangosd's finalize failed durably: the entry STAYS RESOLVING and
        // the book row is never deleted on FAILED (spec 4.3); the resend
        // cadence retries it. Distinct from a fact-mismatch protocol fault,
        // which mangosd never acks FAILED (decision 10).
        fprintf(stderr, "ah-service: resolve uuid=%08x:%08x auction=%u acked"
                        " FAILED - will retry on the cadence\n",
                static_cast<unsigned>(ack.uuid >> 32),
                static_cast<unsigned>(ack.uuid & 0xFFFFFFFFu), e.auctionId);
        return;
    }

    if (ack.status != static_cast<uint8>(RES_APPLIED) &&
        ack.status != static_cast<uint8>(RES_DUPLICATE))
    {
        fprintf(stderr, "ah-service: PROTOCOL FAULT - unknown ResolveAckStatus"
                        " %u for uuid=%08x:%08x - ignored\n",
                static_cast<unsigned>(ack.status),
                static_cast<unsigned>(ack.uuid >> 32),
                static_cast<unsigned>(ack.uuid & 0xFFFFFFFFu));
        return;
    }

    // APPLIED and DUPLICATE are equivalent (spec 4.3: DUPLICATE just means
    // the first APPLIED ack was lost).
    if (IsTerminalResolveKind(e.kind))
    {
        // Terminal kinds: journal APPLIED + auction-row DELETE in ONE txn.
        if (!CommitTerminalApply(ack.uuid, e.auctionId))
        {
            // Local durable apply failed: keep RESOLVING; the cadence
            // re-sends, mangosd answers DUPLICATE, and we retry this commit.
            fprintf(stderr, "ah-service: terminal apply txn FAILED for"
                            " uuid=%08x:%08x auction=%u - kept RESOLVING\n",
                    static_cast<unsigned>(ack.uuid >> 32),
                    static_cast<unsigned>(ack.uuid & 0xFFFFFFFFu),
                    e.auctionId);
            return;
        }
        m_book.RemoveMemoryOnly(e.auctionId);
    }
    else
    {
        // Non-terminal kinds (CANCELLED_UNLOCK, REPAIR_RETURN): journal-only;
        // the book row persists (spec 4.3 [v3 I2/I3]).
        if (!JournalMarkApplied(ack.uuid))
        {
            fprintf(stderr, "ah-service: journal APPLIED mark FAILED for"
                            " uuid=%08x:%08x - kept RESOLVING\n",
                    static_cast<unsigned>(ack.uuid >> 32),
                    static_cast<unsigned>(ack.uuid & 0xFFFFFFFFu));
            return;
        }

        // Defensive (F1 belt-and-suspenders): a boot before the BuildFromRows
        // fix could have mis-frozen this LIVE row BOOK_RESOLVING. Un-freeze it
        // here too, so an old freeze self-heals the next time its resolution
        // acks even without a restart. No-op in normal operation, since the
        // row never left BOOK_LIVE for a non-terminal kind.
        BookRow* row = m_book.Find(e.auctionId);
        if (row != NULL && row->state == static_cast<uint8>(BOOK_RESOLVING))
        {
            row->state = BOOK_LIVE;
        }
    }

    m_resolvingAuctions.erase(e.auctionId);
    m_resolving.erase(it);
}

void MutationHandler::ResendStaleResolving(uint64 now)
{
    std::map<uint64, ResolvingEntry>::iterator it = m_resolving.begin();
    for (; it != m_resolving.end(); ++it)
    {
        ResolvingEntry& e = it->second;
        const uint64 age =
            (now > e.lastSentSec) ? (now - e.lastSentSec) : 0u;
        if (age < RESOLVE_RESEND_SEC)
        {
            continue;
        }

        // Safe: mangosd's resolve:<uuid> applied-record makes re-delivery
        // idempotent (spec 4.3 [v3 C1]).
        IpcMessage msg;
        msg.op = IPC_RESOLVE_APPLY;
        msg.body.append(reinterpret_cast<const uint8*>(e.wire.data()),
                        e.wire.size());
        QueueSend(msg);
        e.lastSentSec = now;
        ++e.attempts;

        if (e.attempts > RESOLVE_STUCK_ALARM_ATTEMPTS)
        {
            fprintf(stderr, "ah-service: STUCK RESOLUTION uuid=%08x:%08x"
                            " auction=%u attempts=%u - mangosd is not"
                            " acking\n",
                    static_cast<unsigned>(it->first >> 32),
                    static_cast<unsigned>(it->first & 0xFFFFFFFFu),
                    e.auctionId, e.attempts);
        }
    }
}

void MutationHandler::PrimeResolvingFromJournal(
    std::vector<AhJournal::JournalRow> const& activeJournal)
{
    uint32 primed = 0;
    for (size_t i = 0; i < activeJournal.size(); ++i)
    {
        AhJournal::JournalRow const& jr = activeJournal[i];
        if (jr.state != static_cast<uint8>(AhJournal::JRN_RESOLVING))
        {
            continue;
        }

        // [FIX A] Cross-restart runId-reuse hazard. The supervisor's runId is
        // NOT persistent: it resets to 1 on every mangosd process restart, and
        // the minter (m_nextSeq) restarts at 0x80000000. An adopted JRN_RESOLVING
        // row keeps its ORIGINAL worker-minted uuid but never advanced the fresh
        // minter, so if this run reuses the runId that minted an adopted row,
        // NextWorkerUuid could re-mint that row's low word for a DIFFERENT
        // auction -> mangosd's ResolutionApplied(uuid) is already true ->
        // RES_DUPLICATE -> that auction's value is silently dropped while the
        // worker deletes its book row (gold/item loss). Skip the minter past any
        // adopted low word owned by THIS run's id so a reused runId cannot
        // collide: m_nextSeq = max(m_nextSeq, low32(uuid) + 1).
        if (static_cast<uint32>(jr.uuid >> 32) == m_runId)
        {
            uint32 const adoptedSeq = static_cast<uint32>(jr.uuid & 0xFFFFFFFFu);
            if (adoptedSeq != 0xFFFFFFFFu && m_nextSeq <= adoptedSeq)
            {
                m_nextSeq = adoptedSeq + 1u;
            }
        }

        // 4.3 at-least-once: track it and re-send the stored ResolveApply
        // blob verbatim, immediately (the first loop pass drains it).
        TrackAndSend(jr.uuid, jr.auctionId, jr.kind, jr.facts,
                     0u /* lastSentSec: pre-clock boot send */);
        ++primed;
    }
    if (primed != 0u)
    {
        printf("ah-service: boot re-sent %u RESOLVING journal entrie(s)\n",
               primed);
    }
}

void MutationHandler::SeedMinterPast(uint32 maxSeq)
{
    // Generalizes [FIX A] (see PrimeResolvingFromJournal) to EVERY retained
    // journal row in THIS minter's half of the seq space -- not just adopted
    // JRN_RESOLVING ones. This minter owns the HIGH half [0x80000000+]; the
    // BotBrain minter owns the low half and is seeded separately by
    // BotBrain::SeedSeqPast. maxSeq = MAX high-half seq for this runId
    // (AhJournal::MaxSeqForRunId(highHalf=true), 0 = none). Terminal JRN_APPLIED
    // rows are retained until pruned, and LoadActive (state IN 1,2,4,5) never
    // returns them, so after a restart that reuses runId=1 the minter -- which
    // restarts at 0x80000000 -- would re-mint a retained uuid and its journal
    // INSERT would hit a duplicate PRIMARY KEY. 0xFFFFFFFF is the seq-exhaustion
    // sentinel: never advance the minter into wraparound.
    if (maxSeq != 0u && maxSeq != 0xFFFFFFFFu && m_nextSeq <= maxSeq)
    {
        m_nextSeq = maxSeq + 1u;
    }
}

// ---------------------------------------------------------------------------
// SP-2 Task 8: bot fold-in (in-process book writes + sell materialization)
// ---------------------------------------------------------------------------

uint8 MutationHandler::WireHouseToHouseId(uint8 house)
{
    // AuctionIntentExecutor.cpp:175-186 mapping: 0->1, 1->6, 2->7.
    switch (house)
    {
        case 0u: return 1u;
        case 1u: return 6u;
        default: return 7u;
    }
}

bool MutationHandler::OnBotBid(uint64 uuid, uint32 auctionId, uint32 bidAmount)
{
    BookRow* row = m_book.Find(auctionId);
    // Admission (existence + lock state) via the shared table: a CANCEL_PREPARED
    // or RESOLVING row rejects exactly as the legacy race-loser (spec 4.3b).
    if (row == NULL || AuctionBook::Admit(OP_BID, row) != BOOK_ERR_OK)
    {
        return false;
    }
    // A row with an un-acked resolution (e.g. a bot-outbid refund) is locked to
    // bot ops until the resolution acks (header contract / section-3 invariant).
    if (HasActiveResolution(auctionId))
    {
        return false;
    }
    if (bidAmount <= row->bid ||
        (row->buyout != 0u && bidAmount >= row->buyout))
    {
        return false;   // must beat current; at/over buyout is a buyout, not a bid
    }

    uint32 const prevBidder = row->bidder;
    uint32 const prevBid    = row->bid;
    if (prevBidder != 0u)
    {
        // Displaces a REAL player: co-commit the book bid + a JRN_RESOLVING
        // refund row (PersistBotBidDisplacing), then track+send the non-terminal
        // RESOLVE_REPAIR_RETURN. bidder=0 (the bot); prior fields carry the
        // refund target. The row stays LIVE, locked until the refund acks.
        uint64 const rUuid = NextUuid();
        ResolveApply ra;
        ra.uuid = rUuid;
        ra.kind = static_cast<uint8>(RESOLVE_REPAIR_RETURN);
        FillFacts(*row, ra.facts);
        ra.facts.priorBidderGuid = prevBidder;
        ra.facts.priorBidAmount  = prevBid;
        ra.facts.curBidderGuid   = 0u;
        ra.facts.curBid          = bidAmount;
        ra.facts.effectiveBid    = bidAmount;

        ByteBuffer bb;
        ra.Encode(bb);
        std::string blob(reinterpret_cast<const char*>(bb.contents()), bb.size());

        if (!PersistBotBidDisplacing(*row, bidAmount, rUuid, blob))
        {
            return false;   // nothing sent; memory untouched
        }
        row->bidder = 0u;
        row->bid    = bidAmount;
        row->state  = static_cast<uint8>(BOOK_LIVE);

        // The refund row is already durable (co-committed above): track+send
        // WITHOUT re-journaling (unlike QueueResolution, which journals).
        TrackAndSend(rUuid, auctionId,
                     static_cast<uint8>(RESOLVE_REPAIR_RETURN), blob, GameTime());
        return true;
    }

    // No displaced player (empty row or the bot raising its own held bid):
    // direct-applied, no wire traffic (spec 6 / section 3).
    if (!PersistBotBidSimple(*row, bidAmount, uuid))
    {
        return false;
    }
    row->bidder = 0u;
    row->bid    = bidAmount;
    return true;
}

bool MutationHandler::OnBotBuyout(uint64 uuid, uint32 auctionId)
{
    BookRow* row = m_book.Find(auctionId);
    if (row == NULL || AuctionBook::Admit(OP_BUYOUT, row) != BOOK_ERR_OK ||
        row->buyout == 0u)
    {
        return false;
    }
    if (HasActiveResolution(auctionId))
    {
        return false;
    }

    // Terminal RESOLVE_WON with curBidderGuid = 0 => mangosd's finalize takes
    // the legacy bot-win destroy branch. Any prior player bidder rides the
    // refund leg (prior* fields). QueueResolution journals JRN_RESOLVING
    // durably, marks the row BOOK_RESOLVING, and tracks+sends.
    MutationFacts facts;
    FillFacts(*row, facts);
    facts.priorBidderGuid = row->bidder;
    facts.priorBidAmount  = row->bid;
    facts.curBidderGuid   = 0u;
    facts.curBid          = row->buyout;
    facts.effectiveBid    = row->buyout;

    if (QueueResolution(auctionId, static_cast<uint8>(RESOLVE_WON), facts,
                        GameTime()) == 0u)
    {
        return false;   // active resolution / window closed / journal failed
    }
    (void)uuid;
    return true;
}

bool MutationHandler::BotSellBegin(SellIntent const& si)
{
    // Journal the intent PENDING (facts = encoded SellIntent) durably BEFORE the
    // send, so a crash after the send still replays the materialization request.
    // auctionId is unknown until mangosd allocates it (sole allocator, spec 8).
    ByteBuffer bb;
    si.Encode(bb);

    AhJournal::JournalRow row;
    row.uuid         = si.uuid;
    row.auctionId    = 0u;
    row.kind         = static_cast<uint8>(JKIND_BOT_SELL);
    row.state        = static_cast<uint8>(AhJournal::JRN_INTENT_PENDING);
    row.facts        = std::string(reinterpret_cast<const char*>(bb.contents()),
                                   bb.size());
    row.createdTime  = static_cast<uint64>(time(NULL));
    row.resolvedTime = 0u;

    if (!PersistIntentPending(row))
    {
        return false;
    }

    // In-memory copy: repurpose resolvedTime as the last-send anchor.
    row.resolvedTime = row.createdTime;
    m_pendingSells[si.uuid] = row;

    IpcMessage msg;
    msg.op = IPC_INTENT_SELL;
    si.Encode(msg.body);
    QueueSend(msg);
    return true;
}

void MutationHandler::OnBotSellResult(uint64 uuid, uint8 status, uint8 reason,
                                      uint32 itemGuid, uint32 auctionId)
{
    std::map<uint64, AhJournal::JournalRow>::iterator it =
        m_pendingSells.find(uuid);
    if (it == m_pendingSells.end())
    {
        // No pending sell: a duplicate/late reply after we already committed
        // (idempotent), or a stray result for a non-bot intent. Ignore.
        return;
    }

    if (status != static_cast<uint8>(INTENT_OK))
    {
        // Rejected: retire the pending row and drop it; mangosd's orphan sweep
        // (Task 13) reaps any item it minted before failing.
        fprintf(stderr, "ah-service: bot sell uuid=%08x:%08x rejected"
                        " (reason=%u) - dropping pending\n",
                static_cast<unsigned>(uuid >> 32),
                static_cast<unsigned>(uuid & 0xFFFFFFFFu),
                static_cast<unsigned>(reason));
        RetireIntentPending(uuid);
        m_pendingSells.erase(it);
        return;
    }

    // [FIX B] Belt-and-suspenders: INTENT_OK must carry a real item guid AND a
    // real auction id. A zero in either (only a first-party mangosd bug could
    // produce it) would build a book row with id 0; fail safe instead -- treat
    // it as a rejection (retire the pending + drop it), never a book commit.
    if (itemGuid == 0u || auctionId == 0u)
    {
        fprintf(stderr, "ah-service: bot sell uuid=%08x:%08x INTENT_OK with"
                        " zero id (item=%u auction=%u) - rejecting, not"
                        " committing\n",
                static_cast<unsigned>(uuid >> 32),
                static_cast<unsigned>(uuid & 0xFFFFFFFFu),
                itemGuid, auctionId);
        RetireIntentPending(uuid);
        m_pendingSells.erase(it);
        return;
    }

    // INTENT_OK: a duplicate reply for an already-committed listing is idempotent.
    if (m_book.Find(auctionId) != NULL)
    {
        m_pendingSells.erase(it);
        return;
    }

    SellIntent si;
    ByteBuffer bb;
    bb.append(reinterpret_cast<const uint8*>(it->second.facts.data()),
              it->second.facts.size());
    if (!si.Decode(bb))
    {
        fprintf(stderr, "ah-service: bot sell uuid=%08x:%08x - undecodable"
                        " pending facts, dropping\n",
                static_cast<unsigned>(uuid >> 32),
                static_cast<unsigned>(uuid & 0xFFFFFFFFu));
        RetireIntentPending(uuid);
        m_pendingSells.erase(it);
        return;
    }

    // Build the listing from the stored intent + the mangosd-minted item guid
    // and allocated auction id (decision 8). Deposit/expire were computed by
    // mangosd at reserve time; the worker only needs a coherent book image.
    BookRow row;
    row.id               = auctionId;
    row.houseId          = WireHouseToHouseId(si.house);
    row.itemGuid         = itemGuid;
    row.itemTemplate     = si.itemId;
    row.itemCount        = si.stack;
    row.randomPropertyId = 0;
    row.owner            = si.botGuid;
    row.buyout           = si.buyout;
    row.expireTime       = GameTime() +
                           static_cast<uint64>(si.durationHrs) * 3600u;
    row.bidder           = 0u;
    row.bid              = 0u;
    row.startbid         = si.bid;
    row.deposit          = 0u;
    row.state            = static_cast<uint8>(BOOK_LIVE);

    if (!PersistBotListing(row, uuid))
    {
        // Commit failed: keep the pending for the resend cadence.
        fprintf(stderr, "ah-service: bot sell listing commit failed for"
                        " auction %u - kept pending\n", auctionId);
        return;
    }
    m_pendingSells.erase(it);
}

void MutationHandler::PrimePendingSellsFromJournal(
    std::vector<AhJournal::JournalRow> const& activeJournal)
{
    uint32 primed = 0;
    for (size_t i = 0; i < activeJournal.size(); ++i)
    {
        AhJournal::JournalRow const& jr = activeJournal[i];
        if (jr.state != static_cast<uint8>(AhJournal::JRN_INTENT_PENDING))
        {
            continue;
        }

        AhJournal::JournalRow row = jr;
        row.resolvedTime = row.createdTime;   // in-memory last-send anchor
        m_pendingSells[jr.uuid] = row;

        SellIntent si;
        ByteBuffer bb;
        bb.append(reinterpret_cast<const uint8*>(jr.facts.data()),
                  jr.facts.size());
        if (si.Decode(bb))
        {
            IpcMessage msg;
            msg.op = IPC_INTENT_SELL;
            si.Encode(msg.body);
            QueueSend(msg);
            ++primed;
        }
    }
    if (primed != 0u)
    {
        printf("ah-service: boot re-sent %u pending bot-sell intent(s)\n",
               primed);
    }
}

void MutationHandler::ResendStalePendingSells(uint64 now)
{
    std::vector<uint64> abandon;
    for (std::map<uint64, AhJournal::JournalRow>::iterator it =
             m_pendingSells.begin(); it != m_pendingSells.end(); ++it)
    {
        AhJournal::JournalRow& row = it->second;

        uint64 const age = (now > row.createdTime) ? (now - row.createdTime) : 0u;
        if (age >= BOT_SELL_ABANDON_SEC)
        {
            abandon.push_back(it->first);
            continue;
        }

        uint64 const sinceSent =
            (now > row.resolvedTime) ? (now - row.resolvedTime) : 0u;
        if (sinceSent < BOT_SELL_RESEND_SEC)
        {
            continue;
        }

        SellIntent si;
        ByteBuffer bb;
        bb.append(reinterpret_cast<const uint8*>(row.facts.data()),
                  row.facts.size());
        if (si.Decode(bb))
        {
            IpcMessage msg;
            msg.op = IPC_INTENT_SELL;
            si.Encode(msg.body);
            QueueSend(msg);
        }
        row.resolvedTime = now;
    }

    for (size_t i = 0; i < abandon.size(); ++i)
    {
        uint64 const uuid = abandon[i];
        RetireIntentPending(uuid);

        IpcMessage log;
        log.op = IPC_CONSOLE;
        char buf[96];
        snprintf(buf, sizeof(buf), "ah-service: abandoned bot sell uuid=%08x:%08x"
                 " (no materialization within %us)",
                 static_cast<unsigned>(uuid >> 32),
                 static_cast<unsigned>(uuid & 0xFFFFFFFFu),
                 static_cast<unsigned>(BOT_SELL_ABANDON_SEC));
        log.body.append(reinterpret_cast<const uint8*>(buf), strlen(buf));
        QueueSend(log);

        m_pendingSells.erase(uuid);
    }
}

size_t MutationHandler::PendingSellCount() const
{
    return m_pendingSells.size();
}

// --- Real persist seams (overridden by TestMutationHandler in --selftest) ---

bool MutationHandler::PersistBotBidSimple(BookRow const& row, uint32 bidAmount,
                                          uint64 uuid)
{
    if (m_db == NULL)
    {
        return true;   // selftest mode
    }

    DatabaseType& db = m_db->Character();
    if (!db.BeginTransaction())
    {
        return false;
    }
    // Bot bid: buyguid = 0 (no player), lastbid = bidAmount (mirrors the legacy
    // UpdateBid persist, AuctionHouseMgr.cpp:1738).
    db.PExecute("UPDATE `auction` SET `buyguid` = '0', `lastbid` = '%u'"
                " WHERE `id` = '%u'", bidAmount, row.id);
    AhJournal::JournalRow jr;
    jr.uuid         = uuid;
    jr.auctionId    = row.id;
    jr.kind         = static_cast<uint8>(JKIND_BOT_BID);
    jr.state        = static_cast<uint8>(AhJournal::JRN_APPLIED);
    jr.facts        = std::string();
    jr.createdTime  = static_cast<uint64>(time(NULL));
    jr.resolvedTime = jr.createdTime;
    AhJournal::Insert(*m_db, jr);
    return db.CommitTransactionChecked();
}

bool MutationHandler::PersistBotBidDisplacing(BookRow const& row, uint32 bidAmount,
                                              uint64 resolveUuid,
                                              std::string const& factsBlob)
{
    if (m_db == NULL)
    {
        return true;   // selftest mode
    }

    DatabaseType& db = m_db->Character();
    if (!db.BeginTransaction())
    {
        return false;
    }
    db.PExecute("UPDATE `auction` SET `buyguid` = '0', `lastbid` = '%u'"
                " WHERE `id` = '%u'", bidAmount, row.id);
    // The refund resolution (facts = encoded ResolveApply) co-commits with the
    // bot bid so the displaced player is never lost (spec 4.3 [v3 I2/I3]).
    AhJournal::JournalRow jr;
    jr.uuid         = resolveUuid;
    jr.auctionId    = row.id;
    jr.kind         = static_cast<uint8>(RESOLVE_REPAIR_RETURN);
    jr.state        = static_cast<uint8>(AhJournal::JRN_RESOLVING);
    jr.facts        = factsBlob;
    jr.createdTime  = static_cast<uint64>(time(NULL));
    jr.resolvedTime = 0u;
    AhJournal::Insert(*m_db, jr);
    return db.CommitTransactionChecked();
}

bool MutationHandler::PersistIntentPending(AhJournal::JournalRow const& row)
{
    if (m_db == NULL)
    {
        return true;   // selftest mode
    }

    if (!m_db->Character().BeginTransaction())
    {
        return false;
    }
    AhJournal::Insert(*m_db, row);
    return m_db->Character().CommitTransactionChecked();
}

bool MutationHandler::PersistBotListing(BookRow const& row, uint64 uuid)
{
    if (m_db == NULL)
    {
        m_book.Insert(row);   // memory-only book (selftest AuctionBook(NULL))
        return true;
    }

    DatabaseType& db = m_db->Character();
    if (!db.BeginTransaction())
    {
        return false;
    }
    m_book.Insert(row);   // appends the auction INSERT to this open txn + memory
    // Retire the PENDING intent row to JRN_APPLIED in the same txn.
    AhJournal::SetState(*m_db, uuid,
                        static_cast<uint8>(AhJournal::JRN_APPLIED),
                        static_cast<uint64>(time(NULL)));
    if (!db.CommitTransactionChecked())
    {
        m_book.RollbackInsert(row.id);
        return false;
    }
    return true;
}

bool MutationHandler::RetireIntentPending(uint64 uuid)
{
    if (m_db == NULL)
    {
        return true;   // selftest mode
    }

    if (!m_db->Character().BeginTransaction())
    {
        return false;
    }
    AhJournal::SetState(*m_db, uuid,
                        static_cast<uint8>(AhJournal::JRN_APPLIED),
                        static_cast<uint64>(time(NULL)));
    return m_db->Character().CommitTransactionChecked();
}
