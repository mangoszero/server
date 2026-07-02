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

#ifndef AH_WORKER_JOURNAL_H
#define AH_WORKER_JOURNAL_H

#include "Common.h"

#include <string>
#include <vector>

class ServiceDatabase;

/**
 * @file Journal.h
 * @brief SP-2 worker journal DAO (ah_worker_journal), the cross-process
 *        outbox/recovery ledger (spec 5.3). Writes that must co-commit with a
 *        book/auction write are appended to the caller's OPEN character-DB
 *        transaction, mirroring CustodyService's contract; reads are
 *        synchronous SELECTs.
 *
 * The facts payload is an encoded wire struct (binary, contains NULs); the
 * DAO stores it as ASCII hex in the BLOB column and decodes on read, because
 * printf-style PExecute / Field::GetCppString truncate at NUL.
 */
namespace AhJournal
{
    enum JournalState : uint8
    {
        JRN_COMMITTED       = 1,  ///< player mutation committed; awaiting nothing
        JRN_RESOLVING       = 2,  ///< worker-initiated resolution sent; awaiting ACK
        JRN_APPLIED         = 3,  ///< terminal; mangosd applied value (prune target)
        JRN_CANCEL_PREPARED = 4,  ///< cancel PREPARE lock; awaiting CONFIRM/ABORT
        JRN_INTENT_PENDING  = 5   ///< bot sell intent sent; awaiting materialization
    };

    struct JournalRow
    {
        uint64      uuid;
        uint32      auctionId;
        uint8       kind;          ///< opcode low byte (player) or ResolveKind
        uint8       state;         ///< JournalState
        std::string facts;         ///< decoded binary payload (empty when none)
        uint64      createdTime;
        uint64      resolvedTime;
    };

    /// Append an INSERT for @p row to the caller's open character-DB txn.
    void Insert(ServiceDatabase& db, JournalRow const& row);

    /// Append a state/resolved_time UPDATE for @p uuid to the open txn.
    void SetState(ServiceDatabase& db, uint64 uuid, uint8 state, uint64 resolvedTime);

    /// Synchronous fetch by uuid. @return true if found.
    bool Get(ServiceDatabase& db, uint64 uuid, JournalRow& out);

    /// Synchronous load of every non-terminal row (state IN 1,2,4,5) at boot.
    void LoadActive(ServiceDatabase& db, std::vector<JournalRow>& out);

    /// Prune JRN_APPLIED rows with resolved_time < @p cutoff (standalone).
    void DeleteAppliedOlderThan(ServiceDatabase& db, uint64 cutoff);
}

#endif // AH_WORKER_JOURNAL_H
