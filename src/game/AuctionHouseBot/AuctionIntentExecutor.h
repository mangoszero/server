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

#ifndef MANGOS_H_AUCTION_INTENT_EXECUTOR
#define MANGOS_H_AUCTION_INTENT_EXECUTOR

#include "Common.h"
#include "Policies/Singleton.h"
#include "IpcMessage.h"

#include <unordered_map>
#include <map>

/// Fwd-decl: the wire struct lives in src/ipc/AuctionIntents.h. Only the .cpp
/// (and the -t ahmaterialize test) need the full definition; declaring it here
/// keeps the ipc header out of every TU that includes this executor.
struct SellIntent;

/**
 * @file AuctionIntentExecutor.h
 * @brief mangosd-side (authority) executor for AH subprocess intents.
 *
 * The out-of-process ah-service child only ADVISES via intents; this
 * executor is the sole authority that mutates gold/items/auctions. It runs
 * on the world thread (driven from World::HandleAhInbound), so it may touch
 * the live sAuctionMgr map without locking.
 *
 * Every intent is re-validated against LIVE state before applying. The
 * child's snapshot may be ~1s stale, so nothing in the intent is trusted
 * except as a request. Application is idempotent (a per-uuid cache dedups
 * redeliveries) and atomic (the underlying AuctionHouse* methods wrap their
 * own DB transactions).
 */

/**
 * @brief Idempotent re-validate-and-apply executor for AH intents.
 *
 * Singleton accessed via @c sAuctionIntentExecutor. Not thread safe by
 * design: all entry points must be called from the world thread.
 */
class AuctionIntentExecutor
{
    public:
        AuctionIntentExecutor()
            : m_applied(0), m_rejected(0), m_duplicate(0),
              m_malformed(0), m_evicted(0), m_ttlSec(0),
              m_lastMalformedLog(0), m_malformedSuppressed(0)
        {
        }

        /**
         * @brief Decode, dedupe, re-validate and apply one inbound intent.
         *
         * Routes on @p in.op (IPC_INTENT_SELL/BID/BUYOUT), decodes the
         * matching struct, dedups by uuid, guards the bot GUID, re-validates
         * against live state and applies. Always populates @p resultOut with
         * an IntentResult-encoded IPC_INTENT_RESULT frame to send back to the
         * child.
         *
         * Malformed bodies (Decode failure) and unroutable opcodes leave
         * @p resultOut empty (op == 0); the caller must not send those.
         *
         * @param in        Inbound frame from the child.
         * @param resultOut Result frame to forward to the child.
         */
        void Apply(const IpcMessage& in, IpcMessage& resultOut);

        /**
         * @brief Drop expired uuid cache entries and refresh the cached TTL.
         *
         * Pops the expired prefix of the expiry-ordered index (entries whose
         * expiry second is <= @p now), so the cost is O(k) in the number of
         * entries actually expiring, not O(n) over the whole cache. Also
         * re-reads + re-clamps the configured TTL here (once/second) so a live
         * config reload is picked up without paying that cost on the per-intent
         * hot path. Intended to be called at most ~once/second from
         * World::Update.
         *
         * @param now Current game-time in seconds.
         */
        void PurgeExpiredUuids(uint32 now);

        /// @return Count of intents accepted and applied (status OK).
        uint64 GetApplied() const { return m_applied; }
        /// @return Count of intents rejected after re-validation.
        uint64 GetRejected() const { return m_rejected; }
        /// @return Count of duplicate (already-seen uuid) intents ignored.
        uint64 GetDuplicate() const { return m_duplicate; }
        /// @return Count of frames that failed to decode (malformed body).
        uint64 GetMalformed() const { return m_malformed; }
        /// @return Count of cache entries force-evicted by the hard cap.
        uint64 GetEvicted() const { return m_evicted; }

        /// @return Number of live uuid entries currently held in the cache.
        size_t GetCacheSize() const { return m_dedup.Size(); }

        /**
         * @brief [SP-2] Reap materialized bot-listing items whose auction never
         *        reached the shared `auction` table.
         *
         * Selects durable @c botlist:<uuid> custody rows older than a grace
         * window (a worker that died between the materialize reply and the
         * book-commit). For each, the minted @c item_instance row is deleted
         * ONLY while it is still bot-owned AND not attached to any mail (so a
         * listing that DID reach the book and later sold/returned -- its item
         * now the buyer's or sitting in the bot's return mail -- is never
         * destroyed), and the stale @c botlist row is always removed (bounding
         * custody_ledger growth for resolved listings too). Runs on the AHBot
         * update tick under WriteAuthority.
         *
         * @param nowSec Current game-time second (unix epoch; == time(NULL)).
         */
        void SweepOrphanMaterializations(uint32 nowSec);

        /**
         * @brief [SP-2] Test-only seam for @c mangosd -t ahmaterialize.
         *
         * Invokes the durable materialization leg directly, bypassing the
         * ApplySell re-validation chain (which needs a fully loaded world that
         * is unavailable under @c -t). Not for production callers -- the live
         * path reaches materialization through Apply() -> ApplySell().
         *
         * @param s         Decoded sell intent.
         * @param resultOut Result frame (IPC_INTENT_RESULT) to fill.
         * @param now       Current game-time in seconds.
         */
        void TestMaterializeSell(SellIntent const& s, IpcMessage& resultOut,
                                 uint32 now);

    private:
        /**
         * @brief Bounded, expiry-ordered uuid dedup cache.
         *
         * Backs the idempotency guarantee: every terminal outcome (OK /
         * REJECTED / DUPLICATE) Remember()s its uuid so a redelivery is deduped
         * for the TTL window. A hostile child that heartbeats can flood unique,
         * child-controlled uuids; an unbounded map would then grow without
         * limit and OOM-kill mangosd (realm-wide outage). This cache is
         * therefore HARD-CAPPED and self-policing:
         *
         *   - @c m_byUuid (unordered_map uuid -> expiry) keeps IsDuplicate()
         *     and Remember() O(1) average.
         *   - @c m_byExpiry (multimap expiry -> uuid) is the expiry-ordered
         *     index. Because the TTL is constant within a tick, entries expire
         *     in (insertion / refresh) order, so Purge() pops only the expired
         *     prefix -- O(k) in the number actually expiring, never an O(n)
         *     scan -- and cap overflow evicts from the FRONT (oldest expiry).
         *
         * Cap = @c MAX_ENTRIES (100000). Worst-case memory is on the order of
         * (sizeof(unordered_map node) + sizeof(multimap node)) * cap, i.e. a
         * few MB -- far below any OOM risk and >> the legitimate working set
         * (the real bot emits a handful of intents per ~20s tick, so a few
         * thousand live entries over a 900s TTL). Eviction is OLDEST-EXPIRY
         * first: the legitimate working set (cap >> legit) is never evicted,
         * and under a flood only attacker entries closest to expiring anyway
         * are dropped. Evicting a not-yet-expired uuid re-opens a tiny
         * double-apply window for THAT uuid only -- acceptable, since it can
         * happen only under an active flood and targets the oldest entries.
         */
        class DedupCache
        {
            public:
                /// Hard cap. ~100k entries is a few MB and >> legit load.
                static const size_t MAX_ENTRIES = 100000u;

                /// @return true if @p uuid is currently remembered. O(1) avg.
                bool Contains(uint64 uuid) const
                {
                    return m_byUuid.find(uuid) != m_byUuid.end();
                }

                /**
                 * @brief Remember @p uuid until @p expiry (game-time second).
                 *
                 * Insert-or-refresh: a refreshed uuid is MOVED to its new
                 * (later) expiry position in the expiry index, so the sliding
                 * window holds -- a continuously-redelivered uuid never ages
                 * out into a re-apply. On cap overflow the OLDEST-expiry entry
                 * is evicted first.
                 *
                 * @param uuid    Intent uuid.
                 * @param expiry  Game-time second at which the entry expires.
                 * @return Number of entries force-evicted by the cap (0 or 1).
                 */
                uint32 Remember(uint64 uuid, uint32 expiry);

                /**
                 * @brief Drop every entry whose expiry second is <= @p now.
                 *
                 * Pops the expired prefix of the expiry-ordered index, so the
                 * cost is O(k) in the number expiring, not O(n).
                 *
                 * @param now Current game-time in seconds.
                 */
                void Purge(uint32 now);

                /// @return Live entry count.
                size_t Size() const { return m_byUuid.size(); }

            private:
                /// Erase the (expiry -> uuid) index entry matching @p uuid.
                void EraseIndex(uint64 uuid, uint32 expiry);

                /// uuid -> expiry second. O(1) membership + value lookup.
                std::unordered_map<uint64, uint32> m_byUuid;
                /// expiry second -> uuid. Ordered index for purge / eviction.
                std::multimap<uint32, uint64> m_byExpiry;
        };

        /**
         * @brief Re-read + re-clamp the configured TTL into @c m_ttlSec.
         *
         * Reads "AH.Service.IntentTtlSec" (default 900) and clamps to
         * [60, 86400] so the dedup window is always a meaningful positive
         * duration. Zero would give same-tick expiry (dedup never holds ->
         * double-apply); negative values wrap to a huge uint32 (never expire).
         * Called lazily on first use and once/second from PurgeExpiredUuids
         * (picking up live config reloads) -- NOT on the per-intent hot path.
         */
        void RefreshTtl();

        /// @return Cached clamped TTL, computing it on first use.
        uint32 IntentTtlSec()
        {
            if (m_ttlSec == 0)
            {
                RefreshTtl();
            }
            return m_ttlSec;
        }

        /**
         * @brief Has @p uuid already reached a terminal outcome?
         * @return true if present (caller should emit INTENT_DUPLICATE).
         */
        bool IsDuplicate(uint64 uuid) const
        {
            return m_dedup.Contains(uuid);
        }

        /**
         * @brief Record @p uuid as processed so redeliveries are deduped.
         *
         * Called on EVERY terminal outcome (OK, REJECTED and DUPLICATE) so a
         * redelivered, already-applied uuid never double-applies, and so a
         * continuously-redelivered uuid keeps its sliding window open. The
         * bounded cache evicts oldest-expiry first if it is at capacity.
         *
         * @param uuid Intent uuid.
         * @param now  Current game-time in seconds (expiry = now + TTL).
         */
        void Remember(uint64 uuid, uint32 now)
        {
            m_evicted += m_dedup.Remember(uuid, now + IntentTtlSec());
        }

        /**
         * @brief Emit a rate-limited hot-path error for a malformed frame.
         *
         * A hostile child can resend malformed frames every drained frame (up
         * to 256/tick); an unrate-limited per-frame error log would fill the
         * disk. Logs at most once per @c MALFORMED_LOG_INTERVAL seconds and
         * folds the suppressed count into the next emitted line.
         *
         * @param kind     Intent kind for the message (e.g. "SellIntent").
         * @param bodySize Wire body size in bytes (for diagnostics).
         */
        void LogMalformed(const char* kind, size_t bodySize);

        /// Re-validate-and-apply for IPC_INTENT_SELL. Returns the outcome.
        void ApplySell(const IpcMessage& in, IpcMessage& resultOut,
                       uint32 now);
        /// Re-validate-and-apply for IPC_INTENT_BID. Returns the outcome.
        void ApplyBid(const IpcMessage& in, IpcMessage& resultOut,
                      uint32 now);
        /// Re-validate-and-apply for IPC_INTENT_BUYOUT. Returns the outcome.
        void ApplyBuyout(const IpcMessage& in, IpcMessage& resultOut,
                         uint32 now);

        /**
         * @brief [SP-2] WriteAuthority materialization leg of IPC_INTENT_SELL.
         *
         * Mints + persists (owner = bot lister) + escrows the listing item,
         * allocates the auction id, records the durable @c botlist:<uuid>
         * idempotency row (kind CUSTODY_ITEM, role ROLE_RESOLUTION, state
         * CST_RESERVED) and fills @p resultOut with
         * IntentResult{INTENT_OK, itemGuid, auctionId} -- the worker then writes
         * the auction book. mangosd does NOT insert the `auction` row. A
         * redelivered uuid replays the recorded ids (survives restart, unlike
         * the in-memory dedup window) and never double-mints. On any failure
         * that cannot yield nonzero ids the reply is INTENT_REJECTED (never
         * INTENT_OK with zero ids).
         *
         * @param s         Decoded, already-validated sell intent.
         * @param resultOut Result frame (IPC_INTENT_RESULT) to fill.
         * @param now       Current game-time in seconds.
         */
        void MaterializeSell(SellIntent const& s, IpcMessage& resultOut,
                             uint32 now);

        /// At most one malformed-frame error log per this many seconds.
        static const uint32 MALFORMED_LOG_INTERVAL = 10u;

        /// Bounded, expiry-ordered uuid dedup cache (see DedupCache).
        DedupCache m_dedup;

        uint64 m_applied;    ///< Accepted-and-applied counter.
        uint64 m_rejected;   ///< Rejected-after-validation counter.
        uint64 m_duplicate;  ///< Duplicate-uuid counter.
        uint64 m_malformed;  ///< Failed-to-decode counter.
        uint64 m_evicted;    ///< Cap-forced cache eviction counter.

        uint32 m_ttlSec;     ///< Cached clamped TTL (0 = not yet computed).

        /// Game-time second of the last emitted malformed-frame error log.
        uint32 m_lastMalformedLog;
        /// Malformed frames suppressed since the last emitted log line.
        uint32 m_malformedSuppressed;
};

/// Convenience define to access the AuctionIntentExecutor singleton.
#define sAuctionIntentExecutor \
    MaNGOS::Singleton<AuctionIntentExecutor>::Instance()

#endif // MANGOS_H_AUCTION_INTENT_EXECUTOR
