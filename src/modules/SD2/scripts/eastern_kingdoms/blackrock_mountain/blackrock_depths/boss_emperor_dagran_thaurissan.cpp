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
 * SDName:      Boss_Emperor_Dagran_Thaurissan
 * SD%Complete: 90
 * SDComment:   With script for Moira
 * SDCategory:  Blackrock Depths
 * EndScriptData
 */

#include "precompiled.h"
#include "blackrock_depths.h"

enum eEmperor
{
    FACTION_NEUTRAL             = 734,
    SAY_AGGRO                   = -1230001,
    SAY_SLAY                    = -1230002,

    SPELL_HANDOFTHAURISSAN      = 17492,
    SPELL_AVATAROFFLAME         = 15636
};

struct boss_emperor_dagran_thaurissanAI : public ScriptedAI
{
    boss_emperor_dagran_thaurissanAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiHandOfThaurissan_Timer;
    uint32 m_uiAvatarOfFlame_Timer;
    // uint32 m_uiCounter;

    void Reset() override
    {
        m_uiHandOfThaurissan_Timer = 4000;
        m_uiAvatarOfFlame_Timer = 25000;
        // m_uiCounter = 0;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoScriptText(SAY_AGGRO, m_creature);
        m_creature->CallForHelp(VISIBLE_RANGE);
    }

    void JustDied(Unit* /*pVictim*/) override
    {
        if (!m_pInstance)
        {
            return;
        }

        if (Creature* pPrincess = m_pInstance->GetSingleCreatureFromStorage(NPC_PRINCESS))
        {
            if (pPrincess->IsAlive())
            {
                pPrincess->SetFactionTemporary(FACTION_NEUTRAL, TEMPFACTION_NONE);
                pPrincess->AI()->EnterEvadeMode();
            }
        }
    }

    void KilledUnit(Unit* /*pVictim*/) override
    {
        DoScriptText(SAY_SLAY, m_creature);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        if (m_uiHandOfThaurissan_Timer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                DoCastSpellIfCan(pTarget, SPELL_HANDOFTHAURISSAN);
            }

            // 3 Hands of Thaurissan will be casted
            // if (m_uiCounter < 3)
            //{
            //    m_uiHandOfThaurissan_Timer = 1000;
            //    ++m_uiCounter;
            //}
            // else
            //{
            m_uiHandOfThaurissan_Timer = 5000;
            // m_uiCounter = 0;
            //}
        }
        else
            { m_uiHandOfThaurissan_Timer -= uiDiff; }

        // AvatarOfFlame_Timer
        if (m_uiAvatarOfFlame_Timer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_AVATAROFFLAME);
            m_uiAvatarOfFlame_Timer = 18000;
        }
        else
            { m_uiAvatarOfFlame_Timer -= uiDiff; }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_emperor_dagran_thaurissan(Creature* pCreature)
{
    return new boss_emperor_dagran_thaurissanAI(pCreature);
}

/*######
## boss_moira_bronzebeard
######*/

enum ePrincess
{
    SPELL_HEAL                  = 15586,
    SPELL_RENEW                 = 10929,
    SPELL_SHIELD                = 10901,
    SPELL_MINDBLAST             = 15587,
    SPELL_SHADOWWORDPAIN        = 15654,
    SPELL_SMITE                 = 10934,
    SPELL_SHADOW_BOLT           = 15537,
    SPELL_OPEN_PORTAL           = 13912
};

struct boss_moira_bronzebeardAI : public ScriptedAI
{
    boss_moira_bronzebeardAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiHeal_Timer;
    uint32 m_uiMindBlast_Timer;
    uint32 m_uiShadowWordPain_Timer;
    uint32 m_uiSmite_Timer;

    void Reset() override
    {
        m_uiHeal_Timer = 12000;                             // These times are probably wrong
        m_uiMindBlast_Timer = 16000;
        m_uiShadowWordPain_Timer = 2000;
        m_uiSmite_Timer = 8000;
    }

    void AttackStart(Unit* pWho) override
    {
        if (m_creature->Attack(pWho, false))
        {
            m_creature->AddThreat(pWho);
            m_creature->SetInCombatWith(pWho);
            pWho->SetInCombatWith(m_creature);

            m_creature->GetMotionMaster()->MoveChase(pWho, 25.0f);
        }
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            if (Creature* pEmperor = m_pInstance->GetSingleCreatureFromStorage(NPC_EMPEROR))
            {
                // if evade, then check if he is alive. If not, start make portal
                if (!pEmperor->IsAlive())
                {
                    m_creature->CastSpell(m_creature, SPELL_OPEN_PORTAL, false);
                }
            }
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        // MindBlast_Timer
        if (m_uiMindBlast_Timer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_MINDBLAST);
            m_uiMindBlast_Timer = 14000;
        }
        else
            { m_uiMindBlast_Timer -= uiDiff; }

        // ShadowWordPain_Timer
        if (m_uiShadowWordPain_Timer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_SHADOWWORDPAIN);
            m_uiShadowWordPain_Timer = 18000;
        }
        else
            { m_uiShadowWordPain_Timer -= uiDiff; }

        // Smite_Timer
        if (m_uiSmite_Timer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_SMITE);
            m_uiSmite_Timer = 10000;
        }
        else
            { m_uiSmite_Timer -= uiDiff; }

        // Heal_Timer
        if (m_uiHeal_Timer < uiDiff)
        {
            if (Creature* pEmperor = m_pInstance->GetSingleCreatureFromStorage(NPC_EMPEROR))
            {
                if (pEmperor->IsAlive() && pEmperor->GetHealthPercent() != 100.0f)
                {
                    DoCastSpellIfCan(pEmperor, SPELL_HEAL);
                }
            }

            m_uiHeal_Timer = 10000;
        }
        else
            { m_uiHeal_Timer -= uiDiff; }

        // No meele?
    }
};

CreatureAI* GetAI_boss_moira_bronzebeard(Creature* pCreature)
{
    return new boss_moira_bronzebeardAI(pCreature);
}

void AddSC_boss_draganthaurissan()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_emperor_dagran_thaurissan";
    pNewScript->GetAI = &GetAI_boss_emperor_dagran_thaurissan;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "boss_moira_bronzebeard";
    pNewScript->GetAI = &GetAI_boss_moira_bronzebeard;
    pNewScript->RegisterSelf();
}
