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

/**
 * ScriptData
 * SDName:      Boss_Garr
 * SD%Complete: 50
 * SDComment:   Garr's enrage is missing
 * SDCategory:  Molten Core
 * EndScriptData
 */

#include "precompiled.h"
#include "molten_core.h"

enum
{
    // Garr spells
    SPELL_ANTIMAGICPULSE        = 19492,
    SPELL_MAGMASHACKLES         = 19496,
    SPELL_ERUPTION_TRIGGER      = 20482,    // target script, dispel and permanent immune to banish anywhere on map
    SPELL_ENRAGE_TRIGGER        = 19515,    // target script, effect dummy anywhere on map

    // Add spells
    SPELL_ERUPTION              = 19497,
    SPELL_MASSIVE_ERUPTION      = 20483,                    // TODO possible on death
    SPELL_IMMOLATE              = 20294,
};

struct boss_garr : public CreatureScript
{
    boss_garr() : CreatureScript("boss_garr") {}

    struct boss_garrAI : public ScriptedAI
    {
        boss_garrAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        }

        ScriptedInstance* m_pInstance;

        uint32 m_uiAntiMagicPulseTimer;
        uint32 m_uiMagmaShacklesTimer;
        uint32 m_uiExplodeAddTimer;
        bool m_bHasFreed;

        void Reset() override
        {
            m_uiAntiMagicPulseTimer = 25 * IN_MILLISECONDS;
            m_uiMagmaShacklesTimer = 15 * IN_MILLISECONDS;
            m_uiExplodeAddTimer = 60 * IN_MILLISECONDS;
            m_bHasFreed = false;
        }

        void Aggro(Unit* /*pWho*/) override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_GARR, IN_PROGRESS);
            }
        }

        void JustDied(Unit* /*pKiller*/) override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_GARR, DONE);
            }
        }

        void JustReachedHome() override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_GARR, FAIL);
            }
        }

        void DamageTaken(Unit *damager, uint32 &damage) override
        {
            if (!m_bHasFreed && m_creature->HealthBelowPctDamaged(50, damage))
            {
                m_pInstance->SetData(TYPE_DO_FREE_GARR_ADDS, 0);
                m_bHasFreed = true;
            }
        }

        void ReceiveAIEvent(AIEventType type, Creature* pSender, Unit* /*pInvoker*/, uint32 /*uiData*/) override
        {
            if (type == AI_EVENT_CUSTOM_B && pSender == m_creature)
            {
                if (Creature* spawn = m_creature->SummonCreature(NPC_FIRESWORN, m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), m_creature->GetOrientation(), TEMPSUMMON_CORPSE_DESPAWN, true))
                    spawn->SetOwnerGuid(ObjectGuid());  // trying to prevent despawn of the summon at Garr death
                m_uiExplodeAddTimer = 25 * IN_MILLISECONDS;
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            // AntiMagicPulse_Timer
            if (m_uiAntiMagicPulseTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_ANTIMAGICPULSE) == CAST_OK)
                {
                    m_uiAntiMagicPulseTimer = urand(10 * IN_MILLISECONDS, 15 * IN_MILLISECONDS);
                }
            }
            else m_uiAntiMagicPulseTimer -= uiDiff;

            // MagmaShackles_Timer
            if (m_uiMagmaShacklesTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_MAGMASHACKLES) == CAST_OK)
                {
                    m_uiMagmaShacklesTimer = urand(8 * IN_MILLISECONDS, 12 * IN_MILLISECONDS);
                }
            }
            else m_uiMagmaShacklesTimer -= uiDiff;

            // Explode an add
            if (m_uiExplodeAddTimer < uiDiff)
            {
                if (urand(0, 1)) // 50% chance to explode, is it too much?
                {
                    ObjectGuid guid = m_pInstance->GetGuid(NPC_FIRESWORN);
                    if (Creature* firesworn = m_pInstance->instance->GetCreature(guid))
                        SendAIEvent(AI_EVENT_CUSTOM_A, firesworn, firesworn, SPELL_MASSIVE_ERUPTION);   // this is an instant explosion with no SPELL_ERUPTION_TRIGGER
                }
                m_uiExplodeAddTimer = 15 * IN_MILLISECONDS;
            }
            else m_uiExplodeAddTimer -= uiDiff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new boss_garrAI(pCreature);
    }
};

struct mob_firesworn : public CreatureScript
{
    mob_firesworn() : CreatureScript("mob_firesworn") {}

    struct mob_fireswornAI : public ScriptedAI
    {
        mob_fireswornAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        }

        ScriptedInstance* m_pInstance;

        uint32 m_uiImmolateTimer;
        uint32 m_uiSeparationCheckTimer;

        void Reset() override
        {
            m_uiImmolateTimer = urand(4 * IN_MILLISECONDS, 8 * IN_MILLISECONDS);    // These times are probably wrong
            m_uiSeparationCheckTimer = 5 * IN_MILLISECONDS;
            m_bExploding = false;
        }

        void DamageTaken(Unit* /*pDealer*/, uint32& uiDamage) override
        {
            if (!m_bExploding && m_creature->HealthBelowPctDamaged(10, uiDamage))
            {
                ReceiveAIEvent(AI_EVENT_CUSTOM_A, m_creature, m_creature, SPELL_ERUPTION);
                uiDamage = 0;
            }
        }

        void ReceiveAIEvent(AIEventType type, Creature* /*pSender*/, Unit* pInvoker, uint32 uiData) override
        {
            if (type == AI_EVENT_CUSTOM_A && pInvoker == m_creature)
            {
                m_creature->CastSpell(m_creature, uiData, true);
                m_creature->ForcedDespawn(100);
                m_bExploding = true;
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim() || m_bExploding)
            {
                return;
            }

            if (m_uiSeparationCheckTimer < uiDiff)
            {
                // Distance guesswork, but should be ok
                Creature* pGarr = m_pInstance->GetSingleCreatureFromStorage(NPC_GARR);
                if (pGarr && pGarr->IsAlive() && !m_creature->IsWithinDist2d(pGarr->GetPositionX(), pGarr->GetPositionY(), 50.0f))
                {
                    DoCastSpellIfCan(m_creature, SPELL_SEPARATION_ANXIETY, CAST_TRIGGERED);
                }

                m_uiSeparationCheckTimer = 5000;
            }
            else m_uiSeparationCheckTimer -= uiDiff;

            // Immolate_Timer
            if (m_uiImmolateTimer < uiDiff)
            {
                if (Unit* pTarget = SelectAttackTarget(ATTACKING_TARGET_RANDOM, 0, SPELL_IMMOLATE))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_IMMOLATE, CAST_TRIGGERED) == CAST_OK)
                    {
                        m_uiImmolateTimer = urand(5 * IN_MILLISECONDS, 10 * IN_MILLISECONDS);
                    }
                }
            }
            else m_uiImmolateTimer -= uiDiff;

            // Cast Erruption and let them die
            if (m_creature->GetHealthPercent() <= 10.0f)
            {
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_ERUPTION);
                m_creature->SetDeathState(JUST_DIED);
                m_creature->RemoveCorpse();
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new mob_fireswornAI(pCreature);
    }
};

void AddSC_boss_garr()
{
    Script* s;
    s = new boss_garr();
    s->RegisterSelf();
    s = new mob_firesworn();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "boss_garr";
    //pNewScript->GetAI = &GetAI_boss_garr;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "mob_firesworn";
    //pNewScript->GetAI = &GetAI_mob_firesworn;
    //pNewScript->RegisterSelf();
}
