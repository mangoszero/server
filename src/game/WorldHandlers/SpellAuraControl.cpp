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
 * @file SpellAuras.cpp
 * @brief Spell aura implementation
 *
 * This file implements the SpellAura class which handles spell auras:
 * - Aura application and removal
 * - Aura effect processing (stat modifiers, DoTs, HoTs, etc.)
 * - Aura stacking rules
 * - Aura dispelling mechanics
 * - Aura periodic effects
 * - Aura duration management
 * - Aura visual effects
 *
 * Auras are persistent effects applied by spells that modify
 * unit stats, deal damage over time, or provide other benefits.
 *
 * @see SpellAura for the aura class
 * @see Spell for spell casting
 */



#include "SpellAuras.h"
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "ObjectAccessor.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Language.h"
#include "TemporarySummon.h"
#include "MapManager.h"

static AuraType const frozenAuraTypes[] = { SPELL_AURA_MOD_ROOT, SPELL_AURA_MOD_STUN, SPELL_AURA_NONE };

/**
 * @brief Applies or removes direct possession control over the target.
 *
 * @param apply True to possess the target; false to release it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModPossess(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    // not possess yourself
    if (GetCasterGuid() == target->GetObjectGuid())
    {
        return;
    }

    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Player* p_caster = (Player*)caster;
    Camera& camera = p_caster->GetCamera();

    if (apply)
    {
        target->addUnitState(UNIT_STAT_CONTROLLED);

        target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_POSSESSED);
        target->SetCharmerGuid(p_caster->GetObjectGuid());
        target->setFaction(p_caster->getFaction());

        // target should became visible at SetView call(if not visible before):
        // otherwise client\p_caster will ignore packets from the target(SetClientControl for example)
        camera.SetView(target);

        p_caster->SetCharm(target);
        p_caster->SetClientControl(target, 1);
        p_caster->SetMover(target);

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();

        if (CharmInfo* charmInfo = target->InitCharmInfo(target))
        {
            charmInfo->InitPossessCreateSpells();
            charmInfo->SetReactState(REACT_PASSIVE);
            charmInfo->SetCommandState(COMMAND_STAY);
        }

        p_caster->PossessSpellInitialize();

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)target)->AIM_Initialize();
        }
        else if (target->GetTypeId() == TYPEID_PLAYER)
        {
            ((Player*)target)->SetClientControl(target, 0);
        }
    }
    else
    {
        p_caster->SetCharm(NULL);

        p_caster->SetClientControl(target, 0);
        p_caster->SetMover(NULL);

        // there is a possibility that target became invisible for client\p_caster at ResetView call:
        // it must be called after movement control unapplying, not before! the reason is same as at aura applying
        camera.ResetView();

        p_caster->RemovePetActionBar();

        // on delete only do caster related effects
        if (m_removeMode == AURA_REMOVE_BY_DELETE)
        {
            return;
        }

        target->clearUnitState(UNIT_STAT_CONTROLLED);

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();

        target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_POSSESSED);

        target->SetCharmerGuid(ObjectGuid());

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            ((Player*)target)->setFactionForRace(target->getRace());
            ((Player*)target)->SetClientControl(target, 1);
        }
        else if (target->GetTypeId() == TYPEID_UNIT)
        {
            CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();
            target->setFaction(cinfo->FactionAlliance);
        }

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)target)->AIM_Initialize();
            target->AttackedBy(caster);
        }
    }
}

/**
 * @brief Applies or removes possession control over a pet.
 *
 * @param apply True to possess the pet; false to release it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModPossessPet(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Unit* target = GetTarget();
    if (target->GetTypeId() != TYPEID_UNIT || !((Creature*)target)->IsPet())
    {
        return;
    }

    Pet* pet = (Pet*)target;

    Player* p_caster = (Player*)caster;
    Camera& camera = p_caster->GetCamera();

    if (apply)
    {
        pet->addUnitState(UNIT_STAT_CONTROLLED);

        // target should became visible at SetView call(if not visible before):
        // otherwise client\p_caster will ignore packets from the target(SetClientControl for example)
        camera.SetView(pet);

        p_caster->SetCharm(pet);
        p_caster->SetClientControl(pet, 1);
        ((Player*)caster)->SetMover(pet);

        pet->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_POSSESSED);

        pet->StopMoving();
        pet->GetMotionMaster()->Clear(false);
        pet->GetMotionMaster()->MoveIdle();
    }
    else
    {
        p_caster->SetCharm(NULL);
        p_caster->SetClientControl(pet, 0);
        p_caster->SetMover(NULL);

        // there is a possibility that target became invisible for client\p_caster at ResetView call:
        // it must be called after movement control unapplying, not before! the reason is same as at aura applying
        camera.ResetView();

        // on delete only do caster related effects
        if (m_removeMode == AURA_REMOVE_BY_DELETE)
        {
            return;
        }

        pet->clearUnitState(UNIT_STAT_CONTROLLED);

        pet->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_POSSESSED);

        pet->AttackStop();

        // out of range pet dismissed
        if (!pet->IsWithinDistInMap(p_caster, pet->GetMap()->GetVisibilityDistance()))
        {
            p_caster->RemovePet(PET_SAVE_REAGENTS);
        }
        else
        {
            pet->GetMotionMaster()->MoveFollow(caster, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
        }
    }
}

/**
 * @brief Applies or removes charm control over the target.
 *
 * @param apply True to charm the target; false to release it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModCharm(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
    {
        return;
    }

    // not charm yourself
    if (GetCasterGuid() == target->GetObjectGuid())
    {
        return;
    }

    Unit* caster = GetCaster();
    if (!caster)
    {
        return;
    }

    if (apply)
    {
        // is it really need after spell check checks?
        target->RemoveSpellsCausingAura(SPELL_AURA_MOD_CHARM, GetHolder());
        target->RemoveSpellsCausingAura(SPELL_AURA_MOD_POSSESS, GetHolder());

        target->SetCharmerGuid(GetCasterGuid());
        target->setFaction(caster->getFaction());
        target->CastStop(target == caster ? GetId() : 0);
        caster->SetCharm(target);

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();
        target->GetMotionMaster()->MovementExpired(true);

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)target)->AIM_Initialize();
            CharmInfo* charmInfo = target->InitCharmInfo(target);
            charmInfo->InitCharmCreateSpells();
            charmInfo->SetReactState(REACT_DEFENSIVE);

            if (caster->GetTypeId() == TYPEID_PLAYER && caster->getClass() == CLASS_WARLOCK)
            {
                CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();
                if (cinfo && cinfo->CreatureType == CREATURE_TYPE_DEMON)
                {
                    // creature with pet number expected have class set
                    if (target->GetByteValue(UNIT_FIELD_BYTES_0, 1) == 0)
                    {
                        if (cinfo->UnitClass == 0)
                        {
                            sLog.outErrorDb("Creature (Entry: %u) have UnitClass = 0 but used in charmed spell, that will be result client crash.", cinfo->Entry);
                        }
                        else
                        {
                            sLog.outError("Creature (Entry: %u) have UnitClass = %u but at charming have class 0!!! that will be result client crash.", cinfo->Entry, cinfo->UnitClass);
                        }

                        target->SetByteValue(UNIT_FIELD_BYTES_0, 1, CLASS_MAGE);
                    }

                    // just to enable stat window
                    charmInfo->SetPetNumber(sObjectMgr.GeneratePetNumber(), true);
                    // if charmed two demons the same session, the 2nd gets the 1st one's name
                    target->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(NULL)));
                }
            }
        }
        else if (Player *plTarget = target->ToPlayer())
        {
            plTarget->SetClientControl(plTarget, 0);
        }

        if (caster->GetTypeId() == TYPEID_PLAYER)
        {
            ((Player*)caster)->CharmSpellInitialize();
        }
    }
    else
    {
        target->SetCharmerGuid(ObjectGuid());

        if (Player *plTarget = target->ToPlayer())
        {
            plTarget->SetClientControl(plTarget, 1);
            plTarget->setFactionForRace(target->getRace());
        }
        else
        {
            CreatureInfo const* cinfo = ((Creature*)target)->GetCreatureInfo();

            // restore faction
            if (((Creature*)target)->IsPet())
            {
                if (Unit* owner = target->GetOwner())
                {
                    target->setFaction(owner->getFaction());
                }
                else if (cinfo)
                {
                    target->setFaction(cinfo->FactionAlliance);
                }
            }
            else if (cinfo)                             // normal creature
            {
                target->setFaction(cinfo->FactionAlliance);
            }

            // restore UNIT_FIELD_BYTES_0
            if (cinfo && caster->GetTypeId() == TYPEID_PLAYER && caster->getClass() == CLASS_WARLOCK && cinfo->CreatureType == CREATURE_TYPE_DEMON)
            {
                // DB must have proper class set in field at loading, not req. restore, including workaround case at apply
                // m_target->SetByteValue(UNIT_FIELD_BYTES_0, 1, cinfo->unit_class);

                if (target->GetCharmInfo())
                {
                    target->GetCharmInfo()->SetPetNumber(0, true);
                }
                else
                {
                    sLog.outError("Aura::HandleModCharm: target (GUID: %u TypeId: %u) has a charm aura but no charm info!", target->GetGUIDLow(), target->GetTypeId());
                }
            }
        }

        caster->SetCharm(NULL);

        if (caster->GetTypeId() == TYPEID_PLAYER)
        {
            ((Player*)caster)->RemovePetActionBar();
        }

        target->CombatStop(true);
        target->DeleteThreatList();
        target->GetHostileRefManager().deleteReferences();
        target->GetMotionMaster()->MovementExpired(true);

        if (target->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)target)->AIM_Initialize();
            //target->AttackedBy(caster);
        }
    }
}

/**
 * @brief Applies or removes the confused control state.
 *
 * @param apply True to apply confusion; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModConfuse(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    // Do not remove it yet if more effects are up, do it for the last effect
    if (!apply && GetTarget()->HasAuraType(SPELL_AURA_MOD_CONFUSE))
    {
        return;
    }

    GetTarget()->SetConfused(apply, GetCasterGuid(), GetId());
}

/**
 * @brief Applies or removes the feared control state.
 *
 * @param apply True to apply fear; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModFear(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    // Do not remove it yet if more effects are up, do it for the last effect
    if (!apply && GetTarget()->HasAuraType(SPELL_AURA_MOD_FEAR))
    {
        return;
    }

    GetTarget()->SetFeared(apply, GetCasterGuid(), GetId());
}

/**
 * @brief Applies or removes feign death handling on the target.
 *
 * @param apply True to apply feign death; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleFeignDeath(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    GetTarget()->SetFeignDeath(apply, GetCasterGuid());
}

/**
 * @brief Applies or removes the disarmed state and updates attack timing.
 *
 * @param apply True to disarm; false to remove disarm.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModDisarm(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!apply && target->HasAuraType(GetModifier()->m_auraname))
    {
        return;
    }

    target->ApplyModFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISARMED, apply);

    if (target->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    // main-hand attack speed already set to special value for feral form already and don't must change and reset at remove.
    if (target->IsInFeralForm())
    {
        return;
    }

    if (apply)
    {
        target->SetAttackTime(BASE_ATTACK, BASE_ATTACK_TIME);
    }
    else
    {
        ((Player*)target)->SetRegularAttackTime();
    }

    target->UpdateDamagePhysical(BASE_ATTACK);
}

/**
 * @brief Applies or removes the stunned state and related frozen handling.
 *
 * @param apply True to stun; false to remove stun.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModStun(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        // Frost stun aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        target->SetStunned(true);
    }
    else
    {
        // Frost stun aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0]; *itr != SPELL_AURA_NONE; ++itr)
            {
                Unit::AuraList const& auras = target->GetAurasByType(*itr);
                for (Unit::AuraList::const_iterator i = auras.begin(); i != auras.end(); ++i)
                {
                    if (GetSpellSchoolMask((*i)->GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                {
                    break;
                }
            }

            if (!found_another)
            {
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
            }
        }

        // Real remove called after current aura remove from lists, check if other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_STUN))
        {
            return;
        }

        target->SetStunned(false);

        // Wyvern Sting
        if (GetSpellProto()->SpellClassSet == SPELLFAMILY_HUNTER && GetSpellProto()->SpellClassMask & UI64LIT(0x00010000))
        {
            Unit* caster = GetCaster();
            if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            uint32 spell_id;
            switch (GetId())
            {
                case 19386: spell_id = 24131; break;
                case 24132: spell_id = 24134; break;
                case 24133: spell_id = 24135; break;
                default:
                    sLog.outError("Spell selection called for unexpected original spell %u, new spell for this spell family?", GetId());
                    return;
            }

            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spell_id);

            if (!spellInfo)
            {
                return;
            }

            caster->CastSpell(target, spellInfo, true, NULL, this);
            return;
        }
    }
    // special rule for allowing Improved Sap
    if (GetSpellProto()->IsFitToFamily(SPELLFAMILY_ROGUE, UI64LIT(0x80)))
    {
        target->SetInDummyCombatState(apply);
        if (!apply)
        {
            target->GetThreatManager().setDirty(true);
            GetCaster()->GetThreatManager().setDirty(true);
        }
    }
}

/**
 * @brief Applies or removes stealth flags and visibility changes.
 *
 * @param apply True to enter stealth; false to leave it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModStealth(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        // drop flag at stealth in bg
        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        // only at real aura add
        if (Real)
        {
            target->SetByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAGS_CREEP);

            if (target->GetTypeId() == TYPEID_PLAYER)
            {
                target->SetByteFlag(PLAYER_FIELD_BYTES2, 1, PLAYER_FIELD_BYTE2_STEALTH);
            }

            // apply only if not in GM invisibility (and overwrite invisibility state)
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
                target->SetVisibility(VISIBILITY_GROUP_STEALTH);
            }

            // for RACE_NIGHTELF stealth
            if (target->GetTypeId() == TYPEID_PLAYER && GetId() == 20580)
            {
                target->CastSpell(target, 21009, true, NULL, this);
            }
        }
    }
    else
    {
        // for RACE_NIGHTELF stealth
        if (Real && target->GetTypeId() == TYPEID_PLAYER && GetId() == 20580)
        {
            target->RemoveAurasDueToSpell(21009);
        }

        // Remove vanish buff if user cancel stealth
        if (m_removeMode == AURA_REMOVE_BY_CANCEL)
        {
            target->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
        }

        // only at real aura remove of _last_ SPELL_AURA_MOD_STEALTH
        if (Real && !target->HasAuraType(SPELL_AURA_MOD_STEALTH))
        {
            // if no GM invisibility
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                target->RemoveByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAGS_CREEP);

                if (target->GetTypeId() == TYPEID_PLAYER)
                {
                    target->RemoveByteFlag(PLAYER_FIELD_BYTES2, 1, PLAYER_FIELD_BYTE2_STEALTH);
                }

                // restore invisibility if any
                if (target->HasAuraType(SPELL_AURA_MOD_INVISIBILITY))
                {
                    target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
                    target->SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
                }
                else
                {
                    target->SetVisibility(VISIBILITY_ON);
                }
            }
        }
    }
}

/**
 * @brief Applies or removes invisibility state and visibility flags.
 *
 * @param apply True to apply invisibility; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleInvisibility(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        target->m_invisibilityMask |= (1 << m_modifier.m_miscvalue);

        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        if (Real && target->GetTypeId() == TYPEID_PLAYER)
        {
            // apply glow vision
            target->SetByteFlag(PLAYER_FIELD_BYTES2, 1, PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);
        }

        // apply only if not in GM invisibility and not stealth
        if (target->GetVisibility() == VISIBILITY_ON)
        {
            // Aura not added yet but visibility code expect temporary add aura
            target->SetVisibility(VISIBILITY_GROUP_NO_DETECT);
            target->SetVisibility(VISIBILITY_GROUP_INVISIBILITY);
        }
    }
    else
    {
        // recalculate value at modifier remove (current aura already removed)
        target->m_invisibilityMask = 0;
        Unit::AuraList const& auras = target->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY);
        for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            target->m_invisibilityMask |= (1 << (*itr)->GetModifier()->m_miscvalue);
        }

        // only at real aura remove and if not have different invisibility auras.
        if (Real && target->m_invisibilityMask == 0)
        {
            // remove glow vision
            if (target->GetTypeId() == TYPEID_PLAYER)
            {
                target->RemoveByteFlag(PLAYER_FIELD_BYTES2, 1, PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW);
            }

            // apply only if not in GM invisibility & not stealthed while invisible
            if (target->GetVisibility() != VISIBILITY_OFF)
            {
                // if have stealth aura then already have stealth visibility
                if (!target->HasAuraType(SPELL_AURA_MOD_STEALTH))
                {
                    target->SetVisibility(VISIBILITY_ON);
                }
            }
        }
    }
}

/**
 * @brief Applies or removes invisibility detection masks.
 *
 * @param apply True to apply detection; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleInvisibilityDetect(bool apply, bool Real)
{
    Unit* target = GetTarget();

    if (apply)
    {
        target->m_detectInvisibilityMask |= (1 << m_modifier.m_miscvalue);
    }
    else
    {
        // recalculate value at modifier remove (current aura already removed)
        target->m_detectInvisibilityMask = 0;
        Unit::AuraList const& auras = target->GetAurasByType(SPELL_AURA_MOD_INVISIBILITY_DETECTION);
        for (Unit::AuraList::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
        {
            target->m_detectInvisibilityMask |= (1 << (*itr)->GetModifier()->m_miscvalue);
        }
    }
    if (Real && target->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)target)->GetCamera().UpdateVisibilityForOwner();
    }
}

/**
 * @brief Applies or removes the detect amore player flag.
 *
 * @param apply True to apply the flag; false to remove it.
 * @param real Unused.
 */
void Aura::HandleDetectAmore(bool apply, bool /*real*/)
{
    GetTarget()->ApplyModByteFlag(PLAYER_FIELD_BYTES2, 1, (PLAYER_FIELD_BYTE2_DETECT_AMORE_0 << m_modifier.m_amount), apply);
}

/**
 * @brief Applies or removes the root state and related frozen handling.
 *
 * @param apply True to root the target; false to unroot it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModRoot(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        // Frost root aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            target->ModifyAuraState(AURA_STATE_FROZEN, apply);
        }

        target->addUnitState(UNIT_STAT_ROOT);

        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            target->SetRoot(true);

            // Clear unit movement flags
            ((Player*)target)->m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
        }
        else
        {
            target->StopMoving();
        }
    }
    else
    {
        // Frost root aura -> freeze/unfreeze target
        if (GetSpellSchoolMask(GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
        {
            bool found_another = false;
            for (AuraType const* itr = &frozenAuraTypes[0]; *itr != SPELL_AURA_NONE; ++itr)
            {
                Unit::AuraList const& auras = target->GetAurasByType(*itr);
                for (Unit::AuraList::const_iterator i = auras.begin(); i != auras.end(); ++i)
                {
                    if (GetSpellSchoolMask((*i)->GetSpellProto()) & SPELL_SCHOOL_MASK_FROST)
                    {
                        found_another = true;
                        break;
                    }
                }
                if (found_another)
                {
                    break;
                }
            }

            if (!found_another)
            {
                target->ModifyAuraState(AURA_STATE_FROZEN, apply);
            }
        }

        // Real remove called after current aura remove from lists, check if other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_ROOT))
        {
            return;
        }

    }

    target->SetImmobilizedState(apply);
}

/**
 * @brief Applies or removes the silenced state and interrupts affected casts.
 *
 * @param apply True to apply silence; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModSilence(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED);
        // Stop cast only spells vs PreventionType == SPELL_PREVENTION_TYPE_SILENCE
        for (uint32 i = CURRENT_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
        {
            if (Spell* spell = target->GetCurrentSpell(CurrentSpellTypes(i)))
            {
                if (spell->m_spellInfo->PreventionType == SPELL_PREVENTION_TYPE_SILENCE)
                    // Stop spells on prepare or casting state
                {
                    target->InterruptSpell(CurrentSpellTypes(i), false);
                }
            }
        }
    }
    else
    {
        // Real remove called after current aura remove from lists, check if other similar auras active
        if (target->HasAuraType(SPELL_AURA_MOD_SILENCE))
        {
            return;
        }

        target->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SILENCED);
    }
}

/**
 * @brief Applies or removes school-based threat generation modifiers.
 *
 * @param apply True to apply the modifier; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModThreat(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target->IsAlive())
    {
        return;
    }

    int level_diff = 0;
    int multiplier = 0;
    switch (GetId())
    {
        // Arcane Shroud
        case 26400:
            level_diff = target->getLevel() - 60;
            multiplier = 2;
            break;
        // The Eye of Diminution
        case 28862:
            level_diff = target->getLevel() - 60;
            multiplier = 1;
            break;
    }

    if (level_diff > 0)
    {
        m_modifier.m_amount += multiplier * level_diff;
    }

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        for (int8 x = 0; x < MAX_SPELL_SCHOOL; ++x)
        {
            if (m_modifier.m_miscvalue & int32(1 << x))
            {
                ApplyPercentModFloatVar(target->m_threatModifier[x], float(m_modifier.m_amount), apply);
            }
        }
    }
}

/**
 * @brief Adds or removes a flat threat amount toward the caster.
 *
 * @param apply True to add threat; false to subtract it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModTotalThreat(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target->IsAlive() || target->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Unit* caster = GetCaster();

    if (!caster || !caster->IsAlive())
    {
        return;
    }

    float threatMod = apply ? float(m_modifier.m_amount) : float(-m_modifier.m_amount);

    target->GetHostileRefManager().threatAssist(caster, threatMod, GetSpellProto());
}

/**
 * @brief Applies or removes taunt behavior on the target.
 *
 * @param apply True to taunt the target; false to fade the taunt.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleModTaunt(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (!target->IsAlive() || !target->CanHaveThreatList())
    {
        return;
    }

    Unit* caster = GetCaster();

    if (!caster || !caster->IsAlive())
    {
        return;
    }

    if (apply)
    {
        target->TauntApply(caster);
    }
    else
    {
        // When taunt aura fades out, mob will switch to previous target if current has less than 1.1 * secondthreat
        target->TauntFadeOut(caster);
    }
}

/*********************************************************/
/***                  MODIFY SPEED                     ***/
/*********************************************************/
void Aura::HandleAuraModIncreaseSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
    {
        return;
    }

    if (Unit* caster = GetCaster())
    {
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(GetSpellProto()->ID, SPELLMOD_SPEED, m_modifier.m_amount);
        }
    }

    GetTarget()->UpdateSpeed(MOVE_RUN, true);
}

/**
 * @brief Refreshes mounted movement speed after aura changes.
 *
 * @param apply Unused.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModIncreaseMountedSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
    {
        return;
    }

    GetTarget()->UpdateSpeed(MOVE_RUN, true);
}

/**
 * @brief Refreshes swim speed after aura changes.
 *
 * @param apply Unused.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModIncreaseSwimSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
    {
        return;
    }

    GetTarget()->UpdateSpeed(MOVE_SWIM, true);
}

/**
 * @brief Applies or removes movement slowing effects and refreshes speeds.
 *
 * @param apply True to apply the slow; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModDecreaseSpeed(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
    {
        return;
    }

    if (Unit* caster = GetCaster())
    {
        if (Player* modOwner = caster->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(GetSpellProto()->ID, SPELLMOD_SPEED, m_modifier.m_amount);
        }
    }

    Unit* target = GetTarget();

    target->UpdateSpeed(MOVE_RUN, true);
    target->UpdateSpeed(MOVE_SWIM, true);
}

/**
 * @brief Refreshes movement to normal-speed handling after aura changes.
 *
 * @param apply Unused.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModUseNormalSpeed(bool /*apply*/, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    target->UpdateSpeed(MOVE_RUN, true);
    target->UpdateSpeed(MOVE_SWIM, true);
}

void Aura::HandleModMechanicImmunity(bool apply, bool /*Real*/)
{
    uint32 misc  = m_modifier.m_miscvalue;
    Unit* target = GetTarget();

    if (apply && GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        uint32 mechanic = 1 << (misc - 1);

        target->RemoveAurasAtMechanicImmunity(mechanic, GetId());
    }

    target->ApplySpellImmune(GetId(), IMMUNITY_MECHANIC, misc, apply);
}

/**
 * @brief Applies or removes immunity to a mechanic mask.
 *
 * @param apply True to apply immunity; false to remove it.
 * @param Real Unused.
 */
void Aura::HandleModMechanicImmunityMask(bool apply, bool /*Real*/)
{
    uint32 mechanic  = m_modifier.m_miscvalue;

    if (apply && GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        GetTarget()->RemoveAurasAtMechanicImmunity(mechanic, GetId());
    }

    // check implemented in Unit::IsImmuneToSpell and Unit::IsImmuneToSpellEffect
}

// this method is called whenever we add / remove aura which gives m_target some imunity to some spell effect
void Aura::HandleAuraModEffectImmunity(bool apply, bool /*Real*/)
{
    Unit* target = GetTarget();

    // when removing flag aura, handle flag drop
    if (!apply && target->GetTypeId() == TYPEID_PLAYER &&
        (GetSpellProto()->AuraInterruptFlags & AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION))
    {
        Player* player = (Player*)target;
        if (BattleGround* bg = player->GetBattleGround())
        {
            bg->EventPlayerDroppedFlag(player);
        }
        else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
        {
            outdoorPvP->HandleDropFlag(player, GetSpellProto()->ID);
        }
    }

    target->ApplySpellImmune(GetId(), IMMUNITY_EFFECT, m_modifier.m_miscvalue, apply);
}

/**
 * @brief Applies or removes immunity to a specific aura state.
 *
 * @param apply True to apply immunity; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModStateImmunity(bool apply, bool Real)
{
    if (apply && Real && GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY))
    {
        Unit::AuraList const& auraList = GetTarget()->GetAurasByType(AuraType(m_modifier.m_miscvalue));
        for (Unit::AuraList::const_iterator itr = auraList.begin(); itr != auraList.end();)
        {
            if (auraList.front() != this)                   // skip itself aura (it already added)
            {
                GetTarget()->RemoveAurasDueToSpell(auraList.front()->GetId());
                itr = auraList.begin();
            }
            else
            {
                ++itr;
            }
        }
    }

    GetTarget()->ApplySpellImmune(GetId(), IMMUNITY_STATE, m_modifier.m_miscvalue, apply);
}

/**
 * @brief Applies or removes spell school immunity and clears affected auras when needed.
 *
 * @param apply True to apply immunity; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModSchoolImmunity(bool apply, bool Real)
{
    Unit* target = GetTarget();
    target->ApplySpellImmune(GetId(), IMMUNITY_SCHOOL, m_modifier.m_miscvalue, apply);

    // remove all flag auras (they are positive, but they must be removed when you are immune)
    if (GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY) && GetSpellProto()->HasAttribute(SPELL_ATTR_EX2_DAMAGE_REDUCED_SHIELD))
    {
        target->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);
    }

    // TODO: optimalize this cycle - use RemoveAurasWithInterruptFlags call or something else
    if (Real && apply &&
        GetSpellProto()->HasAttribute(SPELL_ATTR_EX_DISPEL_AURAS_ON_IMMUNITY) &&
        IsPositiveSpell(GetId()))                    // Only positive immunity removes auras
    {
        uint32 school_mask = m_modifier.m_miscvalue;
        Unit::SpellAuraHolderMap& Auras = target->GetSpellAuraHolderMap();
        for (Unit::SpellAuraHolderMap::iterator iter = Auras.begin(), next; iter != Auras.end(); iter = next)
        {
            next = iter;
            ++next;
            SpellEntry const* spell = iter->second->GetSpellProto();
            if ((GetSpellSchoolMask(spell) & school_mask) &&   // Check for school mask
                !spell->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) &&   // Spells unaffected by invulnerability
                !iter->second->IsPositive() &&         // Don't remove positive spells
                spell->ID != GetId())                  // Don't remove self
            {
                target->RemoveAurasDueToSpell(spell->ID);
                if (Auras.empty())
                {
                    break;
                }
                else
                {
                    next = Auras.begin();
                }
            }
        }
    }
    if (Real && GetSpellProto()->Mechanic == MECHANIC_BANISH)
    {
        if (apply)
        {
            target->addUnitState(UNIT_STAT_ISOLATED);
        }
        else
        {
            target->clearUnitState(UNIT_STAT_ISOLATED);
        }
    }
}

/**
 * @brief Applies or removes immunity to a damage school mask.
 *
 * @param apply True to apply immunity; false to remove it.
 * @param Real Unused.
 */
void Aura::HandleAuraModDmgImmunity(bool apply, bool /*Real*/)
{
    GetTarget()->ApplySpellImmune(GetId(), IMMUNITY_DAMAGE, m_modifier.m_miscvalue, apply);
}

/**
 * @brief Applies or removes dispel immunity for the aura's dispel type.
 *
 * @param apply True to apply immunity; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModDispelImmunity(bool apply, bool Real)
{
    // all applied/removed only at real aura add/remove
    if (!Real)
    {
        return;
    }

    GetTarget()->ApplySpellDispelImmunity(GetSpellProto(), DispelType(m_modifier.m_miscvalue), apply);
}
