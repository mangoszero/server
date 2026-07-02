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



#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "ScriptMgr.h"
#include "GossipDef.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

/**
 * @brief Loads gossip menu headers and validates linked texts, scripts, and conditions.
 *
 * @param gossipScriptSet The set of known gossip scripts to mark as used.
 */
void ObjectMgr::LoadGossipMenu(std::set<uint32>& gossipScriptSet)
{
    m_mGossipMenusMap.clear();
    QueryResult* result = WorldDatabase.Query(
        //           0        1          2            3
            "SELECT `entry`, `text_id`, `script_id`, `condition_id` FROM `gossip_menu`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded gossip_menu, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        GossipMenus gMenu;

        gMenu.entry             = fields[0].GetUInt32();
        gMenu.text_id           = fields[1].GetUInt32();
        gMenu.script_id         = fields[2].GetUInt32();

        gMenu.conditionId       = fields[3].GetUInt16();

        if (!GetGossipText(gMenu.text_id))
        {
            sLog.outErrorDb("Table gossip_menu entry %u are using non-existing text_id %u", gMenu.entry, gMenu.text_id);
            continue;
        }

        // Check script-id
        if (gMenu.script_id)
        {
            ScriptChainMap const* scm = sScriptMgr.GetScriptChainMap(DBS_ON_GOSSIP);
            if (!scm)
            {
                continue;
            }

            if (scm->find(gMenu.script_id) == scm->end())
            {
                sLog.outErrorDb("Table gossip_menu for menu %u, text-id %u have script_id %u that does not exist in `db_scripts [type = %d]`, ignoring", gMenu.entry, gMenu.text_id, gMenu.script_id, DBS_ON_GOSSIP);
                continue;
            }

            // Remove used script id
            gossipScriptSet.erase(gMenu.script_id);
        }

        if (gMenu.conditionId)
        {
            const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(gMenu.conditionId);
            if (!condition)
            {
                sLog.outErrorDb("Table gossip_menu for menu %u, text-id %u has condition_id %u that does not exist in `conditions`, ignoring", gMenu.entry, gMenu.text_id, gMenu.conditionId);
                gMenu.conditionId = 0;
            }
        }

        m_mGossipMenusMap.insert(GossipMenusMap::value_type(gMenu.entry, gMenu));

        ++count;
    }
    while (result->NextRow());

    delete result;

    // post loading tests
    for (uint32 i = 1; i < sCreatureStorage.GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (cInfo->GossipMenuId)
            {
                if (m_mGossipMenusMap.find(cInfo->GossipMenuId) == m_mGossipMenusMap.end())
                {
                    sLog.outErrorDb("Creature (Entry: %u) has GossipMenuId = %u for nonexistent menu", cInfo->Entry, cInfo->GossipMenuId);
                }
            }
        }
    }

    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
    {
        for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
        {
            if (uint32 menuid = itr->GetGossipMenuId())
            {
                if (m_mGossipMenusMap.find(menuid) == m_mGossipMenusMap.end())
                {
                    sLog.outErrorDb("Gameobject (Entry: %u) has gossip_menu_id = %u for nonexistent menu", itr->id, menuid);
                }
            }
        }
    }

    sLog.outString(">> Loaded %u gossip_menu entries", count);
    sLog.outString();
}

/**
 * @brief Loads gossip menu options and validates linked menus, scripts, POIs, and conditions.
 *
 * @param gossipScriptSet The set of known gossip scripts to mark as used.
 */
void ObjectMgr::LoadGossipMenuItems(std::set<uint32>& gossipScriptSet)
{
    m_mGossipMenuItemsMap.clear();

    QueryResult* result = WorldDatabase.Query(
            "SELECT `menu_id`, `id`, `option_icon`, `option_text`, `option_id`, `npc_option_npcflag`, "
            "`action_menu_id`, `action_poi_id`, `action_script_id`, `box_coded`, `box_money`, `box_text`, "
            "`condition_id` "
            "FROM `gossip_menu_option` ORDER BY `menu_id`, `id`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded gossip_menu_option, table is empty!");
        sLog.outString();
        return;
    }

    // prepare data for unused menu ids
    std::set<uint32> menu_ids;                              // for later integrity check
    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))   // check unused menu ids only in strict mode
    {
        for (GossipMenusMap::const_iterator itr = m_mGossipMenusMap.begin(); itr != m_mGossipMenusMap.end(); ++itr)
        {
            if (itr->first)
            {
                menu_ids.insert(itr->first);
            }
        }

        for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
        {
            if (uint32 menuid = itr->GetGossipMenuId())
            {
                menu_ids.erase(menuid);
            }
        }
    }

    // loading
    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    // prepare menuid -> CreatureInfo map for fast access
    typedef  std::multimap<uint32, const CreatureInfo*> Menu2CInfoMap;
    Menu2CInfoMap menu2CInfoMap;
    for (uint32 i = 1;  i < sCreatureStorage.GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (cInfo->GossipMenuId)
            {
                menu2CInfoMap.insert(Menu2CInfoMap::value_type(cInfo->GossipMenuId, cInfo));

                // unused check data preparing part
                if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
                {
                    menu_ids.erase(cInfo->GossipMenuId);
                }
            }
        }
    }

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        GossipMenuItems gMenuItem;

        gMenuItem.menu_id               = fields[0].GetUInt32();
        gMenuItem.id                    = fields[1].GetUInt32();
        gMenuItem.option_icon           = fields[2].GetUInt8();
        gMenuItem.option_text           = fields[3].GetCppString();
        gMenuItem.option_id             = fields[4].GetUInt32();
        gMenuItem.npc_option_npcflag    = fields[5].GetUInt32();
        gMenuItem.action_menu_id        = fields[6].GetInt32();
        gMenuItem.action_poi_id         = fields[7].GetUInt32();
        gMenuItem.action_script_id      = fields[8].GetUInt32();
        gMenuItem.box_coded             = fields[9].GetUInt8() != 0;
        gMenuItem.box_text              = fields[11].GetCppString();

        gMenuItem.conditionId           = fields[12].GetUInt16();

        if (gMenuItem.menu_id)                              // == 0 id is special and not have menu_id data
        {
            if (m_mGossipMenusMap.find(gMenuItem.menu_id) == m_mGossipMenusMap.end())
            {
                sLog.outErrorDb("Gossip menu option (MenuId: %u) for nonexistent menu", gMenuItem.menu_id);
                continue;
            }
        }

        if (gMenuItem.action_menu_id > 0)
        {
            if (m_mGossipMenusMap.find(gMenuItem.action_menu_id) == m_mGossipMenusMap.end())
            {
                sLog.outErrorDb("Gossip menu option (MenuId: %u Id: %u) have action_menu_id = %u for nonexistent menu", gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_menu_id);
            }
            else if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
            {
                menu_ids.erase(gMenuItem.action_menu_id);
            }
        }

        if (gMenuItem.option_icon >= GOSSIP_ICON_MAX)
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has unknown icon id %u. Replacing with GOSSIP_ICON_CHAT", gMenuItem.menu_id, gMenuItem.id, gMenuItem.option_icon);
            gMenuItem.option_icon = GOSSIP_ICON_CHAT;
        }

        if (gMenuItem.option_id == GOSSIP_OPTION_NONE)
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u use option id GOSSIP_OPTION_NONE. Option will never be used", gMenuItem.menu_id, gMenuItem.id);
        }

        if (gMenuItem.option_id >= GOSSIP_OPTION_MAX)
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has unknown option id %u. Option will not be used", gMenuItem.menu_id, gMenuItem.id, gMenuItem.option_id);
        }

        if (gMenuItem.menu_id && gMenuItem.npc_option_npcflag)
        {
            bool found_menu_uses = false;
            bool found_flags_uses = false;

            std::pair<Menu2CInfoMap::const_iterator, Menu2CInfoMap::const_iterator> tm_bounds = menu2CInfoMap.equal_range(gMenuItem.menu_id);
            for (Menu2CInfoMap::const_iterator it2 = tm_bounds.first; !found_flags_uses && it2 != tm_bounds.second; ++it2)
            {
                CreatureInfo const* cInfo = it2->second;

                found_menu_uses = true;

                // some from creatures with gossip menu can use gossip option base at npc_flags
                if (gMenuItem.npc_option_npcflag & cInfo->NpcFlags)
                {
                    found_flags_uses = true;
                }
            }

            if (found_menu_uses && !found_flags_uses)
            {
                sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has `npc_option_npcflag` = %u but creatures using this menu does not have corresponding `NpcFlags`. Option will not accessible in game.", gMenuItem.menu_id, gMenuItem.id, gMenuItem.npc_option_npcflag);
            }
        }

        if (gMenuItem.action_poi_id && !GetPointOfInterest(gMenuItem.action_poi_id))
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u use non-existing action_poi_id %u, ignoring", gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_poi_id);
            gMenuItem.action_poi_id = 0;
        }

        if (gMenuItem.action_script_id)
        {
            ScriptChainMap const* scm = sScriptMgr.GetScriptChainMap(DBS_ON_GOSSIP);
            if (!scm)
            {
                continue;
            }

            if (scm->find(gMenuItem.action_script_id) == scm->end())
            {
                sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u have action_script_id %u that does not exist in `db_scripts [type = %d]`, ignoring", gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_script_id, DBS_ON_GOSSIP);
                continue;
            }

            // Remove used script id
            gossipScriptSet.erase(gMenuItem.action_script_id);
        }

        if (gMenuItem.conditionId)
        {
            const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(gMenuItem.conditionId);
            if (!condition)
            {
                sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has condition_id %u that does not exist in `conditions`, ignoring", gMenuItem.menu_id, gMenuItem.id, gMenuItem.conditionId);
                gMenuItem.conditionId = 0;
            }
        }

        m_mGossipMenuItemsMap.insert(GossipMenuItemsMap::value_type(gMenuItem.menu_id, gMenuItem));

        ++count;
    }
    while (result->NextRow());

    delete result;

    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
    {
        for (std::set<uint32>::const_iterator itr = menu_ids.begin(); itr != menu_ids.end(); ++itr)
        {
            sLog.outErrorDb("Table `gossip_menu` contain unused (in creature or GO or menu options) menu id %u.", *itr);
        }
    }

    sLog.outString(">> Loaded %u gossip_menu_option entries", count);
    sLog.outString();
}

/**
 * @brief Reloads gossip menus, gossip options, and their core-side caches.
 */
void ObjectMgr::LoadGossipMenus()
{
    ScriptChainMap const* scm = sScriptMgr.GetScriptChainMap(DBS_ON_GOSSIP);
    if (!scm)
    {
        return;
    }

    // Check which script-ids in db_scripts type DBS_ON_GOSSIP are not used
    std::set<uint32> gossipScriptSet;
    for (ScriptChainMap::const_iterator itr = scm->begin(); itr != scm->end(); ++itr)
    {
        gossipScriptSet.insert(itr->first);
    }

    // Load gossip_menu and gossip_menu_option data
    sLog.outString("(Re)Loading Gossip menus...");
    LoadGossipMenu(gossipScriptSet);
    sLog.outString("(Re)Loading Gossip menu options...");
    LoadGossipMenuItems(gossipScriptSet);

    for (std::set<uint32>::const_iterator itr = gossipScriptSet.begin(); itr != gossipScriptSet.end(); ++itr)
    {
        sLog.outErrorDb("Table `db_scripts [type = %d]` contains unused script, id %u.", DBS_ON_GOSSIP, *itr);
    }

    LoadCoreSideGossipTextIdCache();
}
