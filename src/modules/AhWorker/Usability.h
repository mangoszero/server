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

#ifndef AH_WORKER_USABILITY_H
#define AH_WORKER_USABILITY_H

#include "Common.h"
#include "BrowseMessages.h"   // PlayerProfile, SkillRank, RepStanding

/// Classic (1.12) item class id for recipes (mangosd ITEM_CLASS_RECIPE == 9).
const uint32 AHW_ITEM_CLASS_RECIPE = 9u;

/// Item-side usability requirement columns. The worker fills this from
/// BrowseRow; the mangosd-side reference evaluator (Task 7) fills the same
/// fields from an ItemPrototype, so both drive the SAME free-function.
struct ItemUsabilityReq
{
    uint32 itemClass;             ///< item_template.class
    uint32 allowableClass;        ///< item_template.AllowableClass (bitmask)
    uint32 allowableRace;         ///< item_template.AllowableRace (bitmask)
    uint32 requiredLevel;         ///< item_template.RequiredLevel
    uint32 itemId;                ///< item_template.entry (mount-override switch)
    uint32 requiredSkill;         ///< item_template.RequiredSkill
    uint32 requiredSkillRank;     ///< item_template.RequiredSkillRank
    uint32 requiredSpell;         ///< item_template.RequiredSpell
    uint32 requiredHonorRank;     ///< item_template.RequiredHonorRank
    uint32 requiredRepFaction;    ///< item_template.RequiredReputationFaction
    uint32 requiredRepRank;       ///< item_template.RequiredReputationRank
    uint32 itemProficiencySkill;  ///< the item's own proficiency skill (0 = none)
};

namespace AhUsability
{
    /// True iff Player::CanUseItem(proto, direct_action=true) + the Item*
    /// overload extras (proficiency, reputation) == EQUIP_ERR_OK, EXCLUDING the
    /// Eluna OnCanUseItem hook (deferred to mangosd) and the recipe-known filter
    /// (a separate knownRecipeCastSpells membership test in BrowseHandler).
    /// minMountLevel/minEpicMountLevel are the two mount-override configs.
    bool IsUsable(const PlayerProfile& p, const ItemUsabilityReq& req,
                  uint32 minMountLevel, uint32 minEpicMountLevel);

    /// The item's own proficiency skill line from (class, subclass), mirroring
    /// Item::GetSkill() (D6). 0 = no proficiency line. Static Classic table.
    uint32 GetItemProficiencySkill(uint32 itemClass, uint32 itemSubClass);
}

#endif // AH_WORKER_USABILITY_H
