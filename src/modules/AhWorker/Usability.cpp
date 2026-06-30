#include "Usability.h"

namespace
{
    uint32 ClassMask(uint8 classId)
    {
        return (classId == 0u) ? 0u : (1u << (classId - 1u));  // Unit::getClassMask
    }
    uint32 RaceMask(uint8 raceId)
    {
        return (raceId == 0u) ? 0u : (1u << (raceId - 1u));    // Unit::getRaceMask
    }
    uint16 SkillRankOf(const PlayerProfile& p, uint32 skillId)
    {
        for (size_t i = 0; i < p.skills.size(); ++i)
        {
            if (p.skills[i].skillId == skillId)
            {
                return p.skills[i].rank;
            }
        }
        return 0u;
    }
    bool HasSpell(const PlayerProfile& p, uint32 spellId)
    {
        for (size_t i = 0; i < p.knownSpells.size(); ++i)
        {
            if (p.knownSpells[i] == spellId)
            {
                return true;
            }
        }
        return false;
    }
    // Reputation rank for a faction; REP_HATED (0) if the player has no standing.
    uint8 RepRankOf(const PlayerProfile& p, uint32 factionId)
    {
        for (size_t i = 0; i < p.reps.size(); ++i)
        {
            if (p.reps[i].factionId == factionId)
            {
                return p.reps[i].rank;
            }
        }
        return 0u;   // REP_HATED
    }

    bool IsRegularMount(uint32 id)
    {
        switch (id)
        {
            case 1132: case 2411: case 2414: case 5655: case 5656: case 5665:
            case 5668: case 5864: case 5872: case 5873: case 8563: case 8588:
            case 8591: case 8592: case 8595: case 8629: case 8631: case 8632:
            case 12325: case 12326: case 12327: case 13321: case 13322:
            case 13331: case 13332: case 13333: case 15277: case 15290:
            case 18241: case 18242: case 18243: case 18244: case 18245:
            case 18246: case 18247: case 18248:
                return true;
            default:
                return false;
        }
    }
    bool IsEpicMount(uint32 id)
    {
        switch (id)
        {
            case 12302: case 12303: case 12330: case 12351: case 12353:
            case 12354: case 13086: case 13326: case 13327: case 13328:
            case 13329: case 13334: case 13335: case 18766: case 18767:
            case 18768: case 18772: case 18773: case 18774: case 18776:
            case 18777: case 18778: case 18785: case 18786: case 18787:
            case 18788: case 18789: case 18790: case 18791: case 18793:
            case 18794: case 18795: case 18796: case 18797: case 18798:
            case 18902:
                return true;
            default:
                return false;
        }
    }
}

namespace AhUsability
{
    bool IsUsable(const PlayerProfile& p, const ItemUsabilityReq& req,
                  uint32 minMountLevel, uint32 minEpicMountLevel)
    {
        // 1. class/race mask (Player.cpp:13078).
        if ((req.allowableClass & ClassMask(p.classId)) == 0u ||
            (req.allowableRace & RaceMask(p.raceId)) == 0u)
        {
            return false;
        }
        // 2. RequiredSkill / RequiredSkillRank (13083-93).
        if (req.requiredSkill != 0u)
        {
            uint16 have = SkillRankOf(p, req.requiredSkill);
            if (have == 0u || have < req.requiredSkillRank)
            {
                return false;
            }
        }
        // 3. RequiredSpell (13095).
        if (req.requiredSpell != 0u && !HasSpell(p, req.requiredSpell))
        {
            return false;
        }
        // 4. Honor rank, direct_action=true (13100): highest rank vs requirement.
        if (req.requiredHonorRank != 0u && p.honorRank < req.requiredHonorRank)
        {
            return false;
        }
        // 5. mount-level config override (13105-185).
        uint32 requiredLevel = req.requiredLevel;
        if (IsRegularMount(req.itemId))
        {
            requiredLevel = minMountLevel;
        }
        else if (IsEpicMount(req.itemId))
        {
            requiredLevel = minEpicMountLevel;
        }
        // 6. level (13187).
        if (uint32(p.level) < requiredLevel)
        {
            return false;
        }
        // 7. item proficiency skill (Item* overload 13046-52).
        if (req.itemProficiencySkill != 0u &&
            SkillRankOf(p, req.itemProficiencySkill) == 0u)
        {
            return false;
        }
        // 8. reputation (Item* overload 13054).
        if (req.requiredRepFaction != 0u &&
            RepRankOf(p, req.requiredRepFaction) < req.requiredRepRank)
        {
            return false;
        }
        // Eluna OnCanUseItem (13192) deferred to mangosd; recipe-known handled
        // by the caller against knownRecipeCastSpells (C2b).
        return true;
    }

    // D6: the item's proficiency skill line from (class, subclass), copied
    // VERBATIM from Item::GetSkill() (src/game/Object/Item.cpp). The worker
    // cannot include the game header, so the SKILL_* ids are inlined as their
    // numeric values from src/game/Server/SharedDefines.h (Skills enum).
    // IMPLEMENTER: confirm EVERY numeric id against SharedDefines.h before
    // committing -- they are frozen Classic constants. 0 = no proficiency line.
    uint32 GetItemProficiencySkill(uint32 itemClass, uint32 itemSubClass)
    {
        // Classic constants: ITEM_CLASS_WEAPON=2 (21 subclasses),
        // ITEM_CLASS_ARMOR=4 (10 subclasses).
        const uint32 ITEM_CLASS_WEAPON = 2u;
        const uint32 ITEM_CLASS_ARMOR  = 4u;
        const uint32 MAX_WEAPON_SUB    = 21u;
        const uint32 MAX_ARMOR_SUB     = 10u;

        // item_weapon_skills[MAX_ITEM_SUBCLASS_WEAPON], same order as Item.cpp:
        //  AXES, 2H_AXES, BOWS, GUNS, MACES, 2H_MACES, POLEARMS, SWORDS,
        //  2H_SWORDS, 0, STAVES, 0, 0, UNARMED(fist), 0, DAGGERS, THROWN,
        //  ASSASSINATION, CROSSBOWS, WANDS, FISHING
        static const uint32 item_weapon_skills[MAX_WEAPON_SUB] =
        {
            44u, 172u, 45u, 46u, 54u,   // axes,2haxes,bows,guns,maces
            160u, 229u, 43u, 55u, 0u,   // 2hmaces,polearms,swords,2hswords,-
            136u, 0u, 0u, 162u, 0u,     // staves,-,-,unarmed/fist,-
            173u, 176u, 253u, 226u, 228u,// daggers,thrown,assassination(SPEAR,253),crossbows,wands
            356u                        // fishing
        };
        // item_armor_skills[MAX_ITEM_SUBCLASS_ARMOR]:
        //  0(misc), CLOTH, LEATHER, MAIL, PLATE_MAIL, 0(buckler-legacy),
        //  SHIELD, 0, 0, 0
        static const uint32 item_armor_skills[MAX_ARMOR_SUB] =
        {
            0u, 415u, 414u, 413u, 293u, // -,cloth,leather,mail,plate
            0u, 433u, 0u, 0u, 0u        // -,shield,-,-,-
        };

        if (itemClass == ITEM_CLASS_WEAPON)
        {
            return (itemSubClass < MAX_WEAPON_SUB) ? item_weapon_skills[itemSubClass] : 0u;
        }
        if (itemClass == ITEM_CLASS_ARMOR)
        {
            return (itemSubClass < MAX_ARMOR_SUB) ? item_armor_skills[itemSubClass] : 0u;
        }
        return 0u;
    }
}
