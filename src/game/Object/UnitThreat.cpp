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

bool Unit::CanHaveThreatList(bool ignoreAliveState/*=false*/) const
{
    // only creatures can have threat list
    if (GetTypeId() != TYPEID_UNIT)
    {
        return false;
    }

    // only alive units can have threat list
    if (!IsAlive() && !ignoreAliveState)
    {
        return false;
    }

    Creature const* creature = ((Creature const*)this);

    // totems can not have threat list
    if (creature->IsTotem())
    {
        return false;
    }

    // pets can not have a threat list, unless they are controlled by a creature
    if (creature->IsPet() && creature->GetOwnerGuid().IsPlayer())
    {
        return false;
    }

    // charmed units can not have a threat list if charmed by player
    if (creature->GetCharmerGuid().IsPlayer())
    {
        return false;
    }

    return true;
}

float Unit::ApplyTotalThreatModifier(float threat, SpellSchoolMask schoolMask)
{
    if (!HasAuraType(SPELL_AURA_MOD_THREAT))
    {
        return threat;
    }

    if (schoolMask == SPELL_SCHOOL_MASK_NONE)
    {
        return threat;
    }

    SpellSchools school = GetFirstSchoolInMask(schoolMask);

    return threat * m_threatModifier[school];
}

void Unit::AddThreat(Unit* pVictim, float threat /*= 0.0f*/, bool crit /*= false*/, SpellSchoolMask schoolMask /*= SPELL_SCHOOL_MASK_NONE*/, SpellEntry const* threatSpell /*= NULL*/)
{
    // Only mobs can manage threat lists
    if (CanHaveThreatList())
    {
        m_ThreatManager.addThreat(pVictim, threat, crit, schoolMask, threatSpell);
    }
}

void Unit::DeleteThreatList()
{
    m_ThreatManager.clearReferences();
}

void Unit::TauntApply(Unit* taunter)
{
    MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT);

    if (!taunter || (taunter->GetTypeId() == TYPEID_PLAYER && ((Player*)taunter)->isGameMaster()))
    {
        return;
    }

    if (!CanHaveThreatList())
    {
        return;
    }

    Unit* target = getVictim();

    if (target && target == taunter)
    {
        return;
    }

    // Only attack taunter if this is a valid target
    if (!hasUnitState(UNIT_STAT_STUNNED | UNIT_STAT_DIED) && !IsSecondChoiceTarget(taunter, true))
    {
        if (GetTargetGuid() || !target)
        {
            SetInFront(taunter);
        }

        if (((Creature*)this)->AI())
        {
            ((Creature*)this)->AI()->AttackStart(taunter);
        }
    }

    m_ThreatManager.tauntApply(taunter);
}

void Unit::TauntFadeOut(Unit* taunter)
{
    MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT);

    if (!taunter || (taunter->GetTypeId() == TYPEID_PLAYER && ((Player*)taunter)->isGameMaster()))
    {
        return;
    }

    if (!CanHaveThreatList())
    {
        return;
    }

    Unit* target = getVictim();

    if (!target || target != taunter)
    {
        return;
    }

    if (m_ThreatManager.isThreatListEmpty())
    {
        m_fixateTargetGuid.Clear();

        if (((Creature*)this)->AI())
        {
            ((Creature*)this)->AI()->EnterEvadeMode();
        }

        if (InstanceData* mapInstance = GetInstanceData())
        {
            mapInstance->OnCreatureEvade((Creature*)this);
        }

        if (m_isCreatureLinkingTrigger)
        {
            GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_EVADE, (Creature*)this);
        }

        return;
    }

    m_ThreatManager.tauntFadeOut(taunter);
    target = m_ThreatManager.getHostileTarget();

    if (target && target != taunter)
    {
        if (GetTargetGuid())
        {
            SetInFront(target);
        }

        if (((Creature*)this)->AI())
        {
            ((Creature*)this)->AI()->AttackStart(target);
        }
    }
}

//======================================================================
/// if pVictim is given, the npc will fixate onto pVictim, if NULL it will remove current fixation
void Unit::FixateTarget(Unit* pVictim)
{
    if (!pVictim)                                           // Remove Fixation
    {
        m_fixateTargetGuid.Clear();
    }
    else if (pVictim->IsTargetableForAttack())              // Apply Fixation
    {
        m_fixateTargetGuid = pVictim->GetObjectGuid();
    }

    // Start attacking the fixated target or the next proper one
    SelectHostileTarget();
}

bool Unit::IsSecondChoiceTarget(Unit* pTarget, bool checkThreatArea)
{
    MANGOS_ASSERT(pTarget && GetTypeId() == TYPEID_UNIT);

    return pTarget->IsImmuneToDamage(GetMeleeDamageSchoolMask()) ||
        pTarget->hasNegativeAuraWithInterruptFlag(AURA_INTERRUPT_FLAG_DAMAGE) ||
        (checkThreatArea && ((Creature*)this)->IsOutOfThreatArea(pTarget));
}

bool Unit::SelectHostileTarget()
{
    // function provides main threat functionality
    // next-victim-selection algorithm and evade mode are called
    // threat list sorting etc.

    MANGOS_ASSERT(GetTypeId() == TYPEID_UNIT);

    if (!this->IsAlive())
    {
        return false;
    }

    // This function only useful once AI has been initialized
    if (!((Creature*)this)->AI())
    {
        return false;
    }

    Unit* target = NULL;
    Unit* oldTarget = getVictim();

    // first check if we should fixate a target
    if (m_fixateTargetGuid)
    {
        if (oldTarget && oldTarget->GetObjectGuid() == m_fixateTargetGuid)
        {
            target = oldTarget;
        }
        else
        {
            Unit* pFixateTarget = GetMap()->GetUnit(m_fixateTargetGuid);
            if (pFixateTarget && pFixateTarget->IsAlive() && !IsSecondChoiceTarget(pFixateTarget, true))
            {
                target = pFixateTarget;
            }
        }
    }
    // then checking if we have some taunt on us
    if (!target)
    {
        const AuraList& tauntAuras = GetAurasByType(SPELL_AURA_MOD_TAUNT);
        Unit* caster;

        // Find first available taunter target
        // Auras are pushed_back, last caster will be on the end
        for (AuraList::const_reverse_iterator aura = tauntAuras.rbegin(); aura != tauntAuras.rend(); ++aura)
        {
            if ((caster = (*aura)->GetCaster()) && caster->IsInMap(this) &&
                caster->IsTargetableForAttack() && caster->isInAccessablePlaceFor((Creature*)this) &&
                !IsSecondChoiceTarget(caster, true))
            {
                target = caster;
                break;
            }
        }
    }

    // No valid fixate target, taunt aura or taunt aura caster is dead, standard target selection
    if (!target && !m_ThreatManager.isThreatListEmpty())
    {
        target = m_ThreatManager.getHostileTarget();
    }

    if (target)
    {
        if (!hasUnitState(UNIT_STAT_CAN_NOT_REACT_OR_LOST_CONTROL))
        {
            SetInFront(target);
            if (oldTarget != target)
            {
                ((Creature*)this)->AI()->AttackStart(target);
            }

            // check if currently selected target is reachable
            // NOTE: path alrteady generated from AttackStart()
            if (!GetMotionMaster()->GetCurrent()->IsReachable())
            {
                // remove all taunts
                RemoveSpellsCausingAura(SPELL_AURA_MOD_TAUNT);

                if (m_ThreatManager.getThreatList().size() < 2)
                {
                    // only one target in list, we have to evade after timer
                    // TODO: make timer - inside Creature class
                    ((Creature*)this)->AI()->EnterEvadeMode();
                }
                else
                {
                    // remove unreachable target from our threat list
                    // next iteration we will select next possible target
                    m_HostileRefManager.deleteReference(target);
                    m_ThreatManager.modifyThreatPercent(target, -101);
                    // remove target from current attacker, do not exit combat settings
                    AttackStop(true);
                }

                return false;
            }
        }
        return true;
    }

    // no target but something prevent go to evade mode
    if (!IsInCombat() || HasAuraType(SPELL_AURA_MOD_TAUNT) || m_dummyCombatState)
    {
        return false;
    }

    // last case when creature don't must go to evade mode:
    // it in combat but attacker not make any damage and not enter to aggro radius to have record in threat list
    // for example at owner command to pet attack some far away creature
    // Note: creature not have targeted movement generator but have attacker in this case
    if (GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
    {
        for (AttackerSet::const_iterator itr = m_attackers.begin(); itr != m_attackers.end(); ++itr)
        {
            if ((*itr)->IsInMap(this) && (*itr)->IsTargetableForAttack() && (*itr)->isInAccessablePlaceFor((Creature*)this))
            {
                return false;
            }
        }
    }

    // enter in evade mode in other case
    m_fixateTargetGuid.Clear();
    ((Creature*)this)->AI()->EnterEvadeMode();

    if (InstanceData* mapInstance = GetInstanceData())
    {
        mapInstance->OnCreatureEvade((Creature*)this);
    }

    if (m_isCreatureLinkingTrigger)
    {
        GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_EVADE, (Creature*)this);
    }

    return false;
}
