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

#ifndef AH_WORKER_MUTATION_HANDLER_H
#define AH_WORKER_MUTATION_HANDLER_H

#include "Common.h"
#include "AuctionBook.h"
#include "Journal.h"
#include "PlayerMutations.h"

#include <deque>
#include <map>

class ServiceDatabase;

/**
 * @file MutationHandler.h
 * @brief SP-2 single-threaded mutation handler (spec v3 section 4.1/4.2).
 *
 * Runs INLINE on the main service-loop thread (the dispatch switch and the
 * tick call it): one serializer, no locks. Validates every player mutation
 * against the authoritative book, commits auction-row + journal writes in ONE
 * checked transaction, and replies complete by-value fact snapshots -- the
 * worker reports book facts, mangosd computes ALL value math (spec 4.5).
 *
 * db == NULL is --selftest mode: journal and transaction steps are skipped
 * (treated as success) and the book runs memory-only.
 *
 * Journal conventions (pinned):
 *  - player-mutation rows carry kind = originating opcode low byte
 *    (0x40 SELL, 0x41 BID, 0x42 BUYOUT, 0x43 CANCEL); rows with state
 *    JRN_RESOLVING carry a ResolveKind. The ranges are disjoint.
 *  - the facts blob of a JRN_COMMITTED row is the encoded PlayerMutationResult;
 *    of a JRN_RESOLVING row, the encoded ResolveApply (contract section 3).
 */

/// Outcome of a pure validation pass (selftest-reachable, no DB).
enum ValidationOutcome : uint8
{
    VALIDATE_ADMIT         = 0,
    VALIDATE_REJECT        = 1,  ///< reply MUT_REJECTED with reasonOut
    VALIDATE_REJECT_SILENT = 2   ///< reply MUT_REJECTED, reason 0: legacy sent NO packet
};

class MutationHandler
{
    public:
        MutationHandler(AuctionBook& book, ServiceDatabase* db, uint32 runId);

        PlayerMutationResult OnSell(PlayerSellIntent const& in);
        PlayerMutationResult OnBid(PlayerBidIntent const& in);
        PlayerMutationResult OnBuyout(PlayerBuyoutIntent const& in);

        /**
         * @brief Cancel two-phase step 1 (spec 4.2): validate ownership, lock
         *        the row (BOOK_CANCEL_PREPARED + a durable JRN_CANCEL_PREPARED
         *        journal row), reply MUT_PREPARED carrying {curBid, curBidder,
         *        deposit} in the facts. Concurrent bids on the prepared row are
         *        rejected as the legacy race-loser (spec 4.3b).
         */
        PlayerMutationResult OnCancelPrepare(PlayerCancelPrepare const& in);

        /**
         * @brief Cancel two-phase step 2. confirm==true commits the removal
         *        (DELETE + journal COMMITTED, one checked txn) -> MUT_OK with
         *        the facts snapshot; confirm==false unlocks (journal APPLIED,
         *        auction untouched) -> MUT_OK. An unknown/expired uuid gets an
         *        explicit MUT_REJECTED_STALE -- the worker answers EVERY
         *        confirm, never silence (spec 4.2 v3).
         */
        PlayerMutationResult OnCancelDecide(uint64 uuid, uint32 auctionId, bool confirm);

        /**
         * @brief Re-arm prepare locks from JRN_CANCEL_PREPARED journal rows at
         *        boot (spec 4.2 v3 section-8 addition: the worker restores the
         *        locks in the reloaded book and awaits CONFIRM/ABORT/timeout).
         *        Call once, after AuctionBook::LoadFromDb, with the same rows.
         */
        void AdoptActiveJournal(std::vector<AhJournal::JournalRow> const& rows);

        /**
         * @brief Prepare-lock timeout sweep (T = CANCEL_PREPARE_TIMEOUT_SECS).
         *
         * A timed-out lock unlocks via a journal-anchored resolution: the
         * prepare row retires to JRN_APPLIED and a NEW worker-minted
         * JRN_RESOLVING row (kind RESOLVE_CANCELLED_UNLOCK, facts = encoded
         * ResolveApply) is inserted in the SAME txn, so the
         * one-ACTIVE-per-auction invariant holds and a late CONFIRM is
         * serialized by the journal (spec 4.2 v3 C2/I5/I6). The ResolveApply
         * is queued for the resolve-send driver.
         */
        void CheckPrepareTimeouts(uint64 nowSecs);

        /**
         * @brief Drain one queued worker-initiated resolution (the resolve-send
         *        task's seam). Entries are ALSO journal-anchored as
         *        JRN_RESOLVING, so a crash before the send loses nothing: the
         *        re-send driver re-encodes from the journal facts blob.
         */
        bool PopQueuedResolve(ResolveApply& out);

        /// PREPARE lock timeout, seconds (spec 4.2 step 4's "T" -- pinned).
        static const uint32 CANCEL_PREPARE_TIMEOUT_SECS = 10u;

        /// GetAuctionOutBid (AuctionHouseMgr.cpp:1502-1510): (bid/100)*5, min 1c.
        static uint32 OutBidAmount(uint32 bid);

        /// Legacy bid validation in legacy order (AuctionHouseHandler.cpp:735-784).
        static ValidationOutcome ValidateBid(BookRow const* row, uint32 bidderGuid,
                                             uint32 bidAmount, uint32 ownerAccount,
                                             uint32 bidderAccount, uint8& reasonOut);
        static ValidationOutcome ValidateBuyout(BookRow const* row, uint32 bidderGuid,
                                                uint32 maxPrice, uint32 ownerAccount,
                                                uint32 bidderAccount, uint8& reasonOut);

        /// Zero every MutationFacts field.
        static void ClearFacts(MutationFacts& out);

        /// Snapshot @p row into the facts. prior*/cur* fields both take the
        /// row's bidder/bid; effectiveBid is left 0 for the caller to set.
        static void FillFacts(BookRow const& row, MutationFacts& out);

    private:
        PlayerMutationResult MakeResult(uint64 uuid, uint8 op, uint8 status,
                                        uint8 reason) const;

        /// SELECT account FROM characters (0 if unknown / selftest mode).
        uint32 LookupAccount(uint32 guidLow) const;

        bool BeginCommit();    ///< BeginTransaction (true in selftest mode)
        bool FinishCommit();   ///< CommitTransactionChecked (true in selftest mode)

        /// Append the JRN_COMMITTED journal row (facts = encoded @p res) to the
        /// open transaction. No-op in selftest mode.
        void JournalCommitted(uint64 uuid, uint32 auctionId, uint8 kind,
                              PlayerMutationResult const& res);

        /// One armed cancel-prepare lock awaiting CONFIRM/ABORT/timeout.
        struct PrepareEntry
        {
            uint32 auctionId;
            uint32 sellerGuid;
            uint64 preparedAt;   ///< seconds, time(NULL) domain
        };
        typedef std::map<uint64, PrepareEntry> PrepareMap;

        /// (m_runId << 32) | m_nextSeq++ -- the BotBrain scheme
        /// (BotBrain.cpp:106) on the 0x80000000+ half of the low word.
        uint64 NextWorkerUuid();

        PrepareMap               m_prepares;
        std::deque<ResolveApply> m_resolveQueue;

        AuctionBook&     m_book;
        ServiceDatabase* m_db;
        uint32           m_runId;

        /// Worker-minted uuid low words start at 0x80000000: BotBrain owns the
        /// 0x00000001+ half of this run-id's sequence space (BotBrain.cpp:106),
        /// so the two minters can never collide.
        uint32           m_nextSeq;

        // Non-copyable: single-owner main-thread state.
        MutationHandler(const MutationHandler&);
        MutationHandler& operator=(const MutationHandler&);
};

#endif // AH_WORKER_MUTATION_HANDLER_H
