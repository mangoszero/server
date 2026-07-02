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

#ifndef MANGOS_AH_MUTATION_PENDING_H
#define MANGOS_AH_MUTATION_PENDING_H

#include "Common.h"
#include <unordered_map>
#include <string>
#include <vector>

/**
 * @file MutationPending.h
 * @brief SP-2 pending-map for player auction mutations forwarded to the worker.
 *
 * NOT SP-1's BrowsePendingMap (spec 5.5): replies apply ALL exactly once (no
 * IsCurrent latest-wins drop -- two fast clicks both apply, in order), the caps
 * are checked up front BEFORE any custody reserve, and there is NO in-process
 * fallback of any kind. Timed-out entries become in-doubt TOMBSTONES (the
 * reservation stays non-terminal, decision 10); a late reply still applies via
 * Take (consume-once), and reconcile-on-reconnect resolves the rest.
 *
 * World-thread owned; no locking. Default-off groundwork: nothing registers
 * here unless AH.Service.WriteAuthority=1.
 */

/// Lifecycle of one pending mutation. For a cancel, PMUT_AWAIT_RESULT doubles
/// as AWAIT_PREPARE; RearmConfirm moves it to PMUT_AWAIT_CONFIRM in the SAME
/// cap slot when the phase-2 CONFIRM is sent (spec 4.2).
enum PendingMutState : uint8
{
    PMUT_AWAIT_RESULT  = 0,  ///< Forwarded; awaiting IPC_PLAYER_RESULT
    PMUT_AWAIT_CONFIRM = 1,  ///< Cancel phase 2: CONFIRM sent, awaiting result
    PMUT_TOMBSTONE     = 2,  ///< In-doubt: player already got AUCTION_ERR_DATABASE
};

/// One outstanding player mutation awaiting a worker reply. Never holds a
/// WorldSession*/Player* -- the finalize re-resolves by playerGuidLow. The
/// custody idem keys are carried so the finalize (Task 11) can flip exactly
/// the rows this forward reserved.
struct PendingMutation
{
    uint64 uuid;                ///< mangosd-minted (AhMintMutationUuid)
    uint32 playerGuidLow;       ///< re-resolve target
    uint16 op;                  ///< originating IpcOpcode (IPC_PLAYER_*)
    uint32 auctionId;           ///< pre-allocated (sell) or targeted auction id
    uint8  state;               ///< PendingMutState
    uint32 sentSec;             ///< time(NULL) at send / at RearmConfirm
    uint32 reservedAmount;      ///< copper reserved (delta on a raise; 0 on sell)
    std::string reserveKey;     ///< "bid:<auc>:<seq>" gold reserve, or empty
    std::string itemKey;        ///< "item:<auc>" escrow row (sell), or empty
    std::string depKey;         ///< "dep:<auc>" deposit row (sell), or empty
};

/**
 * @brief Apply-all consume-once pending map (spec 5.5). NO IsCurrent, NO
 *        in-process fallback. CanRegister MUST be checked before any reserve.
 */
class MutationPendingMap
{
    public:
        static const size_t MAX_PER_PLAYER = 8u;    ///< per-player in-flight cap
        static const size_t MAX_TOTAL      = 4096u; ///< global hard cap

        /// True iff a new entry for @p playerGuidLow fits both caps. Checked
        /// BEFORE any reserve so an at-cap reject costs nothing (spec 5.5).
        /// Tombstones count: they hold live reservations (honest backpressure).
        bool CanRegister(uint32 playerGuidLow) const;

        /// Insert @p pm (uuid supplied by the caller, mangosd scheme). The
        /// caller must have checked CanRegister; a duplicate uuid is a protocol
        /// fault (logged loudly, entry replaced).
        void Register(PendingMutation const& pm);

        /// Consume-once: copy the entry into @p out and erase it (releases the
        /// cap slot). Works on tombstones too -- a late reply still applies.
        /// @return true if the uuid was present.
        bool Take(uint64 uuid, PendingMutation& out);

        /// Mark @p uuid in-doubt: entry KEPT, state = PMUT_TOMBSTONE. The
        /// associated reservation stays non-terminal (decision 10).
        /// @return true if the uuid was present.
        bool Tombstone(uint64 uuid);

        /// Cancel phase 2 (spec 4.2): move the slot to PMUT_AWAIT_CONFIRM with
        /// a fresh TTL anchor. Also re-arms a TOMBSTONE (spec 4.2: "the
        /// tombstone re-arms when CONFIRM is sent"). No-op on a missing uuid.
        void RearmConfirm(uint64 uuid, uint32 nowSec);

        /// Tombstone every non-tombstone entry older than @p ttlSec, appending
        /// each NEWLY in-doubt uuid to @p newlyInDoubt so the caller can emit
        /// the one-time AUCTION_ERR_DATABASE result (spec 4.1 step 5). Entries
        /// are never erased here; already-tombstoned entries are not re-emitted.
        void SweepToTombstones(uint32 nowSec, uint32 ttlSec,
                               std::vector<uint64>& newlyInDoubt);

        /// Read-only lookup WITHOUT consuming (tombstoned/awaiting entries stay
        /// registered): the sweep error-sender and the cancel phase-2 driver
        /// need the entry while it remains in-flight.
        bool Peek(uint64 uuid, PendingMutation& out) const;

        size_t Size() const { return m_map.size(); }

    private:
        std::unordered_map<uint64, PendingMutation> m_map;
        std::unordered_map<uint32, size_t> m_perPlayer; ///< guidLow -> live count
};

/// Mint a mangosd-side mutation uuid: high 32 bits = process boot second, low
/// 32 = monotonic sequence. Disjoint from worker-minted (runId << 32) | seq
/// (runId is a small per-spawn counter), and a fresh boot second keeps this
/// run's uuids from colliding with journal rows persisted by a previous run.
/// World-thread only.
uint64 AhMintMutationUuid();

/// SP-2 Task 10: classify a CMSG_AUCTION_BID forward using mangosd's OWN
/// ledger -- the only bid state mangosd holds under WriteAuthority (the book,
/// including each auction's buyout, is worker-owned). Defined in
/// AuctionHouseHandler.cpp.
///
/// Returns the IPC opcode to forward on and sets @p reserveAmount:
///  - IPC_PLAYER_BID    same-bidder raise proven by exactly one live ROLE_BID
///                      row owned by @p bidderGuidLow; reserveAmount = the
///                      delta (spec I9), the intent carries the full price.
///  - IPC_PLAYER_BUYOUT everything else (fresh bid, displacing bid, ambiguous
///                      rows); reserveAmount = @p price reserved in full as
///                      maxPrice -- the worker computes effectiveBid =
///                      min(maxPrice, buyout) (spec 4.1 value math), so this
///                      covers both a normal bid and a buyout win; the
///                      finalize releases maxPrice - effectiveBid.
///  - 0                 inline reject: the player's own live bid is already
///                      >= price (legacy `price <= auction->bid` =>
///                      AUCTION_ERR_HIGHER_BID). Non-holder stale bids are NOT
///                      pre-rejected here: those guards moved to the worker
///                      (spec I6) and bot-held bids have no ledger row.
uint16 AhClassifyBidForward(uint32 auctionId, uint32 bidderGuidLow, uint32 price,
                            uint32& reserveAmount);

#endif // MANGOS_AH_MUTATION_PENDING_H
