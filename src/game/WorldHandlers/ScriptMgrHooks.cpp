/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
 * @file ScriptMgr.cpp
 * @brief Script system manager implementation
 *
 * This file implements ScriptMgr which manages all game scripts:
 * - Creature AI scripts
 * - GameObject scripts
 * - Item scripts
 * - Area trigger scripts
 * - Spell scripts
 * - Quest scripts
 * - Instance scripts
 *
 * Scripts are loaded from script libraries and provide hooks for
 * customizing game behavior. The script manager routes events to
 * the appropriate script handlers.
 *
 * @see ScriptMgr for the manager class
 * @see ScriptedInstance for instance script base
 */



#include "ScriptMgr.h"
#include "Creature.h"
#include "GameObject.h"
#include "Player.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include "WaypointManager.h"
#include "World.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SQLStorages.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "WaypointMovementGenerator.h"
#include "Mail.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif /* ENABLE_SD3 */
#ifdef CLASSIC
#include "LFGMgr.h"
#endif /* CLASSIC */

/**
 * @brief Creates or retrieves scripted AI for a creature.
 *
 * @param pCreature The creature requiring AI.
 * @return CreatureAI* The scripted AI instance, or NULL when none is available.
 */
CreatureAI* ScriptMgr::GetCreatureAI(Creature* pCreature)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pCreature->GetEluna())
    {
        if (CreatureAI* luaAI = e->GetAI(pCreature))
        {
            return luaAI;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GetCreatureAI(pCreature);
#else
    return NULL;
#endif
}

/**
 * @brief Creates or retrieves scripted AI for a game object.
 *
 * @param pGo The game object requiring AI.
 * @return GameObjectAI* The scripted AI instance, or NULL when none is available.
 */
GameObjectAI* ScriptMgr::GetGameObjectAI(GameObject* pGo)
{
    // TODO - expose in ELuna
#ifdef ENABLE_SD3
    return SD3::GetGameObjectAI(pGo);
#else
    return NULL;
#endif
}

/**
 * @brief Creates scripted instance data for a map.
 *
 * @param pMap The map requiring instance data.
 * @return InstanceData* The scripted instance data, or NULL when unavailable.
 */
InstanceData* ScriptMgr::CreateInstanceData(Map* pMap)
{
#ifdef ENABLE_SD3
    return SD3::CreateInstanceData(pMap);
#else
    return NULL;
#endif
}

/**
 * @brief Dispatches creature gossip hello hooks to scripting engines.
 *
 * @param pPlayer The player starting gossip.
 * @param pCreature The creature handling gossip.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGossipHello(Player* pPlayer, Creature* pCreature)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnGossipHello(pPlayer, pCreature))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GossipHello(pPlayer, pCreature);
#else
    return false;
#endif
}

/**
 * @brief Dispatches game object gossip hello hooks to scripting engines.
 *
 * @param pPlayer The player starting gossip.
 * @param pGameObject The game object handling gossip.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGossipHello(Player* pPlayer, GameObject* pGameObject)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnGossipHello(pPlayer, pGameObject))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOGossipHello(pPlayer, pGameObject);
#else
    return false;
#endif
}

/**
 * @brief Dispatches item gossip hello hooks to scripting engines.
 *
 * @param pPlayer The player starting gossip.
 * @param pItem The item handling gossip.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGossipHello(Player* pPlayer, Item* pItem)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    // TODO ELUNA handler
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::ItemGossipHello(pPlayer, pItem);
#else
    return false;
#endif
}

/**
 * @brief Dispatches creature gossip selection hooks to scripting engines.
 *
 * @param pPlayer The player selecting the option.
 * @param pCreature The gossip creature.
 * @param sender The menu sender identifier.
 * @param action The selected action identifier.
 * @param code Optional code text entered by the player.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code)
{
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (code)
        {
            if (e->OnGossipSelectCode(pPlayer, pCreature, sender, action, code))
            {
                return true;
            }
        }
        else
        {
            if (e->OnGossipSelect(pPlayer, pCreature, sender, action))
            {
                return true;
            }
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    if (code)
    {
        return SD3::GossipSelectWithCode(pPlayer, pCreature, sender, action, code);
    }
    else
    {
        return SD3::GossipSelect(pPlayer, pCreature, sender, action);
    }
#else
    return false;
#endif
}

/**
 * @brief Dispatches game object gossip selection hooks to scripting engines.
 *
 * @param pPlayer The player selecting the option.
 * @param pGameObject The gossip game object.
 * @param sender The menu sender identifier.
 * @param action The selected action identifier.
 * @param code Optional code text entered by the player.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {

        if (code)
        {
            if (e->OnGossipSelectCode(pPlayer, pGameObject, sender, action, code))
            {
                return true;
            }
        }
        else
        {
            if (e->OnGossipSelect(pPlayer, pGameObject, sender, action))
            {
                return true;
            }
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    if (code)
    {
        return SD3::GOGossipSelectWithCode(pPlayer, pGameObject, sender, action, code);
    }
    else
    {
        return SD3::GOGossipSelect(pPlayer, pGameObject, sender, action);
    }
#else
    return false;
#endif
}

/**
 * @brief Dispatches item gossip selection hooks to scripting engines.
 *
 * @param pPlayer The player selecting the option.
 * @param pItem The gossip item.
 * @param sender The menu sender identifier.
 * @param action The selected action identifier.
 * @param code Optional code text entered by the player.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGossipSelect(Player* pPlayer, Item* pItem, uint32 sender, uint32 action, const char* code)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    // TODO Add Eluna handlers
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    if (code)
    {
        return SD3::ItemGossipSelectWithCode(pPlayer, pItem, sender, action, code);
    }
    else
    {
        return SD3::ItemGossipSelect(pPlayer, pItem, sender, action);
    }
#else
    return false;
#endif
}

/**
 * @brief Dispatches creature quest accept hooks to scripting engines.
 *
 * @param pPlayer The player accepting the quest.
 * @param pCreature The quest giver creature.
 * @param pQuest The accepted quest.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnQuestAccept(pPlayer, pCreature, pQuest))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::QuestAccept(pPlayer, pCreature, pQuest);
#else
    return false;
#endif
}

/**
 * @brief Dispatches game object quest accept hooks to scripting engines.
 *
 * @param pPlayer The player accepting the quest.
 * @param pGameObject The quest giver game object.
 * @param pQuest The accepted quest.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnQuestAccept(pPlayer, pGameObject, pQuest))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOQuestAccept(pPlayer, pGameObject, pQuest);
#else
    return false;
#endif
}

/**
 * @brief Dispatches item quest accept hooks to scripting engines.
 *
 * @param pPlayer The player accepting the quest.
 * @param pItem The quest-starting item.
 * @param pQuest The accepted quest.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnQuestAccept(pPlayer, pItem, pQuest))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::ItemQuestAccept(pPlayer, pItem, pQuest);
#else
    return false;
#endif
}

/**
 * @brief Dispatches creature quest reward hooks to scripting engines.
 *
 * @param pPlayer The player receiving the reward.
 * @param pCreature The quest giver creature.
 * @param pQuest The rewarded quest.
 * @param reward The selected reward index or identifier.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 reward)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnQuestReward(pPlayer, pCreature, pQuest, reward))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::QuestRewarded(pPlayer, pCreature, pQuest);
#else
    return false;
#endif
}

/**
 * @brief Dispatches game object quest reward hooks to scripting engines.
 *
 * @param pPlayer The player receiving the reward.
 * @param pGameObject The quest giver game object.
 * @param pQuest The rewarded quest.
 * @param reward The selected reward index or identifier.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 reward)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnQuestReward(pPlayer, pGameObject, pQuest, reward))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOQuestRewarded(pPlayer, pGameObject, pQuest);
#else
    return false;
#endif
}

/**
 * @brief Queries scripted dialog status for a creature gossip source.
 *
 * @param pPlayer The player querying the dialog state.
 * @param pCreature The creature being queried.
 * @return uint32 The dialog status value.
 */
uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, Creature* pCreature)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        e->GetDialogStatus(pPlayer, pCreature);
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GetNPCDialogStatus(pPlayer, pCreature);
#else
    return DIALOG_STATUS_UNDEFINED;
#endif
}

/**
 * @brief Queries scripted dialog status for a game object gossip source.
 *
 * @param pPlayer The player querying the dialog state.
 * @param pGameObject The game object being queried.
 * @return uint32 The dialog status value.
 */
uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, GameObject* pGameObject)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        e->GetDialogStatus(pPlayer, pGameObject);
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GetGODialogStatus(pPlayer, pGameObject);
#else
    return DIALOG_STATUS_UNDEFINED;
#endif
}

/**
 * @brief Dispatches player game object use hooks to scripting engines.
 *
 * @param pPlayer The player using the object.
 * @param pGameObject The used game object.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGameObjectUse(Player* pPlayer, GameObject* pGameObject)
{
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnGameObjectUse(pPlayer, pGameObject))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOUse(pPlayer, pGameObject);
#else
    return false;
#endif
}

/**
 * @brief Dispatches non-player game object use hooks to scripting engines.
 *
 * @param pUnit The unit using the object.
 * @param pGameObject The used game object.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnGameObjectUse(Unit* pUnit, GameObject* pGameObject)
{
    // TODO Add Eluna support

#ifdef ENABLE_SD3
    return SD3::GOUse(pUnit, pGameObject);
#else
    return false;
#endif
}

/**
 * @brief Dispatches item use hooks to scripting engines.
 *
 * @param pPlayer The player using the item.
 * @param pItem The used item.
 * @param targets The item spell cast targets.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (!e->OnUse(pPlayer, pItem, targets))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::ItemUse(pPlayer, pItem, targets);
#else
    return false;
#endif
}

/**
 * @brief Dispatches area trigger hooks to scripting engines.
 *
 * @param pPlayer The player entering the trigger.
 * @param atEntry The area trigger entry.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pPlayer->GetEluna())
    {
        if (e->OnAreaTrigger(pPlayer, atEntry))
        {
            return true;
        }
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::AreaTrigger(pPlayer, atEntry);
#else
    return false;
#endif
}

/**
 * @brief Dispatches npc spell click hooks to scripting engines.
 *
 * @param pPlayer The player clicking the NPC spell interaction.
 * @param pClickedCreature The clicked creature.
 * @param spellId The triggering spell id.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnNpcSpellClick(Player* pPlayer, Creature* pClickedCreature, uint32 spellId)
{
#ifdef ENABLE_SD3
    return SD3::NpcSpellClick(pPlayer, pClickedCreature, spellId);
#else
    return false;
#endif
}

/**
 * @brief Dispatches generic scripted process events to scripting engines.
 *
 * @param eventId The event identifier.
 * @param pSource The event source object.
 * @param pTarget The event target object.
 * @param isStart True when processing the start of the event chain.
 * @return true if a script handled the event; otherwise false.
 */
bool ScriptMgr::OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart)
{
#ifdef ENABLE_SD3
    return SD3::ProcessEvent(eventId, pSource, pTarget, isStart);
#else
    return false;
#endif
}

/**
 * @brief Dispatches dummy spell effect hooks for unit targets.
 *
 * @param pCaster The spell caster.
 * @param spellId The triggering spell id.
 * @param effIndex The spell effect index.
 * @param pTarget The unit target.
 * @param originalCasterGuid The original caster guid.
 * @return true if a script handled the effect; otherwise false.
 */
bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Creature* creature = pTarget->ToCreature())
    {
        if (Eluna* e = pCaster->GetEluna())
        {
            e->OnDummyEffect(pCaster, spellId, effIndex, creature);
        }
    }

#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::EffectDummyUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

/**
 * @brief Dispatches dummy spell effect hooks for game object targets.
 *
 * @param pCaster The spell caster.
 * @param spellId The triggering spell id.
 * @param effIndex The spell effect index.
 * @param pTarget The game object target.
 * @param originalCasterGuid The original caster guid.
 * @return true if a script handled the effect; otherwise false.
 */
bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pCaster->GetEluna())
    {
        e->OnDummyEffect(pCaster, spellId, effIndex, pTarget);
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::EffectDummyGameObject(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

/**
 * @brief Dispatches dummy spell effect hooks for item targets.
 *
 * @param pCaster The spell caster.
 * @param spellId The triggering spell id.
 * @param effIndex The spell effect index.
 * @param pTarget The item target.
 * @param originalCasterGuid The original caster guid.
 * @return true if a script handled the effect; otherwise false.
 */
bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = pCaster->GetEluna())
    {
        e->OnDummyEffect(pCaster, spellId, effIndex, pTarget);
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::EffectDummyItem(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

/**
 * @brief Dispatches script-effect spell hooks for unit targets.
 *
 * @param pCaster The spell caster.
 * @param spellId The triggering spell id.
 * @param effIndex The spell effect index.
 * @param pTarget The unit target.
 * @param originalCasterGuid The original caster guid.
 * @return true if a script handled the effect; otherwise false.
 */
bool ScriptMgr::OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
{
#ifdef ENABLE_SD3
    return SD3::EffectScriptEffectUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

/**
 * @brief Dispatches dummy aura application and removal hooks.
 *
 * @param pAura The aura being processed.
 * @param apply True when applying the aura; false when removing it.
 * @return true if a script handled the aura event; otherwise false.
 */
bool ScriptMgr::OnAuraDummy(Aura const* pAura, bool apply)
{
#ifdef ENABLE_SD3
    return SD3::AuraDummy(pAura, apply);
#else
    return false;
#endif
}
