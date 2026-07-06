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

/**
 * @file Spell.cpp
 * @brief Spell casting and effect implementation
 *
 * This file implements the Spell class which handles spell casting:
 * - Spell validation and casting requirements
 * - Spell effect execution (damage, healing, summon, etc.)
 * - Spell targeting and area effects
 * - Spell cooldowns and resource costs
 * - Spell interruption and pushback
 * - Spell aura application
 * - Spell hit/miss calculations
 *
 * Spells are the primary combat mechanic in WoW, encompassing
 * abilities, talents, and item effects.
 *
 * @see Spell for the spell class
 * @see SpellAura for spell auras
 * @see SpellMgr for spell management
 */



#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "VMapFactory.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Applies all pending spell effects to a unit target entry.
 *
 * @param target The target info entry.
 */
void Spell::DoAllEffectOnTarget(TargetInfo* target)
{
    if (target->processed)                                  // Check target
    {
        return;
    }
    target->processed = true;                               // Target checked in apply effects procedure

    // Get mask of effects for target
    uint32 mask = target->effectMask;

    Unit* unit = m_caster->GetObjectGuid() == target->targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, target->targetGUID);
    if (!unit)
    {
        return;
    }

    // Get original caster (if exist) and calculate damage/healing from him data
    Unit* real_caster = GetAffectiveCaster();
    // FIXME: in case wild GO heal/damage spells will be used target bonuses
    Unit* caster = real_caster ? real_caster : m_caster;

    SpellMissInfo missInfo = target->missCondition;
    // Need init unitTarget by default unit (can changed in code on reflect)
    // Or on missInfo!=SPELL_MISS_NONE unitTarget undefined (but need in trigger subsystem)
    unitTarget = unit;

    // Reset damage/healing counter
    ResetEffectDamageAndHeal();

    // Fill base trigger info
    uint32 procAttacker = m_procAttacker;
    uint32 procVictim   = m_procVictim;
    uint32 procEx       = PROC_EX_NONE;

    // drop proc flags in case target not affected negative effects in negative spell
    // for example caster bonus or animation,
    // except miss case where will assigned PROC_EX_* flags later
    if (((procAttacker | procVictim) & NEGATIVE_TRIGGER_MASK) &&
        !(target->effectMask & m_negativeEffectMask) && missInfo == SPELL_MISS_NONE)
    {
        procAttacker = PROC_FLAG_NONE;
        procVictim   = PROC_FLAG_NONE;
    }

    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed > 0.0f)
    {
        // mark effects that were already handled in Spell::HandleDelayedSpellLaunch on spell launch as processed
        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (IsEffectHandledOnDelayedSpellLaunch(m_spellInfo, SpellEffectIndex(i)))
            {
                mask &= ~(1 << i);
            }
        }

        // maybe used in effects that are handled on hit
        m_damage += target->damage;
    }

    if (missInfo == SPELL_MISS_NONE)                        // In case spell hit target, do all effect on that target
    {
        DoSpellHitOnUnit(unit, mask);
    }
    else if (missInfo == SPELL_MISS_REFLECT)                // In case spell reflect from target, do all effect on caster (if hit)
    {
        if (target->reflectResult == SPELL_MISS_NONE)       // If reflected spell hit caster -> do all effect on him
        {
            DoSpellHitOnUnit(m_caster, mask, true);
            unitTarget = m_caster;

            if (m_caster->GetTypeId() == TYPEID_UNIT)
            {
                m_caster->ToCreature()->LowerPlayerDamageReq(target->damage);
            }
        }
    }
    else                                                    // in 1.12.1 we need explicit miss info
    {
        if (real_caster)
        {
            // Warrior's execute must be returned as 20647 spell result since the client only displays info when receiving this id.
            // Done here because must be based on MeleeSpellHitResult of spell id's 5308/20658/20660/20661/20662.
            if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR && m_spellInfo->IsFitToFamilyMask(0x0000000020000000))
            {
                real_caster->SendSpellMiss(unit, 20647, missInfo);
            }
            else
            {
                real_caster->SendSpellMiss(unit, m_spellInfo->ID, missInfo);
            }
        }

        if (missInfo == SPELL_MISS_MISS || missInfo == SPELL_MISS_RESIST)
        {
            if (real_caster && real_caster != unit)
            {
                // can cause back attack (if detected)
                if (!m_spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO) && !IsPositiveSpell(m_spellInfo->ID) &&
                    m_caster->IsVisibleForOrDetect(unit, unit, false))
                {
                    if (!unit->IsInCombat() && unit->GetTypeId() != TYPEID_PLAYER && ((Creature*)unit)->AI())
                    {
                        ((Creature*)unit)->AI()->AttackedBy(real_caster);
                    }

                    unit->AddThreat(real_caster);
                    unit->SetInCombatWith(real_caster);
                    real_caster->SetInCombatWith(unit);
                }
            }
        }
    }

    // All calculated do it!
    // Do healing and triggers
    if (m_healing)
    {
        bool crit = real_caster && real_caster->IsSpellCrit(unitTarget, m_spellInfo, m_spellSchoolMask);
        uint32 addhealth = m_healing;
        if (crit)
        {
            procEx |= PROC_EX_CRITICAL_HIT;
            addhealth = caster->SpellCriticalHealingBonus(m_spellInfo, addhealth, NULL);
        }
        else
        {
            procEx |= PROC_EX_NORMAL_HIT;
        }

        // Do triggers for unit (reflect triggers passed on hit phase for correct drop charge)
        if (m_canTrigger && missInfo != SPELL_MISS_REFLECT)
        {
            // Some spell expected send main spell info to triggered system
            SpellEntry const* spellInfo = m_spellInfo;
            switch (m_spellInfo->ID)
            {
                case 19968:                                 // Holy Light triggered heal
                case 19993:                                 // Flash of Light triggered heal
                {
                    // stored in unused spell effect basepoints in main spell code
                    uint32 spellid = m_currentBasePoints[EFFECT_INDEX_1];
                    spellInfo = sSpellStore.LookupEntry(spellid);
                }
            }

            caster->ProcDamageAndSpell(unitTarget, real_caster ? procAttacker : uint32(PROC_FLAG_NONE), procVictim, procEx, addhealth, m_attackType, spellInfo);
        }

        int32 gain = caster->DealHeal(unitTarget, addhealth, m_spellInfo, crit);

        if (real_caster)
        {
            unitTarget->GetHostileRefManager().threatAssist(real_caster, float(gain) * 0.5f * sSpellMgr.GetSpellThreatMultiplier(m_spellInfo), m_spellInfo);
        }
    }
    // Do damage and triggers
    else if (m_damage)
    {
        // Fill base damage struct (unitTarget - is real spell target)
        SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->ID, GetFirstSchoolInMask(m_spellSchoolMask));

        if (speed > 0.0f)
        {
            damageInfo.damage = m_damage;
            damageInfo.HitInfo = target->HitInfo;
        }
        // Add bonuses and fill damageInfo struct
        else
        {
            caster->CalculateSpellDamage(&damageInfo, m_damage, m_spellInfo, m_attackType);
        }

        unitTarget->CalculateAbsorbResistBlock(caster, &damageInfo, m_spellInfo);

        caster->DealDamageMods(damageInfo.target, damageInfo.damage, &damageInfo.absorb);

        // Send log damage message to client
        caster->SendSpellNonMeleeDamageLog(&damageInfo);

        procEx = createProcExtendMask(&damageInfo, missInfo);
        procVictim |= PROC_FLAG_TAKEN_ANY_DAMAGE;

        // Do triggers for unit (reflect triggers passed on hit phase for correct drop charge)
        if (m_canTrigger && missInfo != SPELL_MISS_REFLECT)
        {
            caster->ProcDamageAndSpell(unitTarget, real_caster ? procAttacker : uint32(PROC_FLAG_NONE), procVictim, procEx, damageInfo.damage, m_attackType, m_spellInfo);
        }

        // trigger weapon enchants for weapon based spells; exclude spells that stop attack, because may break CC
        if (m_caster->GetTypeId() == TYPEID_PLAYER && m_spellInfo->EquippedItemClass == ITEM_CLASS_WEAPON &&
            !m_spellInfo->HasAttribute(SPELL_ATTR_STOP_ATTACK_TARGET))
        {
            ((Player*)m_caster)->CastItemCombatSpell(unitTarget, m_attackType);
        }

        caster->DealSpellDamage(&damageInfo, true);

        // Bloodthirst
        if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR &&
            m_spellInfo->SpellClassMask & UI64LIT(0x0000000002000000) &&
            m_spellInfo->SpellIconID == 38)
        {
            uint32 BTAura = 0;
            switch (m_spellInfo->ID)
            {
                case 23881: BTAura = 23885; break;
                case 23892: BTAura = 23886; break;
                case 23893: BTAura = 23887; break;
                case 23894: BTAura = 23888; break;
                default:
                    sLog.outError("Spell::EffectSchoolDMG: Spell %u not handled in BTAura", m_spellInfo->ID);
                    break;
            }
            if (BTAura)
            {
                m_caster->CastSpell(m_caster, BTAura, true);
            }
        }
    }
    // Passive spell hits/misses or active spells only misses (only triggers if proc flags set)
    else if (procAttacker || procVictim)
    {
        // Fill base damage struct (unitTarget - is real spell target)
        SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->ID, GetFirstSchoolInMask(m_spellSchoolMask));
        procEx = createProcExtendMask(&damageInfo, missInfo);
        // Do triggers for unit (reflect triggers passed on hit phase for correct drop charge)
        if (m_canTrigger && missInfo != SPELL_MISS_REFLECT)
        {
            caster->ProcDamageAndSpell(unit, real_caster ? procAttacker : uint32(PROC_FLAG_NONE), procVictim, procEx, 0, m_attackType, m_spellInfo);
        }
    }

    // Call scripted function for AI if this spell is casted upon a creature
    if (unit->GetTypeId() == TYPEID_UNIT)
    {
        // cast at creature (or GO) quest objectives update at successful cast finished (+channel finished)
        // ignore pets or autorepeat/melee casts for speed (not exist quest for spells (hm... )
        if (real_caster && !((Creature*)unit)->IsPet() && !IsAutoRepeat() && !IsNextMeleeSwingSpell() && !IsChannelActive())
        {
            if (Player* p = real_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                p->RewardPlayerAndGroupAtCast(unit, m_spellInfo->ID);
            }
        }

        if (((Creature*)unit)->AI())
        {
            ((Creature*)unit)->AI()->SpellHit(m_caster, m_spellInfo);
        }
    }

    // Call scripted function for AI if this spell is casted by a creature
    if (m_caster->GetTypeId() == TYPEID_UNIT && ((Creature*)m_caster)->AI())
    {
        ((Creature*)m_caster)->AI()->SpellHitTarget(unit, m_spellInfo);
    }
    if (real_caster && real_caster != m_caster && real_caster->GetTypeId() == TYPEID_UNIT && ((Creature*)real_caster)->AI())
    {
        ((Creature*)real_caster)->AI()->SpellHitTarget(unit, m_spellInfo);
    }
}

/**
 * @brief Processes spell hit logic and aura application for a unit target.
 *
 * @param unit The unit that was hit.
 * @param effectMask The set of effects to process.
 * @param isReflected True if the spell hit is the result of reflection.
 */
void Spell::DoSpellHitOnUnit(Unit* unit, uint32 effectMask, bool isReflected)
{
    if (!unit || !effectMask)
    {
        return;
    }

    Unit* realCaster = GetAffectiveCaster();

    // Recheck immune (only for delayed spells)
    float speed = m_spellInfo->Speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->Speed : m_spellInfo->Speed;
    if (speed &&
        (unit->IsImmuneToDamage(GetSpellSchoolMask(m_spellInfo)) ||
        unit->IsImmuneToSpell(m_spellInfo, unit == realCaster)))
    {
        if (realCaster)
        {
            realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_IMMUNE);
        }

        ResetEffectDamageAndHeal();
        return;
    }

    if (realCaster && realCaster != unit)
    {
        // Recheck  UNIT_FLAG_NON_ATTACKABLE for delayed spells
        if (speed > 0.0f &&
            unit->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE) &&
            unit->GetCharmerOrOwnerGuid() != m_caster->GetObjectGuid())
        {
            realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_EVADE);
            ResetEffectDamageAndHeal();
            return;
        }

        if (!realCaster->IsFriendlyTo(unit))
        {
            // for delayed spells ignore not visible explicit target
            if (speed > 0.0f && unit == m_targets.getUnitTarget() &&
                !unit->IsVisibleForOrDetect(m_caster, m_caster, false))
            {
                realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_EVADE);
                ResetEffectDamageAndHeal();
                return;
            }

            // not break stealth by cast targeting
            if (!(m_spellInfo->AttributesEx & SPELL_ATTR_EX_NOT_BREAK_STEALTH) && m_spellInfo->ID != 51690 && m_spellInfo->ID != 53055)
            {
                unit->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
            }

            // can cause back attack (if detected), stealth removed at Spell::cast if spell break it
            if (!m_spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO) && !IsPositiveSpell(m_spellInfo->ID) &&
                m_caster->IsVisibleForOrDetect(unit, unit, false))
            {
                // use speedup check to avoid re-remove after above lines
                if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_NOT_BREAK_STEALTH))
                {
                    unit->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
                }

                // caster can be detected but have stealth aura
                m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);

                if (!unit->IsStandState() && !unit->hasUnitState(UNIT_STAT_STUNNED))
                {
                    unit->SetStandState(UNIT_STAND_STATE_STAND);
                }

                switch (m_spellInfo->ID)
                {
                    // Mind Soothe (all ranks)
                    case 453:
                    case 8192:
                    case 10953:
                        //Soothe animal
                    case 9901:
                    case 8955:
                    case 2908:
                        // Gnomish Mind Control Cap
                    case 13180:
                        break;
                    default:
                    {
                        if (!unit->IsInCombat() && unit->GetTypeId() != TYPEID_PLAYER && ((Creature*)unit)->AI())
                        {
                            unit->AttackedBy(realCaster);
                        }

                        unit->AddThreat(realCaster);
                        unit->SetInCombatWith(realCaster);
                        realCaster->SetInCombatWith(unit);

                        if (Player* attackedPlayer = unit->GetCharmerOrOwnerPlayerOrPlayerItself())
                        {
                            realCaster->SetContestedPvP(attackedPlayer);
                        }
                        break;
                    }
                }
            }
        }
        else
        {
            // for delayed spells ignore negative spells (after duel end) for friendly targets
            if (speed > 0.0f && !IsPositiveSpell(m_spellInfo->ID))
            {
                realCaster->SendSpellMiss(unit, m_spellInfo->ID, SPELL_MISS_EVADE);
                ResetEffectDamageAndHeal();
                return;
            }

            // assisting case, healing and resurrection
            if (unit->hasUnitState(UNIT_STAT_ATTACK_PLAYER))
            {
                realCaster->SetContestedPvP();
            }

            if (unit->IsInCombat() && !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_NO_INITIAL_AGGRO))
            {
                realCaster->SetInCombatState(unit->GetCombatTimer() > 0);
                unit->GetHostileRefManager().threatAssist(realCaster, 0.0f, m_spellInfo);
            }
        }
    }

    // Get Data Needed for Diminishing Returns, some effects may have multiple auras, so this must be done on spell hit, not aura add
    m_diminishGroup = GetDiminishingReturnsGroupForSpell(m_spellInfo, m_triggeredByAuraSpell);
    m_diminishLevel = unit->GetDiminishing(m_diminishGroup);
    // Increase Diminishing on unit, current informations for actually casts will use values above
    if ((GetDiminishingReturnsGroupType(m_diminishGroup) == DRTYPE_PLAYER && unit->GetTypeId() == TYPEID_PLAYER) ||
        GetDiminishingReturnsGroupType(m_diminishGroup) == DRTYPE_ALL)
    {
        unit->IncrDiminishing(m_diminishGroup);
    }

    // Apply additional spell effects to target
    CastPreCastSpells(unit);

    if (IsSpellAppliesAura(m_spellInfo, effectMask))
    {
        m_spellAuraHolder = CreateSpellAuraHolder(m_spellInfo, unit, realCaster, m_CastItem);
        m_spellAuraHolder->setDiminishGroup(m_diminishGroup);
    }
    else
    {
        m_spellAuraHolder = NULL;
    }

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
    {
        if (effectMask & (1 << effectNumber))
        {
            HandleEffects(unit, NULL, NULL, SpellEffectIndex(effectNumber), m_damageMultipliers[effectNumber]);
            if (m_applyMultiplierMask & (1 << effectNumber))
            {
                // Get multiplier
                float multiplier = m_spellInfo->EffectChainAmplitude[effectNumber];
                // Apply multiplier mods
                if (realCaster)
                {
                    if (Player* modOwner = realCaster->GetSpellModOwner())
                    {
                        modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_EFFECT_PAST_FIRST, multiplier, this);
                    }
                }
                m_damageMultipliers[effectNumber] *= multiplier;
            }
        }
    }

    // now apply all created auras
    if (m_spellAuraHolder)
    {
        // normally shouldn't happen
        if (!m_spellAuraHolder->IsEmptyHolder())
        {
            int32 duration = m_spellAuraHolder->GetAuraMaxDuration();
            int32 originalDuration = duration;

            if (duration > 0)
            {
                unit->ApplyDiminishingToDuration(m_diminishGroup, duration, m_caster, m_diminishLevel, isReflected);

                // Fully diminished
                if (duration == 0)
                {
                    delete m_spellAuraHolder;
                    return;
                }
            }

            if (duration != originalDuration)
            {
                m_spellAuraHolder->SetAuraMaxDuration(duration);
                m_spellAuraHolder->SetAuraDuration(duration);
            }

            unit->AddSpellAuraHolder(m_spellAuraHolder);
        }
        else
        {
            delete m_spellAuraHolder;
        }
    }
}

/**
 * @brief Applies all pending spell effects to a game object target entry.
 *
 * @param target The game object target info entry.
 */
void Spell::DoAllEffectOnTarget(GOTargetInfo* target)
{
    if (target->processed)                                  // Check target
    {
        return;
    }
    target->processed = true;                               // Target checked in apply effects procedure

    uint32 effectMask = target->effectMask;
    if (!effectMask)
    {
        return;
    }

    GameObject* go = m_caster->GetMap()->GetGameObject(target->targetGUID);
    if (!go)
    {
        return;
    }

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
    {
        if (effectMask & (1 << effectNumber))
        {
            HandleEffects(NULL, NULL, go, SpellEffectIndex(effectNumber));
        }
    }

    // cast at creature (or GO) quest objectives update at successful cast finished (+channel finished)
    // ignore autorepeat/melee casts for speed (not exist quest for spells (hm... )
    if (!IsAutoRepeat() && !IsNextMeleeSwingSpell() && !IsChannelActive())
    {
        if (Player* p = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
        {
            p->RewardPlayerAndGroupAtCast(go, m_spellInfo->ID);
        }
    }
}

/**
 * @brief Applies all pending spell effects to an item target entry.
 *
 * @param target The item target info entry.
 */
void Spell::DoAllEffectOnTarget(ItemTargetInfo* target)
{
    uint32 effectMask = target->effectMask;
    if (!target->item || !effectMask)
    {
        return;
    }

    for (int effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
    {
        if (effectMask & (1 << effectNumber))
        {
            HandleEffects(NULL, target->item, NULL, SpellEffectIndex(effectNumber));
        }
    }
}

/**
 * @brief Precomputes delayed launch damage data for a unit target.
 *
 * @param target The target info entry.
 */
void Spell::HandleDelayedSpellLaunch(TargetInfo* target)
{
    // Get mask of effects for target
    uint32 mask = target->effectMask;

    Unit* unit = m_caster->GetObjectGuid() == target->targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, target->targetGUID);
    if (!unit)
    {
        return;
    }

    // Get original caster (if exist) and calculate damage/healing from him data
    Unit* real_caster = GetAffectiveCaster();
    // FIXME: in case wild GO heal/damage spells will be used target bonuses
    Unit* caster = real_caster ? real_caster : m_caster;

    SpellMissInfo missInfo = target->missCondition;
    // Need init unitTarget by default unit (can changed in code on reflect)
    // Or on missInfo!=SPELL_MISS_NONE unitTarget undefined (but need in trigger subsystem)
    unitTarget = unit;

    // Reset damage/healing counter
    m_damage = 0;
    m_healing = 0; // healing maybe not needed at this point

    // Fill base damage struct (unitTarget - is real spell target)
    SpellNonMeleeDamage damageInfo(caster, unitTarget, m_spellInfo->ID, GetFirstSchoolInMask(m_spellSchoolMask));

    // keep damage amount for reflected spells
    if (missInfo == SPELL_MISS_NONE || (missInfo == SPELL_MISS_REFLECT && target->reflectResult == SPELL_MISS_NONE))
    {
        for (int32 effectNumber = 0; effectNumber < MAX_EFFECT_INDEX; ++effectNumber)
        {
            if (mask & (1 << effectNumber) && IsEffectHandledOnDelayedSpellLaunch(m_spellInfo, SpellEffectIndex(effectNumber)))
            {
                HandleEffects(unit, NULL, NULL, SpellEffectIndex(effectNumber), m_damageMultipliers[effectNumber]);
                if (m_applyMultiplierMask & (1 << effectNumber))
                {
                    // Get multiplier
                    float multiplier = m_spellInfo->EffectChainAmplitude[effectNumber];
                    // Apply multiplier mods
                    if (real_caster)
                    {
                        if (Player* modOwner = real_caster->GetSpellModOwner())
                        {
                            modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_EFFECT_PAST_FIRST, multiplier, this);
                        }
                    }
                    m_damageMultipliers[effectNumber] *= multiplier;
                }
            }
        }

        if (m_damage > 0)
        {
            caster->CalculateSpellDamage(&damageInfo, m_damage, m_spellInfo, m_attackType);
        }
    }

    target->damage = damageInfo.damage;
    target->HitInfo = damageInfo.HitInfo;
}

/**
 * @brief Initializes per-effect damage multipliers and chain-target modifiers.
 */
void Spell::InitializeDamageMultipliers()
{
    for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_spellInfo->Effect[i] == 0)
        {
            continue;
        }

        uint32 EffectChainTarget = m_spellInfo->EffectChainTargets[i];
        if (Unit* realCaster = GetAffectiveCaster())
        {
            if (Player* modOwner = realCaster->GetSpellModOwner())
            {
                modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_JUMP_TARGETS, EffectChainTarget, this);
            }
        }
        m_damageMultipliers[i] = 1.0f;
        if ((m_spellInfo->ImplicitTargetA[i] == TARGET_CHAIN_DAMAGE || m_spellInfo->ImplicitTargetA[i] == TARGET_CHAIN_HEAL) &&
            (EffectChainTarget > 1))
        {
            m_applyMultiplierMask |= (1 << i);
        }
    }
}
