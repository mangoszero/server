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

#ifndef AH_WORKER_BROWSE_HANDLER_H
#define AH_WORKER_BROWSE_HANDLER_H

#include "Common.h"
#include "BrowseMessages.h"
#include "ServiceDatabase.h"
#include "IpcChannel.h"            // IpcClient
#include "Threading/Threading.h"   // ACE_Based::Runnable, Thread
#include "BoundedQueue.h"          // explicit-capacity thread-safe queue
#include <atomic>
#include <string>
#include <vector>

/// Cap on the un-paginated survivor set returned for a deferEluna browse. Must
/// be <= BrowseResult::MAX_ENTRIES. If exceeded, the worker returns tooMany=1.
const uint32 BROWSE_ELUNA_CAP = 1000u;

/// A fetched auction row: the wire entry plus the columns the in-code filters
/// need. Populated by BrowseHandler::Fetch (Task 10); consumed by
/// FilterAndPaginate.
struct BrowseRow
{
    BrowseEntry entry;
    uint32 itemClass;           ///< item_template.class
    uint32 itemSubClass;        ///< item_template.subclass
    uint32 inventoryType;       ///< item_template.InventoryType
    uint32 quality;             ///< item_template.Quality
    uint32 requiredLevel;       ///< item_template.RequiredLevel
    uint32 allowableClass;      ///< item_template.AllowableClass (bitmask)
    uint32 allowableRace;       ///< item_template.AllowableRace (bitmask)
    std::string name;           ///< localized item name (locale-resolved in Fetch)
    uint32 reqSkill;            ///< item_template.RequiredSkill
    uint32 reqSkillRank;        ///< item_template.RequiredSkillRank
    uint32 reqSpell;            ///< item_template.RequiredSpell
    uint32 reqHonorRank;        ///< item_template.RequiredHonorRank
    uint32 reqRepFaction;       ///< item_template.RequiredReputationFaction
    uint32 reqRepRank;          ///< item_template.RequiredReputationRank
    uint32 itemProficiencySkill;///< the item's own proficiency skill (0 if none)
    uint32 castSpellId;         ///< spellid_1 (Spells[0].SpellId); recipe-known match
};

/// Result status for BrowseHandler::Fetch (I3).
enum FetchStatus
{
    FETCH_OK       = 0,   ///< Rows fetched (may be zero if AH genuinely empty)
    FETCH_EMPTY    = 1,   ///< No rows AND confirmed-empty (COUNT(*) probe was 0)
    FETCH_DB_ERROR = 2    ///< DB error -- caller must NOT reply (TTL fallback)
};

namespace BrowseHandler
{
    /// PURE: compose a BIDDER result exactly like the legacy client path.
    /// Client-supplied outbid ids are resolved in client order (including
    /// duplicates), then the requester's current bids follow in row order.
    std::vector<BrowseRow> ComposeBidderRows(const std::vector<BrowseRow>& rows,
                                             const BrowseQuery& q);

    /// PURE: usable filter (Task 6) + name filter (LIST) + pagination OR
    /// defer-un-paginated. Cheap proto filters are assumed SQL-applied.
    BrowseResult FilterAndPaginate(const std::vector<BrowseRow>& rows,
                                   const BrowseQuery& q);

    /// On-demand SELECT for one query (auction JOIN item_instance JOIN world
    /// item_template). Decodes the item_instance blob (Task 4), applies the
    /// locale overlay (I2/D7/V3), then calls FilterAndPaginate. On a NULL
    /// result runs a COUNT(*) probe (I3) to distinguish FETCH_EMPTY from
    /// FETCH_DB_ERROR. Sets status accordingly.
    BrowseResult Fetch(ServiceDatabase& db, const BrowseQuery& q,
                       FetchStatus& status);
}

/// Dedicated worker browse thread. Single thread (FIFO, open-d). Owns per-thread
/// MySQL init/teardown (C4). Explicit-capacity bounded queue (C4: BoundedQueue
/// has no default ctor). Atomic stop. Joined before DB/client shutdown.
class BrowseThread : public ACE_Based::Runnable
{
    public:
        static const size_t QUEUE_CAP      = 256u;                  ///< max queued browses
        static const size_t QUEUE_BYTE_CAP = 8u * 1024u * 1024u;   ///< 8 MB backstop

        BrowseThread(ServiceDatabase& db, IpcClient& cli)
            : m_db(db), m_cli(cli),
              m_queue(QUEUE_CAP, QUEUE_BYTE_CAP),
              m_stop(false),
              m_processed(0), m_rejected(0), m_dbErrors(0)
        {}

        /// Enqueue a decoded query (thread-safe). On queue-full, sends an
        /// immediate tooMany IPC_BROWSE_RESULT (D4) so mangosd falls back
        /// in-process at once instead of waiting the ~10s TTL; returns false.
        bool Submit(const BrowseQuery& q);

        void Stop() { m_stop.store(true); }

        void run() override;

        uint64 Processed() const { return m_processed.load(std::memory_order_relaxed); }
        uint64 Rejected()  const { return m_rejected.load(std::memory_order_relaxed); }
        uint64 DbErrors()  const { return m_dbErrors.load(std::memory_order_relaxed); }

    private:
        ServiceDatabase&          m_db;
        IpcClient&                m_cli;
        BoundedQueue<BrowseQuery> m_queue;
        std::atomic<bool>         m_stop;
        std::atomic<uint64>       m_processed;
        std::atomic<uint64>       m_rejected;
        std::atomic<uint64>       m_dbErrors;

        // Non-copyable.
        BrowseThread(const BrowseThread&);
        BrowseThread& operator=(const BrowseThread&);
};

#endif // AH_WORKER_BROWSE_HANDLER_H
