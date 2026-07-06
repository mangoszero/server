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



#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "VMapFactory.h"
#include "MovementGenerator.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include <math.h>
#include <stdarg.h>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Performs one melee attack update against a victim.
 *
 * @param pVictim The attack victim.
 * @param attType The attack type to use.
 * @param extra True when this is an extra attack proc.
 */
void Unit::AttackerStateUpdate(Unit* pVictim, WeaponAttackType attType, bool extra)
{
    if (hasUnitState(UNIT_STAT_CAN_NOT_REACT) || HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PACIFIED))
    {
        return;
    }

    if (!pVictim->IsAlive())
    {
        return;
    }

    if (IsNonMeleeSpellCasted(false))
    {
        return;
    }

    uint32 hitInfo;
    if (attType == BASE_ATTACK)
    {
        hitInfo = HITINFO_NORMALSWING2;
    }
    else if (attType == OFF_ATTACK)
    {
        hitInfo = HITINFO_LEFTSWING;
    }
    else
    {
        return; // ignore ranged case
    }

    uint32 extraAttacks = m_extraAttacks;

    // melee attack spell casted at main hand attack only
    if (attType == BASE_ATTACK && m_currentSpells[CURRENT_MELEE_SPELL])
    {
        m_currentSpells[CURRENT_MELEE_SPELL]->cast();

        // not recent extra attack only at any non extra attack (melee spell case)
        if (!extra && extraAttacks)
        {
            HandleProcExtraAttackFor(pVictim);
        }

        return;
    }

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MELEE_ATTACK);

    CalcDamageInfo damageInfo;
    CalculateMeleeDamage(pVictim, &damageInfo, attType);
    // Send log damage message to client
    DealDamageMods(pVictim, damageInfo.damage, &damageInfo.absorb);
    SendAttackStateUpdate(&damageInfo);
    ProcDamageAndSpell(damageInfo.target, damageInfo.procAttacker, damageInfo.procVictim, damageInfo.procEx, damageInfo.damage, damageInfo.attackType);
    DealMeleeDamage(&damageInfo, true);

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "AttackerStateUpdate: %s attacked %s for %u dmg, absorbed %u, blocked %u, resisted %u.",
        GetGuidStr().c_str(), pVictim->GetGuidStr().c_str(), damageInfo.damage, damageInfo.absorb, damageInfo.blocked_amount, damageInfo.resist);

    // Owner of pet enters combat upon pet attack
    if (Unit* owner = GetOwner())
    {
        owner->AddThreat(pVictim);
        owner->SetInCombatWith(pVictim);
        pVictim->SetInCombatWith(owner);
    }

    // if damage pVictim call AI reaction
    pVictim->AttackedBy(this);

    // extra attack only at any non extra attack (normal case)
    if (!extra && extraAttacks)
    {
        HandleProcExtraAttackFor(pVictim);
    }
}

/**
 * @brief Rolls a melee hit outcome using current combat chances.
 *
 * @param pVictim The victim being attacked.
 * @param attType The attack type to evaluate.
 * @return The resulting melee hit outcome.
 */
MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType) const
{
    // This is only wrapper

    // Miss chance based on melee
    float miss_chance = MeleeMissChanceCalc(pVictim, attType);

    // Critical hit chance
    float crit_chance = GetUnitCriticalChance(attType, pVictim);

    // stunned target can not dodge and this is check in GetUnitDodgeChance() (returned 0 in this case)
    float dodge_chance = pVictim->GetUnitDodgeChance();
    float block_chance = pVictim->GetUnitBlockChance();
    float parry_chance = pVictim->GetUnitParryChance();

    // Useful if want to specify crit & miss chances for melee, else it could be removed
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "MELEE OUTCOME: miss %f crit %f dodge %f parry %f block %f", miss_chance, crit_chance, dodge_chance, parry_chance, block_chance);

    return RollMeleeOutcomeAgainst(pVictim, attType, int32(crit_chance * 100), int32(miss_chance * 100), int32(dodge_chance * 100), int32(parry_chance * 100), int32(block_chance * 100), false);
}

/**
 * @brief Rolls a melee hit outcome using explicit precomputed chances.
 *
 * @param pVictim The victim being attacked.
 * @param attType The attack type to evaluate.
 * @param crit_chance The critical chance in hundredths of a percent.
 * @param miss_chance The miss chance in hundredths of a percent.
 * @param dodge_chance The dodge chance in hundredths of a percent.
 * @param parry_chance The parry chance in hundredths of a percent.
 * @param block_chance The block chance in hundredths of a percent.
 * @param SpellCasted True when evaluating a spell-based melee hit.
 * @return The resulting melee hit outcome.
 */
MeleeHitOutcome Unit::RollMeleeOutcomeAgainst(const Unit* pVictim, WeaponAttackType attType, int32 crit_chance, int32 miss_chance, int32 dodge_chance, int32 parry_chance, int32 block_chance, bool SpellCasted) const
{
    if (pVictim->GetTypeId() == TYPEID_UNIT && ((Creature*)pVictim)->IsInEvadeMode())
    {
        return MELEE_HIT_EVADE;
    }

    int32 attackerMaxSkillValueForLevel = GetMaxSkillValueForLevel(pVictim);
    int32 victimMaxSkillValueForLevel = pVictim->GetMaxSkillValueForLevel(this);

    int32 attackerWeaponSkill = GetWeaponSkillValue(attType, pVictim);
    int32 victimDefenseSkill = pVictim->GetDefenseSkillValue(this);

    // bonus from skills is 0.04%
    int32 skillBonus  = 4 * (attackerWeaponSkill - victimMaxSkillValueForLevel);
    int32 sum = 0;
    int32 roll = urand(0, 10000);
    int32 tmp = miss_chance;

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: skill bonus of %d for attacker", skillBonus);
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: rolled %d, miss %d, dodge %d, parry %d, block %d, crit %d",
        roll, miss_chance, dodge_chance, parry_chance, block_chance, crit_chance);

    if (tmp > 0 && roll < (sum += tmp))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: MISS");
        return MELEE_HIT_MISS;
    }

    // always crit against a sitting target (except 0 crit chance)
    if (pVictim->GetTypeId() == TYPEID_PLAYER && crit_chance > 0 && !pVictim->IsStandState())
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRIT (sitting victim)");
        return MELEE_HIT_CRIT;
    }

    bool from_behind = !pVictim->HasInArc(M_PI_F, this);

    if (from_behind)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: attack came from behind.");
    }

    // Dodge chance

    // only players can't dodge if attacker is behind
    if (pVictim->GetTypeId() != TYPEID_PLAYER || !from_behind)
    {
        tmp = dodge_chance;
        if ((tmp > 0) &&                  // check if unit _can_ dodge
            ((tmp -= skillBonus) > 0) &&
            roll < (sum += tmp))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: DODGE <%d, %d)", sum - tmp, sum);
            return MELEE_HIT_DODGE;
        }
    }

    // parry chances
    // check if attack comes from behind, nobody can parry or block if attacker is behind
    if (!from_behind)
    {
        if (parry_chance > 0 && (pVictim->GetTypeId() == TYPEID_PLAYER || !(((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_PARRY)))
        {
            parry_chance -= skillBonus;

            if (parry_chance > 0 &&                         // check if unit _can_ parry
                (roll < (sum += parry_chance)))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: PARRY <%d, %d)", sum - parry_chance, sum);
                return MELEE_HIT_PARRY;
            }
        }
    }

    // Max 40% chance to score a glancing blow against mobs that are higher level (can do only players and pets and not with ranged weapon)
    if (attType != RANGED_ATTACK && !SpellCasted &&
        (GetTypeId() == TYPEID_PLAYER || ((Creature*)this)->IsPet()) &&
        pVictim->GetTypeId() != TYPEID_PLAYER && !((Creature*)pVictim)->IsPet() &&
        getLevel() < pVictim->GetLevelForTarget(this))
    {
        // cap possible value (with bonuses > max skill)
        int32 skill = attackerWeaponSkill;
        int32 maxskill = attackerMaxSkillValueForLevel;
        skill = (skill > maxskill) ? maxskill : skill;

        tmp = (10 + 2 * (victimDefenseSkill - skill)) * 100;
        tmp = tmp > 4000 ? 4000 : tmp;
        if (roll < (sum += tmp))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: GLANCING <%d, %d)", sum - 4000, sum);
            return MELEE_HIT_GLANCING;
        }
    }

    // block chances
    // check if attack comes from behind, nobody can parry or block if attacker is behind
    if (!from_behind)
    {
        if (pVictim->GetTypeId() == TYPEID_PLAYER || !(((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_BLOCK))
        {
            tmp = block_chance;
            if ((tmp > 0) &&                   // check if unit _can_ block
                ((tmp -= skillBonus) > 0) &&
                (roll < (sum += tmp)))
            {
                // Critical chance
                tmp = crit_chance;
                if (GetTypeId() == TYPEID_PLAYER && SpellCasted && tmp > 0)
                {
                    if (roll_chance_i(tmp / 100))
                    {
                        DEBUG_LOG("RollMeleeOutcomeAgainst: BLOCKED CRIT");
                        return MELEE_HIT_BLOCK_CRIT;
                    }
                }
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: BLOCK <%d, %d)", sum - tmp, sum);
                return MELEE_HIT_BLOCK;
            }
        }
    }

    // Critical chance
    tmp = crit_chance;

    if (tmp > 0 && roll < (sum += tmp))
    {
        DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRIT <%d, %d)", sum - tmp, sum);
        return MELEE_HIT_CRIT;
    }

    if ((GetTypeId() != TYPEID_PLAYER && !((Creature*)this)->IsPet()) &&
        !(((Creature*)this)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_CRUSH) &&
        !SpellCasted /*Only autoattack can be crashing blow*/)
    {
        // mobs can score crushing blows if they're 3 or more levels above victim
        // or when their weapon skill is 15 or more above victim's defense skill
        tmp = victimDefenseSkill;
        int32 tmpmax = victimMaxSkillValueForLevel;
        // having defense above your maximum (from items, talents etc.) has no effect
        tmp = tmp > tmpmax ? tmpmax : tmp;
        // tmp = mob's level * 5 - player's current defense skill
        tmp = attackerMaxSkillValueForLevel - tmp;
        if (tmp >= 15)
        {
            // add 2% chance per lacking skill point, min. is 15%
            tmp = tmp * 200 - 1500;
            if (roll < (sum += tmp))
            {
                DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: CRUSHING <%d, %d)", sum - tmp, sum);
                return MELEE_HIT_CRUSHING;
            }
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "RollMeleeOutcomeAgainst: NORMAL");
    return MELEE_HIT_NORMAL;
}

/**
 * @brief Rolls a weapon damage amount for an attack type.
 *
 * @param attType The attack type to use.
 * @param normalized True to use normalized player weapon damage.
 * @return The randomized damage amount.
 */
uint32 Unit::CalculateDamage(WeaponAttackType attType, bool normalized)
{
    float min_damage, max_damage;

    if (normalized && GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)this)->CalculateMinMaxDamage(attType, normalized, min_damage, max_damage);
    }
    else
    {
        switch (attType)
        {
            case RANGED_ATTACK:
                min_damage = GetFloatValue(UNIT_FIELD_MINRANGEDDAMAGE);
                max_damage = GetFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE);
                break;
            case BASE_ATTACK:
                min_damage = GetFloatValue(UNIT_FIELD_MINDAMAGE);
                max_damage = GetFloatValue(UNIT_FIELD_MAXDAMAGE);
                break;
            case OFF_ATTACK:
                min_damage = GetFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE);
                max_damage = GetFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE);
                break;
            // Just for good manner
            default:
                min_damage = 0.0f;
                max_damage = 0.0f;
                break;
        }
    }

    if (min_damage > max_damage)
    {
        std::swap(min_damage, max_damage);
    }

    if (max_damage == 0.0f)
    {
        max_damage = 5.0f;
    }

    if (min_damage < 0.0f)
    {
        min_damage = 0.0f;
    }

    if (max_damage < min_damage)
    {
        max_damage = min_damage;
    }

    return urand((uint32)min_damage, (uint32)max_damage);
}

/**
 * @brief Calculates the low-level spell coefficient penalty.
 *
 * @param spellProto The spell entry being evaluated.
 * @return The scaling penalty multiplier.
 */
float Unit::CalculateLevelPenalty(SpellEntry const* spellProto) const
{
    uint32 spellLevel = spellProto->SpellLevel;
    if (spellLevel <= 0)
    {
        return 1.0f;
    }

    float LvlPenalty = 0.0f;

    if (spellLevel < 20)
    {
        LvlPenalty = (20.0f - spellLevel) * 3.75f;
    }

    return (100.0f - LvlPenalty) / 100.0f;
}

/**
 * @brief Sends an attack-start packet for melee combat.
 *
 * @param pVictim The victim being attacked.
 */
void Unit::SendMeleeAttackStart(Unit* pVictim)
{
    WorldPacket data(SMSG_ATTACKSTART, 8 + 8);
    data << GetObjectGuid();
    data << pVictim->GetObjectGuid();

    SendMessageToSet(&data, true);
    DEBUG_FILTER_LOG(LOG_FILTER_COMBAT, "WORLD: Sent SMSG_ATTACKSTART: %s -> %s", GetGuidStr().c_str(), pVictim->GetGuidStr().c_str());
}

/**
 * @brief Sends an attack-stop packet for melee combat.
 *
 * @param victim The victim no longer being attacked.
 */
void Unit::SendMeleeAttackStop(Unit* victim)
{
    if (!victim)
    {
        return;
    }

    WorldPacket data(SMSG_ATTACKSTOP, (8 + 8 + 4));         // guess size, max is 9+9+4
    data << GetPackGUID();
    data << victim->GetPackGUID();                          // can be 0x00...
    data << uint32(0);                                      // can be 0x1
    SendMessageToSet(&data, true);
    DETAIL_FILTER_LOG(LOG_FILTER_COMBAT, "%s %u stopped attacking %s %u", (GetTypeId() == TYPEID_PLAYER ? "player" : "creature"), GetGUIDLow(), (victim->GetTypeId() == TYPEID_PLAYER ? "player" : "creature"), victim->GetGUIDLow());

    /*if (victim->GetTypeId() == TYPEID_UNIT)
    ((Creature*)victim)->AI().EnterEvadeMode(this);*/
}

/**
 * @brief Checks whether an incoming melee or ranged spell can be blocked.
 *
 * @param pCaster The attacking caster.
 * @param spellEntry The spell entry being resolved.
 * @param attackType The related attack type.
 * @return True if the spell is blocked; otherwise, false.
 */
bool Unit::IsSpellBlocked(Unit* pCaster, SpellEntry const* spellEntry, WeaponAttackType attackType)
{
    if (!HasInArc(M_PI_F, pCaster))
    {
        return false;
    }

    if (spellEntry)
    {
        // Some spells can not be blocked
        if (spellEntry->HasAttribute(SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK))
        {
            return false;
        }
    }

    // Check creatures flags_extra for disable block
    if (GetTypeId() == TYPEID_UNIT)
    {
        if (((Creature*)this)->GetCreatureInfo()->ExtraFlags & CREATURE_FLAG_EXTRA_NO_BLOCK)
        {
            return false;
        }
    }

    float blockChance = GetUnitBlockChance();
    blockChance += (int32(pCaster->GetWeaponSkillValue(attackType)) - int32(GetMaxSkillValueForLevel())) * 0.04f;

    return roll_chance_f(blockChance);
}

// Melee based spells can be miss, parry or dodge on this step
// Crit or block - determined on damage calculation phase! (and can be both in some time)
float Unit::MeleeSpellMissChance(Unit* pVictim, WeaponAttackType attType, int32 skillDiff, SpellEntry const* spell)
{
    // Calculate hit chance (more correct for chance mod)
    float hitChance = 0.0f;

    // PvP - PvE melee chances
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        hitChance = 95.0f + skillDiff * 0.04f;
    }
    else if (skillDiff < -10)
    {
        hitChance = 93.0f + (skillDiff + 10) * 0.4f; // 7% base chance to miss for big skill diff (%6 in 3.x)
    }
    else
    {
        hitChance = 95.0f + skillDiff * 0.1f;
    }

    // Hit chance depends from victim auras
    if (attType == RANGED_ATTACK)
    {
        hitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    }
    else
    {
        hitChance += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
    }

    // Spellmod from SPELLMOD_RESIST_MISS_CHANCE
    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spell->ID, SPELLMOD_RESIST_MISS_CHANCE, hitChance);
    }

    // Miss = 100 - hit
    float missChance = 100.0f - hitChance;

    // Bonuses from attacker aura and ratings
    if (attType == RANGED_ATTACK)
    {
        missChance -= m_modRangedHitChance;
    }
    else
    {
        missChance -= m_modMeleeHitChance;
    }

    // Limit miss chance from 0 to 60%
    if (missChance < 0.0f)
    {
        return 0.0f;
    }
    if (missChance > 60.0f)
    {
        return 60.0f;
    }
    return missChance;
}

// Melee based spells hit result calculations
SpellMissInfo Unit::MeleeSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{
    WeaponAttackType attType = BASE_ATTACK;

    if (spell->DefenseType == SPELL_DAMAGE_CLASS_RANGED)
    {
        attType = RANGED_ATTACK;
    }

    // bonus from skills is 0.04% per skill Diff
    int32 attackerWeaponSkill = (spell->EquippedItemClass == ITEM_CLASS_WEAPON) ? int32(GetWeaponSkillValue(attType, pVictim)) : GetMaxSkillValueForLevel();
    int32 skillDiff = attackerWeaponSkill - int32(pVictim->GetMaxSkillValueForLevel(this));
    int32 fullSkillDiff = attackerWeaponSkill - int32(pVictim->GetDefenseSkillValue(this));

    //is this to get a better spread and not have to resort to floats?
    uint32 roll = urand(0, 10000);

    uint32 missChance = uint32(MeleeSpellMissChance(pVictim, attType, fullSkillDiff, spell) * 100.0f);
    // Roll miss
    uint32 tmp = spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS) ? 0 : missChance;
    if (roll < tmp)
    {
        return SPELL_MISS_MISS;
    }

    // Chance resist mechanic (select max value from every mechanic spell effect)
    int32 resist_mech = 0;
    // Get effects mechanic and chance
    for (int eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        int32 effect_mech = GetEffectMechanic(spell, SpellEffectIndex(eff));
        if (effect_mech)
        {
            int32 temp = pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_MECHANIC_RESISTANCE, effect_mech);
            if (resist_mech < temp * 100)
            {
                resist_mech = temp * 100;
            }
        }
    }
    // Roll chance
    tmp += resist_mech;
    if (roll < tmp)
    {
        return SPELL_MISS_RESIST;
    }

    bool canDodge = true;
    bool canParry = true;

    // Same spells can not be parry/dodge
    if (spell->HasAttribute(SPELL_ATTR_IMPOSSIBLE_DODGE_PARRY_BLOCK))
    {
        return SPELL_MISS_NONE;
    }

    // Ranged attack can not be parry/dodge
    if (attType == RANGED_ATTACK)
    {
        return SPELL_MISS_NONE;
    }

    bool from_behind = !pVictim->HasInArc(M_PI_F, this);

    // Check for attack from behind
    if (from_behind)
    {
        // Can`t dodge from behind in PvP (but its possible in PvE)
        if (GetTypeId() == TYPEID_PLAYER && pVictim->GetTypeId() == TYPEID_PLAYER)
        {
            canDodge = false;
        }
        // Can`t parry
        canParry = false;
    }
    // Check creatures flags_extra for disable parry
    if (pVictim->GetTypeId() == TYPEID_UNIT)
    {
        uint32 flagEx = ((Creature*)pVictim)->GetCreatureInfo()->ExtraFlags;
        if (flagEx & CREATURE_FLAG_EXTRA_NO_PARRY)
        {
            canParry = false;
        }
    }

    if (canDodge)
    {
        // Roll dodge
        int32 dodgeChance = int32(pVictim->GetUnitDodgeChance() * 100.0f) - skillDiff * 4;

        if (dodgeChance < 0)
        {
            dodgeChance = 0;
        }

        tmp += dodgeChance;
        if (roll < tmp)
        {
            return SPELL_MISS_DODGE;
        }
    }

    if (canParry)
    {
        // Roll parry
        int32 parryChance = int32(pVictim->GetUnitParryChance() * 100.0f)  - skillDiff * 4;
        // Can`t parry from behind
        if (parryChance < 0)
        {
            parryChance = 0;
        }

        tmp += parryChance;
        if (roll < tmp)
        {
            return SPELL_MISS_PARRY;
        }
    }

    return SPELL_MISS_NONE;
}

// TODO need use unit spell resistances in calculations
SpellMissInfo Unit::MagicSpellHitResult(Unit* pVictim, SpellEntry const* spell)
{
    // Can`t miss on dead target (on skinning for example)
    if (!pVictim->IsAlive())
    {
        return SPELL_MISS_NONE;
    }

    SpellSchoolMask schoolMask = GetSpellSchoolMask(spell);

    // Holy spell resist didn't exist in 1.12.
    if (schoolMask == SPELL_SCHOOL_MASK_HOLY)
    {
        return SPELL_MISS_NONE;
    }

    // PvP - PvE spell misschances per leveldif > 2
    int32 lchance = pVictim->GetTypeId() == TYPEID_PLAYER ? 7 : 11;
    int32 leveldif = int32(pVictim->GetLevelForTarget(this)) - int32(GetLevelForTarget(pVictim));

    // Base hit chance from attacker and victim levels
    int32 modHitChance;
    if (leveldif < 3)
    {
        modHitChance = 96 - leveldif;
    }
    else
    {
        modHitChance = 94 - (leveldif - 2) * lchance;
    }

    // Spellmod from SPELLMOD_RESIST_MISS_CHANCE
    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spell->ID, SPELLMOD_RESIST_MISS_CHANCE, modHitChance);
    }
    // Chance hit from victim SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE auras
    modHitChance += pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_HIT_CHANCE, schoolMask);
    // Reduce spell hit chance for Area of effect spells from victim SPELL_AURA_MOD_AOE_AVOIDANCE aura
    if (IsAreaOfEffectSpell(spell))
    {
        modHitChance -= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_AOE_AVOIDANCE);
    }
    // Chance resist mechanic (select max value from every mechanic spell effect)
    int32 resist_mech = 0;
    // Get effects mechanic and chance
    for (int eff = 0; eff < MAX_EFFECT_INDEX; ++eff)
    {
        int32 effect_mech = GetEffectMechanic(spell, SpellEffectIndex(eff));
        if (effect_mech)
        {
            int32 temp = pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_MECHANIC_RESISTANCE, effect_mech);
            if (resist_mech < temp)
            {
                resist_mech = temp;
            }
        }
    }
    // Apply mod
    modHitChance -= resist_mech;

    // Chance resist debuff
    modHitChance -= pVictim->GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_DEBUFF_RESISTANCE, int32(spell->DispelType));

    int32 HitChance = modHitChance * 100;
    // Increase hit chance from attacker SPELL_AURA_MOD_SPELL_HIT_CHANCE and attacker ratings
    HitChance += int32(m_modSpellHitChance * 100.0f);

    if (HitChance <  100)
    {
        HitChance =  100;
    }
    if (HitChance > 9900)
    {
        HitChance = 9900;
    }

    int32 tmp = spell->HasAttribute(SPELL_ATTR_EX3_CANT_MISS) ? 0 : (10000 - HitChance);

    // Why isn't this urand aswell just as in MeleeSpellHitResult?
    int32 rand = irand(0, 10000);

    if (rand < tmp)
    {
        return SPELL_MISS_RESIST;
    }

    return SPELL_MISS_NONE;
}

// Calculate spell hit result can be:
// Every spell can: Evade/Immune/Reflect/Sucesful hit
// For melee based spells:
//   Miss
//   Dodge
//   Parry
// For spells
//   Resist
SpellMissInfo Unit::SpellHitResult(Unit* pVictim, SpellEntry const* spell, bool CanReflect)
{
    SpellSchoolMask schoolMask = GetSpellSchoolMask(spell);

    // wand case
    bool wand = spell->ID == 5019;
    if (wand && !!(getClassMask() & CLASSMASK_WAND_USERS) && GetTypeId() == TYPEID_PLAYER)
    {
        schoolMask = GetSchoolMask(GetWeaponDamageSchool(RANGED_ATTACK));
    }

    // Return evade for units in evade mode
    if (pVictim->GetTypeId() == TYPEID_UNIT && ((Creature*)pVictim)->IsInEvadeMode())
    {
        return SPELL_MISS_EVADE;
    }

    // Check for immune
    if (!wand && pVictim->IsImmuneToSpell(spell, this == pVictim) && !spell->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY))
    {
        return SPELL_MISS_IMMUNE;
    }

    // All positive spells can`t miss
    // TODO: client not show miss log for this spells - so need find info for this in dbc and use it!
    if (IsPositiveSpell(spell->ID))
    {
        return SPELL_MISS_NONE;
    }

    // Check for immune (use charges)
    if (pVictim->IsImmuneToDamage(schoolMask) && !spell->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY))
    {
        return SPELL_MISS_IMMUNE;
    }

    // Try victim reflect spell
    if (CanReflect)
    {
        int32 reflectchance = pVictim->GetTotalAuraModifier(SPELL_AURA_REFLECT_SPELLS);
        Unit::AuraList const& mReflectSpellsSchool = pVictim->GetAurasByType(SPELL_AURA_REFLECT_SPELLS_SCHOOL);
        for (Unit::AuraList::const_iterator i = mReflectSpellsSchool.begin(); i != mReflectSpellsSchool.end(); ++i)
        {
            if ((*i)->GetModifier()->m_miscvalue & schoolMask)
            {
                reflectchance += (*i)->GetModifier()->m_amount;
            }
        }

        if (reflectchance > 0 && roll_chance_i(reflectchance))
        {
            // Start triggers for remove charges if need (trigger only for victim, and mark as active spell)
            ProcDamageAndSpell(pVictim, PROC_FLAG_NONE, PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT, PROC_EX_REFLECT, 1, BASE_ATTACK, spell);
            return SPELL_MISS_REFLECT;
        }
    }

    switch (spell->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_NONE:
            return SPELL_MISS_NONE;
        case SPELL_DAMAGE_CLASS_MAGIC:
            return MagicSpellHitResult(pVictim, spell);
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
            return MeleeSpellHitResult(pVictim, spell);
    }
    return SPELL_MISS_NONE;
}

/**
 * @brief Calculates the melee miss chance against a victim.
 *
 * @param pVictim The victim being attacked.
 * @param attType The attack type to evaluate.
 * @return The miss chance percentage.
 */
float Unit::MeleeMissChanceCalc(const Unit* pVictim, WeaponAttackType attType) const
{
    if (!pVictim)
    {
        return 0.0f;
    }

    // Base misschance 5%
    float missChance = 5.0f;

    // DualWield - white damage has additional 19% miss penalty
    if (haveOffhandWeapon() && attType != RANGED_ATTACK)
    {
        bool isNormal = false;
        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
        {
            if (m_currentSpells[i] && (GetSpellSchoolMask(m_currentSpells[i]->m_spellInfo) & SPELL_SCHOOL_MASK_NORMAL))
            {
                isNormal = true;
                break;
            }
        }
        if (!isNormal && !m_currentSpells[CURRENT_MELEE_SPELL])
        {
            missChance += 19.0f;
        }
    }

    int32 skillDiff = int32(GetWeaponSkillValue(attType, pVictim)) - int32(pVictim->GetDefenseSkillValue(this));

    // PvP - PvE melee chances
    if (pVictim->GetTypeId() == TYPEID_PLAYER)
    {
        missChance -= skillDiff * 0.04f;
    }
    else if (skillDiff < -10)
    {
        missChance -= (skillDiff + 10) * 0.4f - 2.0f; // 7% base chance to miss for big skill diff (%6 in 3.x)
    }
    else
    {
        missChance -=  skillDiff * 0.1f;
    }

    // Hit chance bonus from attacker based on ratings and auras
    if (attType == RANGED_ATTACK)
    {
        missChance -= m_modRangedHitChance;
    }
    else
    {
        missChance -= m_modMeleeHitChance;
    }

    // Modify miss chance by victim auras
    if (attType == RANGED_ATTACK)
    {
        missChance -= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_HIT_CHANCE);
    }
    else
    {
        missChance -= pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_HIT_CHANCE);
    }

    // Limit miss chance from 0 to 60%
    if (missChance < 0.0f)
    {
        return 0.0f;
    }
    if (missChance > 60.0f)
    {
        return 60.0f;
    }

    return missChance;
}

/**
 * @brief Gets the unit's current dodge chance.
 *
 * @return The dodge chance percentage.
 */
float Unit::GetUnitDodgeChance() const
{
    if (hasUnitState(UNIT_STAT_STUNNED))
    {
        return 0.0f;
    }
    if (GetTypeId() == TYPEID_PLAYER)
    {
        return GetFloatValue(PLAYER_DODGE_PERCENTAGE);
    }
    else
    {
        if (((Creature const*)this)->IsTotem())
        {
            return 0.0f;
        }
        else
        {
            float dodge = 5.0f;
            dodge += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
            return dodge > 0.0f ? dodge : 0.0f;
        }
    }
}

/**
 * @brief Gets the unit's current parry chance.
 *
 * @return The parry chance percentage.
 */
float Unit::GetUnitParryChance() const
{
    if (IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_STUNNED))
    {
        return 0.0f;
    }

    float chance = 0.0f;

    if (GetTypeId() == TYPEID_PLAYER)
    {
        Player const* player = (Player const*)this;
        if (player->CanParry())
        {
            Item* tmpitem = player->GetWeaponForAttack(BASE_ATTACK, true, true);
            if (!tmpitem)
            {
                tmpitem = player->GetWeaponForAttack(OFF_ATTACK, true, true);
            }

            if (tmpitem)
            {
                chance = GetFloatValue(PLAYER_PARRY_PERCENTAGE);
            }
        }
    }
    else if (GetTypeId() == TYPEID_UNIT)
    {
        if (GetCreatureType() == CREATURE_TYPE_HUMANOID)
        {
            chance = 5.0f;
            chance += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
        }
    }

    return chance > 0.0f ? chance : 0.0f;
}

/**
 * @brief Gets the unit's current block chance.
 *
 * @return The block chance percentage.
 */
float Unit::GetUnitBlockChance() const
{
    if (IsNonMeleeSpellCasted(false) || hasUnitState(UNIT_STAT_STUNNED))
    {
        return 0.0f;
    }

    if (GetTypeId() == TYPEID_PLAYER)
    {
        Player const* player = (Player const*)this;
        if (player->CanBlock() && player->CanUseEquippedWeapon(OFF_ATTACK))
        {
            Item* tmpitem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
            if (tmpitem && !tmpitem->IsBroken() && tmpitem->GetProto()->Block)
            {
                return GetFloatValue(PLAYER_BLOCK_PERCENTAGE);
            }
        }
        // is player but has no block ability or no not broken shield equipped
        return 0.0f;
    }
    else
    {
        if (((Creature const*)this)->IsTotem())
        {
            return 0.0f;
        }
        else
        {
            float block = 5.0f;
            block += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
            return block > 0.0f ? block : 0.0f;
        }
    }
}

/**
 * @brief Gets the unit's critical hit chance against a victim.
 *
 * @param attackType The attack type being evaluated.
 * @param pVictim The victim used for defensive adjustments.
 * @return The critical hit chance percentage.
 */
float Unit::GetUnitCriticalChance(WeaponAttackType attackType, const Unit* pVictim) const
{
    float crit;

    if (GetTypeId() == TYPEID_PLAYER)
    {
        switch (attackType)
        {
            case OFF_ATTACK:
            case BASE_ATTACK:
                crit = GetFloatValue(PLAYER_CRIT_PERCENTAGE);
                break;
            case RANGED_ATTACK:
                crit = GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE);
                break;
            // Just for good manner
            default:
                crit = 0.0f;
                break;
        }
    }
    else
    {
        crit = 5.0f;
        crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PERCENT);
    }

    // flat aura mods
    if (attackType == RANGED_ATTACK)
    {
        crit += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_RANGED_CRIT_CHANCE);
    }
    else
    {
        crit += pVictim->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_MELEE_CRIT_CHANCE);
    }

    // Apply crit chance from defence skill
    crit += (int32(GetMaxSkillValueForLevel(pVictim)) - int32(pVictim->GetDefenseSkillValue(this))) * 0.04f;

    if (crit < 0.0f)
    {
        crit = 0.0f;
    }
    return crit;
}
