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
 * SDName:      Boss_General_Angerforge
 * SD%Complete: 100
 * SDComment:   None
 * SDCategory:  Blackrock Depths
 * EndScriptData
 */

#include "precompiled.h"

enum
{
    SPELL_MIGHTYBLOW            = 14099,
    SPELL_HAMSTRING             = 9080,
    SPELL_CLEAVE                = 20691,

    NPC_ANVILRAGE_RESERVIST     = 8901,
    NPC_ANVILRAGE_MEDIC         = 8894,
};

struct boss_general_angerforgeAI : public ScriptedAI
{
    boss_general_angerforgeAI(Creature* pCreature) : ScriptedAI(pCreature) { Reset(); }

    uint32 m_uiMightyBlowTimer;
    uint32 m_uiHamStringTimer;
    uint32 m_uiCleaveTimer;
    uint32 m_uiAddsTimer;
    bool m_bSummonedMedics;

    void Reset() override
    {
        m_uiMightyBlowTimer = 8000;
        m_uiHamStringTimer = 12000;
        m_uiCleaveTimer = 16000;
        m_uiAddsTimer = 0;
        m_bSummonedMedics = false;
    }

    void SummonAdd(uint32 uiEntry)
    {
        float fX, fY, fZ;
        m_creature->GetRandomPoint(m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), 20.0f, fX, fY, fZ);
        m_creature->SummonCreature(uiEntry, fX, fY, fZ, 0.0f, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, 60000);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
        {
            pSummoned->AI()->AttackStart(pTarget);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        // MightyBlow_Timer
        if (m_uiMightyBlowTimer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_MIGHTYBLOW);
            m_uiMightyBlowTimer = 18000;
        }
        else
            { m_uiMightyBlowTimer -= uiDiff; }

        // HamString_Timer
        if (m_uiHamStringTimer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_HAMSTRING);
            m_uiHamStringTimer = 15000;
        }
        else
            { m_uiHamStringTimer -= uiDiff; }

        // Cleave_Timer
        if (m_uiCleaveTimer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE);
            m_uiCleaveTimer = 9000;
        }
        else
            { m_uiCleaveTimer -= uiDiff; }

        // Adds_Timer
        if (m_creature->GetHealthPercent() < 21.0f)
        {
            if (m_uiAddsTimer < uiDiff)
            {
                // summon 3 Adds every 25s
                SummonAdd(NPC_ANVILRAGE_RESERVIST);
                SummonAdd(NPC_ANVILRAGE_RESERVIST);
                SummonAdd(NPC_ANVILRAGE_RESERVIST);

                m_uiAddsTimer = 25000;
            }
            else
            {
                m_uiAddsTimer -= uiDiff;
            }
        }

        // Summon Medics
        if (!m_bSummonedMedics && m_creature->GetHealthPercent() < 21.0f)
        {
            SummonAdd(NPC_ANVILRAGE_MEDIC);
            SummonAdd(NPC_ANVILRAGE_MEDIC);
            m_bSummonedMedics = true;
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_general_angerforge(Creature* pCreature)
{
    return new boss_general_angerforgeAI(pCreature);
}

void AddSC_boss_general_angerforge()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_general_angerforge";
    pNewScript->GetAI = &GetAI_boss_general_angerforge;
    pNewScript->RegisterSelf();
}
