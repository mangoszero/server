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
 * SDName:      Boss_Ambassador_Flamelash
 * SD%Complete: 80
 * SDComment:   Texts missing, Add handling rather guesswork, Add spell Burning Spirit likely won't work
 * SDCategory:  Blackrock Depths
 * EndScriptData
 */

#include "precompiled.h"

enum
{
    SPELL_FIREBLAST             = 15573,
    SPELL_BURNING_SPIRIT        = 13489,
    SPELL_BURNING_SPIRIT_BUFF   = 14744,

    NPC_BURNING_SPIRIT          = 9178,
};

struct boss_ambassador_flamelashAI : public ScriptedAI
{
    boss_ambassador_flamelashAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    uint32 m_uiSpiritTimer;
    int Rand;
    int RandX;
    int RandY;

    void Reset() override
    {
        m_uiSpiritTimer = 12000;
    }

    void SummonSpirits()
    {
        float fX, fY, fZ;
        m_creature->GetRandomPoint(m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ(), 30.0f, fX, fY, fZ);
        m_creature->SummonCreature(NPC_BURNING_SPIRIT, fX, fY, fZ, m_creature->GetAngle(fX, fY) + M_PI_F, TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN, 60000);
    }

    void Aggro(Unit* /*pWho*/) override
    {
        DoCastSpellIfCan(m_creature, SPELL_FIREBLAST);
    }

    void JustSummoned(Creature* pSummoned) override
    {
        pSummoned->GetMotionMaster()->MovePoint(1, m_creature->GetPositionX(), m_creature->GetPositionY(), m_creature->GetPositionZ());
    }

    void SummonedMovementInform(Creature* pSummoned, uint32 /*uiMotionType*/, uint32 uiPointId) override
    {
        if (uiPointId != 1)
        {
            return;
        }

        pSummoned->CastSpell(m_creature, SPELL_BURNING_SPIRIT, true);
    }

    void UpdateAI(const uint32 uiDiff) override
    {
        // Return since we have no target
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        // m_uiSpiritTimer
        if (m_uiSpiritTimer < uiDiff)
        {
            SummonSpirits();
            SummonSpirits();
            SummonSpirits();
            SummonSpirits();

            m_uiSpiritTimer = 20000;
        }
        else
            { m_uiSpiritTimer -= uiDiff; }

        DoMeleeAttackIfReady();
    }
};

bool EffectDummyCreature_spell_boss_ambassador_flamelash(Unit* /*pCaster*/, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Creature* pCreatureTarget, ObjectGuid /*originalCasterGuid*/)
{
    if (uiSpellId == SPELL_BURNING_SPIRIT && uiEffIndex == EFFECT_INDEX_1)
    {
        pCreatureTarget->CastSpell(pCreatureTarget, SPELL_BURNING_SPIRIT_BUFF, true);
        return true;
    }

    return false;
}

CreatureAI* GetAI_boss_ambassador_flamelash(Creature* pCreature)
{
    return new boss_ambassador_flamelashAI(pCreature);
}

void AddSC_boss_ambassador_flamelash()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "boss_ambassador_flamelash";
    pNewScript->GetAI = &GetAI_boss_ambassador_flamelash;
    pNewScript->pEffectDummyNPC = &EffectDummyCreature_spell_boss_ambassador_flamelash;
    pNewScript->RegisterSelf();
}
