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
 * SDName:      Boss_Gehennas
 * SD%Complete: 90
 * SDComment:   None
 * SDCategory:  Molten Core
 * EndScriptData
 */

#include "precompiled.h"
#include "molten_core.h"

enum
{
    SPELL_SHADOW_BOLT           = 19728,                    // 19729 exists too, but can be reflected
    SPELL_RAIN_OF_FIRE          = 19717,
    SPELL_GEHENNAS_CURSE        = 19716
};

struct boss_gehennasAI : public ScriptedAI
{
    boss_gehennasAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiShadowBoltTimer;
    uint32 m_uiRainOfFireTimer;
    uint32 m_uiGehennasCurseTimer;

    void Reset() override
    {
        m_uiShadowBoltTimer = 6000;
        m_uiRainOfFireTimer = 10000;
        m_uiGehennasCurseTimer = 12000;
    }

    void Aggro(Unit* /*pwho*/) override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_GEHENNAS, IN_PROGRESS);
        }
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_GEHENNAS, DONE);
        }
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_GEHENNAS, FAIL);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        // ShadowBolt Timer
        if (m_uiShadowBoltTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 1))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_SHADOW_BOLT) == CAST_OK)
                {
                    m_uiShadowBoltTimer = 7000;
                }
            }
            else                                            // In case someone attempts soloing, we don't need to scan for targets every tick
            {
                m_uiShadowBoltTimer = 7000;
            }
        }
        else
            { m_uiShadowBoltTimer -= uiDiff; }

        // Rain of Fire Timer
        if (m_uiRainOfFireTimer < uiDiff)
        {
            if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
            {
                if (DoCastSpellIfCan(pTarget, SPELL_RAIN_OF_FIRE) == CAST_OK)
                {
                    m_uiRainOfFireTimer = urand(4000, 12000);
                }
            }
        }
        else
            { m_uiRainOfFireTimer -= uiDiff; }

        // GehennasCurse Timer
        if (m_uiGehennasCurseTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_GEHENNAS_CURSE) == CAST_OK)
            {
                m_uiGehennasCurseTimer = 30000;
            }
        }
        else
            { m_uiGehennasCurseTimer -= uiDiff; }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_gehennas(Creature* pCreature)
{
    return new boss_gehennasAI(pCreature);
}

void AddSC_boss_gehennas()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_gehennas";
    pNewScript->GetAI = &GetAI_boss_gehennas;
    pNewScript->RegisterSelf();
}
