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
 * SDName:      Spell_Scripts
 * SD%Complete: 100
 * SDComment:   Spell scripts for dummy effects (if script need access/interaction with parts of other AI or instance, add it in related source files).
 * SDCategory:  Spell
 * EndScriptData
 */

/**
 * ContentData
 * Spell 8913:  Sacred Cleansing
 * Spell 10848: Shroud of Death
 * Spell 17327: Spirit Particles
 * Spell 19512: Apply Salve
 * Spell 21050: Melodious Rapture
 * EndContentData
 */

#include "precompiled.h"

/**
 * When you make a spell effect:
 * - always check spell id and effect index
 * - always return true when the spell is handled by script
 */

enum
{
    // quest 6124/6129
    SPELL_APPLY_SALVE                   = 19512,
    SPELL_SICKLY_AURA                   = 19502,

    NPC_SICKLY_DEER                     = 12298,
    NPC_SICKLY_GAZELLE                  = 12296,

    NPC_CURED_DEER                      = 12299,
    NPC_CURED_GAZELLE                   = 12297,

    // target morbent fel
    SPELL_SACRED_CLEANSING              = 8913,
    NPC_MORBENT                         = 1200,
    NPC_WEAKENED_MORBENT                = 24782,

    // npcs that are only interactable while dead
    SPELL_SHROUD_OF_DEATH               = 10848,
    SPELL_SPIRIT_PARTICLES              = 17327,
    NPC_FRANCLORN_FORGEWRIGHT           = 8888,
    NPC_GAERIYAN                        = 9299,

    // quest 6661
    SPELL_MELODIOUS_RAPTURE             = 21050,
    SPELL_MELODIOUS_RAPTURE_VISUAL      = 21051,
    NPC_DEEPRUN_RAT                     = 13016,
    NPC_ENTHRALLED_DEEPRUN_RAT          = 13017,
};

struct aura_spirit_particles : public AuraScript
{
    aura_spirit_particles() : AuraScript("aura_spirit_particles") {}

    bool OnDummyApply(const Aura* pAura, bool bApply) override
    {
        switch (pAura->GetId())
        {
        case SPELL_SHROUD_OF_DEATH:
        case SPELL_SPIRIT_PARTICLES:
        {
            Creature* pCreature = (Creature*)pAura->GetTarget();

            if (!pCreature || (pCreature->GetEntry() != NPC_FRANCLORN_FORGEWRIGHT && pCreature->GetEntry() != NPC_GAERIYAN))
            {
                return false;
            }

            if (bApply)
            {
                pCreature->m_AuraFlags |= UNIT_AURAFLAG_ALIVE_INVISIBLE;
            }
            else
            {
                pCreature->m_AuraFlags &= ~UNIT_AURAFLAG_ALIVE_INVISIBLE;
            }

            return false;   //TODO why? true
        }
        }

        return false;
    }
};

struct spell_apply_salve : public SpellScript
{
    spell_apply_salve() : SpellScript("spell_apply_salve") {}

    bool EffectDummy(Unit* pCaster, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* pTarget, ObjectGuid /*originalCasterGuid*/) override
    {
        if (uiSpellId == SPELL_APPLY_SALVE && uiEffIndex == EFFECT_INDEX_0)
        {
            if (pCaster->GetTypeId() != TYPEID_PLAYER)
            {
                return true;
            }

            Creature *pCreatureTarget = pTarget->ToCreature();
            if (pCreatureTarget->GetEntry() != NPC_SICKLY_DEER && pCreatureTarget->GetEntry() != NPC_SICKLY_GAZELLE)
            {
                return true;
            }

            // Update entry, remove aura, set the kill credit and despawn
            uint32 uiUpdateEntry = pCreatureTarget->GetEntry() == NPC_SICKLY_DEER ? NPC_CURED_DEER : NPC_CURED_GAZELLE;
            pCreatureTarget->RemoveAurasDueToSpell(SPELL_SICKLY_AURA);
            pCreatureTarget->UpdateEntry(uiUpdateEntry);
            ((Player*)pCaster)->KilledMonsterCredit(uiUpdateEntry);
            pCreatureTarget->ForcedDespawn(20000);
        }
        return true;
    }
};

struct spell_sacred_cleansing : public SpellScript
{
    spell_sacred_cleansing() : SpellScript("spell_sacred_cleansing") {}

    bool EffectDummy(Unit* pCaster, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* pTarget, ObjectGuid /*originalCasterGuid*/) override
    {
        if (uiSpellId == SPELL_SACRED_CLEANSING && uiEffIndex == EFFECT_INDEX_1)
        {
            if (pTarget->ToCreature() && pTarget->GetEntry() != NPC_MORBENT)
            {
                return true;
            }
            pTarget->ToCreature()->UpdateEntry(NPC_WEAKENED_MORBENT);
        }
        return true;
    }
};

struct spell_melodious_rapture : public SpellScript
{
    spell_melodious_rapture() : SpellScript("spell_melodious_rapture") {}

    bool EffectDummy(Unit* pCaster, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* pTarget, ObjectGuid /*originalCasterGuid*/) override
    {
        if (uiSpellId == SPELL_MELODIOUS_RAPTURE && uiEffIndex == EFFECT_INDEX_0)
        {
            Creature *pCreatureTarget = pTarget->ToCreature();
            if (pCaster->GetTypeId() != TYPEID_PLAYER && pCreatureTarget->GetEntry() != NPC_DEEPRUN_RAT)
            {
                return true;
            }

            pCreatureTarget->UpdateEntry(NPC_ENTHRALLED_DEEPRUN_RAT);
            pCreatureTarget->CastSpell(pCreatureTarget, SPELL_MELODIOUS_RAPTURE_VISUAL, false);
            pCreatureTarget->GetMotionMaster()->MoveFollow(pCaster, frand(0.5f, 3.0f), frand(M_PI_F * 0.8f, M_PI_F * 1.2f));

            ((Player*)pCaster)->KilledMonsterCredit(NPC_ENTHRALLED_DEEPRUN_RAT);
        }
        return true;
    }
};

void AddSC_spell_scripts()
{
    Script* s;
    s = new aura_spirit_particles();
    s->RegisterSelf();
    s = new spell_apply_salve();
    s->RegisterSelf();
    s = new spell_sacred_cleansing();
    s->RegisterSelf();
    s = new spell_melodious_rapture();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "spell_dummy_npc";
    //pNewScript->pEffectDummyNPC = &EffectDummyCreature_spell_dummy_npc;
    //pNewScript->pEffectAuraDummy = &EffectAuraDummy_spell_aura_dummy_npc;
    //pNewScript->RegisterSelf();
}
