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



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "LFGMgr.h"
#include "DisableMgr.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

#define PLAYER_SKILL_INDEX(x)       (PLAYER_SKILL_INFO_1_1 + ((x)*3))

#define PLAYER_SKILL_VALUE_INDEX(x) (PLAYER_SKILL_INDEX(x)+1)

#define PLAYER_SKILL_BONUS_INDEX(x) (PLAYER_SKILL_INDEX(x)+2)

#define SKILL_VALUE(x)         PAIR32_LOPART(x)

#define SKILL_MAX(x)           PAIR32_HIPART(x)

#define MAKE_SKILL_VALUE(v, m) MAKE_PAIR32(v,m)

#define SKILL_TEMP_BONUS(x)    int16(PAIR32_LOPART(x))

#define SKILL_PERM_BONUS(x)    int16(PAIR32_HIPART(x))

#define MAKE_SKILL_BONUS(t, p) MAKE_PAIR32(t,p)

/**
 * @brief Applies or removes a base modifier affecting derived combat values.
 *
 * @param modGroup The modifier group to update.
 * @param modType The modifier type to apply.
 * @param amount The modifier amount.
 * @param apply True to apply the modifier; false to remove it.
 */
void Player::HandleBaseModValue(BaseModGroup modGroup, BaseModType modType, float amount, bool apply)
{
    if (modGroup >= BASEMOD_END || modType >= MOD_END)
    {
        sLog.outError("ERROR in HandleBaseModValue(): nonexistent BaseModGroup of wrong BaseModType!");
        return;
    }

    float val = 1.0f;

    switch (modType)
    {
        case FLAT_MOD:
            m_auraBaseMod[modGroup][modType] += apply ? amount : -amount;
            break;
        case PCT_MOD:
            if (amount <= -100.0f)
            {
                amount = -200.0f;
            }

            val = (100.0f + amount) / 100.0f;
            m_auraBaseMod[modGroup][modType] *= apply ? val : (1.0f / val);
            break;
    }

    if (!CanModifyStats())
    {
        return;
    }

    switch (modGroup)
    {
        case CRIT_PERCENTAGE:              UpdateCritPercentage(BASE_ATTACK);                          break;
        case RANGED_CRIT_PERCENTAGE:       UpdateCritPercentage(RANGED_ATTACK);                        break;
        default: break;
    }
}

/**
 * @brief Gets a stored base modifier value.
 *
 * @param modGroup The modifier group to query.
 * @param modType The modifier type to query.
 * @return The stored modifier value.
 */
float Player::GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const
{
    if (modGroup >= BASEMOD_END || modType > MOD_END)
    {
        sLog.outError("trial to access nonexistent BaseModGroup or wrong BaseModType!");
        return 0.0f;
    }

    if (modType == PCT_MOD && m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
    {
        return 0.0f;
    }

    return m_auraBaseMod[modGroup][modType];
}

/**
 * @brief Gets the combined flat and percentage base modifier value for a group.
 *
 * @param modGroup The modifier group to query.
 * @return The total effective modifier value.
 */
float Player::GetTotalBaseModValue(BaseModGroup modGroup) const
{
    if (modGroup >= BASEMOD_END)
    {
        sLog.outError("wrong BaseModGroup in GetTotalBaseModValue()!");
        return 0.0f;
    }

    if (m_auraBaseMod[modGroup][PCT_MOD] <= 0.0f)
    {
        return 0.0f;
    }

    return m_auraBaseMod[modGroup][FLAT_MOD] * m_auraBaseMod[modGroup][PCT_MOD];
}

/**
 * @brief Computes the player's current shield block value.
 *
 * @return The effective shield block amount.
 */
uint32 Player::GetShieldBlockValue() const
{
    float value = (m_auraBaseMod[SHIELD_BLOCK_VALUE][FLAT_MOD] + GetStat(STAT_STRENGTH) / 0.5f - 10) * m_auraBaseMod[SHIELD_BLOCK_VALUE][PCT_MOD];

    value = (value < 0) ? 0 : value;

    return uint32(value);
}

/**
 * @brief Calculates melee critical strike chance gained from agility.
 *
 * @return The melee crit contribution from agility.
 */
float Player::GetMeleeCritFromAgility()
{
    float valLevel1 = 0.0f;
    float valLevel60 = 0.0f;

    // critical
    switch (getClass())
    {
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            valLevel1 = 4.6f;
            valLevel60 = 20.0f;
            break;
        case CLASS_MAGE:
            valLevel1 = 12.9f;
            valLevel60 = 20.0f;
            break;
        case CLASS_ROGUE:
            valLevel1 = 2.2f;
            valLevel60 = 29.0f;
            break;
        case CLASS_HUNTER:
            valLevel1 = 3.5f;
            valLevel60 = 53.0f;
            break;
        case CLASS_PRIEST:
            valLevel1 = 11.0f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARLOCK:
            valLevel1 = 8.4f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARRIOR:
            valLevel1 = 3.9f;
            valLevel60 = 20.0f;
            break;
        default:
            return 0.0f;
    }
    float classrate = valLevel1 * float(60.0f - getLevel()) / 59.0f + valLevel60 * float(getLevel() - 1.0f) / 59.0f;
    return GetStat(STAT_AGILITY) / classrate;
}

/**
 * @brief Calculates dodge chance gained from agility.
 *
 * @return The dodge contribution from agility.
 */
float Player::GetDodgeFromAgility()
{
    float valLevel1 = 0.0f;
    float valLevel60 = 0.0f;

    // critical
    switch (getClass())
    {
        case CLASS_PALADIN:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
            valLevel1 = 4.6f;
            valLevel60 = 20.0f;
            break;
        case CLASS_MAGE:
            valLevel1 = 12.9f;
            valLevel60 = 20.0f;
            break;
        case CLASS_ROGUE:
            valLevel1 = 1.1f;
            valLevel60 = 14.5f;
            break;
        case CLASS_HUNTER:
            valLevel1 = 1.8f;
            valLevel60 = 26.5f;
            break;
        case CLASS_PRIEST:
            valLevel1 = 11.0f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARLOCK:
            valLevel1 = 8.4f;
            valLevel60 = 20.0f;
            break;
        case CLASS_WARRIOR:
            valLevel1 = 3.9f;
            valLevel60 = 20.0f;
            break;
        default:
            return 0.0f;
    }

    float classrate = valLevel1 * float(60.0f - getLevel()) / 59.0f + valLevel60 * float(getLevel() - 1.0f) / 59.0f;

    return GetStat(STAT_AGILITY) / classrate;
}

/**
 * @brief Calculates spell critical strike chance gained from intellect.
 *
 * @return The spell crit contribution from intellect.
 */
float Player::GetSpellCritFromIntellect()
{
    // Chance to crit is computed from INT and LEVEL as follows:
    //   chance = base + INT / (rate0 + rate1 * LEVEL)
    // The formula keeps the crit chance at %5 on every level unless the player
    // increases his intelligence by other means (enchants, buffs, talents, ...)

    //[TZERO] from mangos 3462 for 1.12 MUST BE CHECKED
    //float val = 0.0f;

    static const struct
    {
        float base;
        float rate0, rate1;
    }
    crit_data[MAX_CLASSES] =
    {
        {   0.0f,   0.0f,  10.0f  },                        //  0: unused
        {   0.0f,   0.0f,  10.0f  },                        //  1: warrior
        {   3.70f, 14.77f,  0.65f },                        //  2: paladin
        {   0.0f,   0.0f,  10.0f  },                        //  3: hunter
        {   0.0f,   0.0f,  10.0f  },                        //  4: rogue
        {   2.97f, 10.03f,  0.82f },                        //  5: priest
        {   0.0f,   0.0f,  10.0f  },                        //  6: unused
        {   3.54f, 11.51f,  0.80f },                        //  7: shaman
        {   3.70f, 14.77f,  0.65f },                        //  8: mage
        {   3.18f, 11.30f,  0.82f },                        //  9: warlock
        {   0.0f,   0.0f,  10.0f  },                        // 10: unused
        {   3.33f, 12.41f,  0.79f }                         // 11: druid
    };
    float crit_chance;

    // only players use intelligence for critical chance computations
    if (GetTypeId() == TYPEID_PLAYER)
    {
        int my_class = getClass();
        float crit_ratio = crit_data[my_class].rate0 + crit_data[my_class].rate1 * getLevel();
        crit_chance = crit_data[my_class].base + GetStat(STAT_INTELLECT) / crit_ratio;
    }
    else
    {
        crit_chance = m_baseSpellCritChance;
    }

    crit_chance = crit_chance > 0.0 ? crit_chance : 0.0;

    return crit_chance;
}

/**
 * @brief Calculates health regeneration per spirit tick.
 *
 * @return The health regeneration value based on spirit and class.
 */
float Player::OCTRegenHPPerSpirit()
{
    float regen = 0.0f;

    float Spirit = GetStat(STAT_SPIRIT);
    uint8 Class = getClass();

    switch (Class)
    {
        case CLASS_DRUID:   regen = (Spirit * 0.11 + 1);    break;
        case CLASS_HUNTER:  regen = (Spirit * 0.43 - 5.5);  break;
        case CLASS_MAGE:    regen = (Spirit * 0.11 + 1);    break;
        case CLASS_PALADIN: regen = (Spirit * 0.25);        break;
        case CLASS_PRIEST:  regen = (Spirit * 0.15 + 1.4);  break;
        case CLASS_ROGUE:   regen = (Spirit * 0.84 - 13);   break;
        case CLASS_SHAMAN:  regen = (Spirit * 0.28 - 3.6);  break;
        case CLASS_WARLOCK: regen = (Spirit * 0.12 + 1.5);  break;
        case CLASS_WARRIOR: regen = (Spirit * 1.26 - 22.6); break;
    }

    return regen;
}

/**
 * @brief Calculates mana regeneration per spirit tick.
 *
 * @return The mana regeneration value based on spirit and class.
 */
float Player::OCTRegenMPPerSpirit()
{
    float addvalue = 0.0;

    float Spirit = GetStat(STAT_SPIRIT);
    uint8 Class = getClass();

    switch (Class)
    {
        case CLASS_DRUID:   addvalue = (Spirit / 5 + 15);   break;
        case CLASS_HUNTER:  addvalue = (Spirit / 5 + 15);   break;
        case CLASS_MAGE:    addvalue = (Spirit / 4 + 12.5); break;
        case CLASS_PALADIN: addvalue = (Spirit / 5 + 15);   break;
        case CLASS_PRIEST:  addvalue = (Spirit / 4 + 12.5); break;
        case CLASS_SHAMAN:  addvalue = (Spirit / 5 + 17);   break;
        case CLASS_WARLOCK: addvalue = (Spirit / 5 + 15);   break;
    }

    addvalue /= 2.0f;   // the above addvalue are given per tick which occurs every 2 seconds, hence this divide by 2

    return addvalue;
}

/**
 * @brief Restores attack timers from currently equipped weapon delays.
 */
void Player::SetRegularAttackTime()
{
    for (int i = 0; i < MAX_ATTACK; ++i)
    {
        Item* tmpitem = GetWeaponForAttack(WeaponAttackType(i), true, false);
        if (tmpitem)
        {
            ItemPrototype const* proto = tmpitem->GetProto();
            if (proto->Delay)
            {
                SetAttackTime(WeaponAttackType(i), proto->Delay);
            }
            else
            {
                SetAttackTime(WeaponAttackType(i), BASE_ATTACK_TIME);
            }
        }
    }
}

// skill+step, checking for max value
bool Player::UpdateSkill(uint32 skill_id, uint32 step)
{
    if (!skill_id)
    {
        return false;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(skill_id);
    if (itr == mSkillStatus.end())
    {
        return false;
    }

    SkillStatusData &skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return false;
    }

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);
    uint32 data = GetUInt32Value(valueIndex);
    uint32 value = SKILL_VALUE(data);
    uint32 max = SKILL_MAX(data);

    if ((!max) || (!value) || (value >= max))
    {
        return false;
    }

    if (value * 512 < max * urand(0, 512))
    {
        uint32 new_value = value + step;
        if (new_value > max)
        {
            new_value = max;
        }

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, max));

        if (skillStatus.uState != SKILL_NEW)
        {
            skillStatus.uState = SKILL_CHANGED;
        }

        return true;
    }

    return false;
}

/**
 * @brief Calculates the configured chance to gain a skill point at the current value.
 *
 * @param SkillValue The player's current skill value.
 * @param GrayLevel The value at which gains become gray.
 * @param GreenLevel The value at which gains become green.
 * @param YellowLevel The value at which gains become yellow.
 * @return The gain chance scaled by ten.
 */
inline int SkillGainChance(uint32 SkillValue, uint32 GrayLevel, uint32 GreenLevel, uint32 YellowLevel)
{
    if (SkillValue >= GrayLevel)
    {
        return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_GREY) * 10;
    }
    if (SkillValue >= GreenLevel)
    {
        return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_GREEN) * 10;
    }
    if (SkillValue >= YellowLevel)
    {
        return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_YELLOW) * 10;
    }
    return sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_ORANGE) * 10;
}

/**
 * @brief Attempts to increase a crafting skill based on a spell cast.
 *
 * @param spellid The crafting spell identifier.
 * @return True if the skill increased; otherwise, false.
 */
bool Player::UpdateCraftSkill(uint32 spellid)
{
    DEBUG_LOG("UpdateCraftSkill spellid %d", spellid);

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spellid);

    for (SkillLineAbilityMap::const_iterator _spell_idx = bounds.first; _spell_idx != bounds.second; ++_spell_idx)
    {
        SkillLineAbilityEntry const* skill = _spell_idx->second;
        if (skill->skillId)
        {
            uint32 SkillValue = GetPureSkillValue(skill->skillId);

            uint32 craft_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_CRAFTING);

            return UpdateSkillPro(skill->skillId, SkillGainChance(SkillValue,
                skill->max_value,
                (skill->max_value + skill->min_value) / 2,
                skill->min_value),
                craft_skill_gain);
        }
    }
    return false;
}

/**
 * @brief Attempts to increase a gathering skill using profession-specific gain rules.
 *
 * @param SkillId The skill identifier to update.
 * @param SkillValue The current skill value.
 * @param RedLevel The red difficulty threshold for the source.
 * @param Multiplicator An additional gain chance multiplier.
 * @return True if the skill increased; otherwise, false.
 */
bool Player::UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator)
{
    DEBUG_LOG("UpdateGatherSkill(SkillId %d SkillLevel %d RedLevel %d)", SkillId, SkillValue, RedLevel);

    uint32 gathering_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING);

    // For skinning and Mining chance decrease with level. 1-74 - no decrease, 75-149 - 2 times, 225-299 - 8 times
    switch (SkillId)
    {
        case SKILL_HERBALISM:
        case SKILL_LOCKPICKING:
            return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator, gathering_skill_gain);
        case SKILL_SKINNING:
            if (sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS) == 0)
            {
                return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator, gathering_skill_gain);
            }
            else
            {
                return UpdateSkillPro(SkillId, (SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator) >> (SkillValue / sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS)), gathering_skill_gain);
            }
        case SKILL_MINING:
            if (sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS) == 0)
            {
                return UpdateSkillPro(SkillId, SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator, gathering_skill_gain);
            }
            else
            {
                return UpdateSkillPro(SkillId, (SkillGainChance(SkillValue, RedLevel + 100, RedLevel + 50, RedLevel + 25) * Multiplicator) >> (SkillValue / sWorld.getConfig(CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS)), gathering_skill_gain);
            }
    }
    return false;
}

/**
 * @brief Attempts to increase the player's fishing skill.
 *
 * @return True if the skill increased; otherwise, false.
 */
bool Player::UpdateFishingSkill()
{
    DEBUG_LOG("UpdateFishingSkill");

    uint32 SkillValue = GetPureSkillValue(SKILL_FISHING);

    int32 chance = SkillValue < 75 ? 100 : 2500 / (SkillValue - 50);

    uint32 gathering_skill_gain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING);

    return UpdateSkillPro(SKILL_FISHING, chance * 10, gathering_skill_gain);
}

/**
 * @brief Attempts to increase a skill using an explicit percentage chance.
 *
 * @param SkillId The skill identifier to update.
 * @param Chance The gain chance in tenths of a percent.
 * @param step The amount to increase the skill by.
 * @return True if the skill increased; otherwise, false.
 */
bool Player::UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step)
{
    DEBUG_LOG("UpdateSkillPro(SkillId %d, Chance %3.1f%%)", SkillId, Chance / 10.0);
    if (!SkillId)
    {
        return false;
    }

    if (Chance <= 0)                                        // speedup in 0 chance case
    {
        DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0);
        return false;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(SkillId);
    if (itr == mSkillStatus.end())
    {
        return false;
    }

    SkillStatusData &skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return false;
    }

    uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);

    uint32 data = GetUInt32Value(valueIndex);
    uint16 SkillValue = SKILL_VALUE(data);
    uint16 MaxValue   = SKILL_MAX(data);

    if (!MaxValue || !SkillValue || SkillValue >= MaxValue)
    {
        return false;
    }

    int32 Roll = irand(1, 1000);

    if (Roll <= Chance)
    {
        uint32 new_value = SkillValue + step;
        if (new_value > MaxValue)
        {
            new_value = MaxValue;
        }

        SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(new_value, MaxValue));

        if (skillStatus.uState != SKILL_NEW)
        {
            skillStatus.uState = SKILL_CHANGED;
        }

        DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% taken", Chance / 10.0);
        return true;
    }

    DEBUG_LOG("Player::UpdateSkillPro Chance=%3.1f%% missed", Chance / 10.0);
    return false;
}

/**
 * @brief Attempts to improve the player's weapon skill for an attack type.
 *
 * @param attType The attack type whose weapon skill should be updated.
 */
void Player::UpdateWeaponSkill(WeaponAttackType attType)
{
    // no skill gain in pvp
    Unit* pVictim = getVictim();
    if (pVictim && pVictim->IsCharmerOrOwnerPlayerOrPlayerItself())
    {
        return;
    }

    if (IsInFeralForm())
    {
        return; // always maximized SKILL_FERAL_COMBAT in fact
    }

    if (GetShapeshiftForm() == FORM_TREE)
    {
        return; // use weapon but not skill up
    }

    uint32 weaponSkillGain = sWorld.getConfig(CONFIG_UINT32_SKILL_GAIN_WEAPON);

    Item* pWeapon = GetWeaponForAttack(attType, true, true);
    if (pWeapon && pWeapon->GetProto()->SubClass != ITEM_SUBCLASS_WEAPON_FISHING_POLE)
    {
        UpdateSkill(pWeapon->GetSkill(), weaponSkillGain);
    }
    else if (!pWeapon && attType == BASE_ATTACK)
    {
        UpdateSkill(SKILL_UNARMED, weaponSkillGain);
    }

    UpdateAllCritPercentages();
}

/**
 * @brief Attempts to improve weapon or defense skills from combat.
 *
 * @param pVictim The opposing unit involved in combat.
 * @param attType The attack type used for offensive skill checks.
 * @param defence True to evaluate defense gain; false for weapon skill gain.
 */
void Player::UpdateCombatSkills(Unit* pVictim, WeaponAttackType attType, bool defence)
{
    uint32 plevel = getLevel();                             // if defense than pVictim == attacker
    uint32 greylevel = MaNGOS::XP::GetGrayLevel(plevel);
    uint32 moblevel = pVictim->GetLevelForTarget(this);
    if (moblevel < greylevel)
    {
        return;
    }

    if (moblevel > plevel + 5)
    {
        moblevel = plevel + 5;
    }

    uint32 lvldif = moblevel - greylevel;
    if (lvldif < 3)
    {
        lvldif = 3;
    }

    int32 skilldif = 5 * plevel - (defence ? GetBaseDefenseSkillValue() : GetBaseWeaponSkillValue(attType));

    // Max skill reached for level.
    // Can in some cases be less than 0: having max skill and then .level -1 as example.
    if (skilldif <= 0)
    {
        return;
    }

    float chance = float(3 * lvldif * skilldif) / plevel;
    if (!defence)
    {
        chance *= 0.1f * GetStat(STAT_INTELLECT);
    }

    chance = chance < 1.0f ? 1.0f : chance;                 // minimum chance to increase skill is 1%

    if (roll_chance_f(chance))
    {
        if (defence)
        {
            UpdateDefense();
        }
        else
        {
            UpdateWeaponSkill(attType);
        }
    }
    else
    {
        return;
    }
}

/**
 * @brief Modifies the temporary or permanent bonus for a skill.
 *
 * @param skillid The skill identifier to modify.
 * @param val The bonus amount to add or remove.
 * @param talent True for permanent talent-based bonuses; false for temporary bonuses.
 */
void Player::ModifySkillBonus(uint32 skillid, int32 val, bool talent)
{
    SkillStatusMap::const_iterator itr = mSkillStatus.find(skillid);
    if (itr == mSkillStatus.end() || itr->second.uState == SKILL_DELETED)
    {
        return;
    }

    uint32 bonusIndex = PLAYER_SKILL_BONUS_INDEX(itr->second.pos);

    uint32 bonus_val = GetUInt32Value(bonusIndex);
    int16 temp_bonus = SKILL_TEMP_BONUS(bonus_val);
    int16 perm_bonus = SKILL_PERM_BONUS(bonus_val);

    if (talent)                                         // permanent bonus stored in high part
    {
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS(temp_bonus, perm_bonus + val));
    }
    else                                                // temporary/item bonus stored in low part
    {
        SetUInt32Value(bonusIndex, MAKE_SKILL_BONUS(temp_bonus + val, perm_bonus));
    }
}

/**
 * @brief Updates level-scaled skills to match the player's current level cap.
 */
void Player::UpdateSkillsForLevel()
{
    uint16 maxconfskill = sWorld.GetConfigMaxSkillValue();
    uint32 maxSkill = GetMaxSkillValueForLevel();

    bool alwaysMaxSkill = sWorld.getConfig(CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL);

    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        SkillStatusData &skillStatus = itr->second;
        if (skillStatus.uState == SKILL_DELETED)
        {
            continue;
        }

        uint32 pskill = itr->first;

        SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(pskill);
        if (!pSkill)
        {
            continue;
        }

        if (GetSkillRangeType(pSkill, false) != SKILL_RANGE_LEVEL)
        {
            continue;
        }

        uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);
        uint32 data = GetUInt32Value(valueIndex);
        uint32 max = SKILL_MAX(data);
        uint32 val = SKILL_VALUE(data);

        /// update only level dependent max skill values
        if (max != 1)
        {
            /// maximize skill always
            if (alwaysMaxSkill)
            {
                SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(maxSkill, maxSkill));
                if (skillStatus.uState != SKILL_NEW)
                {
                    skillStatus.uState = SKILL_CHANGED;
                }
            }
            else if (max != maxconfskill)                   /// update max skill value if current max skill not maximized
            {
                SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(val, maxSkill));
                if (skillStatus.uState != SKILL_NEW)
                {
                    skillStatus.uState = SKILL_CHANGED;
                }
            }
        }
    }
}

/**
 * @brief Raises non-profession skills to their current maximum values.
 */
void Player::UpdateSkillsToMaxSkillsForLevel()
{
    for (SkillStatusMap::iterator itr = mSkillStatus.begin(); itr != mSkillStatus.end(); ++itr)
    {
        SkillStatusData &skillStatus = itr->second;
        if (skillStatus.uState == SKILL_DELETED)
        {
            continue;
        }

        uint32 pskill = itr->first;
        if (IsProfessionOrRidingSkill(pskill))
        {
            continue;
        }
        uint32 valueIndex = PLAYER_SKILL_VALUE_INDEX(skillStatus.pos);
        uint32 data = GetUInt32Value(valueIndex);

        uint32 max = SKILL_MAX(data);

        if (max > 1)
        {
            SetUInt32Value(valueIndex, MAKE_SKILL_VALUE(max, max));
            if (skillStatus.uState != SKILL_NEW)
            {
                skillStatus.uState = SKILL_CHANGED;
            }
        }

        if (pskill == SKILL_DEFENSE)
        {
            UpdateDefenseBonusesMod();
        }
    }
}

// This functions sets a skill line value (and adds if doesn't exist yet)
// To "remove" a skill line, set it's values to zero
void Player::SetSkill(uint16 id, uint16 currVal, uint16 maxVal, uint16 step /*=0*/)
{
    if (!id)
    {
        return;
    }

    SkillStatusMap::iterator itr = mSkillStatus.find(id);

    // has skill
    if (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED)
    {
        SkillStatusData &skillStatus = itr->second;
        if (currVal)
        {
            if (step)                                      // need update step
            {
                SetUInt32Value(PLAYER_SKILL_INDEX(skillStatus.pos), MAKE_PAIR32(id, step));
            }

            // update value
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos), MAKE_SKILL_VALUE(currVal, maxVal));
            if (skillStatus.uState != SKILL_NEW)
            {
                skillStatus.uState = SKILL_CHANGED;
            }
            // learnSkillRewardedSpells(id, currVal);       // pre-3.x have only 1 skill level req (so at learning only)
        }
        else                                                // remove
        {
            // clear skill fields
            SetUInt32Value(PLAYER_SKILL_INDEX(skillStatus.pos), 0);
            SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos), 0);
            SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos), 0);

            // mark as deleted or simply remove from map if not saved yet
            if (skillStatus.uState != SKILL_NEW)
            {
                skillStatus.uState = SKILL_DELETED;
            }
            else
            {
                mSkillStatus.erase(itr);
            }

            // remove all spells that related to this skill
            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
            {
                if (SkillLineAbilityEntry const* pAbility = sSkillLineAbilityStore.LookupEntry(j))
                {
                    if (pAbility->skillId == id)
                    {
                        removeSpell(sSpellMgr.GetFirstSpellInChain(pAbility->spellId));
                    }
                }
            }
        }
    }
    else if (currVal)                                       // add
    {
        for (int i = 0; i < PLAYER_MAX_SKILLS; ++i)
        {
            if (!GetUInt32Value(PLAYER_SKILL_INDEX(i)))
            {
                SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(id);
                if (!pSkill)
                {
                    sLog.outError("Skill not found in SkillLineStore: skill #%u", id);
                    return;
                }

                SetUInt32Value(PLAYER_SKILL_INDEX(i), MAKE_PAIR32(id, step));
                SetUInt32Value(PLAYER_SKILL_VALUE_INDEX(i), MAKE_SKILL_VALUE(currVal, maxVal));

                // insert new entry or update if not deleted old entry yet
                if (itr != mSkillStatus.end())
                {
                    itr->second.pos = i;
                    itr->second.uState = SKILL_CHANGED;
                }
                else
                {
                    mSkillStatus.insert(SkillStatusMap::value_type(id, SkillStatusData(i, SKILL_NEW)));
                }

                // apply skill bonuses
                SetUInt32Value(PLAYER_SKILL_BONUS_INDEX(i), 0);

                // temporary bonuses
                AuraList const& mModSkill = GetAurasByType(SPELL_AURA_MOD_SKILL);
                for (AuraList::const_iterator j = mModSkill.begin(); j != mModSkill.end(); ++j)
                {
                    if ((*j)->GetModifier()->m_miscvalue == int32(id))
                    {
                        (*j)->ApplyModifier(true);
                    }
                }

                // permanent bonuses
                AuraList const& mModSkillTalent = GetAurasByType(SPELL_AURA_MOD_SKILL_TALENT);
                for (AuraList::const_iterator j = mModSkillTalent.begin(); j != mModSkillTalent.end(); ++j)
                {
                    if ((*j)->GetModifier()->m_miscvalue == int32(id))
                    {
                        (*j)->ApplyModifier(true);
                    }
                }

                // Learn all spells for skill
                learnSkillRewardedSpells(id, currVal);
                return;
            }
        }
    }
}

/**
 * @brief Checks whether the player currently has a skill line.
 *
 * @param skill The skill identifier to test.
 * @return True if the skill exists and is not deleted; otherwise, false.
 */
bool Player::HasSkill(uint32 skill) const
{
    if (!skill)
    {
        return false;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    return (itr != mSkillStatus.end() && itr->second.uState != SKILL_DELETED);
}

/**
 * @brief Gets the total current value of a skill including bonuses.
 *
 * @param skill The skill identifier to query.
 * @return The current effective skill value.
 */
uint16 Player::GetSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos));

    int32 result = int32(SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

/**
 * @brief Gets the total maximum value of a skill including bonuses.
 *
 * @param skill The skill identifier to query.
 * @return The effective maximum skill value.
 */
uint16 Player::GetMaxSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    uint32 bonus = GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos));

    int32 result = int32(SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos))));
    result += SKILL_TEMP_BONUS(bonus);
    result += SKILL_PERM_BONUS(bonus);
    return result < 0 ? 0 : result;
}

/**
 * @brief Gets the unmodified maximum value of a skill.
 *
 * @param skill The skill identifier to query.
 * @return The stored maximum skill value without bonuses.
 */
uint16 Player::GetPureMaxSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_MAX(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos)));
}

/**
 * @brief Gets the base value of a skill including permanent bonuses.
 *
 * @param skill The skill identifier to query.
 * @return The base skill value with permanent bonuses applied.
 */
uint16 Player::GetBaseSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    int32 result = int32(SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos))));
    result += SKILL_PERM_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos)));
    return result < 0 ? 0 : result;
}

/**
 * @brief Gets the raw stored value of a skill without bonuses.
 *
 * @param skill The skill identifier to query.
 * @return The raw stored skill value.
 */
uint16 Player::GetPureSkillValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_VALUE(GetUInt32Value(PLAYER_SKILL_VALUE_INDEX(skillStatus.pos)));
}

/**
 * @brief Gets the permanent bonus value applied to a skill.
 *
 * @param skill The skill identifier to query.
 * @return The permanent skill bonus.
 */
int16 Player::GetSkillPermBonusValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_PERM_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos)));
}

/**
 * @brief Gets the temporary bonus value applied to a skill.
 *
 * @param skill The skill identifier to query.
 * @return The temporary skill bonus.
 */
int16 Player::GetSkillTempBonusValue(uint32 skill) const
{
    if (!skill)
    {
        return 0;
    }

    SkillStatusMap::const_iterator itr = mSkillStatus.find(skill);
    if (itr == mSkillStatus.end())
    {
        return 0;
    }

    SkillStatusData const& skillStatus = itr->second;
    if (skillStatus.uState == SKILL_DELETED)
    {
        return 0;
    }

    return SKILL_TEMP_BONUS(GetUInt32Value(PLAYER_SKILL_BONUS_INDEX(skillStatus.pos)));
}
