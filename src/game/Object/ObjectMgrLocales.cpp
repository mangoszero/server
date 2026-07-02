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
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "DBCStores.h"
#include "GossipDef.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "Policies/Singleton.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
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
 * @brief Loads localized creature names and subnames from the database.
 */
void ObjectMgr::LoadCreatureLocales()
{
    mCreatureLocaleMap.clear();                             // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`name_loc1`,`subname_loc1`,`name_loc2`,`subname_loc2`,`name_loc3`,`subname_loc3`,`name_loc4`,`subname_loc4`,`name_loc5`,`subname_loc5`,`name_loc6`,`subname_loc6`,`name_loc7`,`subname_loc7`,`name_loc8`,`subname_loc8` FROM `locales_creature`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 creature locale strings. DB table `locales_creature` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetCreatureTemplate(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_creature` has data for not existed creature entry %u, skipped.", entry);
            continue;
        }

        CreatureLocale& data = mCreatureLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[1 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                    {
                        data.Name.resize(idx + 1);
                    }

                    data.Name[idx] = str;
                }
            }
            str = fields[1 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.SubName.size() <= idx)
                    {
                        data.SubName.resize(idx + 1);
                    }

                    data.SubName[idx] = str;
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %zu creature locale strings", mCreatureLocaleMap.size());
    sLog.outString();
}

/**
 * @brief Loads localized gossip menu option and confirmation box text.
 */
void ObjectMgr::LoadGossipMenuItemsLocales()
{
    mGossipMenuItemsLocaleMap.clear();                      // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `menu_id`,`id`,"
        "`option_text_loc1`,`box_text_loc1`,`option_text_loc2`,`box_text_loc2`,"
        "`option_text_loc3`,`box_text_loc3`,`option_text_loc4`,`box_text_loc4`,"
        "`option_text_loc5`,`box_text_loc5`,`option_text_loc6`,`box_text_loc6`,"
        "`option_text_loc7`,`box_text_loc7`,`option_text_loc8`,`box_text_loc8` "
        "FROM `locales_gossip_menu_option`");

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 gossip_menu_option locale strings. DB table `locales_gossip_menu_option` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint16 menuId   = fields[0].GetUInt16();
        uint16 id       = fields[1].GetUInt16();

        GossipMenuItemsMapBounds bounds = GetGossipMenuItemsMapBounds(menuId);

        bool found = false;
        if (bounds.first != bounds.second)
        {
            for (GossipMenuItemsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                if (itr->second.id == id)
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            ERROR_DB_STRICT_LOG("Table `locales_gossip_menu_option` has data for nonexistent gossip menu %u item %u, skipped.", menuId, id);
            continue;
        }

        GossipMenuItemsLocale& data = mGossipMenuItemsLocaleMap[MAKE_PAIR32(menuId, id)];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[2 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.OptionText.size() <= idx)
                    {
                        data.OptionText.resize(idx + 1);
                    }

                    data.OptionText[idx] = str;
                }
            }
            str = fields[2 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.BoxText.size() <= idx)
                    {
                        data.BoxText.resize(idx + 1);
                    }

                    data.BoxText[idx] = str;
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %zu gossip_menu_option locale strings", mGossipMenuItemsLocaleMap.size());
    sLog.outString();
}

/**
 * @brief Loads localized point-of-interest icon names.
 */
void ObjectMgr::LoadPointOfInterestLocales()
{
    mPointOfInterestLocaleMap.clear();                      // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`icon_name_loc1`,`icon_name_loc2`,`icon_name_loc3`,`icon_name_loc4`,`icon_name_loc5`,`icon_name_loc6`,`icon_name_loc7`,`icon_name_loc8` FROM `locales_points_of_interest`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 points_of_interest locale strings. DB table `locales_points_of_interest` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetPointOfInterest(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_points_of_interest` has data for nonexistent POI entry %u, skipped.", entry);
            continue;
        }

        PointOfInterestLocale& data = mPointOfInterestLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (str.empty())
            {
                continue;
            }

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if (data.IconName.size() <= size_t(idx))
                {
                    data.IconName.resize(idx + 1);
                }

                data.IconName[idx] = str;
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %zu points_of_interest locale strings", mPointOfInterestLocaleMap.size());
    sLog.outString();
}

/**
 * @brief Loads localized page text content.
 */
void ObjectMgr::LoadPageTextLocales()
{
    mPageTextLocaleMap.clear();                             // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`text_loc1`,`text_loc2`,`text_loc3`,`text_loc4`,`text_loc5`,`text_loc6`,`text_loc7`,`text_loc8` FROM `locales_page_text`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 PageText locale strings. DB table `locales_page_text` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!sPageTextStore.LookupEntry<PageText>(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_page_text` has data for nonexistent page text entry %u, skipped.", entry);
            continue;
        }

        PageTextLocale& data = mPageTextLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (str.empty())
            {
                continue;
            }

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if ((int32)data.Text.size() <= idx)
                {
                    data.Text.resize(idx + 1);
                }

                data.Text[idx] = str;
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %zu PageText locale strings", mPageTextLocaleMap.size());
    sLog.outString();
}

/**
 * @brief Loads localized npc gossip text variants.
 */
void ObjectMgr::LoadGossipTextLocales()
{
    mNpcTextLocaleMap.clear();                              // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
        "`Text0_0_loc1`,`Text0_1_loc1`,`Text1_0_loc1`,`Text1_1_loc1`,`Text2_0_loc1`,`Text2_1_loc1`,`Text3_0_loc1`,`Text3_1_loc1`,`Text4_0_loc1`,`Text4_1_loc1`,`Text5_0_loc1`,`Text5_1_loc1`,`Text6_0_loc1`,`Text6_1_loc1`,`Text7_0_loc1`,`Text7_1_loc1`,"
        "`Text0_0_loc2`,`Text0_1_loc2`,`Text1_0_loc2`,`Text1_1_loc2`,`Text2_0_loc2`,`Text2_1_loc2`,`Text3_0_loc2`,`Text3_1_loc2`,`Text4_0_loc2`,`Text4_1_loc2`,`Text5_0_loc2`,`Text5_1_loc2`,`Text6_0_loc2`,`Text6_1_loc2`,`Text7_0_loc2`,`Text7_1_loc2`,"
        "`Text0_0_loc3`,`Text0_1_loc3`,`Text1_0_loc3`,`Text1_1_loc3`,`Text2_0_loc3`,`Text2_1_loc3`,`Text3_0_loc3`,`Text3_1_loc3`,`Text4_0_loc3`,`Text4_1_loc3`,`Text5_0_loc3`,`Text5_1_loc3`,`Text6_0_loc3`,`Text6_1_loc3`,`Text7_0_loc3`,`Text7_1_loc3`,"
        "`Text0_0_loc4`,`Text0_1_loc4`,`Text1_0_loc4`,`Text1_1_loc4`,`Text2_0_loc4`,`Text2_1_loc4`,`Text3_0_loc4`,`Text3_1_loc4`,`Text4_0_loc4`,`Text4_1_loc4`,`Text5_0_loc4`,`Text5_1_loc4`,`Text6_0_loc4`,`Text6_1_loc4`,`Text7_0_loc4`,`Text7_1_loc4`,"
        "`Text0_0_loc5`,`Text0_1_loc5`,`Text1_0_loc5`,`Text1_1_loc5`,`Text2_0_loc5`,`Text2_1_loc5`,`Text3_0_loc5`,`Text3_1_loc5`,`Text4_0_loc5`,`Text4_1_loc5`,`Text5_0_loc5`,`Text5_1_loc5`,`Text6_0_loc5`,`Text6_1_loc5`,`Text7_0_loc5`,`Text7_1_loc5`,"
        "`Text0_0_loc6`,`Text0_1_loc6`,`Text1_0_loc6`,`Text1_1_loc6`,`Text2_0_loc6`,`Text2_1_loc6`,`Text3_0_loc6`,`Text3_1_loc6`,`Text4_0_loc6`,`Text4_1_loc6`,`Text5_0_loc6`,`Text5_1_loc6`,`Text6_0_loc6`,`Text6_1_loc6`,`Text7_0_loc6`,`Text7_1_loc6`,"
        "`Text0_0_loc7`,`Text0_1_loc7`,`Text1_0_loc7`,`Text1_1_loc7`,`Text2_0_loc7`,`Text2_1_loc7`,`Text3_0_loc7`,`Text3_1_loc7`,`Text4_0_loc7`,`Text4_1_loc7`,`Text5_0_loc7`,`Text5_1_loc7`,`Text6_0_loc7`,`Text6_1_loc7`,`Text7_0_loc7`,`Text7_1_loc7`, "
        "`Text0_0_loc8`,`Text0_1_loc8`,`Text1_0_loc8`,`Text1_1_loc8`,`Text2_0_loc8`,`Text2_1_loc8`,`Text3_0_loc8`,`Text3_1_loc8`,`Text4_0_loc8`,`Text4_1_loc8`,`Text5_0_loc8`,`Text5_1_loc8`,`Text6_0_loc8`,`Text6_1_loc8`,`Text7_0_loc8`,`Text7_1_loc8` "
        " FROM `locales_npc_text`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 Quest locale strings. DB table `locales_npc_text` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetGossipText(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_npc_text` has data for nonexistent gossip text entry %u, skipped.", entry);
            continue;
        }

        NpcTextLocale& data = mNpcTextLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                std::string str0 = fields[1 + 8 * 2 * (i - 1) + 2 * j].GetCppString();
                if (!str0.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_0[j].size() <= idx)
                        {
                            data.Text_0[j].resize(idx + 1);
                        }

                        data.Text_0[j][idx] = str0;
                    }
                }
                std::string str1 = fields[1 + 8 * 2 * (i - 1) + 2 * j + 1].GetCppString();
                if (!str1.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_1[j].size() <= idx)
                        {
                            data.Text_1[j].resize(idx + 1);
                        }

                        data.Text_1[j][idx] = str1;
                    }
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %zu NpcText locale strings", mNpcTextLocaleMap.size());
    sLog.outString();
}

/**
 * @brief Loads localized gameobject names.
 */
void ObjectMgr::LoadGameObjectLocales()
{
    mGameObjectLocaleMap.clear();                           // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
        "`name_loc1`,`name_loc2`,`name_loc3`,`name_loc4`,`name_loc5`,`name_loc6`,`name_loc7`,`name_loc8` FROM `locales_gameobject`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 gameobject locale strings. DB table `locales_gameobject` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetGameObjectInfo(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_gameobject` has data for nonexistent gameobject entry %u, skipped.", entry);
            continue;
        }

        GameObjectLocale& data = mGameObjectLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                    {
                        data.Name.resize(idx + 1);
                    }

                    data.Name[idx] = str;
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %zu gameobject locale strings", mGameObjectLocaleMap.size());
    sLog.outString();
}
