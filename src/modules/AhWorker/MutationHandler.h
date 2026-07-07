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
#include "AuctionIntents.h"
#include "IpcMessage.h"

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

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

        virtual ~MutationHandler()
        {
        }

        // --- SP-2 Task 7: expiry/win tick + resolve outbox ------------------
        // (spec 4.3 / 4.3b / 7 / decision 10)

        /// Per-tick cap on freshly minted resolutions (flow control).
        static const uint32 RESOLVE_BUDGET_PER_TICK = 16u;
        /// Max un-acked RESOLVE_APPLY frames in flight (window).
        static const uint32 RESOLVE_WINDOW = 64u;
        /// Re-send a RESOLVING entry older than this many game-seconds.
        static const uint32 RESOLVE_RESEND_SEC = 30u;
        /// Log a stuck-resolution alarm once attempts exceed this.
        static const uint32 RESOLVE_STUCK_ALARM_ATTEMPTS = 10u;

        /**
         * @brief Expiry/win scan (spec 4.3b): mint resolutions for LIVE rows
         *        past expireTime, bounded by the per-tick budget and the
         *        un-acked window. Also hands off any Task-6 prepare-timeout
         *        unlock queued via CheckPrepareTimeouts to the same outbox
         *        tracking (PopQueuedResolve/TrackAndSend) so OnResolveAck can
         *        retire it identically to a tick-minted resolution.
         */
        void Tick(uint64 gameTimeNow);

        /**
         * @brief Process a mangosd IPC_RESOLVE_ACK. Terminal kinds (WON,
         *        EXPIRED_NOBID) commit journal-APPLIED + auction-row DELETE
         *        in one checked txn on APPLIED/DUPLICATE and drop the book
         *        row; non-terminal kinds are journal-only and the row
         *        persists. FAILED and failed local txns keep the entry
         *        RESOLVING for the resend cadence.
         */
        void OnResolveAck(ResolveAck const& ack);

        /// Channel-up retry driver (decision 10): re-send RESOLVING entries
        /// older than RESOLVE_RESEND_SEC verbatim; logs a stuck alarm past
        /// RESOLVE_STUCK_ALARM_ATTEMPTS.
        void ResendStaleResolving(uint64 now);

        /**
         * @brief Journal a resolution (JRN_RESOLVING, standalone durable),
         *        mark the row BOOK_RESOLVING for terminal kinds, queue the
         *        IPC_RESOLVE_APPLY frame and track it against the window.
         *
         * @return the worker-minted resolution uuid, or 0 if refused
         *         (active resolution on the auction / window closed /
         *         journal write failed).
         */
        uint64 QueueResolution(uint32 auctionId, uint8 kind,
                               MutationFacts const& facts, uint64 now);

        /// True while auctionId has an un-acked RESOLVING entry in flight.
        bool HasActiveResolution(uint32 auctionId) const;

        /**
         * @brief Boot replay (spec 4.3 at-least-once): track + immediately
         *        re-queue every JRN_RESOLVING journal row, replaying the
         *        stored facts blob verbatim. Call once after LoadFromDb.
         */
        void PrimeResolvingFromJournal(
            std::vector<AhJournal::JournalRow> const& activeJournal);

        /**
         * @brief Cross-restart uuid-collision guard for THIS minter's half.
         *        Seed the minter past @p maxSeq -- the highest seq persisted in
         *        this minter's HIGH half [0x80000000+] for this runId (0 = none),
         *        from AhJournal::MaxSeqForRunId(highHalf=true). The BotBrain
         *        minter owns the low half and is seeded by BotBrain::SeedSeqPast.
         *        Generalizes the RESOLVING-only [FIX A] advance in
         *        PrimeResolvingFromJournal to EVERY retained high-half row --
         *        including terminal JRN_APPLIED, which LoadActive
         *        (state IN 1,2,4,5) never returns. The supervisor runId resets
         *        to 1 each mangosd restart and the minter restarts at
         *        0x80000000, so without this a retained uuid gets re-minted and
         *        its journal INSERT hits a duplicate PRIMARY KEY. Call once at
         *        boot, before the first mint.
         */
        void SeedMinterPast(uint32 maxSeq);

        /// Drain handler-queued outbound frames (caller sends them).
        void TakeOutbound(std::vector<IpcMessage>& out);

        /// IPC_GAMETIME-synced clock (spec 7/M1).
        void SetGameTime(uint64 now);
        uint64 GameTime() const;

        /// Number of un-acked RESOLVING entries in flight.
        size_t ResolvingCount() const;

        /**
         * @brief Worker-minted uuid = (runId << 32) | seq (contract 3).
         *
         * Shares NextWorkerUuid()'s sequence space (same m_nextSeq counter,
         * already based at 0x80000000u) so the two minters can never collide.
         */
        uint64 NextUuid();

        /// WON and EXPIRED_NOBID delete the book row on APPLIED; all other
        /// kinds are journal-only at ack time (spec 4.3 [v3 I2/I3]).
        static bool IsTerminalResolveKind(uint8 kind);

        /// Book-facts snapshot: prior* zeroed, cur* = the row's live values.
        static MutationFacts FactsFromRow(BookRow const& row);

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

        // --- SP-2 Task 8: bot fold-in (spec 5.2 / 5.4 / 6 / 4.3 / decision 4,8) --

        /// Journal `kind` for directly-APPLIED bot rows (spec M2). Disjoint from
        /// ResolveKind (0-3) and the player opcode low bytes (0x40-0x43).
        enum WorkerJournalKind
        {
            JKIND_BOT_SELL = 16,
            JKIND_BOT_BID  = 17
        };

        /// Re-send a still-pending bot-sell materialization older than this.
        static const uint32 BOT_SELL_RESEND_SEC  = 30u;
        /// Abandon a bot-sell materialization that never came back by this age.
        static const uint32 BOT_SELL_ABANDON_SEC = 600u;

        /**
         * @brief Bot bid (bidder identity = 0 per the section-3 cross-check
         *        matrix): admit against the book, then EITHER PersistBotBidSimple
         *        (no displaced player) OR PersistBotBidDisplacing + queue a
         *        non-terminal RESOLVE_REPAIR_RETURN refund resolution for the
         *        displaced player. Returns false on any admission reject or an
         *        active resolution lock.
         */
        bool OnBotBid(uint64 uuid, uint32 auctionId, uint32 bidAmount);

        /**
         * @brief Bot buyout: admit, then queue a terminal RESOLVE_WON
         *        (curBidderGuid = 0 => bot-win destroy branch on mangosd) which
         *        marks the row RESOLVING and deletes it on ack. false on reject.
         */
        bool OnBotBuyout(uint64 uuid, uint32 auctionId);

        /**
         * @brief Bot sell: journal JRN_INTENT_PENDING(uuid, encoded SellIntent)
         *        then send the retained IPC_INTENT_SELL for mangosd to
         *        materialize. The book write is deferred to OnBotSellResult.
         */
        bool BotSellBegin(SellIntent const& si);

        /**
         * @brief Materialization reply: on INTENT_OK commit the listing to the
         *        book + auction row + retire the INTENT_PENDING journal row
         *        (one txn); a reject drops the pending (mangosd's orphan sweep
         *        reaps any minted item); a duplicate/late reply is idempotent.
         */
        void OnBotSellResult(uint64 uuid, uint8 status, uint8 reason,
                             uint32 itemGuid, uint32 auctionId);

        /// Boot replay: re-load JRN_INTENT_PENDING rows into the in-flight map
        /// and re-send each materialization request (idempotent by uuid).
        void PrimePendingSellsFromJournal(
            std::vector<AhJournal::JournalRow> const& activeJournal);

        /// Re-send pending sells older than BOT_SELL_RESEND_SEC; abandon (retire
        /// + IPC_CONSOLE log) those older than BOT_SELL_ABANDON_SEC.
        void ResendStalePendingSells(uint64 now);

        /// In-flight (un-materialized) bot-sell count.
        size_t PendingSellCount() const;

        /// Wire house index -> DBC houseid (AuctionIntentExecutor.cpp:175-186):
        /// 0->1, 1->6, 2->7.
        static uint8 WireHouseToHouseId(uint8 house);

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

    protected:
        // --- DB/send seams (virtual so --selftest can record instead) -------

        /// Standalone durable RESOLVING insert (M1: BEFORE the send).
        virtual bool JournalInsertResolving(AhJournal::JournalRow const& row);

        /// One txn: DELETE auction row + journal -> JRN_APPLIED (terminal ack).
        virtual bool CommitTerminalApply(uint64 uuid, uint32 auctionId);

        /// Journal -> JRN_APPLIED only (non-terminal ack).
        virtual bool JournalMarkApplied(uint64 uuid);

        /// Append one frame to the outbound queue (reliable lane on send).
        virtual void QueueSend(IpcMessage const& msg);

        /// Track a freshly journalled resolution and queue its APPLY frame.
        void TrackAndSend(uint64 uuid, uint32 auctionId, uint8 kind,
                          std::string const& wire, uint64 now);

        // --- SP-2 Task 8: bot value-effect persist seams (virtual so
        //     --selftest can trap them; real bodies write the book/journal in
        //     one checked txn) -------------------------------------------------

        /// One txn: UPDATE auction (buyguid=0,lastbid) + JRN_APPLIED bot-bid row.
        virtual bool PersistBotBidSimple(BookRow const& row, uint32 bidAmount,
                                         uint64 uuid);
        /// One txn: UPDATE auction (buyguid=0,lastbid) + the co-committed
        /// JRN_RESOLVING refund row (facts = encoded ResolveApply).
        virtual bool PersistBotBidDisplacing(BookRow const& row, uint32 bidAmount,
                                             uint64 resolveUuid,
                                             std::string const& factsBlob);
        /// Standalone durable INSERT of a JRN_INTENT_PENDING row (before send).
        virtual bool PersistIntentPending(AhJournal::JournalRow const& row);
        /// One txn: INSERT auction row (+ book insert) + retire the pending row
        /// to JRN_APPLIED.
        virtual bool PersistBotListing(BookRow const& row, uint64 uuid);
        /// Retire a JRN_INTENT_PENDING row to JRN_APPLIED (abandon/reject path).
        virtual bool RetireIntentPending(uint64 uuid);

    private:
        PlayerMutationResult MakeResult(uint64 uuid, uint8 op, uint8 status,
                                        uint8 reason) const;

        /**
         * @brief Commit a NORMAL BID at @p amount on an already-validated LIVE
         *        row: UpdateBid + JRN_COMMITTED in one checked txn, prior* =
         *        the displaced bidder, effectiveBid = amount, row stays LIVE,
         *        rollback-on-commit-failure. Shared by OnBid (op 0x41) and the
         *        below-buyout IPC_PLAYER_BUYOUT leg (op 0x42): @p op stamps both
         *        the reply op and the journal kind. The caller MUST have run the
         *        bid validation to VALIDATE_ADMIT first (row present + LIVE).
         */
        PlayerMutationResult CommitBidAt(uint32 auctionId, uint32 bidder,
                                         uint32 amount, uint8 op, uint64 uuid);

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

        // --- SP-2 Task 7: expiry/win tick + resolve outbox state ------------

        /// One un-acked outbound resolution (decision-10 window bookkeeping).
        struct ResolvingEntry
        {
            uint32      auctionId;   ///< Book row this resolves.
            uint8       kind;        ///< ResolveKind (drives ack terminality).
            uint32      attempts;    ///< Send attempts (1 = first send).
            uint64      lastSentSec; ///< Game-time of the last send (0 = boot).
            std::string wire;        ///< Encoded ResolveApply body (verbatim re-send).
        };

        std::map<uint64, ResolvingEntry> m_resolving;         ///< By uuid.
        std::set<uint32>                 m_resolvingAuctions; ///< Invariant guard.
        std::vector<IpcMessage>          m_outbound;          ///< Drained by the loop.
        uint64                           m_gameTimeNow;       ///< Latest game time.

        // --- SP-2 Task 8: in-flight bot-sell materializations (by uuid).
        // The stored JournalRow.facts is the encoded SellIntent; createdTime is
        // the initial send (abandon anchor), resolvedTime the last send
        // (resend throttle). ---------------------------------------------------
        std::map<uint64, AhJournal::JournalRow> m_pendingSells;

        // Non-copyable: single-owner main-thread state.
        MutationHandler(const MutationHandler&);
        MutationHandler& operator=(const MutationHandler&);
};

#endif // AH_WORKER_MUTATION_HANDLER_H
