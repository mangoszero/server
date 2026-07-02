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
 * @brief Cancels the spell and sends the appropriate interruption notifications.
 */
void Spell::cancel()
{
    if (m_spellState == SPELL_STATE_FINISHED)
    {
        return;
    }

    // channeled spells don't display interrupted message even if they are interrupted, possible other cases with no "Interrupted" message
    bool sendInterrupt = IsChanneledSpell(m_spellInfo) ? false : true;

    m_autoRepeat = false;
    switch (m_spellState)
    {
        case SPELL_STATE_PREPARING:
            CancelGlobalCooldown();

            //(no break)
        case SPELL_STATE_DELAYED:
        {
            SendInterrupted(SPELL_FAILED_INTERRUPTED);

            if (sendInterrupt)
            {
                SendCastResult(SPELL_FAILED_INTERRUPTED);
            }
            break;
        }
        case SPELL_STATE_CASTING:
        {
            for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
            {
                if (ihit->missCondition == SPELL_MISS_NONE)
                {
                    Unit* unit = m_caster->GetObjectGuid() == (*ihit).targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, ihit->targetGUID);
                    if (unit && unit->IsAlive())
                    {
                        unit->RemoveAurasByCasterSpell(m_spellInfo->Id, m_caster->GetObjectGuid());
                    }
                }
            }

            SendChannelUpdate(0);
            SendInterrupted(SPELL_FAILED_INTERRUPTED);

            if (sendInterrupt)
            {
                SendCastResult(SPELL_FAILED_INTERRUPTED);
            }
            break;
        }
        default:
        {
            break;
        }
    }

    finish(false);
    m_caster->RemoveDynObject(m_spellInfo->Id);
    m_caster->RemoveGameObject(m_spellInfo->Id, true);
}

/**
 * @brief Executes the spell cast after preparation has completed.
 *
 * @param skipCheck True to skip the second cast-condition validation.
 */
void Spell::cast(bool skipCheck)
{
    SetExecutedCurrently(true);

    if (!m_caster->CheckAndIncreaseCastCounter())
    {
        if (m_triggeredByAuraSpell)
        {
            sLog.outError("Spell %u triggered by aura spell %u too deep in cast chain for cast. Cast not allowed for prevent overflow stack crash.", m_spellInfo->Id, m_triggeredByAuraSpell->Id);
        }
        else
        {
            sLog.outError("Spell %u too deep in cast chain for cast. Cast not allowed for prevent overflow stack crash.", m_spellInfo->Id);
        }

        SendCastResult(SPELL_FAILED_ERROR);
        finish(false);
        SetExecutedCurrently(false);
        return;
    }

    // update pointers base at GUIDs to prevent access to already nonexistent object
    UpdatePointers();

    // cancel at lost main target unit
    if (!m_targets.getUnitTarget() && m_targets.getUnitTargetGuid() && m_targets.getUnitTargetGuid() != m_caster->GetObjectGuid())
    {
        cancel();
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    if (m_caster->GetTypeId() != TYPEID_PLAYER && m_targets.getUnitTarget() && m_targets.getUnitTarget() != m_caster)
    {
        m_caster->SetInFront(m_targets.getUnitTarget());
    }

    SpellCastResult castResult = CheckPower();
    if (castResult != SPELL_CAST_OK)
    {
        SendInterrupted(castResult);
        SendCastResult(castResult);
        finish(false);
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    // triggered cast called from Spell::prepare where it was already checked
    if (!skipCheck)
    {
        castResult = CheckCast(false);
        if (castResult != SPELL_CAST_OK)
        {
            SendInterrupted(castResult);
            SendCastResult(castResult);
            finish(false);
            m_caster->DecreaseCastCounter();
            SetExecutedCurrently(false);
            return;
        }
    }

    // different triggered (for caster) and pre-cast (casted before apply effect to each target) cases
    switch (m_spellInfo->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            // Bandages
            if (m_spellInfo->Mechanic == MECHANIC_BANDAGE)
            {
                AddPrecastSpell(SPELL_ID_RECENTLY_BANDAGED);
            }
            // Divine Shield, Divine Protection (Blessing of Protection in paladin switch case)
            else if (m_spellInfo->Mechanic == MECHANIC_INVULNERABILITY)
            {
                AddPrecastSpell(25771);                      // Forbearance
            }
            break;
        }
        case SPELLFAMILY_ROGUE:
        {
            // exit stealth on sap when improved sap is not skilled
            if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x00000080) && m_caster->GetTypeId() == TYPEID_PLAYER && (!m_caster->GetAura(14076, SpellEffectIndex(0)) && !m_caster->GetAura(14094, SpellEffectIndex(0)) && !m_caster->GetAura(14095, SpellEffectIndex(0))))
            {
                m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
            }
            break;
        }
        case SPELLFAMILY_WARRIOR:
        {
            break;
        }
        case SPELLFAMILY_PRIEST:
        {
            // Power Word: Shield
            if (m_spellInfo->SpellFamilyName == SPELLFAMILY_PRIEST && m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000000000001))
            {
                AddPrecastSpell(6788);                      // Weakened Soul
            }

            switch (m_spellInfo->Id)
            {
                case 15237: AddTriggeredSpell(23455); break;// Holy Nova, rank 1
                case 15430: AddTriggeredSpell(23458); break;// Holy Nova, rank 2
                case 15431: AddTriggeredSpell(23459); break;// Holy Nova, rank 3
                case 27799: AddTriggeredSpell(27803); break;// Holy Nova, rank 4
                case 27800: AddTriggeredSpell(27804); break;// Holy Nova, rank 5
                case 27801: AddTriggeredSpell(27805); break;// Holy Nova, rank 6
                case 25331: AddTriggeredSpell(25329); break;// Holy Nova, rank 7
                default: break;
            }
            break;
        }
        case SPELLFAMILY_PALADIN:
        {
            // Blessing of Protection (Divine Shield, Divine Protection in generic switch case)
            if (m_spellInfo->Mechanic == MECHANIC_INVULNERABILITY && m_spellInfo->Id != 25771)
            {
                AddPrecastSpell(25771);                      // Forbearance
            }
            break;
        }
        default:
            break;
    }

    // As of patch 1.10.0, Arcane Power will replace Power Infusion
    if (m_spellInfo->Id == 12042)
    {
        m_targets.getUnitTarget()->RemoveAurasDueToSpell(10060);
    }

    // Linked spells (precast chain)
    SpellLinkedSet linkedSet = sSpellMgr.GetSpellLinked(m_spellInfo->Id, SPELL_LINKED_TYPE_PRECAST);
    if (linkedSet.size() > 0)
    {
        for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
        {
            AddPrecastSpell(*itr);
        }
    }

    // Linked spells (triggered chain)
    linkedSet.clear();
    linkedSet = sSpellMgr.GetSpellLinked(m_spellInfo->Id, SPELL_LINKED_TYPE_TRIGGERED);
    if (linkedSet.size() > 0)
    {
        for (SpellLinkedSet::const_iterator itr = linkedSet.begin(); itr != linkedSet.end(); ++itr)
        {
            AddTriggeredSpell(*itr);
        }
    }

    // traded items have trade slot instead of guid in m_itemTargetGUID
    // set to real guid to be sent later to the client
    m_targets.updateTradeSlotItem();

#ifdef ENABLE_ELUNA
    if (Eluna* e = m_caster->GetEluna())
    {
        if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            e->OnSpellCast(m_caster->ToPlayer(), this, skipCheck);
        }
    }
#endif /* ENABLE_ELUNA */

    FillTargetMap();

    if (m_spellState == SPELL_STATE_FINISHED)               // stop cast if spell marked as finish somewhere in FillTargetMap
    {
        m_caster->DecreaseCastCounter();
        SetExecutedCurrently(false);
        return;
    }

    // CAST SPELL
    SendSpellCooldown();

    TakePower();
    TakeReagents();                                         // we must remove reagents before HandleEffects to allow place crafted item in same slot
    TakeAmmo();

    SendCastResult(castResult);
    SendSpellGo();                                          // we must send smsg_spell_go packet before m_castItem delete in TakeCastItem()...

    InitializeDamageMultipliers();

    // Okay, everything is prepared. Now we need to distinguish between immediate and evented delayed spells
    float speed = m_spellInfo->speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->speed : m_spellInfo->speed;
    if (speed > 0.0f)
    {
        // Remove used for cast item if need (it can be already NULL after TakeReagents call
        // in case delayed spell remove item at cast delay start
        TakeCastItem();

        // fill initial spell damage from caster for delayed casted spells
        for (TargetList::iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
        {
            HandleDelayedSpellLaunch(&(*ihit));
        }

        // Okay, maps created, now prepare flags
        m_immediateHandled = false;
        m_spellState = SPELL_STATE_DELAYED;
        SetDelayStart(0);
    }
    else
    {
        // Immediate spell, no big deal
        handle_immediate();
    }

    m_caster->DecreaseCastCounter();
    SetExecutedCurrently(false);
}

/**
 * @brief Handles the full execution path for an immediate spell.
 */
void Spell::handle_immediate()
{
    // process immediate effects (items, ground, etc.) also initialize some variables
    _handle_immediate_phase();

    // start channeling if applicable (after _handle_immediate_phase for get persistent effect dynamic object for channel target
    if (IsChanneledSpell(m_spellInfo) && m_duration)
    {
        m_spellState = SPELL_STATE_CASTING;
        SendChannelStart(m_duration);
    }

    for (TargetList::iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        DoAllEffectOnTarget(&(*ihit));
    }

    for (GOTargetList::iterator ihit = m_UniqueGOTargetInfo.begin(); ihit != m_UniqueGOTargetInfo.end(); ++ihit)
    {
        DoAllEffectOnTarget(&(*ihit));
    }

    // spell is finished, perform some last features of the spell here
    _handle_finish_phase();

    // Remove used for cast item if need (it can be already NULL after TakeReagents call
    TakeCastItem();

    if (m_spellState != SPELL_STATE_CASTING)
    {
        finish(true);                                        // successfully finish spell cast (not last in case autorepeat or channel spell)
    }
}

/**
 * @brief Processes delayed spell impacts that are due at the current offset.
 *
 * @param t_offset The elapsed delay offset in milliseconds.
 * @return The next pending delay time, or zero when finished.
 */
uint64 Spell::handle_delayed(uint64 t_offset)
{
    uint64 next_time = 0;

    if (!m_immediateHandled)
    {
        _handle_immediate_phase();
        m_immediateHandled = true;
    }

    // now recheck units targeting correctness (need before any effects apply to prevent adding immunity at first effect not allow apply second spell effect and similar cases)
    for (TargetList::iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (!ihit->processed)
        {
            if (ihit->timeDelay <= t_offset)
            {
                DoAllEffectOnTarget(&(*ihit));
            }
            else if (next_time == 0 || ihit->timeDelay < next_time)
            {
                next_time = ihit->timeDelay;
            }
        }
    }

    // now recheck gameobject targeting correctness
    for (GOTargetList::iterator ighit = m_UniqueGOTargetInfo.begin(); ighit != m_UniqueGOTargetInfo.end(); ++ighit)
    {
        if (!ighit->processed)
        {
            if (ighit->timeDelay <= t_offset)
            {
                DoAllEffectOnTarget(&(*ighit));
            }
            else if (next_time == 0 || ighit->timeDelay < next_time)
            {
                next_time = ighit->timeDelay;
            }
        }
    }
    // All targets passed - need finish phase
    if (next_time == 0)
    {
        // spell is finished, perform some last features of the spell here
        _handle_finish_phase();

        finish(true);                                       // successfully finish spell cast

        // return zero, spell is finished now
        return 0;
    }
    else
    {
        // spell is unfinished, return next execution time
        return next_time;
    }
}

/**
 * @brief Performs the immediate pre-impact phase shared by instant and delayed spells.
 */
void Spell::_handle_immediate_phase()
{
    // handle some immediate features of the spell here
    HandleThreatSpells();

    m_needSpellLog = IsNeedSendToClient();
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        if (m_spellInfo->Effect[j] == 0)
        {
            continue;
        }

        // apply Send Event effect to ground in case empty target lists
        if (m_spellInfo->Effect[j] == SPELL_EFFECT_SEND_EVENT && !HaveTargetsForEffect(SpellEffectIndex(j)))
        {
            HandleEffects(NULL, NULL, NULL, SpellEffectIndex(j));
            continue;
        }

        // Don't do spell log, if is school damage spell
        if (m_spellInfo->Effect[j] == SPELL_EFFECT_SCHOOL_DAMAGE || m_spellInfo->Effect[j] == 0)
        {
            m_needSpellLog = false;
        }
    }

    // initialize Diminishing Returns Data
    m_diminishLevel = DIMINISHING_LEVEL_1;
    m_diminishGroup = DIMINISHING_NONE;

    // process items
    for (ItemTargetList::iterator ihit = m_UniqueItemInfo.begin(); ihit != m_UniqueItemInfo.end(); ++ihit)
    {
        DoAllEffectOnTarget(&(*ihit));
    }

    // process ground
    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        // persistent area auras target only the ground
        if (m_spellInfo->Effect[j] == SPELL_EFFECT_PERSISTENT_AREA_AURA ||
            //summon a gameobject at the spell's destination xyz
            (m_spellInfo->Effect[j] == SPELL_EFFECT_TRANS_DOOR && m_spellInfo->EffectImplicitTargetA[j] == TARGET_AREAEFFECT_GO_AROUND_DEST))
        {
            HandleEffects(NULL, NULL, NULL, SpellEffectIndex(j));
        }
    }
}

/**
 * @brief Performs post-impact finishing logic before the spell completes.
 */
void Spell::_handle_finish_phase()
{
    // spell log
    if (m_needSpellLog)
    {
        SendLogExecute();
    }

    if (m_caster->m_extraAttacks && m_spellInfo->HasSpellEffect(SPELL_EFFECT_ADD_EXTRA_ATTACKS))
    {
        switch (m_spellInfo->Id)
        {
            // on next swing
            case 15494:
            case 18797:
            case 21919:
            case 20178: // paladin reckoning proc
                break;
            default:
                if (Unit* victim = m_caster->getVictim())
                {
                    m_caster->HandleProcExtraAttackFor(victim);
                }
                else
                {
                    m_caster->m_extraAttacks = 0;   // do not allow to accumulate instant extra attacks
                }
                break;
        }
    }
}

/**
 * @brief Applies and sends cooldown data for player casts when appropriate.
 */
void Spell::SendSpellCooldown()
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* _player = (Player*)m_caster;

    // (1) have infinity cooldown but set at aura apply, (2) passive cooldown at triggering
    if (m_spellInfo->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE) || m_spellInfo->HasAttribute(SPELL_ATTR_PASSIVE))
    {
        return;
    }

    _player->AddSpellAndCategoryCooldowns(m_spellInfo, m_CastItem ? m_CastItem->GetEntry() : 0, this);
}

/**
 * @brief Updates the spell state machine during preparation or channeling.
 *
 * @param difftime The elapsed update time in milliseconds.
 */
void Spell::update(uint32 difftime)
{
    // update pointers based at it's GUIDs
    UpdatePointers();

    if (!m_targets.getUnitTargetGuid().IsEmpty() && !m_targets.getUnitTarget())
    {
        cancel();
        return;
    }

    // check for target going invisiblity/fake death
    if (Unit* target = m_targets.getUnitTarget())
    {
        if (!target->IsVisibleForOrDetect(m_caster, m_caster, true) || target->HasAuraType(SPELL_AURA_FEIGN_DEATH))
        {
            if (m_caster->GetTargetGuid() == target->GetObjectGuid())
            {
                m_caster->SetTargetGuid(ObjectGuid());
            }
            cancel();
            return;
        }
    }

    if (m_targets.getUnitTarget() && (m_targets.getUnitTarget() != m_caster) && IsSingleTargetSpell(m_spellInfo) &&
        !IsNextMeleeSwingSpell() && !IsAutoRepeat() && !m_IsTriggeredSpell)
    {
        if (!m_caster->IsWithinLOSInMap(m_targets.getUnitTarget()))
        {
            cancel();
            return;
        }
    }

    // check if the player or unit caster has moved before the spell finished (exclude casting on vehicles)
    if (((m_caster->GetTypeId() == TYPEID_PLAYER || m_caster->GetTypeId() == TYPEID_UNIT) && m_timer != 0) &&
        (m_castPositionX != m_caster->GetPositionX() || m_castPositionY != m_caster->GetPositionY() || m_castPositionZ != m_caster->GetPositionZ()) &&
        (m_spellInfo->Effect[EFFECT_INDEX_0] != SPELL_EFFECT_STUCK || !m_caster->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLINGFAR)))
    {
        // always cancel for channeled spells
        if (m_spellState == SPELL_STATE_CASTING)
        {
            cancel();
        }
        // don't cancel for melee, autorepeat, triggered and instant spells
        else if (!IsNextMeleeSwingSpell() && !IsAutoRepeat() && !m_IsTriggeredSpell && (m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_MOVEMENT))
        {
            cancel();
        }
    }

    switch (m_spellState)
    {
        case SPELL_STATE_PREPARING:
        {
            if (m_timer)
            {
                if (difftime >= m_timer)
                {
                    m_timer = 0;
                }
                else
                {
                    m_timer -= difftime;
                }
            }

            if (m_timer == 0 && !IsNextMeleeSwingSpell() && !IsAutoRepeat())
            {
                cast();
            }
            break;
        }
        case SPELL_STATE_CASTING:
        {
            if (m_timer > 0)
            {
                if (m_caster->GetTypeId() == TYPEID_PLAYER || m_caster->GetTypeId() == TYPEID_UNIT)
                {
                    // check if player has jumped before the channeling finished
                    if (m_caster->m_movementInfo.HasMovementFlag(MOVEFLAG_FALLING))
                    {
                        cancel();
                    }

                    // check for incapacitating player states
                    if (m_caster->hasUnitState(UNIT_STAT_CAN_NOT_REACT))
                    {
                        // certain channel spells are not interrupted
                        if (!m_spellInfo->HasAttribute(SPELL_ATTR_EX_CHANNELED_1) && !m_spellInfo->HasAttribute(SPELL_ATTR_EX3_UNK28))
                        {
                            cancel();
                        }
                    }

                    // check if player has turned if flag is set
                    if (m_spellInfo->ChannelInterruptFlags & CHANNEL_FLAG_TURNING && m_castOrientation != m_caster->GetOrientation())
                    {
                        if (m_caster->GetTypeId() == TYPEID_PLAYER)
                        {
                            if (static_cast<Player*>(m_caster)->GetMover()->GetObjectGuid() == m_caster->GetObjectGuid())
                            {
                                cancel();
                            }
                        }
                        else
                        {
                            cancel();
                        }
                    }
                }

                // check if there are alive targets left
                if (!IsAliveUnitPresentInTargetList())
                {
                    SendChannelUpdate(0);
                    finish();
                }

                if (difftime >= m_timer)
                {
                    m_timer = 0;
                }
                else
                {
                    m_timer -= difftime;
                }
            }

            if (m_timer == 0)
            {
                SendChannelUpdate(0);

                // channeled spell processed independently for quest targeting
                // cast at creature (or GO) quest objectives update at successful cast channel finished
                // ignore autorepeat/melee casts for speed (not exist quest for spells (hm... )
                if (!IsAutoRepeat() && !IsNextMeleeSwingSpell())
                {
                    if (Player* p = m_caster->GetCharmerOrOwnerPlayerOrPlayerItself())
                    {
                        for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                        {
                            TargetInfo const& target = *ihit;
                            if (!target.targetGUID.IsCreature())
                            {
                                continue;
                            }

                            Unit* unit = m_caster->GetObjectGuid() == target.targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, target.targetGUID);
                            if (unit == NULL)
                            {
                                continue;
                            }

                            p->RewardPlayerAndGroupAtCast(unit, m_spellInfo->Id);
                        }

                        for (GOTargetList::const_iterator ihit = m_UniqueGOTargetInfo.begin(); ihit != m_UniqueGOTargetInfo.end(); ++ihit)
                        {
                            GOTargetInfo const& target = *ihit;

                            GameObject* go = m_caster->GetMap()->GetGameObject(target.targetGUID);
                            if (!go)
                            {
                                continue;
                            }

                            p->RewardPlayerAndGroupAtCast(go, m_spellInfo->Id);
                        }
                    }
                }

                finish();
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

/**
 * @brief Finalizes the spell and performs successful-completion side effects.
 *
 * @param ok True when the spell completed successfully; false otherwise.
 */
void Spell::finish(bool ok)
{
    if (!m_caster)
    {
        return;
    }

    if (m_spellState == SPELL_STATE_FINISHED)
    {
        return;
    }

    // remove/restore spell mods before m_spellState update
    if (Player* modOwner = m_caster->GetSpellModOwner())
    {
        if (ok || m_spellState != SPELL_STATE_PREPARING)    // fail after start channeling or throw to target not affect spell mods
        {
            modOwner->RemoveSpellMods(this);
        }
        else
        {
            modOwner->ResetSpellModsDueToCanceledSpell(this);
        }
    }

    m_spellState = SPELL_STATE_FINISHED;

    // other code related only to successfully finished spells
    if (!ok)
    {
        return;
    }

    // handle SPELL_AURA_ADD_TARGET_TRIGGER auras
    Unit::AuraList const& targetTriggers = m_caster->GetAurasByType(SPELL_AURA_ADD_TARGET_TRIGGER);
    for (Unit::AuraList::const_iterator i = targetTriggers.begin(); i != targetTriggers.end(); ++i)
    {
        if (!(*i)->isAffectedOnSpell(m_spellInfo))
        {
            continue;
        }
        for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
        {
            if (ihit->missCondition == SPELL_MISS_NONE)
            {
                // check m_caster->GetGUID() let load auras at login and speedup most often case
                Unit* unit = m_caster->GetObjectGuid() == ihit->targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, ihit->targetGUID);
                if (unit && unit->IsAlive())
                {
                    SpellEntry const* auraSpellInfo = (*i)->GetSpellProto();
                    SpellEffectIndex auraSpellIdx = (*i)->GetEffIndex();
                    // Calculate chance at that moment (can be depend for example from combo points)
                    int32 auraBasePoints = (*i)->GetBasePoints();
                    int32 chance = m_caster->CalculateSpellDamage(unit, auraSpellInfo, auraSpellIdx, &auraBasePoints);
                    if (roll_chance_i(chance))
                    {
                        m_caster->CastSpell(unit, auraSpellInfo->EffectTriggerSpell[auraSpellIdx], true, NULL, (*i));
                    }
                }
            }
        }
    }

    // Heal caster for all health leech from all targets
    if (m_healthLeech)
    {
        m_caster->DealHeal(m_caster, uint32(m_healthLeech), m_spellInfo);
    }

    if (IsMeleeAttackResetSpell())
    {
        m_caster->resetAttackTimer(BASE_ATTACK);
        if (m_caster->haveOffhandWeapon())
        {
            m_caster->resetAttackTimer(OFF_ATTACK);
        }
    }

    /** if (IsRangedAttackResetSpell())
     *      m_caster->resetAttackTimer(RANGED_ATTACK); */

    // Clear combo at finish state
    if (m_caster->GetTypeId() == TYPEID_PLAYER && NeedsComboPoints(m_spellInfo))
    {
        // Not drop combopoints if negative spell and if any miss on enemy exist
        bool needDrop = true;
        if (!IsPositiveSpell(m_spellInfo->Id))
        {
            for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
            {
                if (ihit->missCondition != SPELL_MISS_NONE && ihit->targetGUID != m_caster->GetObjectGuid())
                {
                    needDrop = false;
                    break;
                }
            }
        }
        if (needDrop)
        {
            ((Player*)m_caster)->ClearComboPoints();
        }
    }

    // call triggered spell only at successful cast (after clear combo points -> for add some if need)
    if (!m_TriggerSpells.empty())
    {
        CastTriggerSpells();
    }

    // Stop Attack for some spells
    if (m_spellInfo->HasAttribute(SPELL_ATTR_STOP_ATTACK_TARGET))
    {
        m_caster->AttackStop();
    }

#ifdef ENABLE_PLAYERBOTS
    if (!m_caster->GetMapId())
    {
        return;
    }
#endif
}

/**
 * @brief Consumes ammunition or durability for ranged attacks.
 */
void Spell::TakeAmmo()
{
    if (m_attackType == RANGED_ATTACK && m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        Item* pItem = ((Player*)m_caster)->GetWeaponForAttack(RANGED_ATTACK, true, false);

        // wands don't have ammo
        if (!pItem || pItem->GetProto()->SubClass == ITEM_SUBCLASS_WEAPON_WAND)
        {
            return;
        }

        if (pItem->GetProto()->InventoryType == INVTYPE_THROWN)
        {
            if (pItem->GetMaxStackCount() == 1)
            {
                // decrease durability for non-stackable throw weapon
                ((Player*)m_caster)->DurabilityPointLossForEquipSlot(EQUIPMENT_SLOT_RANGED);
            }
            else
            {
                // decrease items amount for stackable throw weapon
                uint32 count = 1;
                ((Player*)m_caster)->DestroyItemCount(pItem, count, true);
            }
        }
        else if (uint32 ammo = ((Player*)m_caster)->GetUInt32Value(PLAYER_AMMO_ID))
        {
            ((Player*)m_caster)->DestroyItemCount(ammo, 1, true);
        }
    }
}
