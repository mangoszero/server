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

namespace BrowseHandler
{
    /// PURE: usable filter (Task 6) + name filter (LIST) + pagination OR
    /// defer-un-paginated. Cheap proto filters are assumed SQL-applied.
    BrowseResult FilterAndPaginate(const std::vector<BrowseRow>& rows,
                                   const BrowseQuery& q);
}

#endif // AH_WORKER_BROWSE_HANDLER_H
