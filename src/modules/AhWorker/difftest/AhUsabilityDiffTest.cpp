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

// Differential drift guard: AhUsability::IsUsable (worker port) vs
// AhUsabilityRef::Evaluate (production reference). Both must agree on every
// synthetic (profile, item) pair, or one side has drifted. Separate exe; never
// linked into production mangosd.
//
#include "Usability.h"              // worker port (ah_usability)
#include "Object/AhUsabilityRef.h"  // reference (compiled in directly)
#include <cstdio>
#include <vector>

namespace
{
    struct Ctx
    {
        const PlayerProfile* p;
    };

    uint16 SkillCb(void* c, uint32 id)
    {
        const PlayerProfile* p = ((Ctx*)c)->p;
        for (size_t i = 0; i < p->skills.size(); ++i)
        {
            if (p->skills[i].skillId == static_cast<uint16>(id))
            {
                return p->skills[i].rank;
            }
        }
        return 0;
    }

    bool SpellCb(void* c, uint32 id)
    {
        const PlayerProfile* p = ((Ctx*)c)->p;
        for (size_t i = 0; i < p->knownSpells.size(); ++i)
        {
            if (p->knownSpells[i] == id)
            {
                return true;
            }
        }
        return false;
    }

    uint8 RepCb(void* c, uint32 f)
    {
        const PlayerProfile* p = ((Ctx*)c)->p;
        for (size_t i = 0; i < p->reps.size(); ++i)
        {
            if (p->reps[i].factionId == static_cast<uint16>(f))
            {
                return p->reps[i].rank;
            }
        }
        return 0;
    }

    uint32 ClassMask(uint8 c)
    {
        return c ? 1u << (c - 1u) : 0u;
    }

    uint32 RaceMask(uint8 r)
    {
        return r ? 1u << (r - 1u) : 0u;
    }

    // Helper to build an AhRefItem from an ItemUsabilityReq (mirror struct).
    AhRefItem ToRefItem(const ItemUsabilityReq& it)
    {
        AhRefItem ri;
        ri.itemClass            = it.itemClass;
        ri.allowableClass       = it.allowableClass;
        ri.allowableRace        = it.allowableRace;
        ri.requiredLevel        = it.requiredLevel;
        ri.itemId               = it.itemId;
        ri.requiredSkill        = it.requiredSkill;
        ri.requiredSkillRank    = it.requiredSkillRank;
        ri.requiredSpell        = it.requiredSpell;
        ri.requiredHonorRank    = it.requiredHonorRank;
        ri.requiredRepFaction   = it.requiredRepFaction;
        ri.requiredRepRank      = it.requiredRepRank;
        ri.itemProficiencySkill = it.itemProficiencySkill;
        return ri;
    }
}

int main()
{
    const uint32 MM = 40u;   // minMountLevel
    const uint32 EM = 60u;   // minEpicMountLevel

    // Battery of player profiles covering every gated dimension.
    std::vector<PlayerProfile> profiles;

    // Profile 0: warrior, human, level 40, honor rank 2, skill 43 (swords) rank
    // 200, known spell 123, friendly with faction 609.
    {
        PlayerProfile p;
        p.classId   = 1;   // warrior
        p.raceId    = 1;   // human
        p.level     = 40;
        p.honorRank = 2;
        SkillRank s;
        s.skillId = 43;
        s.rank    = 200;
        p.skills.push_back(s);
        p.knownSpells.push_back(123u);
        RepStanding r;
        r.factionId = 609;
        r.rank      = 5;  // honored
        p.reps.push_back(r);
        profiles.push_back(p);
    }

    // Profile 1: mage, gnome, level 60, high honor rank (14), no skills/spells/reps.
    {
        PlayerProfile p;
        p.classId   = 8;   // mage
        p.raceId    = 5;   // gnome
        p.level     = 60;
        p.honorRank = 14;
        profiles.push_back(p);
    }

    // Profile 2: paladin, dwarf, level 1, no honor, no skills/spells/reps.
    {
        PlayerProfile p;
        p.classId   = 2;   // paladin
        p.raceId    = 2;   // dwarf
        p.level     = 1;
        p.honorRank = 0;
        profiles.push_back(p);
    }

    // Battery of item requirements — one per gated dimension, plus mount overrides.
    std::vector<ItemUsabilityReq> items;

    // Base: open to all classes/races, level 30, no extra gates.
    ItemUsabilityReq base;
    base.itemClass            = 2;
    base.allowableClass       = 0xFFFFFFFFu;
    base.allowableRace        = 0xFFFFFFFFu;
    base.requiredLevel        = 30u;
    base.itemId               = 999u;
    base.requiredSkill        = 0u;
    base.requiredSkillRank    = 0u;
    base.requiredSpell        = 0u;
    base.requiredHonorRank    = 0u;
    base.requiredRepFaction   = 0u;
    base.requiredRepRank      = 0u;
    base.itemProficiencySkill = 0u;
    items.push_back(base);                                     // [0] pass all

    // [1] class gate — only warrior (bit 0x80 = class 8, mage); warriors fail.
    {
        ItemUsabilityReq v = base;
        v.allowableClass = 0x80u;   // class 8 (mage)
        items.push_back(v);
    }
    // [2] race gate — only dwarf (bit 0x02 = race 2); others fail.
    {
        ItemUsabilityReq v = base;
        v.allowableRace = 0x02u;
        items.push_back(v);
    }
    // [3] level gate — requires level 60; only profile 1 passes.
    {
        ItemUsabilityReq v = base;
        v.requiredLevel = 60u;
        items.push_back(v);
    }
    // [4] skill present + rank pass — skill 43 rank >= 150; profile 0 (rank 200) passes.
    {
        ItemUsabilityReq v = base;
        v.requiredSkill     = 43u;
        v.requiredSkillRank = 150u;
        items.push_back(v);
    }
    // [5] skill present + rank FAIL — skill 43 rank >= 300; profile 0 (rank 200) fails.
    {
        ItemUsabilityReq v = base;
        v.requiredSkill     = 43u;
        v.requiredSkillRank = 300u;
        items.push_back(v);
    }
    // [6] RequiredSpell present (spell 123) — profile 0 knows it; others fail.
    {
        ItemUsabilityReq v = base;
        v.requiredSpell = 123u;
        items.push_back(v);
    }
    // [7] RequiredSpell absent (spell 999) — no profile knows it; all fail.
    {
        ItemUsabilityReq v = base;
        v.requiredSpell = 999u;
        items.push_back(v);
    }
    // [8] honor rank gate — requires rank 5; only profile 1 (rank 14) passes.
    {
        ItemUsabilityReq v = base;
        v.requiredHonorRank = 5u;
        items.push_back(v);
    }
    // [9] reputation pass — faction 609, rank >= 4; profile 0 (rank 5) passes.
    {
        ItemUsabilityReq v = base;
        v.requiredRepFaction = 609u;
        v.requiredRepRank    = 4u;
        items.push_back(v);
    }
    // [10] reputation FAIL — faction 609, rank >= 7 (exalted); only profile 0 (rank
    // 5 = honored) fails.
    {
        ItemUsabilityReq v = base;
        v.requiredRepFaction = 609u;
        v.requiredRepRank    = 7u;
        items.push_back(v);
    }
    // [11] itemProficiencySkill present — skill 43 must be known; profile 0 passes.
    {
        ItemUsabilityReq v = base;
        v.itemProficiencySkill = 43u;
        items.push_back(v);
    }
    // [12] itemProficiencySkill absent — skill 44 not held by anyone; all fail.
    {
        ItemUsabilityReq v = base;
        v.itemProficiencySkill = 44u;
        items.push_back(v);
    }
    // [13] regular mount (item 1132) — mount level override replaces requiredLevel
    // with MM (40); level-1 profile fails, level-40+ pass.
    {
        ItemUsabilityReq v = base;
        v.itemId       = 1132u;
        v.requiredLevel = 60u;   // raw proto level; mount override should use MM=40
        items.push_back(v);
    }
    // [14] epic mount (item 12302) — override uses EM (60); only level-60 passes.
    {
        ItemUsabilityReq v = base;
        v.itemId        = 12302u;
        v.requiredLevel = 30u;   // raw proto level; epic override should use EM=60
        items.push_back(v);
    }

    int mismatches = 0;
    int total      = 0;

    for (size_t pi = 0; pi < profiles.size(); ++pi)
    {
        Ctx ctx;
        ctx.p = &profiles[pi];

        for (size_t ii = 0; ii < items.size(); ++ii)
        {
            const ItemUsabilityReq& it = items[ii];

            // Worker side.
            bool worker = AhUsability::IsUsable(profiles[pi], it, MM, EM);

            // Reference side. AH always queries with directAction=true (CanUseItem
            // Item* default), which is the semantics the worker's IsUsable
            // replicates. Calling with false would make honor-gated items report
            // spurious drift.
            AhRefItem ri = ToRefItem(it);
            bool ref = (AhUsabilityRef::Evaluate(
                            ClassMask(profiles[pi].classId),
                            RaceMask(profiles[pi].raceId),
                            static_cast<uint32>(profiles[pi].level),
                            static_cast<uint32>(profiles[pi].honorRank),
                            /*directAction=*/true,
                            MM, EM,
                            ri, SkillCb, SpellCb, RepCb, &ctx)
                        == AHUSE_OK);

            ++total;
            if (worker != ref)
            {
                ++mismatches;
                printf("DRIFT p%zu i%zu: worker=%d ref=%d\n",
                       pi, ii, worker ? 1 : 0, ref ? 1 : 0);
            }
        }
    }

    printf("ah_usability_difftest: %d/%d agree, %d drift\n",
           total - mismatches, total, mismatches);
    return (mismatches == 0) ? 0 : 1;
}
