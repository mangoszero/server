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
 * @brief Validates whether the spell can currently be cast.
 *
 * @param strict True to perform full pre-cast validation including global cooldown checks.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckCast(bool strict)
{
    // check cooldowns to prevent cheating (ignore passive spells, that client side visual only)
    if (m_caster->GetTypeId() == TYPEID_PLAYER && !m_spellInfo->HasAttribute(SPELL_ATTR_PASSIVE) &&
        ((Player*)m_caster)->HasSpellCooldown(m_spellInfo->ID))
    {
        if (m_triggeredByAuraSpell)
        {
            return SPELL_FAILED_DONT_REPORT;
        }
        else
        {
            return SPELL_FAILED_NOT_READY;
        }
    }

    // check global cooldown
    if (strict && !m_IsTriggeredSpell && HasGlobalCooldown())
    {
        return SPELL_FAILED_NOT_READY;
    }

    // only allow triggered spells if at an ended battleground
    if (!m_IsTriggeredSpell && m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        if (BattleGround* bg = ((Player*)m_caster)->GetBattleGround())
        {
            if (bg->GetStatus() == STATUS_WAIT_LEAVE)
            {
                return SPELL_FAILED_DONT_REPORT;
            }
        }
    }

    if (!m_IsTriggeredSpell && m_spellInfo->ID == 2479) //honorless target as non-triggered spell
    {
        return SPELL_FAILED_DONT_REPORT;
    }

    if (!m_IsTriggeredSpell && IsNonCombatSpell(m_spellInfo) &&
        m_caster->IsInCombat())
    {
        return SPELL_FAILED_AFFECTING_COMBAT;
    }

    // Backstab position check
    if (m_spellInfo->ID == 53 || m_spellInfo->ID == 2589 || m_spellInfo->ID == 7159)
    {
        if (Unit* target = m_targets.getUnitTarget())
        {
            if (target->HasInArc(M_PI_F, m_caster))
            {
                SendCastResult(SPELL_FAILED_NOT_BEHIND);
                return SPELL_FAILED_NOT_BEHIND;
            }
        }
    }

    if (m_caster->GetTypeId() == TYPEID_PLAYER && !((Player*)m_caster)->isGameMaster() &&
        sWorld.getConfig(CONFIG_BOOL_VMAP_INDOOR_CHECK) &&
        VMAP::VMapFactory::createOrGetVMapManager()->isLineOfSightCalcEnabled())
    {
        if (m_spellInfo->HasAttribute(SPELL_ATTR_OUTDOORS_ONLY) &&
            !m_caster->GetMap()->GetTerrain()->IsOutdoors(m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ()))
        {
            return SPELL_FAILED_ONLY_OUTDOORS;
        }

        if (m_spellInfo->HasAttribute(SPELL_ATTR_INDOORS_ONLY) &&
            m_caster->GetMap()->GetTerrain()->IsOutdoors(m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ()))
        {
            return SPELL_FAILED_ONLY_INDOORS;
        }
    }
    // only check at first call, Stealth auras are already removed at second call
    // for now, ignore triggered spells
    if (strict && !m_IsTriggeredSpell)
    {
        // Can not be used in this stance/form
        SpellCastResult shapeError = GetErrorAtShapeshiftedCast(m_spellInfo, m_caster->GetShapeshiftForm());
        if (shapeError != SPELL_CAST_OK)
        {
            return shapeError;
        }

        if (m_spellInfo->HasAttribute(SPELL_ATTR_ONLY_STEALTHED) && !(m_caster->HasStealthAura()))
        {
            return SPELL_FAILED_ONLY_STEALTHED;
        }
    }

    // caster state requirements
    if (m_spellInfo->CasterAuraState && !m_caster->HasAuraState(AuraState(m_spellInfo->CasterAuraState)))
    {
        return SPELL_FAILED_CANT_DO_THAT_YET;
    }

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        // cancel autorepeat spells if cast start when moving
        // (not wand currently autorepeat cast delayed to moving stop anyway in spell update code)
        if (((Player*)m_caster)->isMoving())
        {
            // skip stuck spell to allow use it in falling case and apply spell limitations at movement
            if ((!((Player*)m_caster)->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLINGFAR) || m_spellInfo->Effect[EFFECT_INDEX_0] != SPELL_EFFECT_STUCK) &&
                (IsAutoRepeat() || (m_spellInfo->AuraInterruptFlags & AURA_INTERRUPT_FLAG_NOT_SEATED) != 0))
            {
                return SPELL_FAILED_MOVING;
            }
        }

        if (!m_IsTriggeredSpell && NeedsComboPoints(m_spellInfo) &&
            (!m_targets.getUnitTarget() || m_targets.getUnitTarget()->GetObjectGuid() != ((Player*)m_caster)->GetComboTargetGuid()))
        {
            // warrior not have real combo-points at client side but use this way for mark allow Overpower use
            return m_caster->getClass() == CLASS_WARRIOR ? SPELL_FAILED_CANT_DO_THAT_YET : SPELL_FAILED_NO_COMBO_POINTS;
        }

        // Loatheb Corrupted Mind spell failed
        switch (m_spellInfo->SpellClassSet)
        {
            case SPELLFAMILY_DRUID:
            case SPELLFAMILY_PRIEST:
            case SPELLFAMILY_SHAMAN:
            case SPELLFAMILY_PALADIN:
            {
                if (m_spellInfo->HasSpellEffect(SPELL_EFFECT_HEAL) || IsSpellHaveAura(m_spellInfo, SPELL_AURA_PERIODIC_HEAL) ||
                    m_spellInfo->HasSpellEffect(SPELL_EFFECT_DISPEL))
                {
                    Unit::AuraList const& auraClassScripts = m_caster->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                    for (Unit::AuraList::const_iterator itr = auraClassScripts.begin(); itr != auraClassScripts.end();)
                    {
                        if ((*itr)->GetModifier()->m_miscvalue == 4327)
                        {
                            return SPELL_FAILED_FIZZLE;
                        }
                        else
                        {
                            ++itr;
                        }
                    }
                }
            }
        }
    }

    if (Unit* target = m_targets.getUnitTarget())
    {
        //Soothe animal
        bool foundSootheAnimal = true; //will be set to false in the default case
        switch (m_spellInfo->ID)
        {
            case 9901:
            case 8955:
            case 2908:
                break;
            default:
                foundSootheAnimal = false;
                break;
        }
        //Perhaps this should be done for all spells?
        if (foundSootheAnimal)
        {
            if (target->getLevel() > m_spellInfo->MaxTargetLevel)
            {
                return SPELL_FAILED_HIGHLEVEL;
            }
        }

        // Swiftmend
        if (m_spellInfo->ID == 18562)                       // future versions have special aura state for this
        {
            if (!target->GetAura(SPELL_AURA_PERIODIC_HEAL, SPELLFAMILY_DRUID, UI64LIT(0x50)))
            {
                return SPELL_FAILED_TARGET_AURASTATE;
            }
        }

        // Tame Beast trigger
        if (m_spellInfo->ID == 1515)
        {
            SpellCastResult castResult = CanTameUnit(false);
            if (castResult != SPELL_CAST_OK)
            {
                return castResult;
            }
        }

        // give error message when applying lower hot rank to higher hot rank on target
        if (!m_spellInfo->HasSpellEffect(SPELL_EFFECT_HEAL) && IsSpellHaveAura(m_spellInfo, SPELL_AURA_PERIODIC_HEAL))
        {
            Unit::AuraList const& mPeriodicHeal = target->GetAurasByType(SPELL_AURA_PERIODIC_HEAL);
            for (Unit::AuraList::const_iterator i = mPeriodicHeal.begin(); i != mPeriodicHeal.end(); ++i)
            {
                if ((*i)->GetSpellProto()->SpellClassSet == m_spellInfo->SpellClassSet)
                {
                    if (m_spellInfo->IsFitToFamilyMask((*i)->GetSpellProto()->SpellClassMask))
                    {
                        if (CompareAuraRanks(m_spellInfo->ID, (*i)->GetSpellProto()->ID) < 0)
                        {
                            return SPELL_FAILED_MORE_POWERFUL_SPELL_ACTIVE;
                        }
                    }
                }
            }
        }

        if (!(m_spellInfo->SpellClassSet == SPELLFAMILY_WARRIOR && m_spellInfo->SpellClassMask & UI64LIT(0x100000000))) // the Shield Slam does not depend on its dispel effect
        {
            // Fill possible dispel list
            bool isDispell = false;
            bool isEmpty = true;

            // As of Patch 1.10.0, dispel effects now check if there is something to dispel first
            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                // Dispell Magic
                switch (m_spellInfo->Effect[i])
                {
                    case SPELL_EFFECT_DISPEL:
                    {
                        // It is a dispell spell
                        isDispell = true;

                        // Create dispel mask by dispel type
                        uint32 dispel_type = m_spellInfo->EffectMiscValue[i];
                        uint32 dispelMask = GetDispellMask(DispelType(dispel_type));
                        Unit::SpellAuraHolderMap const& auras = target->GetSpellAuraHolderMap();
                        for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                        {
                            SpellAuraHolder* holder = itr->second;
                            uint32 disp = (1 << holder->GetSpellProto()->DispelType);
                            if (disp & dispelMask)
                            {
                                if (holder->GetSpellProto()->DispelType == DISPEL_MAGIC)
                                {
                                    bool positive = true;
                                    if (!holder->IsPositive())
                                    {
                                        positive = false;
                                    }
                                    else
                                    {
                                        positive = (holder->GetSpellProto()->AttributesEx & SPELL_ATTR_EX_CANT_BE_REFLECTED) == 0;
                                    }

                                    // do not remove positive auras if friendly target
                                    //               negative auras if non-friendly target
                                    if (positive == target->IsFriendlyTo(m_caster))
                                    {
                                        continue;
                                    }
                                }

                                isEmpty = false;
                                break;
                            }
                        }
                        break;
                    }
                }
            }

            // Ok if exist some buffs for dispel try dispel it
            if (isDispell && isEmpty)
            {
                return SPELL_FAILED_NOTHING_TO_DISPEL;
            }
        }

        if (!m_IsTriggeredSpell && IsDeathOnlySpell(m_spellInfo) && target->IsAlive())
        {
            return SPELL_FAILED_TARGET_NOT_DEAD;
        }

        // totem immunity for channeled spells(needs to be before spell cast)
        // spell attribs for player channeled spells
        if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_CHANNEL_TRACK_TARGET) &&
            target->GetTypeId() == TYPEID_UNIT &&
            ((Creature*)target)->IsTotem())
        {
            return SPELL_FAILED_IMMUNE;
        }

        // Power Infusion: As of patch 1.10, this is no longer usable if the target
        // has Arcane Power aura from mage.
        if (m_spellInfo->ID == 10060)    // 10060 = Power Infusion
        {
            if (target->HasAura(12042))    // 12042 = Arcane Power
            {
                return SPELL_FAILED_MORE_POWERFUL_SPELL_ACTIVE;
            }
        }

        bool non_caster_target = target != m_caster && !IsSpellWithCasterSourceTargetsOnly(m_spellInfo);

        if (non_caster_target)
        {
            // Not allow casting on flying player
            if (target->IsTaxiFlying())
            {
                return SPELL_FAILED_BAD_TARGETS;
            }

            if (!m_IsTriggeredSpell && !DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, NULL, SPELL_DISABLE_LOS) && VMAP::VMapFactory::checkSpellForLoS(m_spellInfo->ID) && !m_caster->IsWithinLOSInMap(target))
            {
                return SPELL_FAILED_LINE_OF_SIGHT;
            }

            // auto selection spell rank implemented in WorldSession::HandleCastSpellOpcode
            // this case can be triggered if rank not found (too low-level target for first rank)
            if (m_caster->GetTypeId() == TYPEID_PLAYER && !m_CastItem && !m_IsTriggeredSpell)
            {
                // spell expected to be auto-downranking in cast handle, so must be same
                if (m_spellInfo != sSpellMgr.SelectAuraRankForLevel(m_spellInfo, target->getLevel()))
                {
                    return SPELL_FAILED_LOWLEVEL;
                }
            }

            if (strict && m_spellInfo->HasAttribute(SPELL_ATTR_EX3_TARGET_ONLY_PLAYER) && target->GetTypeId() != TYPEID_PLAYER && !IsAreaOfEffectSpell(m_spellInfo))
            {
                return SPELL_FAILED_BAD_TARGETS;
            }
        }
        else if (m_caster == target)
        {
            if (m_caster->GetTypeId() == TYPEID_PLAYER && m_caster->IsInWorld())
            {
                // Additional check for some spells
                // If 0 spell effect empty - client not send target data (need use selection)
                // TODO: check it on next client version
                if (m_targets.m_targetMask == TARGET_FLAG_SELF &&
                    m_spellInfo->ImplicitTargetA[EFFECT_INDEX_1] == TARGET_CHAIN_DAMAGE)
                {
                    target = m_caster->GetMap()->GetUnit(((Player*)m_caster)->GetSelectionGuid());
                    if (!target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    // Arcane Missile self cast forbidden
                    if (m_spellInfo->SpellClassSet == SPELLFAMILY_MAGE &&
                        m_spellInfo->SpellClassMask & UI64LIT(0x00000800) &&
                        m_caster == target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    // Gnomish Death Ray self cast forbidden
                    if (m_spellInfo->ID == 13278 && m_caster == target)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    m_targets.setUnitTarget(target);
                }
            }

            // Some special spells with non-caster only mode

            // Fire Shield
            if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARLOCK &&
                m_spellInfo->SpellIconID == 16)
            {
                return SPELL_FAILED_BAD_TARGETS;
            }
        }

        // check pet presents
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (m_spellInfo->ImplicitTargetA[j] == TARGET_PET)
            {
                Pet* pet = m_caster->GetPet();
                if (!pet)
                {
                    if (m_triggeredByAuraSpell)             // not report pet not existence for triggered spells
                    {
                        return SPELL_FAILED_DONT_REPORT;
                    }
                    else
                    {
                        return SPELL_FAILED_NO_PET;
                    }
                }
                else if (!pet->IsAlive())
                {
                    return SPELL_FAILED_TARGETS_DEAD;
                }
                break;
            }
        }

        // check creature type
        // ignore self casts (including area casts when caster selected as target)
        if (non_caster_target)
        {
            if (!CheckTargetCreatureType(target))
            {
                if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    return SPELL_FAILED_TARGET_IS_PLAYER;
                }
                else
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }

            // simple cases
            bool explicit_target_mode = false;
            bool target_hostile = false;
            bool target_hostile_checked = false;
            bool target_friendly = false;
            bool target_friendly_checked = false;
            for (int k = 0; k < MAX_EFFECT_INDEX;  ++k)
            {
                if (IsExplicitPositiveTarget(m_spellInfo->ImplicitTargetA[k]))
                {
                    if (!target_hostile_checked)
                    {
                        target_hostile_checked = true;
                        target_hostile = m_caster->IsHostileTo(target);
                    }

                    if (target_hostile)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    explicit_target_mode = true;
                }
                else if (IsExplicitNegativeTarget(m_spellInfo->ImplicitTargetA[k]))
                {
                    if (!target_friendly_checked)
                    {
                        target_friendly_checked = true;
                        target_friendly = m_caster->IsFriendlyTo(target);
                    }

                    if (target_friendly)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }

                    explicit_target_mode = true;
                }
            }
            // TODO: this check can be applied and for player to prevent cheating when IsPositiveSpell will return always correct result.
            // check target for pet/charmed casts (not self targeted), self targeted cast used for area effects and etc
            if (!explicit_target_mode && m_caster->GetTypeId() == TYPEID_UNIT && m_caster->GetCharmerOrOwnerGuid())
            {
                // check correctness positive/negative cast target (pet cast real check and cheating check)
                if (IsPositiveSpell(m_spellInfo->ID))
                {
                    if (!target_hostile_checked)
                    {
                        target_hostile = m_caster->IsHostileTo(target);
                    }

                    if (target_hostile)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
                else
                {
                    if (!target_friendly_checked)
                    {
                        target_friendly = m_caster->IsFriendlyTo(target);
                    }

                    if (target_friendly)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
            }
        }

        if (IsPositiveSpell(m_spellInfo->ID))
        {
            if (target->IsImmuneToSpell(m_spellInfo, target == m_caster))
            {
                return SPELL_FAILED_TARGET_AURASTATE;
            }
        }

        // Must be behind the target.
        if (m_spellInfo->AttributesExB == SPELL_ATTR_EX2_FACING_TARGETS_BACK && (m_spellInfo->HasAttribute(SPELL_ATTR_EX_FACING_TARGET) && target->HasInArc(M_PI_F, m_caster)))
        {
            SendInterrupted(SPELL_FAILED_NOT_BEHIND);
            return SPELL_FAILED_NOT_BEHIND;
        }

        // Target must be facing you.
        if ((m_spellInfo->Attributes == (SPELL_ATTR_ABILITY | SPELL_ATTR_NOT_SHAPESHIFT | SPELL_ATTR_DONT_AFFECT_SHEATH_STATE | SPELL_ATTR_STOP_ATTACK_TARGET)) && !target->HasInArc(M_PI_F, m_caster))
        {
            SendInterrupted(SPELL_FAILED_NOT_INFRONT);
            return SPELL_FAILED_NOT_INFRONT;
        }

        // check if target is in combat
        if (non_caster_target && m_spellInfo->HasAttribute(SPELL_ATTR_EX_NOT_IN_COMBAT_TARGET) && target->IsInCombat())
        {
            return SPELL_FAILED_TARGET_AFFECTING_COMBAT;
        }

        // check if target is affected by Spirit of Redemption (Aura: 27827)
        if (target->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            return SPELL_FAILED_BAD_TARGETS;
        }

    }
    // zone check
    uint32 zone, area;
    m_caster->GetZoneAndAreaId(zone, area);

    SpellCastResult locRes = sSpellMgr.GetSpellAllowedInLocationError(m_spellInfo, m_caster->GetMapId(), zone, area,
        m_caster->GetCharmerOrOwnerPlayerOrPlayerItself());
    if (locRes != SPELL_CAST_OK)
    {
        return locRes;
    }

    // not let players cast spells at mount (and let do it to creatures)
    if (m_caster->IsMounted() && m_caster->GetTypeId() == TYPEID_PLAYER && !m_IsTriggeredSpell &&
        !IsPassiveSpell(m_spellInfo) && !m_spellInfo->HasAttribute(SPELL_ATTR_CASTABLE_WHILE_MOUNTED))
    {
        if (m_caster->IsTaxiFlying())
        {
            return SPELL_FAILED_NOT_ON_TAXI;
        }
        else
        {
            return SPELL_FAILED_NOT_MOUNTED;
        }
    }

    // always (except passive spells) check items (focus object can be required for any type casts)
    if (!IsPassiveSpell(m_spellInfo))
    {
        SpellCastResult castResult = CheckItems();
        if (castResult != SPELL_CAST_OK)
        {
            return castResult;
        }
    }

    // Database based targets from spell_target_script
    if (m_UniqueTargetInfo.empty())                         // skip second CheckCast apply (for delayed spells for example)
    {
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT ||
                m_spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT ||
                m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT_COORDINATES ||
                m_spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT_COORDINATES ||
                m_spellInfo->ImplicitTargetA[j] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                m_spellInfo->ImplicitTargetB[j] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
            {
                SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->ID);

                if (bounds.first == bounds.second)
                {
                    if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT || m_spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT)
                    {
                        sLog.outErrorDb("Spell entry %u, effect %i has EffectImplicitTargetA/EffectImplicitTargetB = TARGET_SCRIPT, but creature are not defined in `spell_script_target`", m_spellInfo->ID, j);
                    }

                    if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT_COORDINATES || m_spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT_COORDINATES)
                    {
                        sLog.outErrorDb("Spell entry %u, effect %i has EffectImplicitTargetA/EffectImplicitTargetB = TARGET_SCRIPT_COORDINATES, but gameobject or creature are not defined in `spell_script_target`", m_spellInfo->ID, j);
                    }

                    if (m_spellInfo->ImplicitTargetA[j] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT || m_spellInfo->ImplicitTargetB[j] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                    {
                        sLog.outErrorDb("Spell entry %u, effect %i has EffectImplicitTargetA/EffectImplicitTargetB = TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT, but gameobject are not defined in `spell_script_target`", m_spellInfo->ID, j);
                    }
                }

                SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex);
                float range = GetSpellMaxRange(srange);

                // override range with default when it's not provided
                if (!range)
                {
                    range = m_caster->GetMap()->IsDungeon() ? DEFAULT_VISIBILITY_INSTANCE : DEFAULT_VISIBILITY_DISTANCE;
                }

                Creature* targetExplicit = NULL;            // used for cases where a target is provided (by script for example)
                Creature* creatureScriptTarget = NULL;
                GameObject* goScriptTarget = NULL;

                for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                {
                    if (i_spellST->CanNotHitWithSpellEffect(SpellEffectIndex(j)))
                    {
                        continue;
                    }

                    switch (i_spellST->type)
                    {
                        case SPELL_TARGET_TYPE_GAMEOBJECT:
                        {
                            GameObject* p_GameObject = NULL;

                            if (i_spellST->targetEntry)
                            {
                                MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*m_caster, i_spellST->targetEntry, range);
                                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(p_GameObject, go_check);
                                Cell::VisitGridObjects(m_caster, checker, range);

                                if (p_GameObject)
                                {
                                    // remember found target and range, next attempt will find more near target with another entry
                                    creatureScriptTarget = NULL;
                                    goScriptTarget = p_GameObject;
                                    range = go_check.GetLastRange();
                                }
                            }
                            else if (focusObject)           // Focus Object
                            {
                                float frange = m_caster->GetDistance(focusObject);
                                if (range >= frange)
                                {
                                    creatureScriptTarget = NULL;
                                    goScriptTarget = focusObject;
                                    range = frange;
                                }
                            }
                            break;
                        }
                        case SPELL_TARGET_TYPE_CREATURE:
                        case SPELL_TARGET_TYPE_DEAD:
                        default:
                        {
                            Creature* p_Creature = NULL;

                            // check if explicit target is provided and check it up against database valid target entry/state
                            if (Unit* pTarget = m_targets.getUnitTarget())
                            {
                                if (pTarget->GetTypeId() == TYPEID_UNIT && pTarget->GetEntry() == i_spellST->targetEntry)
                                {
                                    if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)pTarget)->IsCorpse())
                                    {
                                        // always use spellMaxRange, in case GetLastRange returned different in a previous pass
                                        if (pTarget->IsWithinDistInMap(m_caster, GetSpellMaxRange(srange)))
                                        {
                                            targetExplicit = (Creature*)pTarget;
                                        }
                                    }
                                    else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && pTarget->IsAlive())
                                    {
                                        // always use spellMaxRange, in case GetLastRange returned different in a previous pass
                                        if (pTarget->IsWithinDistInMap(m_caster, GetSpellMaxRange(srange)))
                                        {
                                            targetExplicit = (Creature*)pTarget;
                                        }
                                    }
                                }
                            }

                            // no target provided or it was not valid, so use closest in range
                            if (!targetExplicit)
                            {
                                MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*m_caster, i_spellST->targetEntry, i_spellST->type != SPELL_TARGET_TYPE_DEAD, i_spellST->type == SPELL_TARGET_TYPE_DEAD, range);
                                MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(p_Creature, u_check);

                                // Visit all, need to find also Pet* objects
                                Cell::VisitAllObjects(m_caster, searcher, range);

                                range = u_check.GetLastRange();
                            }

                            // always prefer provided target if it's valid
                            if (targetExplicit)
                            {
                                creatureScriptTarget = targetExplicit;
                            }
                            else if (p_Creature)
                            {
                                creatureScriptTarget = p_Creature;
                            }

                            if (creatureScriptTarget)
                            {
                                goScriptTarget = NULL;
                            }

                            break;
                        }
                    }
                }

                if (creatureScriptTarget)
                {
                    // store coordinates for TARGET_SCRIPT_COORDINATES
                    if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT_COORDINATES ||
                        m_spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT_COORDINATES)
                    {
                        m_targets.setDestination(creatureScriptTarget->GetPositionX(), creatureScriptTarget->GetPositionY(), creatureScriptTarget->GetPositionZ());

                        if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT_COORDINATES && m_spellInfo->Effect[j] != SPELL_EFFECT_PERSISTENT_AREA_AURA)
                        {
                            AddUnitTarget(creatureScriptTarget, SpellEffectIndex(j));
                        }
                    }
                    // store explicit target for TARGET_SCRIPT
                    else
                    {
                        if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT ||
                            m_spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT)
                        {
                            AddUnitTarget(creatureScriptTarget, SpellEffectIndex(j));
                        }
                    }
                }
                else if (goScriptTarget)
                {
                    // store coordinates for TARGET_SCRIPT_COORDINATES
                    if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT_COORDINATES ||
                        m_spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT_COORDINATES)
                    {
                        m_targets.setDestination(goScriptTarget->GetPositionX(), goScriptTarget->GetPositionY(), goScriptTarget->GetPositionZ());

                        if (m_spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT_COORDINATES && m_spellInfo->Effect[j] != SPELL_EFFECT_PERSISTENT_AREA_AURA)
                        {
                            AddGOTarget(goScriptTarget, SpellEffectIndex(j));
                        }
                    }
                    // store explicit target for TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT
                    else
                    {
                        if (m_spellInfo->ImplicitTargetA[j] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                            m_spellInfo->ImplicitTargetB[j] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                        {
                            AddGOTarget(goScriptTarget, SpellEffectIndex(j));
                        }
                    }
                }
                // Missing DB Entry or targets for this spellEffect.
                else
                {
                    /** For TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT makes DB targets optional not required for now
                     * TODO: Makes more research for this target type
                     */
                    if (m_spellInfo->ImplicitTargetA[j] != TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT)
                    {
                        // not report target not existence for triggered spells
                        if (m_triggeredByAuraSpell || m_IsTriggeredSpell)
                        {
                            return SPELL_FAILED_DONT_REPORT;
                        }
                        else
                        {
                            return SPELL_FAILED_BAD_TARGETS;
                        }
                    }
                }
            }
        }
    }

    if (!m_IsTriggeredSpell)
    {
        if (!m_triggeredByAuraSpell)
        {
            SpellCastResult castResult = CheckRange(strict);
            if (castResult != SPELL_CAST_OK)
            {
                return castResult;
            }
            if (Unit* target = m_targets.getUnitTarget())
            {
                if (m_caster->GetTypeId() == TYPEID_PLAYER &&
                    (sSpellMgr.GetSpellFacingFlag(m_spellInfo->ID) & SPELL_FACING_FLAG_INFRONT) &&
                    !m_caster->HasInArc(M_PI_F, target))
                {
                    return SPELL_FAILED_UNIT_NOT_INFRONT;
                }
            }
        }
    }

    if (!m_IsTriggeredSpell)                                // triggered spell does not use power
    {
        SpellCastResult castResult = CheckPower();
        if (castResult != SPELL_CAST_OK)
        {
            return castResult;
        }
    }

    if (!m_IsTriggeredSpell)                                // triggered spell not affected by stun/etc
    {
        SpellCastResult castResult = CheckCasterAuras();
        if (castResult != SPELL_CAST_OK)
        {
            return castResult;
        }
    }

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        // for effects of spells that have only one target
        switch (m_spellInfo->Effect[i])
        {
            case SPELL_EFFECT_DUMMY:
            {
                if (m_spellInfo->SpellIconID == 1648)       // Execute
                {
                    if (!m_targets.getUnitTarget() || m_targets.getUnitTarget()->GetHealth() > m_targets.getUnitTarget()->GetMaxHealth() * 0.2)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
                else if (m_spellInfo->SpellIconID == 156)   // Holy Shock
                {
                    // spell different for friends and enemies
                    // hart version required facing
                    if (m_targets.getUnitTarget() && !m_caster->IsFriendlyTo(m_targets.getUnitTarget()) && !m_caster->HasInArc(M_PI_F, m_targets.getUnitTarget()))
                    {
                        return SPELL_FAILED_UNIT_NOT_INFRONT;
                    }
                }
                break;
            }
            case SPELL_EFFECT_DISTRACT:                     // All nearby enemies must not be in combat
            {
                if (m_targets.m_targetMask & (TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_SOURCE_LOCATION))
                {
                    UnitList targetsCombat;
                    float radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[i]));

                    FillAreaTargets(targetsCombat, radius, PUSH_DEST_CENTER, SPELL_TARGETS_AOE_DAMAGE);

                    if (targetsCombat.empty())
                    {
                        break;
                    }

                    for (UnitList::iterator itr = targetsCombat.begin(); itr != targetsCombat.end(); ++itr)
                    {
                        if ((*itr)->IsInCombat())
                        {
                            return SPELL_FAILED_TARGET_IN_COMBAT;
                        }
                    }
                }
                break;
            }
            case SPELL_EFFECT_SCHOOL_DAMAGE:
            {
                // Hammer of Wrath
                if (m_spellInfo->SpellVisualID == 7250)
                {
                    if (!m_targets.getUnitTarget())
                    {
                        return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                    }

                    if (m_targets.getUnitTarget()->GetHealth() > m_targets.getUnitTarget()->GetMaxHealth() * 0.2)
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
                // Conflagrate
                else if (m_spellInfo->SpellClassSet == SPELLFAMILY_WARLOCK && m_spellInfo->SpellClassMask & UI64LIT(0x0000000000000200))
                {
                    if (!m_targets.getUnitTarget())
                    {
                        return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                    }

                    // for caster applied auras only
                    bool found = false;
                    Unit::AuraList const& mPeriodic = m_targets.getUnitTarget()->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                    for (Unit::AuraList::const_iterator i = mPeriodic.begin(); i != mPeriodic.end(); ++i)
                    {
                        if ((*i)->GetSpellProto()->SpellClassSet == SPELLFAMILY_WARLOCK &&
                            (*i)->GetCasterGuid() == m_caster->GetObjectGuid() &&
                            // Immolate
                            ((*i)->GetSpellProto()->SpellClassMask & UI64LIT(0x0000000000000004)))
                        {
                            found = true;
                            break;
                        }
                    }

                    if (!found)
                    {
                        return SPELL_FAILED_TARGET_AURASTATE;
                    }
                }
                break;
            }
            case SPELL_EFFECT_TAMECREATURE:
            {
                if (m_triggeredBySpellInfo == NULL)
                {
                    SpellCastResult castResult = CanTameUnit(true);
                    if (castResult != SPELL_CAST_OK)
                    {
                        return castResult;
                    }
                }
                break;
            }
            case SPELL_EFFECT_LEARN_SPELL:
            {
                if (m_spellInfo->ImplicitTargetA[i] != TARGET_PET)
                {
                    break;
                }

                Pet* pet = m_caster->GetPet();

                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                SpellEntry const* learn_spellproto = sSpellStore.LookupEntry(m_spellInfo->EffectTriggerSpell[i]);

                if (!learn_spellproto)
                {
                    return SPELL_FAILED_NOT_KNOWN;
                }

                if (!pet->CanTakeMoreActiveSpells(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TOO_MANY_SKILLS;
                }

                if (m_spellInfo->SpellLevel > pet->getLevel())
                {
                    return SPELL_FAILED_LOWLEVEL;
                }

                if (!pet->HasTPForSpell(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TRAINING_POINTS;
                }

                break;
            }
            case SPELL_EFFECT_LEARN_PET_SPELL:
            {
                Pet* pet = m_caster->GetPet();

                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                SpellEntry const* learn_spellproto = sSpellStore.LookupEntry(m_spellInfo->EffectTriggerSpell[i]);

                if (!learn_spellproto)
                {
                    return SPELL_FAILED_NOT_KNOWN;
                }

                if (!pet->CanTakeMoreActiveSpells(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TOO_MANY_SKILLS;
                }

                if (m_spellInfo->SpellLevel > pet->getLevel())
                {
                    return SPELL_FAILED_LOWLEVEL;
                }

                if (!pet->HasTPForSpell(learn_spellproto->ID))
                {
                    return SPELL_FAILED_TRAINING_POINTS;
                }

                break;
            }
            case SPELL_EFFECT_FEED_PET:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                Item* foodItem = m_targets.getItemTarget();
                if (!foodItem)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                Pet* pet = m_caster->GetPet();

                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                if (!pet->HaveInDiet(foodItem->GetProto()))
                {
                    return SPELL_FAILED_WRONG_PET_FOOD;
                }

                if (!pet->GetCurrentFoodBenefitLevel(foodItem->GetProto()->ItemLevel))
                {
                    return SPELL_FAILED_FOOD_LOWLEVEL;
                }

                if (pet->IsInCombat())
                {
                    return SPELL_FAILED_AFFECTING_COMBAT;
                }

                break;
            }
            case SPELL_EFFECT_POWER_BURN:
            case SPELL_EFFECT_POWER_DRAIN:
            {
                // Can be area effect, Check only for players and not check if target - caster (spell can have multiply drain/burn effects)
                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                {
                    if (Unit* target = m_targets.getUnitTarget())
                    {
                        if (target != m_caster && int32(target->GetPowerType()) != m_spellInfo->EffectMiscValue[i])
                        {
                            return SPELL_FAILED_BAD_TARGETS;
                        }
                    }
                }
                break;
            }
            case SPELL_EFFECT_CHARGE:
            {
                if (m_caster->hasUnitState(UNIT_STAT_ROOT))
                {
                    return SPELL_FAILED_ROOTED;
                }

                break;
            }
            case SPELL_EFFECT_SKINNING:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER || !m_targets.getUnitTarget() || m_targets.getUnitTarget()->GetTypeId() != TYPEID_UNIT)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (!m_targets.getUnitTarget()->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE))
                {
                    return SPELL_FAILED_TARGET_UNSKINNABLE;
                }

                Creature* creature = (Creature*)m_targets.getUnitTarget();
                if (creature->GetCreatureType() != CREATURE_TYPE_CRITTER && (!creature->lootForBody || creature->lootForSkin || !creature->loot.empty()))
                {
                    return SPELL_FAILED_TARGET_NOT_LOOTED;
                }

                uint32 skill = creature->GetCreatureInfo()->GetRequiredLootSkill();

                int32 skillValue = ((Player*)m_caster)->GetSkillValue(skill);
                int32 TargetLevel = m_targets.getUnitTarget()->getLevel();
                int32 ReqValue = (skillValue < 100 ? (TargetLevel - 10) * 10 : TargetLevel * 5);
                if (ReqValue > skillValue)
                {
                    return SPELL_FAILED_SKILL_NOT_HIGH_ENOUGH;
                }

                // chance for fail at orange skinning attempt
                if (m_spellState != SPELL_STATE_CREATED &&
                    skillValue < sWorld.GetConfigMaxSkillValue() &&
                    (ReqValue < 0 ? 0 : ReqValue) > irand(skillValue - 25, skillValue + 37))
                {
                    return SPELL_FAILED_TRY_AGAIN;
                }

                break;
            }
            case SPELL_EFFECT_OPEN_LOCK_ITEM:
            case SPELL_EFFECT_OPEN_LOCK:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER) // only players can open locks, gather etc.
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                // we need a go target in case of TARGET_GAMEOBJECT (for other targets acceptable GO and items)
                if (m_spellInfo->ImplicitTargetA[i] == TARGET_GAMEOBJECT)
                {
                    if (!m_targets.getGOTarget())
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }

                // get the lock entry
                uint32 lockId;
                if (GameObject* go = m_targets.getGOTarget())
                {
                    // Prevent opening two times a chest in same time.
                    if (go->GetGoType() == GAMEOBJECT_TYPE_CHEST && go->GetGoState() == GO_STATE_ACTIVE)
                    {
                        return SPELL_FAILED_CHEST_IN_USE;
                    }

                    // In BattleGround players can use only flags and banners
                    if (((Player*)m_caster)->InBattleGround() &&
                        !((Player*)m_caster)->CanUseBattleGroundObject())
                    {
                        return SPELL_FAILED_TRY_AGAIN;
                    }

                    lockId = go->GetGOInfo()->GetLockId();
                    if (!lockId)
                    {
                        return SPELL_FAILED_ALREADY_OPEN;
                    }

                    // Is the lock within the spell max range?
                    if (!IsLockInRange(go) && go->GetGoType() == GAMEOBJECT_TYPE_TRAP)
                    {
                        return SPELL_FAILED_OUT_OF_RANGE;
                    }
                }
                else if (Item* item = m_targets.getItemTarget())
                {
                    // not own (trade?)
                    if (item->GetOwner() != m_caster)
                    {
                        return SPELL_FAILED_ITEM_GONE;
                    }

                    lockId = item->GetProto()->LockID;

                    // if already unlocked
                    if (!lockId || item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_UNLOCKED))
                    {
                        return SPELL_FAILED_ALREADY_OPEN;
                    }
                }
                else
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                SkillType skillId = SKILL_NONE;
                int32 reqSkillValue = 0;
                int32 skillValue = 0;

                // check lock compatibility
                SpellCastResult res = CanOpenLock(SpellEffectIndex(i), lockId, skillId, reqSkillValue, skillValue);
                if (res != SPELL_CAST_OK)
                {
                    return res;
                }

                // chance for fail at orange mining/herb/LockPicking gathering attempt
                // second check prevent fail at rechecks
                // Check must be executed at the end of  the cast.
                if (m_spellState != SPELL_STATE_CREATED && skillId != SKILL_NONE)
                {
                    bool canFailAtMax = skillId != SKILL_HERBALISM && skillId != SKILL_MINING;

                    // chance for failure in orange gather / lockpick (gathering skill can't fail at maxskill)
                    if ((canFailAtMax || skillValue < sWorld.GetConfigMaxSkillValue()) && reqSkillValue > irand(skillValue - 25, skillValue + 37))
                    {
                        return SPELL_FAILED_TRY_AGAIN;
                    }
                }
                break;
            }
            case SPELL_EFFECT_SUMMON_DEAD_PET:
            {
                Creature* pet = m_caster->GetPet();

                if (pet && pet->IsAlive())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (!pet)
                {
                    if (Player* player = m_caster->ToPlayer())
                    {
                        PetDatabaseStatus status = Pet::GetStatusFromDB(player);
                        if (status == PET_DB_NO_PET)
                        {
                            return SPELL_FAILED_NO_PET;
                        }
                        else if (status == PET_DB_ALIVE)
                        {
                            return SPELL_FAILED_TARGET_NOT_DEAD;
                        }
                    }
                    else
                    {
                        return SPELL_FAILED_NO_PET;
                    }
                }

                break;
            }
            // Don't make this check for SPELL_EFFECT_SUMMON_CRITTER, SPELL_EFFECT_SUMMON_WILD or SPELL_EFFECT_SUMMON_GUARDIAN.
            // These won't show up in m_caster->GetPetGUID()
            case SPELL_EFFECT_SUMMON:
            case SPELL_EFFECT_SUMMON_POSSESSED:
            case SPELL_EFFECT_SUMMON_PHANTASM:
            case SPELL_EFFECT_SUMMON_DEMON:
            {
                if (m_caster->GetPetGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                break;
            }
            case SPELL_EFFECT_SUMMON_PET:
            {
                Player* plr = m_caster->ToPlayer();
                if (m_caster->GetPetGuid())                 // let warlock do a replacement summon
                {
                    if (plr && m_caster->getClass() != CLASS_WARLOCK)
                    {
                        return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                    }
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (plr)
                {
                    PetDatabaseStatus status = Pet::GetStatusFromDB(plr);
                    if (status == PET_DB_DEAD)
                    {
                        return SPELL_FAILED_TARGETS_DEAD;
                    }
                    else if ((plr->getClass() == CLASS_HUNTER) && (status == PET_DB_NO_PET))
                    {
                        return SPELL_FAILED_NO_PET;
                    }
                }
                break;
            }
            case SPELL_EFFECT_SUMMON_PLAYER:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
                if (!((Player*)m_caster)->GetSelectionGuid())
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                Player* target = sObjectMgr.GetPlayer(((Player*)m_caster)->GetSelectionGuid());
                if (!target || ((Player*)m_caster) == target || !target->IsInSameRaidWith((Player*)m_caster))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                // check if our map is dungeon
                if (sMapStore.LookupEntry(m_caster->GetMapId())->IsDungeon())
                {
                    InstanceTemplate const* instance = ObjectMgr::GetInstanceTemplate(m_caster->GetMapId());
                    if (m_caster->GetMap() != target->GetMap())
                    {
                        return SPELL_FAILED_TARGET_NOT_IN_INSTANCE;
                    }
                    if (instance->levelMin > target->getLevel())
                    {
                        return SPELL_FAILED_LOWLEVEL;
                    }
                    if (instance->levelMax && instance->levelMax < target->getLevel())
                    {
                        return SPELL_FAILED_HIGHLEVEL;
                    }
                }
                break;
            }
            case SPELL_EFFECT_LEAP:
            case SPELL_EFFECT_TELEPORT_UNITS_FACE_CASTER:
            {
                if (!m_caster || m_caster->IsTaxiFlying())
                {
                    return SPELL_FAILED_NOT_ON_TAXI;
                }

                // Blink has leap first and then removing of auras with root effect
                // need further research with this
                if (m_spellInfo->Effect[i] != SPELL_EFFECT_LEAP)
                {
                    if (m_caster->hasUnitState(UNIT_STAT_ROOT))
                    {
                        return SPELL_FAILED_ROOTED;
                    }
                }

                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                {
                    if (((Player*)m_caster)->HasMovementFlag(MOVEFLAG_ONTRANSPORT))
                    {
                        return SPELL_FAILED_NOT_ON_TRANSPORT;
                    }

                    // not allow use this effect at battleground until battleground start
                    if (BattleGround const* bg = ((Player*)m_caster)->GetBattleGround())
                    {
                        if (bg->GetStatus() != STATUS_IN_PROGRESS)
                        {
                            return SPELL_FAILED_TRY_AGAIN;
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
    }

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        // Do not check in case of junk in DBC
        if (!IsAuraApplyEffect(m_spellInfo, SpellEffectIndex(i)))
        {
            continue;
        }

        // Possible Unit-target for the spell
        Unit* expectedTarget = m_caster->GetMap() ? m_caster->GetMap()->GetUnit(GetPrefilledOrUnitTargetGuid(SpellEffectIndex(i))) : NULL;

        switch (m_spellInfo->EffectAura[i])
        {
            case SPELL_AURA_MOD_POSSESS:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return SPELL_FAILED_UNKNOWN;
                }

                if (expectedTarget == m_caster)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (m_caster->GetPetGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (m_caster->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (expectedTarget->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (int32(expectedTarget->getLevel()) > CalculateDamage(SpellEffectIndex(i), expectedTarget))
                {
                    return SPELL_FAILED_HIGHLEVEL;
                }
                break;
            }
            case SPELL_AURA_MOD_CHARM:
            {
                if (expectedTarget == m_caster)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }

                if (m_caster->GetPetGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_SUMMON;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (m_caster->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (expectedTarget->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                if (int32(expectedTarget->getLevel()) > CalculateDamage(SpellEffectIndex(i), expectedTarget))
                {
                    return SPELL_FAILED_HIGHLEVEL;
                }
                break;
            }
            case SPELL_AURA_MOD_POSSESS_PET:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return SPELL_FAILED_UNKNOWN;
                }

                if (m_caster->GetCharmGuid())
                {
                    return SPELL_FAILED_ALREADY_HAVE_CHARM;
                }

                if (m_caster->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }

                Pet* pet = m_caster->GetPet();
                if (!pet)
                {
                    return SPELL_FAILED_NO_PET;
                }

                if (pet->GetCharmerGuid())
                {
                    return SPELL_FAILED_CHARMED;
                }
                break;
            }
            case SPELL_AURA_MOUNTED:
            {
                if (m_caster->IsInWater())
                {
                    return SPELL_FAILED_ONLY_ABOVEWATER;
                }

                if (m_caster->GetTypeId() == TYPEID_PLAYER && ((Player*)m_caster)->GetTransport())
                {
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;
                }

                /// Specific case for Temple of Ahn'Qiraj mounts as they are usable only in AQ40 and are the only mounts allowed here
                /// TBC and above handle this by using m_spellInfo->AreaId
                bool isAQ40Mounted = false;

                switch (m_spellInfo->ID)
                {
                    case 25863:    // spell used by ingame item for Black Qiraji mount (legendary reward)
                    case 26655:    // spells also related to Black Qiraji mount but use/trigger unknown
                    case 26656:
                    case 31700:
                        if (m_caster->GetMapId() == 531)
                        {
                            isAQ40Mounted = true;
                        }
                        break;
                    case 25953:    // spells of the 4 regular AQ40 mounts
                    case 26054:
                    case 26055:
                    case 26056:
                        if (m_caster->GetMapId() == 531)
                        {
                            isAQ40Mounted = true;
                            break;
                        }
                        else
                        {
                            return SPELL_FAILED_NOT_HERE;
                        }
                    default:
                        break;
                }

                // Ignore map check if spell have AreaId. AreaId already checked and this prevent special mount spells
                if (!isAQ40Mounted && m_caster->GetTypeId() == TYPEID_PLAYER && !sMapStore.LookupEntry(m_caster->GetMapId())->IsMountAllowed() && !m_IsTriggeredSpell) //[-ZERO] && !m_spellInfo->AreaId)
                {
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;
                }

                if (m_caster->GetAreaId() == 35)
                {
                    return SPELL_FAILED_NO_MOUNTS_ALLOWED;
                }

                if (m_caster->IsInDisallowedMountForm())
                {
                    return SPELL_FAILED_NOT_SHAPESHIFT;
                }
                break;
            }
            case SPELL_AURA_RANGED_ATTACK_POWER_ATTACKER_BONUS:
            {
                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                // can be casted at non-friendly unit or own pet/charm
                if (m_caster->IsFriendlyTo(expectedTarget))
                {
                    return SPELL_FAILED_TARGET_FRIENDLY;
                }
                break;
            }
            case SPELL_AURA_PERIODIC_MANA_LEECH:
            {
                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (m_caster->GetTypeId() != TYPEID_PLAYER || m_CastItem)
                {
                    break;
                }

                if (expectedTarget->GetPowerType() != POWER_MANA)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
                break;
            }
            case SPELL_AURA_WATER_WALK:
            {
                if (!expectedTarget)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }

                if (expectedTarget->GetTypeId() == TYPEID_PLAYER)
                {
                    Player const* player = static_cast<Player const*>(expectedTarget);

                    // Player is not allowed to cast water walk on shapeshifted/mounted player
                    if (player->GetShapeshiftForm() != FORM_NONE || player->IsMounted())
                    {
                        return SPELL_FAILED_BAD_TARGETS;
                    }
                }
            }
            default:
                break;
        }
    }

    // check trade slot case (last, for allow catch any another cast problems)
    if (m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
        {
            return SPELL_FAILED_NOT_TRADING;
        }

        Player* pCaster = ((Player*)m_caster);
        TradeData* my_trade = pCaster->GetTradeData();

        if (!my_trade)
        {
            return SPELL_FAILED_NOT_TRADING;
        }

        TradeSlots slot = TradeSlots(m_targets.getItemTargetGuid().GetRawValue());
        if (slot != TRADE_SLOT_NONTRADED)
        {
            return SPELL_FAILED_ITEM_NOT_READY;
        }

        // if trade not complete then remember it in trade data
        if (!my_trade->IsInAcceptProcess())
        {
            // Spell will be casted at completing the trade. Silently ignore at this place
            my_trade->SetSpell(m_spellInfo->ID, m_CastItem);
            return SPELL_FAILED_DONT_REPORT;
        }
    }

    // all ok
    return SPELL_CAST_OK;
}

/**
 * @brief Validates whether a pet or charmed unit can cast the spell.
 *
 * @param target An optional explicit target override.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckPetCast(Unit* target)
{
    if (!m_caster->IsAlive())
    {
        return SPELL_FAILED_CASTER_DEAD;
    }

    if (m_caster->IsNonMeleeSpellCasted(false))             // prevent spellcast interruption by another spellcast
    {
        return SPELL_FAILED_SPELL_IN_PROGRESS;
    }
    if (m_caster->IsInCombat() && IsNonCombatSpell(m_spellInfo))
    {
        return SPELL_FAILED_AFFECTING_COMBAT;
    }

    if (m_caster->GetTypeId() == TYPEID_UNIT && (((Creature*)m_caster)->IsPet() || m_caster->IsCharmed()))
    {
        // dead owner (pets still alive when owners ressed?)
        if (m_caster->GetCharmerOrOwner() && !m_caster->GetCharmerOrOwner()->IsAlive())
        {
            return SPELL_FAILED_CASTER_DEAD;
        }

        if (!target && m_targets.getUnitTarget())
        {
            target = m_targets.getUnitTarget();
        }

        bool need = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (m_spellInfo->ImplicitTargetA[i] == TARGET_CHAIN_DAMAGE ||
                m_spellInfo->ImplicitTargetA[i] == TARGET_SINGLE_FRIEND ||
                m_spellInfo->ImplicitTargetA[i] == TARGET_SINGLE_FRIEND_2 ||
                m_spellInfo->ImplicitTargetA[i] == TARGET_DUELVSPLAYER ||
                m_spellInfo->ImplicitTargetA[i] == TARGET_SINGLE_PARTY ||
                m_spellInfo->ImplicitTargetA[i] == TARGET_CURRENT_ENEMY_COORDINATES)
            {
                need = true;
                if (!target)
                {
                    return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
                }
                break;
            }
        }
        if (need)
        {
            m_targets.setUnitTarget(target);
        }

        Unit* _target = m_targets.getUnitTarget();

        // for target dead/target not valid
        if (_target)
        {
            if (!_target->IsTargetableForAttack())
            {
                return SPELL_FAILED_BAD_TARGETS;             // guessed error
            }

            if (IsPositiveSpell(m_spellInfo->ID))
            {
                if (m_caster->IsHostileTo(_target))
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }
            else
            {
                bool duelvsplayertar = false;
                for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                {
                    // TARGET_DUELVSPLAYER is positive AND negative
                    duelvsplayertar |= (m_spellInfo->ImplicitTargetA[j] == TARGET_DUELVSPLAYER);
                }
                if (m_caster->IsFriendlyTo(target) && !duelvsplayertar)
                {
                    return SPELL_FAILED_BAD_TARGETS;
                }
            }
        }
        // cooldown
        if (((Creature*)m_caster)->HasSpellCooldown(m_spellInfo->ID))
        {
            return SPELL_FAILED_NOT_READY;
        }
    }

    return CheckCast(true);
}

/**
 * @brief Checks whether active caster auras prevent this spell from being cast.
 *
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckCasterAuras() const
{
    // Flag drop spells totally immuned to caster auras
    // FIXME: find more nice check for all totally immuned spells
    // HasAttribute(SPELL_ATTR_EX3_UNK28) ?
    if (m_spellInfo->ID == 23336 ||                         // Alliance Flag Drop
        m_spellInfo->ID == 23334)                       // Horde Flag Drop
    {
        return SPELL_CAST_OK;
    }

    uint8 school_immune = 0;
    uint32 mechanic_immune = 0;
    uint32 dispel_immune = 0;

    // Check if the spell grants school or mechanic immunity.
    // We use bitmasks so the loop is done only once and not on every aura check below.
    if (m_spellInfo->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (m_spellInfo->EffectAura[i] == SPELL_AURA_SCHOOL_IMMUNITY)
            {
                school_immune |= uint32(m_spellInfo->EffectMiscValue[i]);
            }
            else if (m_spellInfo->EffectAura[i] == SPELL_AURA_MECHANIC_IMMUNITY)
            {
                mechanic_immune |= 1 << uint32(m_spellInfo->EffectMiscValue[i] - 1);
            }
            else if (m_spellInfo->EffectAura[i] == SPELL_AURA_MECHANIC_IMMUNITY_MASK)
            {
                mechanic_immune |= uint32(m_spellInfo->EffectMiscValue[i]);
            }
            else if (m_spellInfo->EffectAura[i] == SPELL_AURA_DISPEL_IMMUNITY)
            {
                dispel_immune |= GetDispellMask(DispelType(m_spellInfo->EffectMiscValue[i]));
            }
        }
    }

    // Check whether the cast should be prevented by any state you might have.
    SpellCastResult prevented_reason = SPELL_CAST_OK;
    // Have to check if there is a stun aura. Otherwise will have problems with ghost aura apply while logging out
    uint32 unitflag = m_caster->GetUInt32Value(UNIT_FIELD_FLAGS);     // Get unit state
    /** [-ZERO]
     * if (unitflag & UNIT_FLAG_STUNNED && !(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_STUNNED))
     *     prevented_reason = SPELL_FAILED_STUNNED;
     * else if (unitflag & UNIT_FLAG_CONFUSED && !(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_CONFUSED))
     * {
     *     prevented_reason = SPELL_FAILED_CONFUSED;
     * }
     * else if (unitflag & UNIT_FLAG_FLEEING && !(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_FEARED))
     * {
     *      prevented_reason = SPELL_FAILED_FLEEING;
     * }
     * else
     */
    if (unitflag & UNIT_FLAG_SILENCED && m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
    {
        prevented_reason = SPELL_FAILED_SILENCED;
    }
    else if (unitflag & UNIT_FLAG_PACIFIED && m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY)
    {
        prevented_reason = SPELL_FAILED_PACIFIED;
    }

    if (m_caster->IsSchoolLockedOut(GetSpellSchoolMask(m_spellInfo)))
    {
        prevented_reason = SPELL_FAILED_SILENCED;
    }

    // Attr must make flag drop spell totally immune from all effects
    if (prevented_reason != SPELL_CAST_OK)
    {
        if (school_immune || mechanic_immune || dispel_immune)
        {
            // Checking auras is needed now, because you are prevented by some state but the spell grants immunity.
            Unit::SpellAuraHolderMap const& auras = m_caster->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
            {
                SpellAuraHolder* holder = itr->second;
                SpellEntry const* pEntry = holder->GetSpellProto();

                if ((GetSpellSchoolMask(pEntry) & school_immune) && !pEntry->HasAttribute(SPELL_ATTR_EX_UNAFFECTED_BY_SCHOOL_IMMUNE))
                {
                    continue;
                }
                if ((1 << (pEntry->DispelType)) & dispel_immune)
                {
                    continue;
                }

                for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i));
                    if (!aura)
                    {
                        continue;
                    }

                    if (GetSpellMechanicMask(pEntry, 1 << i) & mechanic_immune)
                    {
                        continue;
                    }
                    // Make a second check for spell failed so the right SPELL_FAILED message is returned.
                    // That is needed when your casting is prevented by multiple states and you are only immune to some of them.
                    switch (aura->GetModifier()->m_auraname)
                    {
                        /** Zero
                         *  case SPELL_AURA_MOD_STUN:
                         *     if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_STUNNED))
                         *     {
                         *         return SPELL_FAILED_STUNNED;
                         *     }
                         *     break;
                         *  case SPELL_AURA_MOD_CONFUSE:
                         *     if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_CONFUSED))
                         *     {
                         *         return SPELL_FAILED_CONFUSED;
                         *     }
                         *     break;
                         *  case SPELL_AURA_MOD_FEAR:
                         *     if (!(m_spellInfo->AttributesEx5 & SPELL_ATTR_EX5_USABLE_WHILE_FEARED))
                         *     {
                         *         return SPELL_FAILED_FLEEING;
                         *     }
                         *     break;
                         */
                        case SPELL_AURA_MOD_SILENCE:
                        case SPELL_AURA_MOD_PACIFY:
                        case SPELL_AURA_MOD_PACIFY_SILENCE:
                            if (m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_PACIFY)
                            {
                                return SPELL_FAILED_PACIFIED;
                            }
                            else if (m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
                            {
                                return SPELL_FAILED_SILENCED;
                            }
                            break;
                        default: break;
                    }
                }
            }
        }
        // You are prevented from casting and the spell casted does not grant immunity. Return a failed error.
        else
        {
            return prevented_reason;
        }
    }
    return SPELL_CAST_OK;
}

/**
 * @brief Checks whether the spell can be automatically cast on a target.
 *
 * @param target The target being evaluated.
 * @return True if automatic casting is allowed; otherwise, false.
 */
bool Spell::CanAutoCast(Unit* target)
{
    ObjectGuid targetguid = target->GetObjectGuid();

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (m_spellInfo->Effect[j] == SPELL_EFFECT_APPLY_AURA)
        {
            if (m_spellInfo->CumulativeAura <= 1)
            {
                if (target->HasAura(m_spellInfo->ID, SpellEffectIndex(j)))
                {
                    return false;
                }
            }
            else
            {
                if (Aura* aura = target->GetAura(m_spellInfo->ID, SpellEffectIndex(j)))
                {
                    if (aura->GetStackAmount() >= m_spellInfo->CumulativeAura)
                    {
                        return false;
                    }
                }
            }
        }
        else if (IsAreaAuraEffect(m_spellInfo->Effect[j]))
        {
            if (target->HasAura(m_spellInfo->ID, SpellEffectIndex(j)))
            {
                return false;
            }
        }
    }

    SpellCastResult result = CheckPetCast(target);

    if (result == SPELL_CAST_OK || result == SPELL_FAILED_UNIT_NOT_INFRONT)
    {
        FillTargetMap();
        // check if among target units, our WANTED target is as well (->only self cast spells return false)
        for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
        {
            if (ihit->targetGUID == targetguid)
            {
                return true;
            }
        }
    }
    return false;                                           // target invalid
}

/**
 * @brief Validates spell range requirements for the current targets.
 *
 * @param strict True to use strict range validation.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckRange(bool strict)
{
    Unit* target = m_targets.getUnitTarget();

    // special range cases
    switch (m_spellInfo->RangeIndex)
    {
        // self cast doesn't need range checking -- also for Starshards fix
        // spells that can be cast anywhere also need no check
        case SPELL_RANGE_IDX_SELF_ONLY:
        case SPELL_RANGE_IDX_ANYWHERE:
            return SPELL_CAST_OK;
        // combat range spells are treated differently
        case SPELL_RANGE_IDX_COMBAT:
        {
            if (target)
            {
                if (target == m_caster)
                {
                    return SPELL_CAST_OK;
                }

                float range_mod = strict ? 0.0f : 5.0f;
                if (Player* modOwner = m_caster->GetSpellModOwner())
                {
                    float base = ATTACK_DISTANCE;
                    range_mod += modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_RANGE, base, this);
                }

                // with additional 5 dist for non stricted case (some melee spells have delay in apply
                return m_caster->CanReachWithMeleeAttack(target, range_mod) ? SPELL_CAST_OK : SPELL_FAILED_OUT_OF_RANGE;
            }
            break;                                          // let continue in generic way for no target
        }
        case SPELL_RANGE_IDX_SHORT:
        {
            if ((m_spellInfo->SpellClassSet == SPELLFAMILY_HUNTER && (m_spellInfo->SpellClassMask & UI64LIT(0x00000080) || m_spellInfo->SpellClassMask & 0x800000)))
            {
                Pet* pet = m_caster->GetPet();
                if (pet)
                {
                    float max_range = GetSpellMaxRange(sSpellRangeStore.LookupEntry(SPELL_RANGE_IDX_SHORT));
                    return m_caster->IsWithinDistInMap(pet, max_range) ? SPELL_CAST_OK : SPELL_FAILED_OUT_OF_RANGE;
                }
            }
            break;
        }
    }

    // add radius of caster and ~5 yds "give" for non stricred (landing) check
    float range_mod = strict ? 1.25f : 6.25;

    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(m_spellInfo->RangeIndex);
    float max_range = GetSpellMaxRange(srange) + range_mod;
    float min_range = GetSpellMinRange(srange);

    if (Player* modOwner = m_caster->GetSpellModOwner())
    {
        modOwner->ApplySpellMod(m_spellInfo->ID, SPELLMOD_RANGE, max_range, this);
    }

    if (target && target != m_caster)
    {
        // distance from target in checks
        float dist = m_caster->GetCombatDistance(target, m_spellInfo->RangeIndex == SPELL_RANGE_IDX_COMBAT);

        if (dist > max_range)
        {
            return SPELL_FAILED_OUT_OF_RANGE;
        }
        if (min_range && dist < min_range)
        {
            return SPELL_FAILED_TOO_CLOSE;
        }
    }

    // TODO verify that such spells really use bounding radius
    if (m_targets.m_targetMask == TARGET_FLAG_DEST_LOCATION && m_targets.m_destX != 0 && m_targets.m_destY != 0 && m_targets.m_destZ != 0)
    {
        if (!m_caster->IsWithinDist3d(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, max_range))
        {
            return SPELL_FAILED_OUT_OF_RANGE;
        }
        if (min_range && m_caster->IsWithinDist3d(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, min_range))
        {
            return SPELL_FAILED_TOO_CLOSE;
        }
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Calculates the final power cost for a spell cast.
 *
 * @param spellInfo The spell prototype being cast.
 * @param caster The casting unit.
 * @param spell The active spell instance, if available.
 * @param castItem The cast item, if the spell originates from an item.
 * @return The resulting power cost.
 */
uint32 Spell::CalculatePowerCost(SpellEntry const* spellInfo, Unit* caster, Spell const* spell, Item* castItem)
{
    // item cast not used power
    if (castItem)
    {
        return 0;
    }

    // Spell drain all exist power on cast (Only paladin lay of Hands)
    if (spellInfo->HasAttribute(SPELL_ATTR_EX_DRAIN_ALL_POWER))
    {
        // If power type - health drain all
        if (spellInfo->PowerType == POWER_HEALTH)
        {
            return caster->GetHealth();
        }
        // Else drain all power
        if (spellInfo->PowerType < MAX_POWERS)
        {
            return caster->GetPower(Powers(spellInfo->PowerType));
        }
        sLog.outError("Spell::CalculateManaCost: Unknown power type '%d' in spell %d", spellInfo->PowerType, spellInfo->ID);
        return 0;
    }

    // Base powerCost
    int32 powerCost = spellInfo->ManaCost;
    // PCT cost from total amount
    if (spellInfo->ManaCostPct)
    {
        switch (spellInfo->PowerType)
        {
            // health as power used
            case POWER_HEALTH:
                powerCost += spellInfo->ManaCostPct * caster->GetCreateHealth() / 100;
                break;
            case POWER_MANA:
                powerCost += spellInfo->ManaCostPct * caster->GetCreateMana() / 100;
                break;
            case POWER_RAGE:
            case POWER_FOCUS:
            case POWER_ENERGY:
            case POWER_HAPPINESS:
                powerCost += spellInfo->ManaCostPct * caster->GetMaxPower(Powers(spellInfo->PowerType)) / 100;
                break;
            default:
                sLog.outError("Spell::CalculateManaCost: Unknown power type '%d' in spell %d", spellInfo->PowerType, spellInfo->ID);
                return 0;
        }
    }

    SpellSchools school = GetFirstSchoolInMask(spell ? spell->m_spellSchoolMask : GetSpellSchoolMask(spellInfo));
    // Flat mod from caster auras by spell school
    powerCost += caster->GetInt32Value(UNIT_FIELD_POWER_COST_MODIFIER + school);
    // Apply cost mod by spell
    if (spell)
    {
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(spellInfo->ID, SPELLMOD_COST, powerCost, spell);
        }
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_LEVEL_DAMAGE_CALCULATION))
    {
        powerCost = int32(powerCost / (1.117f * spellInfo->SpellLevel / caster->getLevel() - 0.1327f));
    }

    // PCT mod from user auras by school
    powerCost = int32(powerCost * (1.0f + caster->GetFloatValue(UNIT_FIELD_POWER_COST_MULTIPLIER + school)));
    if (powerCost < 0)
    {
        powerCost = 0;
    }
    return powerCost;
}

/**
 * @brief Checks whether the caster has enough power to cast the spell.
 *
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckPower()
{
    // never check power for triggered spells
    if (m_IsTriggeredSpell)
    {
        return SPELL_CAST_OK;
    }

    // item cast not used power
    if (m_CastItem)
    {
        return SPELL_CAST_OK;
    }

    // Questgivers ignore power requirements for scripts, any other creature (except a per) is checked only in combat
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        // power for pets should be checked
        if (!m_caster->GetObjectGuid().IsPet() && m_caster->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER) && !m_caster->IsInCombat())
        {
            return SPELL_CAST_OK;
        }
    }

    // health as power used - need check health amount
    if (m_spellInfo->PowerType == POWER_HEALTH)
    {
        if (m_caster->GetHealth() <= m_powerCost)
        {
            return SPELL_FAILED_CANT_DO_THAT_YET;
        }
    }
    else  // any power except health
    {
        // Check valid power type: since the static data (m_spellInfo) are checked, the check should be done elsewhere
        // [+ZERO] actual DBC power values are 0..3 and uint32(-2)

        // Check power amount
        Powers powerType = Powers(m_spellInfo->PowerType);
        if (m_caster->GetPower(powerType) < m_powerCost)
        {
            return SPELL_FAILED_NO_POWER;
        }
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Determines whether reagent and item requirements should be ignored.
 *
 * @return True if item requirements are ignored; otherwise, false.
 */
bool Spell::IgnoreItemRequirements() const
{
    if (m_IsTriggeredSpell)
    {
        /// Not own traded item (in trader trade slot) req. reagents including triggered spell case
        if (Item* targetItem = m_targets.getItemTarget())
        {
            if (targetItem->GetOwnerGuid() != m_caster->GetObjectGuid())
            {
                return false;
            }
        }

        /// Some triggered spells have same reagents that have master spell
        /// expected in test: master spell have reagents in first slot then triggered don't must use own
        if (m_triggeredBySpellInfo && !m_triggeredBySpellInfo->Reagent[0])
        {
            return false;
        }

        return true;
    }

    return false;
}

/**
 * @brief Validates cast item, target item, reagent, focus, and item-based spell requirements.
 *
 * @return The resulting cast status.
 */
SpellCastResult Spell::CheckItems()
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return SPELL_CAST_OK;
    }

    Player* p_caster = (Player*)m_caster;

    // cast item checks
    if (m_CastItem)
    {
        if (m_CastItem->IsInTrade())
        {
            return SPELL_FAILED_ITEM_GONE;
        }

        uint32 itemid = m_CastItem->GetEntry();
        if (!p_caster->HasItemCount(itemid, 1))
        {
            return SPELL_FAILED_ITEM_NOT_READY;
        }

        ItemPrototype const* proto = m_CastItem->GetProto();
        if (!proto)
        {
            return SPELL_FAILED_ITEM_NOT_READY;
        }

        for (int i = 0; i < 5; ++i)
        {
            if (proto->Spells[i].SpellCharges)
            {
                if (m_CastItem->GetSpellCharges(i) == 0)
                {
                    return SPELL_FAILED_NO_CHARGES_REMAIN;
                }
            }
        }

        // consumable cast item checks
        if (proto->Class == ITEM_CLASS_CONSUMABLE && m_targets.getUnitTarget())
        {
            // such items should only fail if there is no suitable effect at all - see Rejuvenation Potions for example
            SpellCastResult failReason = SPELL_CAST_OK;
            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                // skip check, pet not required like checks, and for TARGET_PET m_targets.getUnitTarget() is not the real target but the caster
                if (m_spellInfo->ImplicitTargetA[i] == TARGET_PET)
                {
                    continue;
                }

                if (m_spellInfo->Effect[i] == SPELL_EFFECT_HEAL)
                {
                    if (m_targets.getUnitTarget()->GetHealth() == m_targets.getUnitTarget()->GetMaxHealth())
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_HEALTH;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }

                // Mana Potion, Rage Potion, Thistle Tea(Rogue), ...
                if (m_spellInfo->Effect[i] == SPELL_EFFECT_ENERGIZE)
                {
                    if (m_spellInfo->EffectMiscValue[i] < 0 || m_spellInfo->EffectMiscValue[i] >= MAX_POWERS)
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_MANA;
                        continue;
                    }

                    Powers power = Powers(m_spellInfo->EffectMiscValue[i]);
                    uint8 targetClass = m_targets.getUnitTarget()->getClass();
                    /* Mana */
                    if (power == POWER_MANA)
                    {
                        if (targetClass == CLASS_WARRIOR || targetClass == CLASS_ROGUE)
                        {
                            failReason = SPELL_FAILED_BAD_TARGETS;
                            continue;
                        }
                    }
                    /* Rage */
                    else if (power == POWER_RAGE)
                    {
                        if (targetClass != CLASS_WARRIOR && targetClass != CLASS_DRUID)
                        {
                            failReason = SPELL_FAILED_BAD_TARGETS;
                            continue;
                        }
                    }
                    /* Energy */
                    else if (power == POWER_ENERGY)
                    {
                        if (targetClass != CLASS_ROGUE && targetClass != CLASS_DRUID)
                        {
                            failReason = SPELL_FAILED_BAD_TARGETS;
                            continue;
                        }
                    }
                    if (m_targets.getUnitTarget()->GetPower(power) == m_targets.getUnitTarget()->GetMaxPower(power))
                    {
                        failReason = SPELL_FAILED_ALREADY_AT_FULL_MANA;
                        continue;
                    }
                    else
                    {
                        failReason = SPELL_CAST_OK;
                        break;
                    }
                }
            }
            if (failReason != SPELL_CAST_OK)
            {
                return failReason;
            }
        }
    }

    // check target item (for triggered case not report error)
    if (m_targets.getItemTargetGuid())
    {
        if (m_caster->GetTypeId() != TYPEID_PLAYER)
        {
            return m_IsTriggeredSpell && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
                ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_BAD_TARGETS;
        }

        if (!m_targets.getItemTarget())
        {
            return m_IsTriggeredSpell  && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
                ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_ITEM_GONE;
        }

        if (!m_targets.getItemTarget()->IsFitToSpellRequirements(m_spellInfo))
        {
            return m_IsTriggeredSpell  && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM)
                ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;
        }
    }
    // if not item target then required item must be equipped (for triggered case not report error)
    else
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER && !((Player*)m_caster)->HasItemFitToSpellReqirements(m_spellInfo))
        {
            return m_IsTriggeredSpell ? SPELL_FAILED_DONT_REPORT : SPELL_FAILED_EQUIPPED_ITEM_CLASS;
        }
    }

    // check spell focus object
    if (m_spellInfo->RequiresSpellFocus)
    {
        GameObject* ok = NULL;
        MaNGOS::GameObjectFocusCheck go_check(m_caster, m_spellInfo->RequiresSpellFocus);
        MaNGOS::GameObjectSearcher<MaNGOS::GameObjectFocusCheck> checker(ok, go_check);
        Cell::VisitGridObjects(m_caster, checker, m_caster->GetMap()->GetVisibilityDistance());

        if (!ok)
        {
            return SPELL_FAILED_REQUIRES_SPELL_FOCUS;
        }

        focusObject = ok;                                   // game object found in range
    }

    // check reagents (ignore triggered spells with reagents processed by original spell) and special reagent ignore case.
    if (!IgnoreItemRequirements())
    {
        if (!p_caster->CanNoReagentCast(m_spellInfo))
        {
            for (uint32 i = 0; i < MAX_SPELL_REAGENTS; ++i)
            {
                if (m_spellInfo->Reagent[i] <= 0)
                {
                    continue;
                }

                uint32 itemid    = m_spellInfo->Reagent[i];
                uint32 itemcount = m_spellInfo->ReagentCount[i];

                // if CastItem is also spell reagent
                if (m_CastItem && m_CastItem->GetEntry() == itemid)
                {
                    ItemPrototype const* proto = m_CastItem->GetProto();
                    if (!proto)
                    {
                        return SPELL_FAILED_ITEM_NOT_READY;
                    }
                    for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
                    {
                        // CastItem will be used up and does not count as reagent
                        int32 charges = m_CastItem->GetSpellCharges(s);
                        if (proto->Spells[s].SpellCharges < 0 && !(proto->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE) && abs(charges) < 2)
                        {
                            ++itemcount;
                            break;
                        }
                    }
                }

                if (!p_caster->HasItemCount(itemid, itemcount))
                {
                    return SPELL_FAILED_ITEM_NOT_READY;
                }
            }
        }

        // check totem-item requirements (items presence in inventory)
        uint32 totems = MAX_SPELL_TOTEMS;
        for (int i = 0; i < MAX_SPELL_TOTEMS ; ++i)
        {
            if (m_spellInfo->Totem[i] != 0)
            {
                if (p_caster->HasItemCount(m_spellInfo->Totem[i], 1))
                {
                    totems -= 1;
                    continue;
                }
            }
            else
            {
                totems -= 1;
            }
        }

        if (totems != 0)
        {
            return SPELL_FAILED_ITEM_GONE;                   //[-ZERO] not sure of it
        }

        /**[-ZERO] to rewrite?
         * // Check items for TotemCategory  (items presence in inventory)
         * uint32 TotemCategory = MAX_SPELL_TOTEM_CATEGORIES;
         * for (int i= 0; i < MAX_SPELL_TOTEM_CATEGORIES; ++i)
         * {
         *     if (m_spellInfo->TotemCategory[i] != 0)
         *     {
         *         if (p_caster->HasItemTotemCategory(m_spellInfo->TotemCategory[i]))
         *         {
         *             TotemCategory -= 1;
         *             continue;
         *         }
         *     }
         *     else
         *     {
         *         TotemCategory -= 1;
         *     }
         * }

         * if (TotemCategory != 0)
         * {
         *     return SPELL_FAILED_TOTEM_CATEGORY;             // 0x7B
         * }
         */
    }

    // special checks for spell effects
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        switch (m_spellInfo->Effect[i])
        {
            case SPELL_EFFECT_CREATE_ITEM:
            {
                if (!m_IsTriggeredSpell && m_spellInfo->EffectItemType[i])
                {
                    ItemPosCountVec dest;
                    InventoryResult msg = p_caster->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, m_spellInfo->EffectItemType[i], 1);
                    if (msg != EQUIP_ERR_OK)
                    {
                        p_caster->SendEquipError(msg, NULL, NULL, m_spellInfo->EffectItemType[i]);
                        return SPELL_FAILED_DONT_REPORT;
                    }
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_ITEM:
            {
                Item* targetItem = m_targets.getItemTarget();
                if (!targetItem)
                {
                    return SPELL_FAILED_ITEM_GONE;
                }

                if (targetItem->GetProto()->ItemLevel < m_spellInfo->BaseLevel)
                {
                    return SPELL_FAILED_LOWLEVEL;
                }
                // Check for armor kit spells: Heavy(2833), Thick(10344), Rugged(19057), Core(22725)
                if (m_CastItem && m_CastItem->GetProto())
                {
                    static uint32 const armorKitSpells[] = { 2833, 10344, 19057, 22725 };
                    for (uint32 const spellId : armorKitSpells)
                    {
                        if (m_spellInfo->ID == spellId)
                        {
                            int32 minLevel = int32(m_CastItem->GetProto()->RequiredLevel) - 5;
                            if (minLevel > 0 && int32(targetItem->GetProto()->ItemLevel) < minLevel)
                                return SPELL_FAILED_LOWLEVEL;
                            break;
                        }
                    }
                }
                // Not allow enchant in trade slot for some enchant type
                if (targetItem->GetOwner() != m_caster)
                {
                    uint32 enchant_id = m_spellInfo->EffectMiscValue[i];
                    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                    if (!pEnchant)
                    {
                        return SPELL_FAILED_ERROR;
                    }
                    if (pEnchant->Flags & ENCHANTMENT_CAN_SOULBOUND)
                    {
                        return SPELL_FAILED_NOT_TRADEABLE;
                    }
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
            {
                Item* item = m_targets.getItemTarget();
                if (!item)
                {
                    return SPELL_FAILED_ITEM_GONE;
                }
                // Not allow enchant in trade slot for some enchant type
                if (item->GetOwner() != m_caster)
                {
                    uint32 enchant_id = m_spellInfo->EffectMiscValue[i];
                    SpellItemEnchantmentEntry const* pEnchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
                    if (!pEnchant)
                    {
                        return SPELL_FAILED_ERROR;
                    }
                    if (pEnchant->Flags & ENCHANTMENT_CAN_SOULBOUND)
                    {
                        return SPELL_FAILED_NOT_TRADEABLE;
                    }
                }
                break;
            }
            case SPELL_EFFECT_ENCHANT_HELD_ITEM:
                // check item existence in effect code (not output errors at offhand hold item effect to main hand for example
                break;
            case SPELL_EFFECT_DISENCHANT:
            {
                if (!m_targets.getItemTarget())
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }

                // prevent disenchanting in trade slot
                if (m_targets.getItemTarget()->GetOwnerGuid() != m_caster->GetObjectGuid())
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }

                ItemPrototype const* itemProto = m_targets.getItemTarget()->GetProto();
                if (!itemProto)
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }

                // must have disenchant loot (other static req. checked at item prototype loading)
                if (!itemProto->DisenchantID)
                {
                    return SPELL_FAILED_CANT_BE_DISENCHANTED;
                }
                break;
            }
            case SPELL_EFFECT_WEAPON_DAMAGE:
            case SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL:
            {
                if (m_caster->GetTypeId() != TYPEID_PLAYER)
                {
                    return SPELL_FAILED_TARGET_NOT_PLAYER;
                }
                if (m_attackType != RANGED_ATTACK)
                {
                    break;
                }
                Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(m_attackType, true, false);
                if (!pItem)
                {
                    return SPELL_FAILED_EQUIPPED_ITEM;
                }

                switch (pItem->GetProto()->SubClass)
                {
                    case ITEM_SUBCLASS_WEAPON_THROWN:
                    {
                        uint32 ammo = pItem->GetEntry();
                        if (!((Player*)m_caster)->HasItemCount(ammo, 1))
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }
                        break;
                    }
                    case ITEM_SUBCLASS_WEAPON_GUN:
                    case ITEM_SUBCLASS_WEAPON_BOW:
                    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                    {
                        uint32 ammo = ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID);
                        if (!ammo)
                        {
                            // Requires No Ammo
                            if (m_caster->GetDummyAura(46699))
                            {
                                break;                       // skip other checks
                            }

                            return SPELL_FAILED_NO_AMMO;
                        }

                        ItemPrototype const* ammoProto = ObjectMgr::GetItemPrototype(ammo);
                        if (!ammoProto)
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }

                        if (ammoProto->Class != ITEM_CLASS_PROJECTILE)
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }

                        // check ammo ws. weapon compatibility
                        switch (pItem->GetProto()->SubClass)
                        {
                            case ITEM_SUBCLASS_WEAPON_BOW:
                            case ITEM_SUBCLASS_WEAPON_CROSSBOW:
                                if (ammoProto->SubClass != ITEM_SUBCLASS_ARROW)
                                {
                                    return SPELL_FAILED_NO_AMMO;
                                }
                                break;
                            case ITEM_SUBCLASS_WEAPON_GUN:
                                if (ammoProto->SubClass != ITEM_SUBCLASS_BULLET)
                                {
                                    return SPELL_FAILED_NO_AMMO;
                                }
                                break;
                            default:
                                return SPELL_FAILED_NO_AMMO;
                        }

                        if (!((Player*)m_caster)->HasItemCount(ammo, 1))
                        {
                            return SPELL_FAILED_NO_AMMO;
                        }
                        break;
                    }
                    case ITEM_SUBCLASS_WEAPON_WAND:
                        break;
                    default:
                        break;
                }
                break;
            }
            default: break;
        }
    }

    return SPELL_CAST_OK;
}
