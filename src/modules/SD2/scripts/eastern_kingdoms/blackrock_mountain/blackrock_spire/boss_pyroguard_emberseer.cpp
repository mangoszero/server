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
 * SDName:      Boss_Pyroguard_Emberseer
 * SD%Complete: 90
 * SDComment:   Dummy spells used during the transformation may need further research
 * SDCategory:  Blackrock Spire
 * EndScriptData
 */

#include "precompiled.h"
#include "blackrock_spire.h"

enum
{
    // Intro emote/say
    EMOTE_NEAR              = -1229001,
    EMOTE_FULL              = -1229002,
    SAY_FREE                = -1229003,

    MAX_GROWING_STACKS      = 20,

    // Intro spells

    SPELL_FIRE_SHIELD       = 13376,                        // not sure what's the purpose of this
    SPELL_DESPAWN_EMBERSEER = 16078,                        // not sure what's the purpose of this
    SPELL_FREEZE_ANIM       = 16245,                        // not sure what's the purpose of this
    SPELL_FULL_STRENGHT     = 16047,
    SPELL_GROWING           = 16049,                        // stacking aura
    SPELL_BONUS_DAMAGE      = 16534,                        // triggered on full grow
    SPELL_TRANSFORM         = 16052,

    // Combat spells
    SPELL_FIRENOVA          = 23462,
    SPELL_FLAMEBUFFET       = 23341,
    SPELL_PYROBLAST         = 20228                         // guesswork, but best fitting in spells-area, was 17274 (has mana cost)
};

struct boss_pyroguard_emberseer : public CreatureScript
{
    boss_pyroguard_emberseer() : CreatureScript("boss_pyroguard_emberseer") {}

    struct boss_pyroguard_emberseerAI : public ScriptedAI
    {
        boss_pyroguard_emberseerAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_pInstance = pCreature->GetInstanceData();
        }

        InstanceData* m_pInstance;

        uint32 m_uiEncageTimer;
        uint32 m_uiFireNovaTimer;
        uint32 m_uiFlameBuffetTimer;
        uint32 m_uiPyroBlastTimer;
        uint8 m_uiGrowingStacks;

        void Reset() override
        {
            m_uiEncageTimer = 10000;
            m_uiFireNovaTimer = 6000;
            m_uiFlameBuffetTimer = 3000;
            m_uiPyroBlastTimer = 14000;
            m_uiGrowingStacks = 0;

            m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }

        void JustDied(Unit* /*pKiller*/) override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_EMBERSEER, DONE);
            }
        }

        void JustReachedHome() override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_EMBERSEER, FAIL);
            }
        }

        void ReceiveAIEvent(AIEventType type, Creature* pSender, Unit* /*pInvoker*/, uint32 /*uiMiscValue*/) override
        {
            if (type == AI_EVENT_CUSTOM_A && pSender == m_creature) //defined in the spell
                DoHandleEmberseerGrowing();
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            // Cast Encage spell on OOC timer
            if (m_uiEncageTimer)
            {
                if (m_uiEncageTimer <= uiDiff)
                {
                    if (!m_pInstance)
                    {
                        script_error_log("Instance Blackrock Spire: ERROR Failed to load instance data for this instace.");
                        return;
                    }

                    m_pInstance->SetData64(NPC_PYROGUARD_EMBERSEER, m_creature->GetGUID());

                    m_uiEncageTimer = 0;
                }
                else
                {
                    m_uiEncageTimer -= uiDiff;
                }
            }

            // Return since we have no target
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            // FireNova Timer
            if (m_uiFireNovaTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_FIRENOVA) == CAST_OK)
                {
                    m_uiFireNovaTimer = 6000;
                }
            }
            else
            {
                m_uiFireNovaTimer -= uiDiff;
            }

            // FlameBuffet Timer
            if (m_uiFlameBuffetTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, SPELL_FLAMEBUFFET) == CAST_OK)
                {
                    m_uiFlameBuffetTimer = 14000;
                }
            }
            else
            {
                m_uiFlameBuffetTimer -= uiDiff;
            }

            // PyroBlast Timer
            if (m_uiPyroBlastTimer < uiDiff)
            {
                if (Unit* pTarget = m_creature->SelectAttackingTarget(ATTACKING_TARGET_RANDOM, 0))
                {
                    if (DoCastSpellIfCan(pTarget, SPELL_PYROBLAST) == CAST_OK)
                    {
                        m_uiPyroBlastTimer = 15000;
                    }
                }
            }
            else
            {
                m_uiPyroBlastTimer -= uiDiff;
            }

            DoMeleeAttackIfReady();
        }

    private:
        // Wrapper to handle the transformation
        void DoHandleEmberseerGrowing()
        {
            ++m_uiGrowingStacks;

            if (m_uiGrowingStacks == MAX_GROWING_STACKS * 0.5f)
            {
                DoScriptText(EMOTE_NEAR, m_creature);
            }
            else if (m_uiGrowingStacks == MAX_GROWING_STACKS)
            {
                DoScriptText(EMOTE_FULL, m_creature);
                DoScriptText(SAY_FREE, m_creature);

                // Note: the spell order needs further research
                DoCastSpellIfCan(m_creature, SPELL_FULL_STRENGHT, CAST_TRIGGERED);
                DoCastSpellIfCan(m_creature, SPELL_BONUS_DAMAGE, CAST_TRIGGERED);
                DoCastSpellIfCan(m_creature, SPELL_TRANSFORM, CAST_TRIGGERED);

                // activate all runes
                if (m_pInstance)
                {
                    m_pInstance->SetData(MAX_ENCOUNTER, DO_ACTIVATE_RUNES);
                    // Redundant check: if for some reason the event isn't set in progress until this point - avoid using the altar again when the boss is fully grown
                    m_pInstance->SetData(TYPE_EMBERSEER, IN_PROGRESS);
                }

                m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new boss_pyroguard_emberseerAI(pCreature);
    }
};

struct spell_emberseer_growing : public SpellScript
{
    spell_emberseer_growing() : SpellScript("spell_emberseer_growing") {}

    bool EffectDummy(Unit* /*pCaster*/, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* pCreatureTarget, ObjectGuid /*originalCasterGuid*/) override
    {
        // always check spellid and effectindex
        if (uiSpellId == SPELL_GROWING && uiEffIndex == EFFECT_INDEX_0)
        {
            if (CreatureAI* pEmberseerAI = pCreatureTarget->ToCreature()->AI())
            {
                pEmberseerAI->SendAIEvent(AI_EVENT_CUSTOM_A, pCreatureTarget->ToCreature(), pCreatureTarget->ToCreature(), 0);
            }
            return true;
        }
        return false;
    }
};

void AddSC_boss_pyroguard_emberseer()
{
    Script *s;
    s = new boss_pyroguard_emberseer();
    s->RegisterSelf();
    s = new spell_emberseer_growing();
    s->RegisterSelf();

    //Script* pNewScript;

    //pNewScript = new Script;
    //pNewScript->Name = "boss_pyroguard_emberseer";
    //pNewScript->GetAI = &GetAI_boss_pyroguard_emberseer;
    //pNewScript->pEffectDummyNPC = &EffectDummyCreature_pyroguard_emberseer;
    //pNewScript->RegisterSelf();
}
