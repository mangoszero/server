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
 * SDName:      Npc_Professions
 * SD%Complete: 80
 * SDComment:   Provides learning, unlearning and relearning options for profession specializations.
 * SDCategory:  NPCs
 * EndScriptData
 */

#include "precompiled.h"




/*###
# start menues for GO (engineering and leatherworking)
###*/

/*bool GOUse_go_soothsaying_for_dummies(Player* pPlayer, GameObject* pGo)
{
    pPlayer->PlayerTalkClass->GetGossipMenu()->AddMenuItem(0,GOSSIP_LEARN_DRAGON, GOSSIP_SENDER_INFO, GOSSIP_ACTION_INFO_DEF, "", 0);

    pPlayer->SEND_GOSSIP_MENU(5584, pGo->GetObjectGuid());

    return true;
}*/


/*###
#
###*/

void AddSC_npc_professions()
{
    //Script* pNewScript;

    /*pNewScript = new Script;
    pNewScript->Name = "go_soothsaying_for_dummies";
    pNewScript->pGOUse =  &GOUse_go_soothsaying_for_dummies;
    // pNewScript->pGossipSelect = &GossipSelect_go_soothsaying_for_dummies;
    pNewScript->RegisterSelf();*/
}
