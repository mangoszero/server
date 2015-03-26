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
 * SDName:      Boss_Ebonroc
 * SD%Complete: 100
 * SDComment:   None
 * SDCategory:  Blackwing Lair
 * EndScriptData
 */

#include "precompiled.h"
#include "blackwing_lair.h"

enum
{
    SPELL_SHADOW_FLAME          = 22539,
    SPELL_WING_BUFFET           = 18500,
    SPELL_SHADOW_OF_EBONROC     = 23340,
    SPELL_THRASH                = 3391
};

struct boss_ebonrocAI : public ScriptedAI
{
    boss_ebonrocAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        Reset();
    }

    ScriptedInstance* m_pInstance;

    uint32 m_uiShadowFlameTimer;
    uint32 m_uiWingBuffetTimer;
    uint32 m_uiShadowOfEbonrocTimer;
    uint32 m_uiTrashTimer;

    void Reset() override
    {
        m_uiShadowFlameTimer        = 15000;                // These times are probably wrong
        m_uiWingBuffetTimer         = 30000;
        m_uiShadowOfEbonrocTimer    = 45000;
        m_uiTrashTimer              = 25000;
    }

    void Aggro(Unit* /*pWho*/) override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_EBONROC, IN_PROGRESS);
        }
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_EBONROC, DONE);
        }
    }

    void JustReachedHome() override
    {
        if (m_pInstance)
        {
            m_pInstance->SetData(TYPE_EBONROC, FAIL);
        }
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        // Shadow Flame Timer
        if (m_uiShadowFlameTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_SHADOW_FLAME) == CAST_OK)
            {
                m_uiShadowFlameTimer = urand(12000, 15000);
            }
        }
        else
            { m_uiShadowFlameTimer -= uiDiff; }

        // Wing Buffet Timer
        if (m_uiWingBuffetTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_WING_BUFFET) == CAST_OK)
            {
                m_uiWingBuffetTimer = 25000;
            }
        }
        else
            { m_uiWingBuffetTimer -= uiDiff; }

        // Shadow of Ebonroc Timer
        if (m_uiShadowOfEbonrocTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_SHADOW_OF_EBONROC) == CAST_OK)
            {
                m_uiShadowOfEbonrocTimer = urand(25000, 35000);
            }
        }
        else
            { m_uiShadowOfEbonrocTimer -= uiDiff; }

        // Thrash Timer
        if (m_uiTrashTimer < uiDiff)
        {
            if (DoCastSpellIfCan(m_creature, SPELL_THRASH) == CAST_OK)
                { m_uiTrashTimer = 20000; }
        }
        else
            { m_uiTrashTimer -= uiDiff; }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_ebonroc(Creature* pCreature)
{
    return new boss_ebonrocAI(pCreature);
}

void AddSC_boss_ebonroc()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_ebonroc";
    pNewScript->GetAI = &GetAI_boss_ebonroc;
    pNewScript->RegisterSelf();
}
