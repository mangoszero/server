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
#include "Utilities/Util.h"   // Utf8FitTo / Utf8toWStr (src/shared; worker links `shared`)

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

    /// Usable verdict. Task 6 replaces the body with AhUsability::IsUsable +
    /// the recipe-known check against q.knownRecipeCastSpells. Stub = true.
    bool RowUsable(const BrowseRow& /*r*/, const BrowseQuery& /*q*/)
    {
        return true;
    }
}

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
                ++res.totalcount;
                res.entries.push_back(r.entry);
                continue;
            }

            if (defer)
            {
                // Un-paginated: collect every survivor (capped). totalcount
                // counts all survivors even past the cap so mangosd can log it.
                ++res.totalcount;
                if (res.entries.size() < BROWSE_ELUNA_CAP)
                {
                    res.entries.push_back(r.entry);
                }
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
                res.tooMany = 1u;
                res.entries.clear();   // mangosd does a full in-process browse instead
            }
        }

        return res;
    }
}
