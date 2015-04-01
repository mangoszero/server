/**
 * ScriptDev2 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2015 getMangos <https://www.getmangos.eu/>
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
 * SDName:      Instance_Blackfathom_Deeps
 * SD%Complete: 50
 * SDComment:   None
 * SDCategory:  Blackfathom Deeps
 * EndScriptData
 */

#include "precompiled.h"
#include "blackfathom_deeps.h"

/*######
## go_fire_of_akumai
######*/

struct go_fire_of_akumai : public GameObjectScript
{
    go_fire_of_akumai() : GameObjectScript("go_fire_of_akumai") {}

    bool OnUse(Player* /*pPlayer*/, GameObject* pGo) override
    {
        InstanceData* pInstance = pGo->GetInstanceData();

        if (!pInstance)
        {
            return true;
        }

        if (pInstance->GetData(TYPE_SHRINE) == DONE)
        {
            return true;
        }

        if (pInstance->GetData(TYPE_KELRIS) == DONE)
        {
            pInstance->SetData(TYPE_SHRINE, IN_PROGRESS);
            return false;
        }

        return true;
    }
};

/*######
## go_fathom_stone
######*/

struct go_fathom_stone : public GameObjectScript
{
    go_fathom_stone() : GameObjectScript("go_fathom_stone") {}

    bool OnUse(Player* pPlayer, GameObject* pGo) override
    {
        ScriptedInstance* pInstance = (ScriptedInstance*)pGo->GetInstanceData();

        if (!pInstance)
        {
            return false;
        }

        if (pInstance->GetData(TYPE_STONE) == DONE)
        {
            return false;
        }
        else
        {
            pPlayer->SummonCreature(NPC_BARON_AQUANIS, -782.21f, -63.26f, -42.43f, 2.36f, TEMPSUMMON_TIMED_OOC_DESPAWN, 120000);
        }

        pInstance->SetData(TYPE_STONE, DONE);
        return true;
    }
};

void AddSC_blackfathom_deeps()
{
    Script* s;
    s = new go_fire_of_akumai();
    s->RegisterSelf();
    s = new go_fathom_stone();
    s->RegisterSelf();
}
