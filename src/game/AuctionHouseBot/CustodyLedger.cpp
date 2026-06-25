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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "CustodyLedger.h"

#include "Common.h"
#include "Database/DatabaseEnv.h"

/// Column order used by SELECT queries (matches the struct field order for
/// LoadNonTerminal / Get):
///   0:id  1:idem_key  2:kind  3:role  4:state  5:owner_guid
///   6:beneficiary_guid  7:amount  8:item_guid  9:auction_id
///   10:created_time  11:resolved_time
#define CUSTODY_SELECT_COLS \
    "`id`,`idem_key`,`kind`,`role`,`state`,`owner_guid`," \
    "`beneficiary_guid`,`amount`,`item_guid`,`auction_id`," \
    "`created_time`,`resolved_time`"

/// Populate a CustodyRow from a QueryResult row whose fields were fetched in
/// CUSTODY_SELECT_COLS order.
static void FillRow(Field* f, CustodyRow& row)
{
    row.id              = f[0].GetUInt32();
    row.idemKey         = f[1].GetCppString();
    row.kind            = f[2].GetUInt8();
    row.role            = f[3].GetUInt8();
    row.state           = f[4].GetUInt8();
    row.ownerGuid       = f[5].GetUInt32();
    row.beneficiaryGuid = f[6].GetUInt32();
    row.amount          = f[7].GetUInt32();
    row.itemGuid        = f[8].GetUInt32();
    row.auctionId       = f[9].GetUInt32();
    row.createdTime     = f[10].GetUInt64();
    row.resolvedTime    = f[11].GetUInt64();
}

void CustodyLedger::Insert(CustodyRow const& r)
{
    std::string key = r.idemKey;
    CharacterDatabase.escape_string(key);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('%s','%u','%u','%u','%u','%u','%u','%u','%u','" UI64FMTD "','" UI64FMTD "')",
        key.c_str(),
        uint32(r.kind), uint32(r.role), uint32(r.state),
        r.ownerGuid, r.beneficiaryGuid, r.amount, r.itemGuid, r.auctionId,
        r.createdTime, r.resolvedTime);
}

void CustodyLedger::SetState(std::string const& idemKey, uint8 newState, uint64 resolvedTime)
{
    std::string key = idemKey;
    CharacterDatabase.escape_string(key);
    CharacterDatabase.PExecute(
        "UPDATE `custody_ledger` SET `state`='%u',`resolved_time`='" UI64FMTD "' "
        "WHERE `idem_key`='%s'",
        uint32(newState), resolvedTime, key.c_str());
}

void CustodyLedger::SetAmount(std::string const& idemKey, uint32 newAmount)
{
    std::string key = idemKey;
    CharacterDatabase.escape_string(key);
    CharacterDatabase.PExecute(
        "UPDATE `custody_ledger` SET `amount`='%u' WHERE `idem_key`='%s'",
        newAmount, key.c_str());
}

bool CustodyLedger::HasRows(uint32 auctionId)
{
    QueryResult* res = CharacterDatabase.PQuery(
        "SELECT 1 FROM `custody_ledger` WHERE `auction_id`=%u LIMIT 1", auctionId);
    if (!res)
    {
        return false;
    }
    delete res;
    return true;
}

void CustodyLedger::LoadNonTerminal(std::vector<CustodyRow>& out)
{
    QueryResult* result = CharacterDatabase.Query(
        "SELECT " CUSTODY_SELECT_COLS " FROM `custody_ledger` WHERE `state`=0");
    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        CustodyRow row;
        FillRow(fields, row);
        out.push_back(row);
    }
    while (result->NextRow());

    delete result;
}

bool CustodyLedger::Get(std::string const& idemKey, CustodyRow& out)
{
    std::string key = idemKey;
    CharacterDatabase.escape_string(key);
    QueryResult* result = CharacterDatabase.PQuery(
        "SELECT " CUSTODY_SELECT_COLS " FROM `custody_ledger` WHERE `idem_key`='%s'",
        key.c_str());
    if (!result)
    {
        return false;
    }

    Field* fields = result->Fetch();
    FillRow(fields, out);
    delete result;
    return true;
}

bool CustodyLedger::GetLiveBidKey(uint32 auctionId, std::string& out)
{
    QueryResult* result = CharacterDatabase.PQuery(
        "SELECT `idem_key` FROM `custody_ledger` "
        "WHERE `auction_id`=%u AND `role`=%u AND `state`=%u LIMIT 1",
        auctionId, uint32(ROLE_BID), uint32(CST_RESERVED));
    if (!result)
    {
        return false;
    }

    out = result->Fetch()[0].GetCppString();
    delete result;
    return true;
}

uint32 CustodyLedger::NextBidSeq(uint32 auctionId)
{
    QueryResult* result = CharacterDatabase.PQuery(
        "SELECT COUNT(*) FROM `custody_ledger` "
        "WHERE `auction_id`=%u AND `role`=%u",
        auctionId, uint32(ROLE_BID));
    if (!result)
    {
        return 0;
    }

    uint32 seq = result->Fetch()[0].GetUInt32();
    delete result;
    return seq;
}

void CustodyLedger::DeleteTerminalOlderThan(uint64 cutoffTime)
{
    CharacterDatabase.PExecute(
        "DELETE FROM `custody_ledger` "
        "WHERE `state` IN (1,2) AND `resolved_time` < '" UI64FMTD "'",
        cutoffTime);
}
