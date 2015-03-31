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
 * SDName:      Zulfarrak
 * SD%Complete: 100
 * SDComment:   None
 * SDCategory:  Zul'Farrak
 * EndScriptData
 */

/**
 * ContentData
 * event_go_zulfarrak_gong
 * event_spell_unlocking
 * at_zulfarrak
 * EndContentData
 */

#include "precompiled.h"
#include "zulfarrak.h"

//TODO transfer event scripts into instance script which also accepts events
/*######
## event_go_zulfarrak_gong
######*/

struct event_go_zulfarrak_gong : public MapEventScript
{
    event_go_zulfarrak_gong() : MapEventScript("event_go_zulfarrak_gong") {}

    bool OnReceived(uint32 /*uiEventId*/, Object* pSource, Object* /*pTarget*/, bool bIsStart) override
    {
        if (bIsStart && pSource->GetTypeId() == TYPEID_PLAYER)
        {
            if (InstanceData* pInstance = ((Player*)pSource)->GetInstanceData())
            {
                if (pInstance->GetData(TYPE_GAHZRILLA) == NOT_STARTED || pInstance->GetData(TYPE_GAHZRILLA) == FAIL)
                {
                    pInstance->SetData(TYPE_GAHZRILLA, IN_PROGRESS);
                    return false;                               // Summon Gahz'rilla by Database Script
                }
                else
                {
                    return true;    // Prevent DB script summoning Gahz'rilla
                }
            }
        }
        return false;
    }
};

/*######
## event_spell_unlocking
######*/

struct event_spell_unlocking : public MapEventScript
{
    event_spell_unlocking() : MapEventScript("event_spell_unlocking") {}

    bool OnReceived(uint32 /*uiEventId*/, Object* pSource, Object* /*pTarget*/, bool bIsStart) override
    {
        if (bIsStart && pSource->GetTypeId() == TYPEID_PLAYER)
        {
            if (InstanceData* pInstance = ((Player*)pSource)->GetInstanceData())
            {
                if (pInstance->GetData(TYPE_PYRAMID_EVENT) == NOT_STARTED)
                {
                    pInstance->SetData(TYPE_PYRAMID_EVENT, IN_PROGRESS);
                    return false;                               // Summon pyramid trolls by Database Script
                }
                else
                {
                    return true;
                }
            }
        }
        return false;
    }
};

/*######
## at_zulfarrak
######*/

struct at_zulfarrak : public AreaTriggerScript
{
    at_zulfarrak() : AreaTriggerScript("at_zulfarrak") {}

    bool OnTrigger(Player* pPlayer, AreaTriggerEntry const* pAt) override
    {
        if (pAt->id == AREATRIGGER_ANTUSUL)
        {
            if (pPlayer->isGameMaster() || pPlayer->IsDead())
            {
                return false;
            }

            ScriptedInstance* pInstance = (ScriptedInstance*)pPlayer->GetInstanceData();

            if (!pInstance)
            {
                return false;
            }

            if (pInstance->GetData(TYPE_ANTUSUL) == NOT_STARTED || pInstance->GetData(TYPE_ANTUSUL) == FAIL)
            {
                if (Creature* pAntuSul = pInstance->GetSingleCreatureFromStorage(NPC_ANTUSUL))
                {
                    if (pAntuSul->IsAlive())
                    {
                        pAntuSul->AI()->AttackStart(pPlayer);
                    }
                }
            }
        }

        return false;
    }
};

void AddSC_zulfarrak()
{
    Script* s;
    s = new event_go_zulfarrak_gong();
    s->RegisterSelf();
    s = new event_spell_unlocking();
    s->RegisterSelf();
    s = new at_zulfarrak();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "event_go_zulfarrak_gong";
    //pNewScript->pProcessEventId = &ProcessEventId_event_go_zulfarrak_gong;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "event_spell_unlocking";
    //pNewScript->pProcessEventId = &ProcessEventId_event_spell_unlocking;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "at_zulfarrak";
    //pNewScript->pAreaTrigger = &AreaTrigger_at_zulfarrak;
    //pNewScript->RegisterSelf();
}
