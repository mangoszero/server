/*
 * Copyright (C) 2010-2012 Anathema Script Engine project <http://valkyrie-wow.com/>
 */

/* ScriptData
SDName: Npc_Onyxian_Warder
SD%Complete:
SDComment: 
SDCategory: Onyxia's Lair
EndScriptData */

#include "precompiled.h"

enum
{
    SPELL_CLEAVE            = 15284,
    SPELL_FIRE_NOVA         = 20203,
    SPELL_FLAME_LASH        = 18958,
    SPELL_PIERCE_ARMOR      = 12097
};

struct npc_onyxian_warder : public CreatureScript
{
    npc_onyxian_warder() : CreatureScript("npc_onyxian_warder") {}

    struct npc_onyxian_warderAI : public ScriptedAI
    {
        npc_onyxian_warderAI(Creature* pCreature) : ScriptedAI(pCreature) { }

        uint32 Cleave_Timer;
        uint32 Fire_Nova_Timer;
        uint32 Flame_Lash_Timer;
        uint32 Pierce_Armor_Timer;

        void Reset()
        {
            Cleave_Timer = urand(1000, 5000);
            Fire_Nova_Timer = urand(2500, 7500);
            Flame_Lash_Timer = urand(5000, 15000);
            Pierce_Armor_Timer = urand(10000, 20000);
        }

        void Aggro(Unit* /*pWho*/)
        {
            m_creature->SetInCombatWithZone();
            m_creature->CallForHelp(20.0f);
        }

        void UpdateAI(const uint32 uiDiff)
        {
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
                return;

            if (Cleave_Timer < uiDiff)
            {
                Cleave_Timer = urand(6000, 9000);
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_CLEAVE);
            }
            else
                Cleave_Timer -= uiDiff;

            if (Fire_Nova_Timer < uiDiff)
            {
                Fire_Nova_Timer = urand(4000, 9000);
                DoCastSpellIfCan(m_creature, SPELL_FIRE_NOVA);
            }
            else
                Fire_Nova_Timer -= uiDiff;

            if (Flame_Lash_Timer < uiDiff)
            {
                Flame_Lash_Timer = urand(9000, 20000);
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_FLAME_LASH);
            }
            else
                Flame_Lash_Timer -= uiDiff;

            if (Pierce_Armor_Timer < uiDiff)
            {
                Pierce_Armor_Timer = urand(20000, 35000);
                DoCastSpellIfCan(m_creature->getVictim(), SPELL_PIERCE_ARMOR);
            }
            else
                Pierce_Armor_Timer -= uiDiff;

            DoMeleeAttackIfReady();
        }

    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_onyxian_warderAI(pCreature);
    }
};

void AddSC_npc_onyxian_warder()
{
    Script *s;
    s = new npc_onyxian_warder();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_onyxian_warder";
    //pNewScript->GetAI = &GetAI_npc_onyxian_warderAI;
    //pNewScript->RegisterSelf();
}