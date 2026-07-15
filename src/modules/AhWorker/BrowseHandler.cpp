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

#include "BrowseHandler.h"
#include "Usability.h"
#include "Utilities/Util.h"   // Utf8FitTo / Utf8toWStr (src/shared; worker links `shared`)
#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "ItemInstanceFields.h"
#include "Log/Log.h"
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace
{
    /// Unicode-correct name match -- EXACT parity with the in-process AH
    /// (V4: AuctionHouseMgr::BuildListAuctionItems uses Utf8FitTo, NOT an ASCII
    /// fold). The needle arrives already lower-cased UTF-8 from mangosd
    /// (wstrToLower + WStrToUtf8); the caller converts it to wide ONCE.
    /// Utf8FitTo lowercases the row name (UTF-8 -> wide -> wstrToLower) and does a
    /// wide substring search, so non-Latin locale names match correctly. The
    /// worker links `shared`, so Util.h needs no game header.
    bool NameMatches(const std::string& rowName, const std::wstring& needleLowerW)
    {
        if (needleLowerW.empty())
        {
            return true;
        }
        return Utf8FitTo(rowName, needleLowerW);
    }

    /// Usable verdict. Full parity with Player::CanUseItem(proto, direct_action=true)
    /// plus the Item* overload extras (proficiency, reputation), minus the Eluna
    /// OnCanUseItem hook (deferred to mangosd). Also filters known recipes (C2b).
    bool RowUsable(const BrowseRow& r, const BrowseQuery& q)
    {
        ItemUsabilityReq req;
        req.itemClass            = r.itemClass;
        req.allowableClass       = r.allowableClass;
        req.allowableRace        = r.allowableRace;
        req.requiredLevel        = r.requiredLevel;
        req.itemId               = r.entry.itemEntry;
        req.requiredSkill        = r.reqSkill;
        req.requiredSkillRank    = r.reqSkillRank;
        req.requiredSpell        = r.reqSpell;
        req.requiredHonorRank    = r.reqHonorRank;
        req.requiredRepFaction   = r.reqRepFaction;
        req.requiredRepRank      = r.reqRepRank;
        req.itemProficiencySkill = r.itemProficiencySkill;
        if (!AhUsability::IsUsable(q.profile, req, q.minMountLevel, q.minEpicMountLevel))
        {
            return false;
        }
        // Recipe-known (C2b): a recipe whose CAST spell (spellid_1) is in the
        // mangosd-derived known set is filtered out. The worker NEVER resolves
        // the taught spell; mangosd already did the EffectTriggerSpell[0] ->
        // HasSpell resolution to build knownRecipeCastSpells.
        if (req.itemClass == AHW_ITEM_CLASS_RECIPE && r.castSpellId != 0u)
        {
            for (size_t i = 0; i < q.knownRecipeCastSpells.size(); ++i)
            {
                if (q.knownRecipeCastSpells[i] == r.castSpellId)
                {
                    return false;
                }
            }
        }
        return true;
    }
}

// ---------------------------------------------------------------------------
// Anonymous helpers for Fetch
// ---------------------------------------------------------------------------

namespace
{
    /// Build the SQL house-scope clause.
    /// allHouses (AllowTwoSide.Interaction.Auction) => neutral house (houseid 7).
    /// Else by team: 0 -> (1,2,3)  1 -> (4,5,6)  2 -> 7.
    std::string HouseClause(const BrowseQuery& q)
    {
        if (q.allHouses != 0u)
        {
            return " AND a.houseid = 7";
        }
        if (q.house == 0u)
        {
            return " AND a.houseid IN (1,2,3)";
        }
        if (q.house == 1u)
        {
            return " AND a.houseid IN (4,5,6)";
        }
        return " AND a.houseid = 7";
    }
}

// ---------------------------------------------------------------------------
// BrowseHandler::Fetch
// ---------------------------------------------------------------------------

namespace BrowseHandler
{

std::vector<BrowseRow> ComposeBidderRows(const std::vector<BrowseRow>& rows,
                                         const BrowseQuery& q)
{
    std::vector<BrowseRow> composed;
    composed.reserve(q.outbidIds.size() + rows.size());

    std::unordered_map<uint32, const BrowseRow*> byAuctionId;
    byAuctionId.reserve(rows.size());
    for (size_t i = 0; i < rows.size(); ++i)
    {
        byAuctionId[rows[i].entry.id] = &rows[i];
    }

    // Legacy parity: trust the validated client list, preserve its order and
    // duplicates, and silently skip ids absent from this house-scoped result.
    for (size_t i = 0; i < q.outbidIds.size(); ++i)
    {
        std::unordered_map<uint32, const BrowseRow*>::const_iterator it =
            byAuctionId.find(q.outbidIds[i]);
        if (it != byAuctionId.end())
        {
            composed.push_back(*it->second);
        }
    }

    // Fetch orders rows by auction id, matching BuildListBidderItems' map
    // traversal. An auction also present above intentionally appears twice.
    for (size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i].entry.bidderGuidLow == q.requesterGuidLow)
        {
            composed.push_back(rows[i]);
        }
    }

    return composed;
}

BrowseResult Fetch(ServiceDatabase& db, const BrowseQuery& q, FetchStatus& status)
{
    status = FETCH_OK;
    std::vector<BrowseRow> rows;

    // D7: build the world-DB qualifier ONCE as a std::string so there is no
    // dangling .c_str() from a temporary. Empty => unqualified (same schema).
    const std::string worldDb   = db.WorldDbName();
    const std::string worldQual = worldDb.empty()
        ? std::string()
        : ("`" + worldDb + "`.");

    // Locale overlay (I2/D7/V3): a SINGLE LEFT JOIN locales_item — NOT a
    // per-row N+1. V3: the column suffix is the LocaleConstant DIRECTLY
    // (name_loc{lc}, NO +1), bounds-checked against MAX_LOCALE. The join
    // condition and the row-read guard MUST match exactly so f[26] is only
    // read when the join was added.
    std::string locCol;
    std::string locJoin;
    if (q.localeIndex >= 1 && int(q.localeIndex) < MAX_LOCALE)
    {
        char lc[64];
        snprintf(lc, sizeof(lc), ", li.name_loc%d", int(q.localeIndex));
        locCol  = lc;
        locJoin = " LEFT JOIN " + worldQual + "locales_item li ON a.item_template = li.entry";
    }

    // auction + item_instance live in the character DB; item_template (and
    // its locale overlay) in the world DB. Blob decoded in code (Task 4).
    std::string sql =
        "SELECT a.id, a.item_template, a.item_count, a.item_randompropertyid,"
        "       a.itemowner, a.buyoutprice, a.lastbid, a.startbid, a.time, a.buyguid,"
        "       ii.data,"
        "       it.class, it.subclass, it.InventoryType, it.Quality, it.RequiredLevel,"
        "       it.AllowableClass, it.AllowableRace, it.RequiredSkill, it.RequiredSkillRank,"
        "       it.RequiredSpell, it.RequiredHonorRank, it.RequiredReputationFaction,"
        "       it.RequiredReputationRank, it.name, it.spellid_1";
    sql += locCol;                   // trailing localized-name column (f[26]) when set
    sql += " FROM auction a";
    sql += " JOIN item_instance ii ON a.itemguid = ii.guid";
    sql += " JOIN " + worldQual + "item_template it ON a.item_template = it.entry";
    sql += locJoin;
    sql += " WHERE 1=1";
    sql += HouseClause(q);

    if (q.kind == static_cast<uint8>(BROWSE_LIST))
    {
        char buf[128];
        if (q.itemClass != 0xFFFFFFFFu)
        {
            snprintf(buf, sizeof(buf), " AND it.class = %u", q.itemClass);
            sql += buf;
        }
        if (q.itemSubClass != 0xFFFFFFFFu)
        {
            snprintf(buf, sizeof(buf), " AND it.subclass = %u", q.itemSubClass);
            sql += buf;
        }
        if (q.inventoryType != 0xFFFFFFFFu)
        {
            snprintf(buf, sizeof(buf), " AND it.InventoryType = %u", q.inventoryType);
            sql += buf;
        }
        if (q.quality != 0xFFFFFFFFu)
        {
            snprintf(buf, sizeof(buf), " AND it.Quality >= %u", q.quality);
            sql += buf;
        }
        if (q.levelmin != 0u)
        {
            if (q.levelmax != 0u)
            {
                snprintf(buf, sizeof(buf), " AND it.RequiredLevel >= %u AND it.RequiredLevel <= %u",
                         q.levelmin, q.levelmax);
            }
            else
            {
                snprintf(buf, sizeof(buf), " AND it.RequiredLevel >= %u", q.levelmin);
            }
            sql += buf;
        }
    }
    else if (q.kind == static_cast<uint8>(BROWSE_OWNER))
    {
        char buf[96];
        snprintf(buf, sizeof(buf), " AND a.itemowner = %u", q.requesterGuidLow);
        sql += buf;
    }
    else
    {
        // BIDDER needs both parts of the legacy reply: auctions currently bid
        // by the requester plus client-supplied outbid ids. The decoded rows
        // are composed in exact client-visible order after this single query.
        char buf[64];
        snprintf(buf, sizeof(buf), " AND (a.buyguid = %u", q.requesterGuidLow);
        sql += buf;
        if (!q.outbidIds.empty())
        {
            sql += " OR a.id IN (";
            for (size_t i = 0; i < q.outbidIds.size(); ++i)
            {
                if (i != 0u)
                {
                    sql += ',';
                }
                snprintf(buf, sizeof(buf), "%u", q.outbidIds[i]);
                sql += buf;
            }
            sql += ')';
        }
        sql += ')';
    }

    sql += " ORDER BY a.id";

    QueryResult* result = db.Character().Query(sql.c_str());
    if (!result)
    {
        // 0 rows from the FILTERED query. This is almost always a legitimate
        // empty result: a filtered LIST that matched nothing, an OWNER list with
        // no auctions, a BIDDER list with no bids (e.g. right after a buyout
        // deletes the won auction), or an empty house. Distinguish that from a
        // genuinely-broken cross-DB JOIN by probing the BARE JOIN (NO WHERE
        // filters). If it returns a row, the cross-DB wiring is fine and this
        // query simply matched nothing -> serve an empty list. (Comparing a
        // filtered 0 against an unfiltered COUNT(*) false-positived on EVERY
        // empty filtered browse.)
        // F3: probe the SAME schema surface the real query uses -- including the
        // localized-name join/column when a locale overlay is active -- so a
        // locale-specific schema fault (missing locales_item / name_locN) is
        // reported as a DB error rather than slipping through as a false "empty".
        std::string probeSql =
            "SELECT 1" + locCol +
            " FROM auction a"
            " JOIN item_instance ii ON a.itemguid = ii.guid"
            " JOIN " + worldQual + "item_template it ON a.item_template = it.entry"
            + locJoin +
            " LIMIT 1";
        QueryResult* joinProbe = db.Character().Query(probeSql.c_str());
        if (joinProbe)
        {
            delete joinProbe;
            status = FETCH_EMPTY;
            return BrowseHandler::FilterAndPaginate(rows, q);
        }

        // I3: the bare JOIN returned nothing either. NULL can be a genuinely
        // empty auction table OR a DB failure. Probe with COUNT(*) to
        // distinguish. If that also fails => FETCH_DB_ERROR and we do NOT send
        // any reply (mangosd's TTL sweep -> "AH unavailable").
        QueryResult* probe = db.Character().Query("SELECT COUNT(*) FROM auction");
        if (!probe)
        {
            status = FETCH_DB_ERROR;
            BrowseResult empty;
            empty.queryId     = q.queryId;
            empty.kind        = q.kind;
            empty.elunaPending = 0u;
            empty.tooMany     = 0u;
            empty.totalcount  = 0u;
            return empty;
        }
        const uint32 auctionRows = probe->Fetch()[0].GetUInt32();
        delete probe;
        if (auctionRows != 0u)
        {
            // Rows exist in auction but the JOIN returned nothing => DB/config
            // failure (cross-DB JOIN broken, missing grant, etc.).
            status = FETCH_DB_ERROR;
            sLog.outError("BrowseHandler::Fetch: JOIN empty but auction has %u rows"
                          " (cross-DB/config failure)", auctionRows);
            BrowseResult empty;
            empty.queryId     = q.queryId;
            empty.kind        = q.kind;
            empty.elunaPending = 0u;
            empty.tooMany     = 0u;
            empty.totalcount  = 0u;
            return empty;
        }
        // auction is genuinely empty.
        status = FETCH_EMPTY;
        return BrowseHandler::FilterAndPaginate(rows, q);
    }

    do
    {
        Field* f = result->Fetch();
        BrowseRow r;

        const uint32 aId      = f[0].GetUInt32();
        const uint32 itemTmpl = f[1].GetUInt32();
        const uint32 itemCnt  = f[2].GetUInt32();
        const int32  randProp = f[3].GetInt32();    // auction.item_randompropertyid
        const uint32 owner    = f[4].GetUInt32();
        const uint32 buyout   = f[5].GetUInt32();
        const uint32 lastbid  = f[6].GetUInt32();
        const uint32 startbid = f[7].GetUInt32();
        const uint64 expire   = f[8].GetUInt64();
        const uint32 buyguid  = f[9].GetUInt32();

        const std::string blob = f[10].GetCppString();
        ItemInstanceFields iif = AhItemBlob::Decode(blob);

        r.entry.id            = aId;
        r.entry.itemEntry     = itemTmpl;
        r.entry.enchantId     = iif.valid ? iif.enchantId     : 0u;
        r.entry.randomPropId  = static_cast<uint32>(randProp); // auction col is authoritative
        r.entry.suffixFactor  = iif.valid ? iif.suffixFactor  : 0u;
        r.entry.count         = itemCnt;                       // auction col authoritative
        r.entry.charges       = iif.valid ? iif.charges        : 0;
        r.entry.ownerGuidLow  = owner;
        r.entry.startbid      = startbid;

        // outbid = GetAuctionOutBid(): (bid/100)*5, minimum 1 if bid>0.
        if (lastbid != 0u)
        {
            uint32 ob = (lastbid / 100u) * 5u;
            if (ob == 0u)
            {
                ob = 1u;
            }
            r.entry.outbid = ob;
        }
        else
        {
            r.entry.outbid = 0u;
        }

        r.entry.buyout       = buyout;

        {
            time_t now    = time(NULL);
            uint32 leftMs = 0u;
            if (static_cast<time_t>(expire) > now)
            {
                leftMs = static_cast<uint32>(
                    (expire - static_cast<uint64>(now)) * 1000u);
            }
            r.entry.timeLeftMs = leftMs;
        }

        r.entry.bidderGuidLow = buyguid;
        // curBid: if bid && startbid > bid, use startbid; else lastbid.
        r.entry.curBid = (lastbid && startbid > lastbid) ? startbid : lastbid;

        r.itemClass       = f[11].GetUInt32();
        r.itemSubClass    = f[12].GetUInt32();
        r.inventoryType   = f[13].GetUInt32();
        r.quality         = f[14].GetUInt32();
        r.requiredLevel   = f[15].GetUInt32();
        r.allowableClass  = f[16].GetUInt32();
        r.allowableRace   = f[17].GetUInt32();
        r.reqSkill        = f[18].GetUInt32();
        r.reqSkillRank    = f[19].GetUInt32();
        r.reqSpell        = f[20].GetUInt32();
        r.reqHonorRank    = f[21].GetUInt32();
        r.reqRepFaction   = f[22].GetUInt32();
        r.reqRepRank      = f[23].GetUInt32();
        r.name            = f[24].GetCppString();   // enUS Name1 (locale overlay below)
        r.castSpellId     = f[25].GetUInt32();       // spellid_1

        // D6: fill the item's own proficiency skill from (class, subclass) for
        // FULL parity with BuildAuctionInfo. Never 0 for equippable weapon/armor.
        r.itemProficiencySkill =
            AhUsability::GetItemProficiencySkill(r.itemClass, r.itemSubClass);

        // Locale overlay (I2/D7/V3): f[26] is present ONLY when the join was
        // added (same guard as the join condition above). A NULL/empty value
        // keeps the enUS Name1.
        if (q.localeIndex >= 1 && int(q.localeIndex) < MAX_LOCALE)
        {
            std::string loc = f[26].GetCppString();
            if (!loc.empty())
            {
                r.name = loc;
            }
        }

        rows.push_back(r);
    }
    while (result->NextRow());
    delete result;

    if (q.kind == static_cast<uint8>(BROWSE_BIDDER))
    {
        rows = BrowseHandler::ComposeBidderRows(rows, q);
    }

    status = rows.empty() ? FETCH_EMPTY : FETCH_OK;
    return BrowseHandler::FilterAndPaginate(rows, q);
}

} // namespace BrowseHandler (Fetch)

// ---------------------------------------------------------------------------
// BrowseThread
// ---------------------------------------------------------------------------

bool BrowseThread::Submit(const BrowseQuery& q)
{
    if (m_queue.push(q))
    {
        return true;
    }
    m_rejected.fetch_add(1, std::memory_order_relaxed);
    // D4: queue saturated. Reply tooMany=1 NOW so mangosd tells the player the
    // AH is unavailable immediately instead of waiting the ~10s TTL (coordinator
    // model: no in-process fallback).
    // IpcClient::SendFrame is CONFIRMED thread-safe (see header comment).
    BrowseResult full;
    full.queryId      = q.queryId;
    full.kind         = q.kind;
    full.elunaPending = 0u;
    full.tooMany      = 1u;
    full.totalcount   = 0u;
    IpcMessage rm;
    rm.op = IPC_BROWSE_RESULT;
    full.Encode(rm.body);
    m_cli.SendFrame(rm);
    return false;
}

void BrowseThread::run()
{
    // C4: per-thread MySQL init/teardown for this thread's connection.
    m_db.Character().ThreadStart();

    BrowseQuery q;
    while (!m_stop.load())
    {
        if (m_queue.pop(q))
        {
            FetchStatus st = FETCH_OK;
            BrowseResult res = BrowseHandler::Fetch(m_db, q, st);
            if (st == FETCH_DB_ERROR)
            {
                // I3: no reply -- mangosd's TTL sweep tells the player the AH is
                // unavailable (coordinator model: no in-process fallback).
                m_dbErrors.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            IpcMessage rm;
            rm.op = IPC_BROWSE_RESULT;
            if (res.entries.size() > BrowseResult::MAX_ENTRIES)
            {
                BrowseResult capped;
                capped.queryId      = res.queryId;
                capped.kind         = res.kind;
                capped.elunaPending = 0u;
                capped.tooMany      = 1u;
                capped.totalcount   = 0u;
                capped.Encode(rm.body);
            }
            else
            {
                res.Encode(rm.body);
            }
            // I8: preflight outbound body size; never emit over the MAXLEN cap.
            if (rm.body.size() > BrowseResult::MAX_WIRE)
            {
                // Should be impossible within MAX_ENTRIES; fail safe by
                // replying tooMany so mangosd tells the player the AH is
                // unavailable (coordinator model: no in-process fallback).
                BrowseResult capped;
                capped.queryId      = res.queryId;
                capped.kind         = res.kind;
                capped.elunaPending = 0u;
                capped.tooMany      = 1u;
                capped.totalcount   = 0u;
                rm.body.clear();
                capped.Encode(rm.body);
            }
            m_cli.SendFrame(rm);
            m_processed.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            ACE_Based::Thread::Sleep(5);
        }
    }

    m_db.Character().ThreadEnd();
}

// ---------------------------------------------------------------------------

namespace BrowseHandler
{
    BrowseResult FilterAndPaginate(const std::vector<BrowseRow>& rows,
                                   const BrowseQuery& q)
    {
        BrowseResult res;
        res.queryId      = q.queryId;
        res.kind         = q.kind;
        res.elunaPending = 0u;
        res.tooMany      = 0u;
        res.totalcount   = 0u;

        const bool isList = (q.kind == static_cast<uint8>(BROWSE_LIST));
        const bool defer  = isList && (q.deferEluna != 0u);

        // V4: convert the (already lower-cased UTF-8) needle to wide ONCE for
        // Utf8FitTo parity. Empty needle matches everything. If a NON-empty needle
        // fails to convert (malformed UTF-8), match NOTHING -- mirrors the live
        // in-process handler, which returns early on a bad Utf8toWStr (v4-verify
        // R2: an empty wneedle must NOT silently fall through to match-all).
        std::wstring wneedle;
        bool needleBad = false;
        if (isList && !q.searchedName.empty())
        {
            needleBad = !Utf8toWStr(q.searchedName, wneedle);
        }

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const BrowseRow& r = rows[i];

            if (isList)
            {
                if (needleBad)
                {
                    continue;   // malformed search name -> no matches (parity)
                }
                if (q.usable != 0u && !RowUsable(r, q))
                {
                    continue;
                }
                if (!NameMatches(r.name, wneedle))
                {
                    continue;
                }
            }

            if (!isList)
            {
                // OWNER/BIDDER: every row is an entry.
                if (res.entries.size() >= BrowseResult::MAX_ENTRIES)
                {
                    // The IPC decoder rejects a larger count. Decline now so
                    // mangosd consumes the pending request and immediately
                    // reports the AH as unavailable instead of timing out.
                    res.tooMany = 1u;
                    res.entries.clear();
                    res.totalcount = 0u;
                    return res;
                }
                ++res.totalcount;
                res.entries.push_back(r.entry);
                continue;
            }

            if (defer)
            {
                // Un-paginated full set (capped): the common exact-parity path --
                // mangosd runs Eluna over all of these, then paginates. totalcount
                // counts all survivors even past the cap so the >cap edge below can
                // detect it.
                if (res.entries.size() < BROWSE_ELUNA_CAP)
                {
                    res.entries.push_back(r.entry);
                }
                ++res.totalcount;
            }
            else
            {
                // Match the in-process single-pass order: emit while count < 50
                // and totalcount >= listfrom, then bump totalcount per survivor.
                if (res.entries.size() < 50u && res.totalcount >= q.listfrom)
                {
                    res.entries.push_back(r.entry);
                }
                ++res.totalcount;
            }
        }

        if (defer)
        {
            res.elunaPending = 1u;
            if (res.totalcount > BROWSE_ELUNA_CAP)
            {
                // >cap (decision #2): too many survivors for an EXACT deferred-Eluna
                // pass -- mangosd can only veto what the worker ships, so a
                // pre-paginated page would be short (vetoed items not backfilled)
                // with a pre-veto totalcount. The coordinator does not serve an
                // approximate result: the worker declines (tooMany) and mangosd
                // sends "AH unavailable". Rare: needs >cap survivors AND a bound
                // OnCanUseItem Lua hook.
                res.tooMany      = 1u;
                res.elunaPending = 0u;
                res.entries.clear();
                res.totalcount   = 0u;
            }
        }

        return res;
    }
}
