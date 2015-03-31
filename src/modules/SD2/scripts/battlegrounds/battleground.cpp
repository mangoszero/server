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
 * SDName:      Battleground
 * SD%Complete: 100
 * SDComment:   Spirit guides in battlegrounds will revive all players every 30 sec.
 * SDCategory:  Battlegrounds
 * EndScriptData
 */

#include "precompiled.h"

/**
 * Script Info
 *
 * Spirit guides in battlegrounds resurrecting many players at once every 30
 * seconds through a channeled spell, which gets autocasted the whole time.
 *
 * If a spirit guide despawns all players around him will get teleported to
 * the next spirit guide.
 *
 * this script handles gossipHello and JustDied also allows autocast of the
 * channeled spell.
 */

enum
{
    SPELL_SPIRIT_HEAL_CHANNEL       = 22011,                // Spirit Heal Channel

    SPELL_SPIRIT_HEAL               = 22012,                // Spirit Heal

    SPELL_WAITING_TO_RESURRECT      = 2584                  // players who cancel this aura don't want a resurrection
};

struct npc_spirit_guide : public CreatureScript
{
    npc_spirit_guide() : CreatureScript("npc_spirit_guide") {}

    bool OnGossipHello(Player* pPlayer, Creature* /*pCreature*/) override
    {
        pPlayer->CastSpell(pPlayer, SPELL_WAITING_TO_RESURRECT, true);
        return true;
    }

    struct npc_spirit_guideAI : public ScriptedAI
    {
        npc_spirit_guideAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            pCreature->SetActiveObjectState(true);
            Reset();
        }

        void Reset() override {}

        void UpdateAI(const uint32 /*uiDiff*/) override
        {
            // auto cast the whole time this spell
            if (!m_creature->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            {
                m_creature->CastSpell(m_creature, SPELL_SPIRIT_HEAL_CHANNEL, false);
            }
        }

        void CorpseRemoved(uint32&) override
        {
            // TODO: would be better to cast a dummy spell
            Map* pMap = m_creature->GetMap();

            if (!pMap || !pMap->IsBattleGround())
            {
                return;
            }

            Map::PlayerList const& PlayerList = pMap->GetPlayers();

            for (Map::PlayerList::const_iterator itr = PlayerList.begin(); itr != PlayerList.end(); ++itr)
            {
                Player* pPlayer = itr->getSource();
                if (!pPlayer || !pPlayer->IsWithinDistInMap(m_creature, 20.0f) || !pPlayer->HasAura(SPELL_WAITING_TO_RESURRECT))
                {
                    continue;
                }

                // repop player again - now this node won't be counted and another node is searched
                pPlayer->RepopAtGraveyard();
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_spirit_guideAI(pCreature);
    }
};

void AddSC_battleground()
{
    Script *s;
    s = new npc_spirit_guide();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_spirit_guide";
    //pNewScript->GetAI = &GetAI_npc_spirit_guide;
    //pNewScript->pGossipHello = &GossipHello_npc_spirit_guide;
    //pNewScript->RegisterSelf();
}
