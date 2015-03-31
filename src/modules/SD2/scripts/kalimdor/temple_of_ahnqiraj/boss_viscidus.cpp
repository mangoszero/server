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
 * SDName:      Boss_Viscidus
 * SD%Complete: 40
 * SDComment:   Only basic spells - freeze and explode events require more research
 * SDCategory:  Temple of Ahn'Qiraj
 * EndScriptData
 */

#include "precompiled.h"
#include "temple_of_ahnqiraj.h"

enum
{
    // Timer spells
    SPELL_POISON_SHOCK          = 25993,
    SPELL_POISONBOLT_VOLLEY     = 25991,
    SPELL_TOXIN                 = 26575,                    // Triggers toxin cloud - 25989

    // Debuffs gained by the boss on frost damage
    SPELL_VISCIDUS_SLOWED       = 26034,
    SPELL_VISCIDUS_SLOWED_MORE  = 26036,
    SPELL_VISCIDUS_FREEZE       = 25937,

    // When frost damage exceeds a certain limit, then boss explodes
    SPELL_REJOIN_VISCIDUS       = 25896,
    SPELL_VISCIDUS_EXPLODE      = 25938,
    SPELL_VISCIDUS_SUICIDE      = 26003,                    // cast when boss explodes and is below 5% Hp - should trigger 26002

    SPELL_MEMBRANE_VISCIDUS     = 25994,                    // damage reduction spell
    // SPELL_VISCIDUS_WEAKNESS   = 25926,                   // aura which procs at damage - should trigger the slow spells
    // SPELL_VISCIDUS_SHRINKS    = 25893,                   // removed from DBC TODO check this "synced mess"
    // SPELL_VISCIDUS_SHRINKS_2  = 27934,                   // removed from DBC
    // SPELL_VISCIDUS_GROWS      = 25897,                   // removed from DBC
    // SPELL_SUMMON_GLOBS        = 25885,                   // summons npc 15667 using spells from 25865 to 25884; All spells have target coords - removed from DBC
    // SPELL_VISCIDUS_TELEPORT   = 25904,                   // removed from DBC
    // SPELL_SUMMONT_TRIGGER     = 26564,                   // summons 15992 - removed from DBC

    NPC_GLOB_OF_VISCIDUS        = 15667
};

struct boss_viscidus : public CreatureScript
{
    boss_viscidus() : CreatureScript("boss_viscidus") {}

    struct boss_viscidusAI : public ScriptedAI
    {
        boss_viscidusAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        }

        ScriptedInstance* m_pInstance;

        uint32 m_uiPoisonShockTimer;
        uint32 m_uiPoisonBoltVolleyTimer;

        void Reset() override
        {
            m_uiPoisonShockTimer = urand(7000, 12000);
            m_uiPoisonBoltVolleyTimer = urand(10000, 15000);

            DoCastSpellIfCan(m_creature, SPELL_MEMBRANE_VISCIDUS);
        }

        void Aggro(Unit* /*pWho*/) override
        {
            DoCastSpellIfCan(m_creature, SPELL_TOXIN);

            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_VISCIDUS, IN_PROGRESS);
            }
        }

        void JustReachedHome() override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_VISCIDUS, FAIL);
            }
        }

        void JustDied(Unit* /*pKiller*/) override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_VISCIDUS, DONE);
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            if (m_uiPoisonShockTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_POISON_SHOCK) == CAST_OK)
                {
                    m_uiPoisonShockTimer = urand(7000, 12000);
                }
            }
            else
            {
                m_uiPoisonShockTimer -= uiDiff;
            }

            if (m_uiPoisonBoltVolleyTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_POISONBOLT_VOLLEY) == CAST_OK)
                {
                    m_uiPoisonBoltVolleyTimer = urand(10000, 15000);
                }
            }
            else
            {
                m_uiPoisonBoltVolleyTimer -= uiDiff;
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new boss_viscidusAI(pCreature);
    }
};

void AddSC_boss_viscidus()
{
    Script* s;
    s = new boss_viscidus();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "boss_viscidus";
    //pNewScript->GetAI = &GetAI_boss_viscidus;
    //pNewScript->RegisterSelf();
}
