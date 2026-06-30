#include "AhUsabilityRef.h"

namespace
{
    // Copied verbatim from src/modules/AhWorker/Usability.cpp (frozen Classic constant).
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

namespace AhUsabilityRef
{
    AhUseResult Evaluate(uint32 classMask, uint32 raceMask, uint32 level,
                         uint32 honorRank, bool directAction,
                         uint32 minMountLevel, uint32 minEpicMountLevel,
                         const AhRefItem& item,
                         uint16 (*skillRank)(void* ctx, uint32 skillId),
                         bool   (*hasSpell)(void* ctx, uint32 spellId),
                         uint8  (*repRank)(void* ctx, uint32 factionId),
                         void* ctx)
    {
        // 1. class/race mask (Player.cpp:13078-80).
        if ((item.allowableClass & classMask) == 0u ||
            (item.allowableRace & raceMask) == 0u)
        {
            return AHUSE_NEVER_USE;
        }
        // 2. RequiredSkill / Rank (13083-93).
        if (item.requiredSkill != 0u)
        {
            uint16 have = skillRank(ctx, item.requiredSkill);
            if (have == 0u)
            {
                return AHUSE_NO_PROFICIENCY;
            }
            if (have < item.requiredSkillRank)
            {
                return AHUSE_CANT_EQUIP_SKILL;
            }
        }
        // 3. RequiredSpell (13095-98).
        if (item.requiredSpell != 0u && !hasSpell(ctx, item.requiredSpell))
        {
            return AHUSE_NO_PROFICIENCY;
        }
        // 4. Honor rank -- ONLY when directAction (13100-03, D2).
        if (directAction && item.requiredHonorRank != 0u &&
            honorRank < item.requiredHonorRank)
        {
            return AHUSE_CANT_EQUIP_RANK;
        }
        // 5. mount-level config override (13105-185).
        uint32 requiredLevel = item.requiredLevel;
        if (IsRegularMount(item.itemId))
        {
            requiredLevel = minMountLevel;
        }
        else if (IsEpicMount(item.itemId))
        {
            requiredLevel = minEpicMountLevel;
        }
        // 6. level (13187-90).
        if (level < requiredLevel)
        {
            return AHUSE_CANT_EQUIP_LEVEL;
        }
        // 7. item proficiency (Item* overload 13046-52; 0 when proto overload).
        if (item.itemProficiencySkill != 0u &&
            skillRank(ctx, item.itemProficiencySkill) == 0u)
        {
            return AHUSE_NO_PROFICIENCY;
        }
        // 8. reputation (Item* overload 13054; 0 when proto overload).
        if (item.requiredRepFaction != 0u &&
            repRank(ctx, item.requiredRepFaction) < item.requiredRepRank)
        {
            return AHUSE_CANT_EQUIP_REPUTATION;
        }
        // Eluna OnCanUseItem is NOT here (Player::CanUseItem runs it after).
        return AHUSE_OK;
    }
}
