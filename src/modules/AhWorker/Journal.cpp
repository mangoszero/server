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

#include "Journal.h"
#include "ServiceDatabase.h"
#include "Database/DatabaseEnv.h"

#include <ctime>
#include <limits>

namespace
{
    enum PruneProtection
    {
        PRUNE_PROTECT_COMMITTED_RECONCILE,
        PRUNE_PROTECT_APPLIED_IDEMPOTENCY
    };

    /// Encode binary @p in as lowercase ASCII hex (NUL-safe for SQL literals).
    std::string HexEncode(std::string const& in)
    {
        static const char* k = "0123456789abcdef";
        std::string out;
        out.reserve(in.size() * 2u);
        for (size_t i = 0; i < in.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(in[i]);
            out.push_back(k[c >> 4]);
            out.push_back(k[c & 0x0Fu]);
        }
        return out;
    }

    /// Decode ASCII hex @p in back to bytes. Malformed input yields "".
    std::string HexDecode(std::string const& in)
    {
        std::string out;
        if (in.size() % 2u != 0u)
        {
            return out;
        }
        out.reserve(in.size() / 2u);
        for (size_t i = 0; i + 1u < in.size(); i += 2u)
        {
            int hi = 0;
            int lo = 0;
            char a = in[i];
            char b = in[i + 1u];
            hi = (a >= '0' && a <= '9') ? (a - '0')
               : (a >= 'a' && a <= 'f') ? (a - 'a' + 10)
               : (a >= 'A' && a <= 'F') ? (a - 'A' + 10) : -1;
            lo = (b >= '0' && b <= '9') ? (b - '0')
               : (b >= 'a' && b <= 'f') ? (b - 'a' + 10)
               : (b >= 'A' && b <= 'F') ? (b - 'A' + 10) : -1;
            if (hi < 0 || lo < 0)
            {
                return std::string();
            }
            out.push_back(static_cast<char>((hi << 4) | lo));
        }
        return out;
    }

    bool CountPruneCandidates(ServiceDatabase& db, char const* ageColumn,
                              uint8 state, uint64 cutoff, uint32 limit,
                              PruneProtection protection, uint32& count)
    {
        char const* protectionClause = "";
        if (protection == PRUNE_PROTECT_COMMITTED_RECONCILE)
        {
            protectionClause =
                " AND NOT EXISTS (SELECT 1 FROM `custody_ledger` AS `c` "
                "WHERE `c`.`auction_id` = `j`.`auction_id` "
                "AND `c`.`state` = 0)";
        }
        else if (protection == PRUNE_PROTECT_APPLIED_IDEMPOTENCY)
        {
            protectionClause =
                " AND NOT EXISTS (SELECT 1 FROM `custody_ledger` AS `c` "
                "WHERE `c`.`idem_key` = CONCAT('resolve:', `j`.`uuid`)) "
                "AND NOT EXISTS (SELECT 1 FROM `custody_ledger` AS `c` "
                "WHERE `c`.`idem_key` = CONCAT('botlist:', `j`.`uuid`))";
        }
        QueryResult* result = db.Character().PQuery(
            "SELECT COUNT(*) FROM ("
            "SELECT `j`.`uuid` FROM `ah_worker_journal` AS `j` "
            "WHERE `j`.`state` = %u AND `j`.`%s` < %llu%s "
            "ORDER BY `j`.`%s`, `j`.`uuid` LIMIT %u"
            ") AS `prune_candidates`",
            static_cast<uint32>(state), ageColumn,
            static_cast<unsigned long long>(cutoff), protectionClause,
            ageColumn, limit);
        if (result == NULL)
        {
            return false;
        }

        count = result->Fetch()[0].GetUInt32();
        delete result;
        return true;
    }
}

void AhJournal::Insert(ServiceDatabase& db, JournalRow const& row)
{
    std::string const hexFacts = HexEncode(row.facts);
    db.Character().PExecute(
        "INSERT INTO `ah_worker_journal` "
        "(`uuid`,`auction_id`,`kind`,`state`,`facts`,`created_time`,`resolved_time`) "
        "VALUES (%llu, %u, %u, %u, '%s', %llu, %llu)",
        static_cast<unsigned long long>(row.uuid),
        row.auctionId, static_cast<uint32>(row.kind),
        static_cast<uint32>(row.state), hexFacts.c_str(),
        static_cast<unsigned long long>(row.createdTime),
        static_cast<unsigned long long>(row.resolvedTime));
}

void AhJournal::SetState(ServiceDatabase& db, uint64 uuid, uint8 state,
                         uint64 resolvedTime)
{
    db.Character().PExecute(
        "UPDATE `ah_worker_journal` SET `state` = %u, `resolved_time` = %llu "
        "WHERE `uuid` = %llu",
        static_cast<uint32>(state),
        static_cast<unsigned long long>(resolvedTime),
        static_cast<unsigned long long>(uuid));
}

void AhJournal::SetAppliedNow(ServiceDatabase& db, uint64 uuid)
{
    uint64 wallNow = static_cast<uint64>(time(NULL));
    if (wallNow == std::numeric_limits<uint64>::max())
    {
        // Fail safe: retain the row instead of making a fresh APPLIED row look
        // ancient to a wall-clock retention cutoff.
        wallNow = UINT64_C(0x7FFFFFFFFFFFFFFF);
    }
    SetState(db, uuid, JRN_APPLIED, wallNow);
}

bool AhJournal::Get(ServiceDatabase& db, uint64 uuid, JournalRow& out)
{
    QueryResult* result = db.Character().PQuery(
        "SELECT `uuid`,`auction_id`,`kind`,`state`,`facts`,"
        "`created_time`,`resolved_time` "
        "FROM `ah_worker_journal` WHERE `uuid` = %llu",
        static_cast<unsigned long long>(uuid));
    if (result == NULL)
    {
        return false;
    }
    Field* f = result->Fetch();
    out.uuid         = static_cast<uint64>(f[0].GetUInt64());
    out.auctionId    = f[1].GetUInt32();
    out.kind         = static_cast<uint8>(f[2].GetUInt32());
    out.state        = static_cast<uint8>(f[3].GetUInt32());
    out.facts        = HexDecode(f[4].GetCppString());
    out.createdTime  = static_cast<uint64>(f[5].GetUInt64());
    out.resolvedTime = static_cast<uint64>(f[6].GetUInt64());
    delete result;
    return true;
}

bool AhJournal::MaxSeqForRunId(ServiceDatabase& db, uint32 runId, bool highHalf,
                              uint32& outMaxSeq)
{
    // Contiguous PRIMARY KEY(uuid) sub-range for the requested minter's half of
    // this runId's low-32 space -- a sargable range scan (no function wraps the
    // indexed column). HIGH half = the MutationHandler minter [0x80000000+];
    // LOW half = the BotBrain minter [1, 0x7FFFFFFF] (seq 0 is never minted).
    uint64 const runBase = static_cast<uint64>(runId) << 32;
    uint64 const lo = runBase | (highHalf ? 0x80000000ULL : 0x00000001ULL);
    uint64 const hi = runBase | (highHalf ? 0xFFFFFFFFULL : 0x7FFFFFFFULL);

    QueryResult* result = db.Character().PQuery(
        "SELECT MAX(`uuid`) FROM `ah_worker_journal` "
        "WHERE `uuid` BETWEEN %llu AND %llu",
        static_cast<unsigned long long>(lo),
        static_cast<unsigned long long>(hi));
    if (result == NULL)
    {
        // A no-GROUP-BY MAX() ALWAYS returns exactly one row (NULL when the
        // range is empty), so a NULL RESULT is a genuine query error, never an
        // empty run. Fail closed: a 0 seq here would silently disarm the seed
        // and re-open the duplicate-PK collision.
        return false;
    }
    Field* f = result->Fetch();
    // Empty range -> MAX is NULL -> GetUInt64 yields 0 -> seq 0 (nothing to skip
    // past). GetUInt64 (not GetUInt32) because the low-32 can reach 0xFFFFFFFF,
    // above the signed-int parse range.
    outMaxSeq = static_cast<uint32>(f[0].GetUInt64() & 0xFFFFFFFFULL);
    delete result;
    return true;
}

void AhJournal::LoadActive(ServiceDatabase& db, std::vector<JournalRow>& out)
{
    QueryResult* result = db.Character().PQuery(
        "SELECT `uuid`,`auction_id`,`kind`,`state`,`facts`,"
        "`created_time`,`resolved_time` "
        "FROM `ah_worker_journal` WHERE `state` IN (1,2,4,5)");
    if (result == NULL)
    {
        return;
    }
    do
    {
        Field* f = result->Fetch();
        JournalRow r;
        r.uuid         = static_cast<uint64>(f[0].GetUInt64());
        r.auctionId    = f[1].GetUInt32();
        r.kind         = static_cast<uint8>(f[2].GetUInt32());
        r.state        = static_cast<uint8>(f[3].GetUInt32());
        r.facts        = HexDecode(f[4].GetCppString());
        r.createdTime  = static_cast<uint64>(f[5].GetUInt64());
        r.resolvedTime = static_cast<uint64>(f[6].GetUInt64());
        out.push_back(r);
    }
    while (result->NextRow());
    delete result;
}

bool AhJournal::DeleteTerminalBatchOlderThan(ServiceDatabase& db, uint64 cutoff,
                                             uint32 batchRows, bool& hasMore,
                                             uint32& deletedRows)
{
    hasMore = false;
    deletedRows = 0u;
    if (batchRows == 0u)
    {
        return true;
    }

    uint32 committedCount = 0u;
    uint32 appliedCount = 0u;
    if (!CountPruneCandidates(db, "created_time", JRN_COMMITTED, cutoff,
                              batchRows, PRUNE_PROTECT_COMMITTED_RECONCILE,
                              committedCount) ||
        !CountPruneCandidates(db, "resolved_time", JRN_APPLIED, cutoff,
                              batchRows, PRUNE_PROTECT_APPLIED_IDEMPOTENCY,
                              appliedCount))
    {
        return false;
    }

    if (committedCount == 0u && appliedCount == 0u)
    {
        return true;
    }

    if (!db.Character().BeginTransaction())
    {
        return false;
    }

    bool queued = true;
    if (committedCount != 0u)
    {
        queued = db.Character().PExecute(
            "DELETE FROM `ah_worker_journal` "
            "WHERE `state` = %u AND `created_time` < %llu "
            "AND NOT EXISTS (SELECT 1 FROM `custody_ledger` AS `c` "
            "WHERE `c`.`auction_id` = `ah_worker_journal`.`auction_id` "
            "AND `c`.`state` = 0) "
            "ORDER BY `created_time`, `uuid` LIMIT %u",
            static_cast<uint32>(JRN_COMMITTED),
            static_cast<unsigned long long>(cutoff), committedCount);
    }
    if (queued && appliedCount != 0u)
    {
        queued = db.Character().PExecute(
            "DELETE FROM `ah_worker_journal` "
            "WHERE `state` = %u AND `resolved_time` < %llu "
            "AND NOT EXISTS (SELECT 1 FROM `custody_ledger` AS `c` "
            "WHERE `c`.`idem_key` = CONCAT('resolve:', "
            "`ah_worker_journal`.`uuid`)) "
            "AND NOT EXISTS (SELECT 1 FROM `custody_ledger` AS `c` "
            "WHERE `c`.`idem_key` = CONCAT('botlist:', "
            "`ah_worker_journal`.`uuid`)) "
            "ORDER BY `resolved_time`, `uuid` LIMIT %u",
            static_cast<uint32>(JRN_APPLIED),
            static_cast<unsigned long long>(cutoff), appliedCount);
    }
    if (!queued)
    {
        db.Character().RollbackTransaction();
        return false;
    }
    if (!db.Character().CommitTransactionChecked())
    {
        return false;
    }

    deletedRows = committedCount + appliedCount;
    hasMore = committedCount == batchRows || appliedCount == batchRows;
    return true;
}
