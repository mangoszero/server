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
 * \fn int32 Unit::SpellBonusWithCoeffs(Unit* pCaster, SpellEntry const* spellProto, int32 total, int32 benefit, int32 ap_benefit,  DamageEffectType damagetype, bool donePart, Spell const* spell)
 * \brief This method is calculating the total amount of damage done including spell power.
 *
 * If benefit is 0, this function won't do anything. If pCaster isn't player, the default coefficient 1.0 will be used.
 * The spell_bonus_data table of the database is used here to define custom spell coefficients based on damage type.
 *
 * The spell bonus coefficient are always chosen by this priority:
 * For Donepart : weapon_done > direct_done > direct
 * For Takenpart : weapon_taken > direct_taken > direct
 *
 * \param pCaster Pointer to the player casting the spell.
 * \param spellProto Constant Pointer to the spell actually caster.
 * \param total int32 represents the already calculated damage for the caster spell without spell power bonuses.
 * \param benefit int32 represents the total amount of spell power bonuses.
 * \param ap_benefit int32 -- TO BE DOCUMENTED
 * \param damagetype DamageEffectType represents the type of damage (DIRECT or DOT) -- See DamageEffectType enum.
 * \param donePart bool represents whether the damage are issued from ...Done methods or from ...Taken methods.
 *
 * \return int32 Total amount of damage including spell power bonuses.
 */
int32 Unit::SpellBonusWithCoeffs(Unit* pCaster, SpellEntry const* spellProto, int32 total, int32 benefit, int32 ap_benefit,  DamageEffectType damagetype, bool donePart, Spell const* spell)
{
    // Just don't waste time into this function if there's no benefit.
    if (!benefit)
    {
        return total;
    }

    // Distribute Damage over multiple effects, reduce by AoE
    float coeff = 1.0f;
    SpellEntry const* levelPenaltySpell = spell ? spell->GetSpellBonusLevelPenaltySpell(spellProto) : spellProto;
    bool const useTriggeredHealBonus = damagetype == HEAL && levelPenaltySpell != spellProto;

    // Not apply this to creature casted spells
    if (pCaster->GetTypeId() == TYPEID_UNIT && !((Creature*)this)->IsPet())
    {
        coeff = 1.0f;
    }
    // Check for table values
    else if (SpellBonusEntry const* bonus = sSpellMgr.GetSpellBonusData(spellProto->ID))
    {
        switch (damagetype)
        {
            case DOT:
                coeff = bonus->dot_damage;
                break;
            case HEAL:
                if (useTriggeredHealBonus)
                {
                    coeff = donePart ? (bonus->direct_damage_done ? bonus->direct_damage_done : bonus->direct_damage)
                        : (bonus->direct_damage_taken ? bonus->direct_damage_taken : bonus->direct_damage);
                }
                break;
            case SPELL_DIRECT_DAMAGE:
                // Special check for bonus damage applying on spells depending on the equiped weapon.
                if (pCaster->GetTypeId() == TYPEID_PLAYER && damagetype == SPELL_DIRECT_DAMAGE)
                {
                    Item* item = ((Player*)pCaster)->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);

                    if (donePart)
                    {
                        if (item)
                        {
                            switch (item->GetProto()->InventoryType)
                            {
                                case INVTYPE_2HWEAPON:
                                    coeff = (bonus->two_hand_direct_damage_done ? bonus->two_hand_direct_damage_done
                                        : ( bonus->two_hand_direct_damage ? bonus->two_hand_direct_damage : bonus->direct_damage_done ));
                                    break;
                                case INVTYPE_WEAPON:
                                case INVTYPE_WEAPONMAINHAND:
                                case INVTYPE_WEAPONOFFHAND:
                                    coeff = (bonus->one_hand_direct_damage_done ? bonus->one_hand_direct_damage_done
                                        : ( bonus->one_hand_direct_damage ? bonus->one_hand_direct_damage : bonus->direct_damage_done ));
                                    break;
                            }

                            // None of the priority fields have been populated in DB.
                            if (!coeff)
                            {
                                coeff = bonus->direct_damage;
                            }
                        } else {
                            coeff = (bonus->direct_damage_done ? bonus->direct_damage_done : bonus->direct_damage);
                        }
                    }
                    else
                    {
                        if (item)
                        {
                            switch (item->GetProto()->InventoryType)
                            {
                                case INVTYPE_2HWEAPON:
                                    coeff = (bonus->two_hand_direct_damage_taken ? bonus->two_hand_direct_damage_taken
                                        : ( bonus->two_hand_direct_damage ? bonus->two_hand_direct_damage : bonus->direct_damage_taken ));
                                    break;
                                case INVTYPE_WEAPON:
                                case INVTYPE_WEAPONMAINHAND:
                                case INVTYPE_WEAPONOFFHAND:
                                    coeff = (bonus->one_hand_direct_damage_taken ? bonus->one_hand_direct_damage_taken
                                        : ( bonus->one_hand_direct_damage ? bonus->one_hand_direct_damage : bonus->direct_damage_taken ));
                                    break;
                            }
                            // None of the priority fields have been populated in DB.
                            if (!coeff)
                            {
                                coeff = bonus->direct_damage;
                            }
                        }
                        else
                        {
                            coeff = (bonus->direct_damage_taken ? bonus->direct_damage_taken : bonus->direct_damage);
                        }
                    }
                    break;
                }
            default:
                break;
        }

        // apply ap bonus at done part calculation only (it flat total mod so common with taken)
        if (donePart && (bonus->ap_bonus || bonus->ap_dot_bonus))
        {
            float ap_bonus = damagetype == DOT ? bonus->ap_dot_bonus : bonus->ap_bonus;

            total += int32(ap_bonus * (GetTotalAttackPowerValue(IsSpellRequiresRangedAP(spellProto) ? RANGED_ATTACK : BASE_ATTACK) + ap_benefit));
        }
    }
    // Default calculation
    else
    {
        coeff = CalculateDefaultCoefficient(spellProto, damagetype);
    }

    float LvlPenalty = CalculateLevelPenalty(levelPenaltySpell);

    // Seal of Righteousness PROC and Flash of Light receive benefit from Spell Damage and Healing too low.
    if (spellProto->SpellClassSet == SPELLFAMILY_PALADIN && (spellProto->SpellIconID == 25 || spellProto->SpellIconID == 242))
    {
        LvlPenalty = 1.0f;
    }

    // Spellmod SpellDamage
    if (Player* modOwner = GetSpellModOwner())
    {
        coeff *= 100.0f;
        modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_SPELL_BONUS_DAMAGE, coeff);
        coeff /= 100.0f;
    }

    total += int32(benefit * coeff * LvlPenalty);

    return total;
};

/**
 * Calculates caster part of spell damage bonuses,
 * also includes different bonuses dependent from target auras
 */
uint32 Unit::SpellDamageBonusDone(Unit* pVictim, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack)
{
    if (!spellProto || !pVictim || damagetype == DIRECT_DAMAGE)
    {
        return pdamage;
    }

    // For totems get damage bonus from owner (statue isn't totem in fact)
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsTotem() && ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
    {
        if (Unit* owner = GetOwner())
        {
            return owner->SpellDamageBonusDone(pVictim, spellProto, pdamage, damagetype);
        }
    }

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    float DoneTotalMod = 1.0f;
    int32 DoneTotal = 0;

    // Creature damage
    if (GetTypeId() == TYPEID_UNIT && !((Creature*)this)->IsPet())
    {
        DoneTotalMod *= Creature::_GetSpellDamageMod(((Creature*)this)->GetCreatureInfo()->Rank);
    }

    AuraList const& mModDamagePercentDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
    for (AuraList::const_iterator i = mModDamagePercentDone.begin(); i != mModDamagePercentDone.end(); ++i)
    {
        if (((*i)->GetModifier()->m_miscvalue & GetSpellSchoolMask(spellProto)) &&
            (*i)->GetSpellProto()->EquippedItemClass == -1 &&
            // -1 == any item class (not wand then)
            (*i)->GetSpellProto()->EquippedItemInvTypes == 0)
            // 0 == any inventory type (not wand then)
        {
            DoneTotalMod *= ((*i)->GetModifier()->m_amount + 100.0f) / 100.0f;
        }
    }

    // Add flat bonus from spell damage versus
    DoneTotal += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_FLAT_SPELL_DAMAGE_VERSUS, creatureTypeMask);

    // Add pct bonus from spell damage versus
    DoneTotalMod *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_VERSUS, creatureTypeMask);

    // Add flat bonus from spell damage creature
    DoneTotal += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_CREATURE, creatureTypeMask);

    // done scripted mod (take it from owner)
    Unit* owner = GetOwner();
    if (!owner)
    {
        owner = this;
    }

    AuraList const& mOverrideClassScript = owner->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for (AuraList::const_iterator i = mOverrideClassScript.begin(); i != mOverrideClassScript.end(); ++i)
    {
        if (!(*i)->isAffectedOnSpell(spellProto))
        {
            continue;
        }

        switch ((*i)->GetModifier()->m_miscvalue)
        {
            case 4418: // Increased Shock Damage
            case 4554: // Increased Lightning Damage
            {
                DoneTotal += (*i)->GetModifier()->m_amount;
                break;
            }
            case 4555: // Improved Moonfire
            {
                DoneTotalMod *= ((*i)->GetModifier()->m_amount + 100.0f) / 100.0f;
                break;
            }
        }
    }

    // Done fixed damage bonus auras
    int32 DoneAdvertisedBenefit = SpellBaseDamageBonusDone(GetSpellSchoolMask(spellProto));

    // Pets just add their bonus damage to their spell damage
    // note that their spell damage is just gain of their own auras
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsPet())
    {
        DoneAdvertisedBenefit += ((Pet*)this)->GetBonusDamage();
    }

    // apply ap bonus and benefit affected by spell power implicit coeffs and spell level penalties
    DoneTotal = SpellBonusWithCoeffs(this, spellProto, DoneTotal, DoneAdvertisedBenefit, 0, damagetype, true);

    float tmpDamage = (int32(pdamage) + DoneTotal * int32(stack)) * DoneTotalMod;
    // apply spellmod to Done damage (flat and pct)
    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spellProto->ID, damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, tmpDamage);
    }

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

/**
 * Calculates target part of spell damage bonuses,
 * will be called on each tick for periodic damage over time auras
 */
uint32 Unit::SpellDamageBonusTaken(Unit* pCaster, SpellEntry const* spellProto, uint32 pdamage, DamageEffectType damagetype, uint32 stack)
{
    if (!spellProto || !pCaster || damagetype == DIRECT_DAMAGE)
    {
        return pdamage;
    }

    uint32 schoolMask = GetSpellSchoolMask(spellProto);

    // Taken total percent damage auras
    float TakenTotalMod = 1.0f;
    int32 TakenTotal = 0;

    // ..taken
    TakenTotalMod *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);

    // Taken fixed damage bonus auras
    int32 TakenAdvertisedBenefit = SpellBaseDamageBonusTaken(GetSpellSchoolMask(spellProto));

    // apply benefit affected by spell power implicit coeffs and spell level penalties
    TakenTotal = SpellBonusWithCoeffs(pCaster, spellProto, TakenTotal, TakenAdvertisedBenefit, 0, damagetype, false);

    float tmpDamage = (int32(pdamage) + TakenTotal * int32(stack)) * TakenTotalMod;

    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

/**
 * @brief Computes the unit's advertised spell damage bonus for a school mask.
 *
 * @param schoolMask The spell school mask.
 * @return The advertised damage bonus.
 */
int32 Unit::SpellBaseDamageBonusDone(SpellSchoolMask schoolMask)
{
    int32 DoneAdvertisedBenefit = 0;

    // ..done
    AuraList const& mDamageDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (AuraList::const_iterator i = mDamageDone.begin(); i != mDamageDone.end(); ++i)
    {
        if (((*i)->GetModifier()->m_miscvalue & schoolMask) != 0 &&
            (*i)->GetSpellProto()->EquippedItemClass == -1 &&                   // -1 == any item class (not wand then)
            (*i)->GetSpellProto()->EquippedItemInvTypes == 0)          //  0 == any inventory type (not wand then)
        {
            DoneAdvertisedBenefit += (*i)->GetModifier()->m_amount;
        }
    }

    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Damage bonus from stats
        AuraList const& mDamageDoneOfStatPercent = GetAurasByType(SPELL_AURA_MOD_SPELL_DAMAGE_OF_STAT_PERCENT);
        for (AuraList::const_iterator i = mDamageDoneOfStatPercent.begin(); i != mDamageDoneOfStatPercent.end(); ++i)
        {
            if ((*i)->GetModifier()->m_miscvalue & schoolMask)
            {
                // stat used stored in miscValueB for this aura
                Stats usedStat = STAT_SPIRIT;
                DoneAdvertisedBenefit += int32(GetStat(usedStat) * (*i)->GetModifier()->m_amount / 100.0f);
            }
        }
    }
    return DoneAdvertisedBenefit;
}

/**
 * @brief Computes the target's advertised spell damage taken bonus for a school mask.
 *
 * @param schoolMask The spell school mask.
 * @return The advertised taken-damage bonus.
 */
int32 Unit::SpellBaseDamageBonusTaken(SpellSchoolMask schoolMask)
{
    int32 TakenAdvertisedBenefit = 0;

    // ..taken
    AuraList const& mDamageTaken = GetAurasByType(SPELL_AURA_MOD_DAMAGE_TAKEN);
    for (AuraList::const_iterator i = mDamageTaken.begin(); i != mDamageTaken.end(); ++i)
    {
        if (((*i)->GetModifier()->m_miscvalue & schoolMask) != 0)
        {
            TakenAdvertisedBenefit += (*i)->GetModifier()->m_amount;
        }
    }

    return TakenAdvertisedBenefit;
}

/**
 * @brief Checks whether a spell cast critically hits a victim.
 *
 * @param pVictim The spell victim.
 * @param spellProto The spell entry.
 * @param schoolMask The spell school mask.
 * @param attackType The associated attack type.
 * @return True if the spell crits; otherwise, false.
 */
bool Unit::IsSpellCrit(Unit* pVictim, SpellEntry const* spellProto, SpellSchoolMask schoolMask, WeaponAttackType attackType)
{
    // Creatures shouldn't crit with spells
    if (GetTypeId() == TYPEID_UNIT && !((Creature*)this)->IsPet() && !((Creature*)this)->IsTotem())
    {
        return false;
    }

    // not critting spell
    if (spellProto->HasAttribute(SPELL_ATTR_EX2_CANT_CRIT))
    {
        return false;
    }

    float crit_chance = 0.0f;
    switch (spellProto->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_NONE:
            return false;
        case SPELL_DAMAGE_CLASS_MAGIC:
        {
            if (schoolMask & SPELL_SCHOOL_MASK_NORMAL)
            {
                crit_chance = 0.0f;
            }
            // For other schools
            else if (GetTypeId() == TYPEID_PLAYER)
            {
                crit_chance = ((Player*)this)->m_SpellCritPercentage[GetFirstSchoolInMask(schoolMask)];
            }
            else
            {
                crit_chance = float(m_baseSpellCritChance);
                crit_chance += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
            }
            // taken
            if (pVictim)
            {
                if (!IsPositiveSpell(spellProto->ID))
                {
                    // Modify critical chance by victim SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE
                    crit_chance += pVictim->GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE, schoolMask);
                }

                // scripted (increase crit chance ... against ... target by x%)
                AuraList const& mOverrideClassScript = GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                for (AuraList::const_iterator i = mOverrideClassScript.begin(); i != mOverrideClassScript.end(); ++i)
                {
                    if (!((*i)->isAffectedOnSpell(spellProto)))
                    {
                        continue;
                    }
                    switch ((*i)->GetModifier()->m_miscvalue)
                    {
                        // Shatter
                        case 849: if (pVictim->IsFrozen()) { crit_chance += 10.0f; } break;
                        case 910: if (pVictim->IsFrozen()) { crit_chance += 20.0f; } break;
                        case 911: if (pVictim->IsFrozen()) { crit_chance += 30.0f; } break;
                        case 912: if (pVictim->IsFrozen()) { crit_chance += 40.0f; } break;
                        case 913: if (pVictim->IsFrozen()) { crit_chance += 50.0f; } break;
                        default:
                            break;
                    }
                }
            }
            break;
        }
        case SPELL_DAMAGE_CLASS_MELEE:
        case SPELL_DAMAGE_CLASS_RANGED:
        {
            if (pVictim)
            {
                crit_chance = GetUnitCriticalChance(attackType, pVictim);
            }

            crit_chance += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, schoolMask);
            break;
        }
        default:
            return false;
    }
    // percent done
    // only players use intelligence for critical chance computations
    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_CRITICAL_CHANCE, crit_chance);
    }

    crit_chance = crit_chance > 0.0f ? crit_chance : 0.0f;
    if (roll_chance_f(crit_chance))
    {
        return true;
    }
    return false;
}

/**
 * @brief Applies critical damage bonuses for a spell hit.
 *
 * @param spellProto The spell entry.
 * @param damage The base damage.
 * @param pVictim The victim, if any.
 * @return The damage after critical bonuses.
 */
uint32 Unit::SpellCriticalDamageBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim)
{
    // Calculate critical bonus
    int32 crit_bonus;
    switch (spellProto->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_MELEE:                      // for melee based spells is 100%
        case SPELL_DAMAGE_CLASS_RANGED:
            crit_bonus = damage;
            break;
        default:
            crit_bonus = damage / 2;                        // for spells is 50%
            break;
    }

    // adds additional damage to crit_bonus (from talents)
    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_CRIT_DAMAGE_BONUS, crit_bonus);
    }

    if (!pVictim)
    {
        return damage += crit_bonus;
    }

    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();

    int32 critPctDamageMod = GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, creatureTypeMask);

    if (critPctDamageMod != 0)
    {
        crit_bonus = int32(crit_bonus * float((100.0f + critPctDamageMod) / 100.0f));
    }

    if (crit_bonus > 0)
    {
        damage += crit_bonus;
    }

    return damage;
}

/**
 * @brief Applies critical healing bonuses for a spell heal.
 *
 * @param spellProto The spell entry.
 * @param damage The base healing amount.
 * @param pVictim The healed victim, if any.
 * @return The healing after critical bonuses.
 */
uint32 Unit::SpellCriticalHealingBonus(SpellEntry const* spellProto, uint32 damage, Unit* pVictim)
{
    // Calculate critical bonus
    int32 crit_bonus;
    switch (spellProto->DefenseType)
    {
        case SPELL_DAMAGE_CLASS_MELEE:                      // for melee based spells is 100%
        case SPELL_DAMAGE_CLASS_RANGED:
            // TODO: write here full calculation for melee/ranged spells
            crit_bonus = damage;
            break;
        default:
            crit_bonus = damage / 2;                        // for spells is 50%
            break;
    }

    if (pVictim)
    {
        uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
        crit_bonus = int32(crit_bonus * GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRIT_PERCENT_VERSUS, creatureTypeMask));
    }

    if (crit_bonus > 0)
    {
        damage += crit_bonus;
    }

    return damage;
}

/**
 * Calculates caster part of healing spell bonuses,
 * also includes different bonuses dependent from target auras
 */
uint32 Unit::SpellHealingBonusDone(Unit* pVictim, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack, Spell const* spell)
{
    // For totems get healing bonus from owner (statue isn't totem in fact)
    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsTotem() && ((Totem*)this)->GetTotemType() != TOTEM_STATUE)
    {
        if (Unit* owner = GetOwner())
        {
            return owner->SpellHealingBonusDone(pVictim, spellProto, healamount, damagetype, stack, spell);
        }
    }

    // No heal amount for this class spells
    if (spellProto->DefenseType == SPELL_DAMAGE_CLASS_NONE)
    {
        return healamount < 0 ? 0 : healamount;
    }

    // Healing Done
    // Done total percent damage auras
    float  DoneTotalMod = 1.0f;
    int32  DoneTotal = 0;

    // Healing done percent
    AuraList const& mHealingDonePct = GetAurasByType(SPELL_AURA_MOD_HEALING_DONE_PERCENT);
    for (AuraList::const_iterator i = mHealingDonePct.begin(); i != mHealingDonePct.end(); ++i)
    {
        DoneTotalMod *= (100.0f + (*i)->GetModifier()->m_amount) / 100.0f;
    }

    // done scripted mod (take it from owner)
    Unit* owner = GetOwner();
    if (!owner)
    {
        owner = this;
    }
    AuraList const& mOverrideClassScript = owner->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
    for (AuraList::const_iterator i = mOverrideClassScript.begin(); i != mOverrideClassScript.end(); ++i)
    {
        if (!(*i)->isAffectedOnSpell(spellProto))
        {
            continue;
        }
        switch ((*i)->GetModifier()->m_miscvalue)
        {
            case 4415: // Increased Rejuvenation Healing
            case 3736: // Hateful Totem of the Third Wind / Increased Lesser Healing Wave / Savage Totem of the Third Wind
                DoneTotal += (*i)->GetModifier()->m_amount;
                break;
            default:
                break;
        }
    }

    // Done fixed damage bonus auras
    int32 DoneAdvertisedBenefit  = SpellBaseHealingBonusDone(GetSpellSchoolMask(spellProto));

    // apply ap bonus and benefit affected by spell power implicit coeffs and spell level penalties
    DoneTotal = SpellBonusWithCoeffs(this, spellProto, DoneTotal, DoneAdvertisedBenefit, 0, damagetype, true, spell);

    // use float as more appropriate for negative values and percent applying
    float heal = (healamount + DoneTotal * int32(stack)) * DoneTotalMod;
    // apply spellmod to Done amount
    if (Player* modOwner = GetSpellModOwner())
    {
        modOwner->ApplySpellMod(spellProto->ID, damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, heal);
    }

    return heal < 0 ? 0 : uint32(heal);
}

/**
 * Calculates target part of healing spell bonuses,
 * will be called on each tick for periodic damage over time auras
 */
uint32 Unit::SpellHealingBonusTaken(Unit* pCaster, SpellEntry const* spellProto, int32 healamount, DamageEffectType damagetype, uint32 stack, Spell const* spell)
{
    float  TakenTotalMod = 1.0f;

    // Healing taken percent
    float minval = float(GetMaxNegativeAuraModifier(SPELL_AURA_MOD_HEALING_PCT));
    if (minval)
    {
        TakenTotalMod *= (100.0f + minval) / 100.0f;
    }

    float maxval = float(GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HEALING_PCT));
    if (maxval)
    {
        TakenTotalMod *= (100.0f + maxval) / 100.0f;
    }

    // No heal amount for this class spells
    if (spellProto->DefenseType == SPELL_DAMAGE_CLASS_NONE)
    {
        healamount = int32(healamount * TakenTotalMod);
        return healamount < 0 ? 0 : healamount;
    }

    // Healing Done
    // Done total percent damage auras
    int32  TakenTotal = 0;

    // Taken fixed damage bonus auras
    int32 TakenAdvertisedBenefit = SpellBaseHealingBonusTaken(GetSpellSchoolMask(spellProto));

    // Blessing of Light dummy effects healing taken from Holy Light and Flash of Light
    if (spellProto->SpellClassSet == SPELLFAMILY_PALADIN && (spellProto->SpellClassMask & UI64LIT(0x0000000000006000)))
    {
        AuraList const& mDummyAuras = GetAurasByType(SPELL_AURA_DUMMY);
        for (AuraList::const_iterator i = mDummyAuras.begin(); i != mDummyAuras.end(); ++i)
        {
            if ((*i)->GetSpellProto()->SpellVisualID == 300 && ((*i)->GetSpellProto()->SpellClassMask & UI64LIT(0x0000000010000000)))
            {
                // Flash of Light
                if ((spellProto->SpellClassMask & UI64LIT(0x0000000000002000)) && (*i)->GetEffIndex() == EFFECT_INDEX_1)
                {
                    TakenTotal += (*i)->GetModifier()->m_amount;
                }
                // Holy Light
                else if ((spellProto->SpellClassMask & UI64LIT(0x0000000000004000)) && (*i)->GetEffIndex() == EFFECT_INDEX_0)
                {
                    TakenTotal += (*i)->GetModifier()->m_amount;
                }
            }
        }
    }

    // apply benefit affected by spell power implicit coeffs and spell level penalties
    TakenTotal = SpellBonusWithCoeffs(pCaster, spellProto, TakenTotal, TakenAdvertisedBenefit, 0, damagetype, false, spell);

    // Taken mods
    // Healing Wave cast
    if (spellProto->SpellClassSet == SPELLFAMILY_SHAMAN && (spellProto->SpellClassMask & UI64LIT(0x0000000000000040)))
    {
        // Search for Healing Way on Victim
        Unit::AuraList const& auraDummy = GetAurasByType(SPELL_AURA_DUMMY);
        for (Unit::AuraList::const_iterator itr = auraDummy.begin(); itr != auraDummy.end(); ++itr)
        {
            if ((*itr)->GetId() == 29203)
            {
                TakenTotalMod *= ((*itr)->GetModifier()->m_amount + 100.0f) / 100.0f;
            }
        }
    }

    // use float as more appropriate for negative values and percent applying
    float heal = (healamount + TakenTotal * int32(stack)) * TakenTotalMod;

    return heal < 0 ? 0 : uint32(heal);
}

/**
 * @brief Computes the unit's advertised healing bonus for a school mask.
 *
 * @param schoolMask The spell school mask.
 * @return The advertised healing bonus.
 */
int32 Unit::SpellBaseHealingBonusDone(SpellSchoolMask schoolMask)
{
    int32 AdvertisedBenefit = 0;

    AuraList const& mHealingDone = GetAurasByType(SPELL_AURA_MOD_HEALING_DONE);
    for (AuraList::const_iterator i = mHealingDone.begin(); i != mHealingDone.end(); ++i)
    {
        if (((*i)->GetModifier()->m_miscvalue & schoolMask) != 0)
        {
            AdvertisedBenefit += (*i)->GetModifier()->m_amount;
        }
    }

    // Healing bonus of spirit, intellect and strength
    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Healing bonus from stats
        AuraList const& mHealingDoneOfStatPercent = GetAurasByType(SPELL_AURA_MOD_SPELL_HEALING_OF_STAT_PERCENT);
        for (AuraList::const_iterator i = mHealingDoneOfStatPercent.begin(); i != mHealingDoneOfStatPercent.end(); ++i)
        {
            // 1.12.* have only 1 stat type support
            Stats usedStat = STAT_SPIRIT;
            AdvertisedBenefit += int32(GetStat(usedStat) * (*i)->GetModifier()->m_amount / 100.0f);
        }
    }
    return AdvertisedBenefit;
}

/**
 * @brief Computes the target's advertised healing taken bonus for a school mask.
 *
 * @param schoolMask The spell school mask.
 * @return The advertised healing taken bonus.
 */
int32 Unit::SpellBaseHealingBonusTaken(SpellSchoolMask schoolMask)
{
    int32 AdvertisedBenefit = 0;
    AuraList const& mDamageTaken = GetAurasByType(SPELL_AURA_MOD_HEALING);
    for (AuraList::const_iterator i = mDamageTaken.begin(); i != mDamageTaken.end(); ++i)
    {
        if ((*i)->GetModifier()->m_miscvalue & schoolMask)
        {
            AdvertisedBenefit += (*i)->GetModifier()->m_amount;
        }
    }
    return AdvertisedBenefit;
}

/**
 * @brief Checks whether the unit is immune to an entire spell.
 *
 * @param spellInfo The spell entry to test.
 * @param castOnSelf Unused self-cast flag placeholder.
 * @return True if the spell is immune; otherwise, false.
 */
bool Unit::IsImmuneToSpell(SpellEntry const* spellInfo, bool /*castOnSelf*/)
{
    if (!spellInfo)
    {
        return false;
    }

    // TODO add spellEffect immunity checks!, player with flag in bg is immune to immunity buffs from other friendly players!
    // SpellImmuneList const& dispelList = m_spellImmune[IMMUNITY_EFFECT];

    SpellImmuneList const& dispelList = m_spellImmune[IMMUNITY_DISPEL];
    for (SpellImmuneList::const_iterator itr = dispelList.begin(); itr != dispelList.end(); ++itr)
    {
        if (itr->type == spellInfo->DispelType)
        {
            return true;
        }
    }

    if (!spellInfo->HasAttribute(SPELL_ATTR_EX_UNAFFECTED_BY_SCHOOL_IMMUNE) &&          // unaffected by school immunity
        !spellInfo->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))           // can remove immune (by dispell or immune it)
    {
        SpellImmuneList const& schoolList = m_spellImmune[IMMUNITY_SCHOOL];
        for (SpellImmuneList::const_iterator itr = schoolList.begin(); itr != schoolList.end(); ++itr)
        {
            if (!(IsPositiveSpell(itr->spellId) && IsPositiveSpell(spellInfo->ID)) &&
                (itr->type & GetSpellSchoolMask(spellInfo)))
            {
                return true;
            }
        }
    }

    if (uint32 mechanic = spellInfo->Mechanic)
    {
        SpellImmuneList const& mechanicList = m_spellImmune[IMMUNITY_MECHANIC];
        for (SpellImmuneList::const_iterator itr = mechanicList.begin(); itr != mechanicList.end(); ++itr)
        {
            if (itr->type == mechanic)
            {
                return true;
            }
        }

        AuraList const& immuneAuraApply = GetAurasByType(SPELL_AURA_MECHANIC_IMMUNITY_MASK);
        for (AuraList::const_iterator iter = immuneAuraApply.begin(); iter != immuneAuraApply.end(); ++iter)
        {
            if ((*iter)->GetModifier()->m_miscvalue & (1 << (mechanic - 1)))
            {
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Checks whether the unit is immune to a specific spell effect.
 *
 * @param spellInfo The spell entry to test.
 * @param index The effect index.
 * @param castOnSelf Unused self-cast flag placeholder.
 * @return True if the effect is immune; otherwise, false.
 */
bool Unit::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool /*castOnSelf*/) const
{
    // If m_immuneToEffect type contain this effect type, IMMUNE effect.
    uint32 effect = spellInfo->Effect[index];
    SpellImmuneList const& effectList = m_spellImmune[IMMUNITY_EFFECT];
    for (SpellImmuneList::const_iterator itr = effectList.begin(); itr != effectList.end(); ++itr)
    {
        if (itr->type == effect)
        {
            return true;
        }
    }

    if (uint32 mechanic = spellInfo->EffectMechanic[index])
    {
        SpellImmuneList const& mechanicList = m_spellImmune[IMMUNITY_MECHANIC];
        for (SpellImmuneList::const_iterator itr = mechanicList.begin(); itr != mechanicList.end(); ++itr)
        {
            if (itr->type == mechanic)
            {
                return true;
            }
        }

        AuraList const& immuneAuraApply = GetAurasByType(SPELL_AURA_MECHANIC_IMMUNITY_MASK);
        for (AuraList::const_iterator iter = immuneAuraApply.begin(); iter != immuneAuraApply.end(); ++iter)
        {
            if ((*iter)->GetModifier()->m_miscvalue & (1 << (mechanic - 1)))
            {
                return true;
            }
        }
    }

    if (uint32 aura = spellInfo->EffectAura[index])
    {
        SpellImmuneList const& list = m_spellImmune[IMMUNITY_STATE];
        for (SpellImmuneList::const_iterator itr = list.begin(); itr != list.end(); ++itr)
        {
            if (itr->type == aura)
            {
                return true;
            }
        }
    }
    return false;
}

/**
 * Calculates caster part of melee damage bonuses,
 * also includes different bonuses dependent from target auras
 */
uint32 Unit::MeleeDamageBonusDone(Unit* pVictim, uint32 pdamage, WeaponAttackType attType, SpellEntry const* spellProto, DamageEffectType damagetype, uint32 stack)
{
    if (!pVictim)
    {
        return pdamage;
    }

    if (pdamage == 0)
    {
        return pdamage;
    }

    // Paladin Holy Spells such as seal of righteousness, seal of command or judgement of command are all calculated in other functions.
    if (spellProto && GetSpellSchoolMask(spellProto) == SPELL_SCHOOL_MASK_HOLY && GetTypeId() == TYPEID_PLAYER)
    {
        return pdamage;
    }

    // differentiate for weapon damage based spells
    bool isWeaponDamageBasedSpell = !(spellProto && (damagetype == DOT || spellProto->HasSpellEffect(SPELL_EFFECT_SCHOOL_DAMAGE)));
    Item*  pWeapon          = GetTypeId() == TYPEID_PLAYER ? ((Player*)this)->GetWeaponForAttack(attType, true, false) : NULL;
    uint32 creatureTypeMask = pVictim->GetCreatureTypeMask();
    uint32 schoolMask       = uint32(spellProto ? GetSpellSchoolMask(spellProto) : GetMeleeDamageSchoolMask());

    // FLAT damage bonus auras
    // =======================
    int32 DoneFlat  = 0;
    int32 APbonus   = 0;

    // ..done flat, already included in weapon damage based spells
    if (!isWeaponDamageBasedSpell)
    {
        AuraList const& mModDamageDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_DONE);
        for (AuraList::const_iterator i = mModDamageDone.begin(); i != mModDamageDone.end(); ++i)
        {
            if ((*i)->GetModifier()->m_miscvalue & schoolMask &&                         // schoolmask has to fit with the intrinsic spell school
                (*i)->GetModifier()->m_miscvalue & GetMeleeDamageSchoolMask() &&         // AND schoolmask has to fit with weapon damage school (essential for non-physical spells)
                (((*i)->GetSpellProto()->EquippedItemClass == -1) ||                     // general, weapon independent
                (pWeapon && pWeapon->IsFitToSpellRequirements((*i)->GetSpellProto()))))  // OR used weapon fits aura requirements
            {
                DoneFlat += (*i)->GetModifier()->m_amount;
            }
        }

        // Pets just add their bonus damage to their melee damage
        if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsPet())
        {
            DoneFlat += ((Pet*)this)->GetBonusDamage();
        }
    }

    // ..done flat (by creature type mask)
    DoneFlat += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_CREATURE, creatureTypeMask);

    // ..done flat (base at attack power for marked target and base at attack power for creature type)
    if (attType == RANGED_ATTACK)
    {
        APbonus += pVictim->GetTotalAuraModifier(SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS);
        APbonus += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_RANGED_ATTACK_POWER_VERSUS, creatureTypeMask);
    }
    else
    {
        APbonus += pVictim->GetTotalAuraModifier(SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS);
        APbonus += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_MELEE_ATTACK_POWER_VERSUS, creatureTypeMask);
    }

    // PERCENT damage auras
    // ====================
    float DonePercent   = 1.0f;

    // ..done pct, already included in weapon damage based spells
    if (!isWeaponDamageBasedSpell)
    {
        AuraList const& mModDamagePercentDone = GetAurasByType(SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        for (AuraList::const_iterator i = mModDamagePercentDone.begin(); i != mModDamagePercentDone.end(); ++i)
        {
            if ((*i)->GetModifier()->m_miscvalue & schoolMask &&                         // schoolmask has to fit with the intrinsic spell school
                (((*i)->GetSpellProto()->EquippedItemClass == -1) ||                     // general, weapon independent
                (pWeapon && pWeapon->IsFitToSpellRequirements((*i)->GetSpellProto()))))  // OR used weapon fits aura requirements
            {
                DonePercent *= ((*i)->GetModifier()->m_amount + 100.0f) / 100.0f;
            }
        }

        if (attType == OFF_ATTACK)
        {
            DonePercent *= GetModifierValue(UNIT_MOD_DAMAGE_OFFHAND, TOTAL_PCT); // no school check required
        }
    }

    // ..done pct (by creature type mask)
    DonePercent *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_DONE_VERSUS, creatureTypeMask);

    // special dummys/class scripts and other effects
    // =============================================
    Unit* owner = GetOwner();
    if (!owner)
    {
        owner = this;
    }

    // final calculation
    // =================

    float DoneTotal = 0.0f;

    // scaling of non weapon based spells
    if (!isWeaponDamageBasedSpell)
    {
        // apply ap bonus and benefit affected by spell power implicit coeffs and spell level penalties
        DoneTotal = SpellBonusWithCoeffs(this, spellProto, DoneTotal, DoneFlat, APbonus, damagetype, true);
    }
    // weapon damage based spells
    else if (APbonus || DoneFlat)
    {
        bool normalized = spellProto ? spellProto->HasSpellEffect(SPELL_EFFECT_NORMALIZED_WEAPON_DMG) : false;
        DoneTotal += int32(APbonus / 14.0f * GetAPMultiplier(attType, normalized));

        // for weapon damage based spells we still have to apply damage done percent mods
        // (that are already included into pdamage) to not-yet included DoneFlat
        // e.g. from doneVersusCreature, apBonusVs...
        UnitMods unitMod;
        switch (attType)
        {
            default:
            case BASE_ATTACK:   unitMod = UNIT_MOD_DAMAGE_MAINHAND; break;
            case OFF_ATTACK:    unitMod = UNIT_MOD_DAMAGE_OFFHAND;  break;
            case RANGED_ATTACK: unitMod = UNIT_MOD_DAMAGE_RANGED;   break;
        }

        DoneTotal += DoneFlat;

        DoneTotal *= GetModifierValue(unitMod, TOTAL_PCT);
    }

    float tmpDamage = float(int32(pdamage) + DoneTotal * int32(stack)) * DonePercent;

    // apply spellmod to Done damage
    if (spellProto)
    {
        if (Player* modOwner = GetSpellModOwner())
        {
            modOwner->ApplySpellMod(spellProto->ID, damagetype == DOT ? SPELLMOD_DOT : SPELLMOD_DAMAGE, tmpDamage);
        }
    }

    // bonus result can be negative
    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}

/**
 * Calculates target part of melee damage bonuses,
 * will be called on each tick for periodic damage over time auras
 */
uint32 Unit::MeleeDamageBonusTaken(Unit* pCaster, uint32 pdamage, WeaponAttackType attType, SpellEntry const* spellProto, DamageEffectType damagetype, uint32 stack)
{
    if (!pCaster)
    {
        return pdamage;
    }

    if (pdamage == 0)
    {
        return pdamage;
    }

    // Paladin Holy Spells such as seal of righteousness, seal of command or judgement of command are all calculated in other functions.
    if (spellProto && GetSpellSchoolMask(spellProto) == SPELL_SCHOOL_MASK_HOLY && pCaster->GetTypeId() == TYPEID_PLAYER)
    {
        return pdamage;
    }

    // differentiate for weapon damage based spells
    bool isWeaponDamageBasedSpell = !(spellProto && (damagetype == DOT || spellProto->HasSpellEffect(SPELL_EFFECT_SCHOOL_DAMAGE)));
    uint32 schoolMask       = uint32(spellProto ? GetSpellSchoolMask(spellProto) :GetMeleeDamageSchoolMask());

    // FLAT damage bonus auras
    // =======================
    int32 TakenFlat = 0;

    // ..taken flat (base at attack power for marked target and base at attack power for creature type)
    if (attType == RANGED_ATTACK)
    {
        TakenFlat += GetTotalAuraModifier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN);
    }
    else
    {
        TakenFlat += GetTotalAuraModifier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN);
    }

    // ..taken flat (by school mask)
    TakenFlat += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_DAMAGE_TAKEN, schoolMask);

    // PERCENT damage auras
    // ====================
    float TakenPercent  = 1.0f;

    // ..taken pct (by school mask)
    TakenPercent *= GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN, schoolMask);

    // ..taken pct (melee/ranged)
    if (attType == RANGED_ATTACK)
    {
        TakenPercent *= GetTotalAuraMultiplier(SPELL_AURA_MOD_RANGED_DAMAGE_TAKEN_PCT);
    }
    else
    {
        TakenPercent *= GetTotalAuraMultiplier(SPELL_AURA_MOD_MELEE_DAMAGE_TAKEN_PCT);
    }

    // final calculation
    // =================

    // scaling of non weapon based spells
    if (!isWeaponDamageBasedSpell)
    {
        // apply benefit affected by spell power implicit coeffs and spell level penalties
        TakenFlat = SpellBonusWithCoeffs(pCaster, spellProto, 0, TakenFlat, 0, damagetype, false);
    }

    float tmpDamage = float(int32(pdamage) + TakenFlat * int32(stack)) * TakenPercent;

    // bonus result can be negative
    return tmpDamage > 0 ? uint32(tmpDamage) : 0;
}
