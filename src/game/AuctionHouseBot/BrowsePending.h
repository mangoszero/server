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

#ifndef MANGOS_AH_BROWSE_PENDING_H
#define MANGOS_AH_BROWSE_PENDING_H

#include "Common.h"
#include "BrowseMessages.h"
#include "Utilities/ByteBuffer.h"
#include <unordered_map>
#include <map>
#include <vector>
#include <string>

/// One outstanding browse request awaiting a worker reply. Holds the FULL
/// fallback request so a timeout/send-failure can re-run in-process by
/// re-resolving the player by playerGuidLow (C1/I4). Never holds a WorldSession*.
struct PendingBrowse
{
    uint32 accountId;
    uint32 playerGuidLow;     ///< re-resolve target (I4)
    uint8  kind;              ///< BrowseKind
    uint32 sentSec;          ///< time(NULL) at send; sweep uses GetGameTime(), within-tick ordering guarantees no underflow
    uint32 seq;              ///< per-(char,kind) sequence (I4 stale-guard)
    // Full fallback request:
    uint8  house;
    uint8  allHouses;
    uint32 itemClass;
    uint32 itemSubClass;
    uint32 inventoryType;
    uint32 quality;
    uint8  levelmin;
    uint8  levelmax;
    uint8  usable;
    uint8  deferEluna;
    uint32 listfrom;
    std::wstring wname;                  ///< lower-cased UTF-16 (in-process Utf8FitTo)
};

class BrowsePendingMap
{
    public:
        /// D3: hard cap on outstanding entries (mirrors DedupCache::MAX_ENTRIES).
        /// Register returns 0 at the cap so the caller serves in-process inline.
        static const size_t MAX_PENDING = 4096u;

        BrowsePendingMap() : m_nextId(1u) {}

        /// Allocate a monotonic non-zero queryId, or 0 if at MAX_PENDING (D3).
        uint64 Register(const PendingBrowse& pb, uint32 nowSec);
        bool Take(uint64 queryId, PendingBrowse& out);
        /// Drop entries older than ttlSec; append each dropped entry to timedOut.
        void Sweep(uint32 nowSec, uint32 ttlSec, std::vector<PendingBrowse>& timedOut);

        /// Bump + return the current sequence for (player, kind).
        uint32 NextSeqFor(uint32 playerGuidLow, uint8 kind);
        /// True iff seq is the latest issued for (player, kind).
        bool IsCurrent(uint32 playerGuidLow, uint8 kind, uint32 seq) const;

        size_t Size() const { return m_map.size(); }

    private:
        static uint64 SeqKey(uint32 guid, uint8 kind)
        { return (uint64(guid) << 8) | uint64(kind); }

        std::unordered_map<uint64, PendingBrowse> m_map;
        std::map<uint64, uint32> m_seq;   ///< (guid,kind) -> latest seq
        uint64 m_nextId;
};

// (V2: the v3 AH_ELUNA_PASS_PER_TICK budget was removed -- it kept unchecked
// over-budget entries as survivors, which counted/showed Lua-vetoed items. The
// deferred-Eluna pass now runs OnCanUseItem over the FULL surviving set, bounded
// by the worker's 1000-entry deferEluna cap. See Task 11 Step 6.)

/// Assemble SMSG body: uint32 count, each entry in BuildAuctionInfo wire order
/// (GUIDs wrapped HIGHGUID_PLAYER), uint32 totalcount.
void AhAssembleBrowseListBody(const std::vector<BrowseEntry>& entries,
                              uint32 totalcount, ByteBuffer& out);

/// V6/D9: build the recipe cast->taught map once. Defined in
/// AuctionHouseHandler.cpp; called from the TOP of AuctionHouseMgr::LoadAuctionItems
/// (before its empty-AH early return). Idempotent.
void AhEnsureRecipeCastMap();

#endif // MANGOS_AH_BROWSE_PENDING_H
