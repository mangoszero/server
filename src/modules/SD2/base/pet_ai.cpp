/**
 * ScriptDev2 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013  ScriptDev2 <http://www.scriptdev2.com/>
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

/* ScriptData
SDName: ScriptedPetAI
SD%Complete: 50
SDComment: Intended to be used with Guardian/Protector/Minipets. Little/no control over when pet enter/leave combat. Must be considered to be under development.
SDCategory: Npc
EndScriptData */

#include "precompiled.h"
#include "pet_ai.h"

ScriptedPetAI::ScriptedPetAI(Creature* pCreature) : CreatureAI(pCreature)
{}

bool ScriptedPetAI::IsVisible(Unit* pWho) const
{
    return pWho && m_creature->IsWithinDist(pWho, VISIBLE_RANGE)
           && pWho->IsVisibleForOrDetect(m_creature, m_creature, true);
}

void ScriptedPetAI::MoveInLineOfSight(Unit* pWho)
{
    if (m_creature->getVictim())
    {
        return;
    }

    if (!m_creature->GetCharmInfo() || !m_creature->GetCharmInfo()->HasReactState(REACT_AGGRESSIVE))
    {
        return;
    }

    if (m_creature->CanInitiateAttack() && pWho->IsTargetableForAttack() &&
        m_creature->IsHostileTo(pWho) && pWho->isInAccessablePlaceFor(m_creature))
    {
        if (!m_creature->CanFly() && m_creature->GetDistanceZ(pWho) > CREATURE_Z_ATTACK_RANGE)
        {
            return;
        }

        if (m_creature->IsWithinDistInMap(pWho, m_creature->GetAttackDistance(pWho)) && m_creature->IsWithinLOSInMap(pWho))
        {
            pWho->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
            AttackStart(pWho);
        }
    }
}

void ScriptedPetAI::AttackStart(Unit* pWho)
{
    if (pWho && m_creature->Attack(pWho, true))
    {
        m_creature->GetMotionMaster()->MoveChase(pWho);
    }
}

void ScriptedPetAI::AttackedBy(Unit* pAttacker)
{
    if (m_creature->getVictim())
    {
        return;
    }

    if (m_creature->GetCharmInfo() && !m_creature->GetCharmInfo()->HasReactState(REACT_PASSIVE) &&
        m_creature->CanReachWithMeleeAttack(pAttacker))
    {
        AttackStart(pAttacker);
    }
}

void ScriptedPetAI::ResetPetCombat()
{
    Unit* pOwner = m_creature->GetCharmerOrOwner();

    if (pOwner && m_creature->GetCharmInfo() && m_creature->GetCharmInfo()->HasCommandState(COMMAND_FOLLOW))
    {
        m_creature->GetMotionMaster()->MoveFollow(pOwner, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
    }
    else
    {
        m_creature->GetMotionMaster()->Clear(false);
        m_creature->GetMotionMaster()->MoveIdle();
    }

    m_creature->AttackStop();

    debug_log("SD2: ScriptedPetAI reset pet combat and stop attack.");
    Reset();
}

void ScriptedPetAI::UpdatePetAI(const uint32 /*uiDiff*/)
{
    DoMeleeAttackIfReady();
}

void ScriptedPetAI::UpdateAI(const uint32 uiDiff)
{
    if (!m_creature->IsAlive())                             // should not be needed, IsAlive is checked in mangos before calling UpdateAI
    {
        return;
    }

    // UpdateAllies() is done in the generic PetAI in Mangos, but we can't do this from script side.
    // Unclear what side effects this has, but is something to be resolved from Mangos.

    if (m_creature->getVictim())                            // in combat
    {
        if (!m_creature->getVictim()->IsTargetableForAttack())
        {
            // target no longer valid for pet, so either attack stops or new target are selected
            // doesn't normally reach this, because of how petAi is designed in Mangos. CombatStop
            // are called before this update diff, and then pet will already have no victim.
            ResetPetCombat();
            return;
        }

        // update when in combat
        UpdatePetAI(uiDiff);
    }
    else if (m_creature->GetCharmInfo())
    {
        Unit* pOwner = m_creature->GetCharmerOrOwner();

        if (!pOwner)
        {
            return;
        }

        if (pOwner->IsInCombat() && !m_creature->GetCharmInfo()->HasReactState(REACT_PASSIVE))
        {
            // Not correct in all cases.
            // When mob initiate attack by spell, pet should not start attack before spell landed.
            AttackStart(pOwner->getAttackerForHelper());
        }
        else if (m_creature->GetCharmInfo()->HasCommandState(COMMAND_FOLLOW))
        {
            // not following, so start follow
            if (!m_creature->hasUnitState(UNIT_STAT_FOLLOW))
            {
                m_creature->GetMotionMaster()->MoveFollow(pOwner, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
            }

            // update when not in combat
            UpdatePetOOCAI(uiDiff);
        }
    }
}
