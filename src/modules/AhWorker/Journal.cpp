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

namespace
{
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

void AhJournal::DeleteAppliedOlderThan(ServiceDatabase& db, uint64 cutoff)
{
    db.Character().DirectPExecute(
        "DELETE FROM `ah_worker_journal` WHERE `state` = %u AND `resolved_time` < %llu",
        static_cast<uint32>(JRN_APPLIED),
        static_cast<unsigned long long>(cutoff));
}
