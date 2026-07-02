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
#include "MapManager.h"
#include "ProgressBar.h"
#include "Util.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "SQLStorages.h"
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
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

/**
 * @brief Loads saved game teleport locations.
 */
void ObjectMgr::LoadGameTele()
{
    m_GameTeleMap.clear();                                  // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, `name` FROM `game_tele`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded `game_tele`, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 id         = fields[0].GetUInt32();

        GameTele gt;

        gt.position_x     = fields[1].GetFloat();
        gt.position_y     = fields[2].GetFloat();
        gt.position_z     = fields[3].GetFloat();
        gt.orientation    = fields[4].GetFloat();
        gt.mapId          = fields[5].GetUInt32();
        gt.name           = fields[6].GetCppString();

        if (!MapManager::IsValidMapCoord(gt.mapId, gt.position_x, gt.position_y, gt.position_z, gt.orientation))
        {
            sLog.outErrorDb("Wrong position for id %u (name: %s) in `game_tele` table, ignoring.", id, gt.name.c_str());
            continue;
        }

        if (!Utf8toWStr(gt.name, gt.wnameLow))
        {
            sLog.outErrorDb("Wrong UTF8 name for id %u in `game_tele` table, ignoring.", id);
            continue;
        }

        wstrToLower(gt.wnameLow);

        m_GameTeleMap[id] = gt;

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u GameTeleports", count);
    sLog.outString();
}

/**
 * @brief Finds a game teleport by exact or partial name.
 *
 * @param name The teleport name to search for.
 * @return The matching teleport, or null if none matches.
 */
GameTele const* ObjectMgr::GetGameTele(const std::string& name) const
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return NULL;
    }

    // converting string that we try to find to lower case
    wstrToLower(wname);

    // Alternative first GameTele what contains wnameLow as substring in case no GameTele location found
    const GameTele* alt = NULL;
    for (GameTeleMap::const_iterator itr = m_GameTeleMap.begin(); itr != m_GameTeleMap.end(); ++itr)
    {
        if (itr->second.wnameLow == wname)
        {
            return &itr->second;
        }
        else if (alt == NULL && itr->second.wnameLow.find(wname) != std::wstring::npos)
        {
            alt = &itr->second;
        }
    }
    return alt;
}

/**
 * @brief Adds a new game teleport and stores it in the database.
 *
 * @param tele The teleport data to add.
 * @return true if the teleport was stored successfully; otherwise, false.
 */
bool ObjectMgr::AddGameTele(GameTele& tele)
{
    // find max id
    uint32 new_id = 0;
    for (GameTeleMap::const_iterator itr = m_GameTeleMap.begin(); itr != m_GameTeleMap.end(); ++itr)
    {
        if (itr->first > new_id)
        {
            new_id = itr->first;
        }
    }

    // use next
    ++new_id;

    if (!Utf8toWStr(tele.name, tele.wnameLow))
    {
        return false;
    }

    wstrToLower(tele.wnameLow);

    m_GameTeleMap[new_id] = tele;
    std::string safeName(tele.name);
    WorldDatabase.escape_string(safeName);

    return WorldDatabase.PExecuteLog("INSERT INTO `game_tele` "
        "(`id`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`name`) "
        "VALUES (%u,%f,%f,%f,%f,%u,'%s')",
        new_id, tele.position_x, tele.position_y, tele.position_z,
        tele.orientation, tele.mapId, safeName.c_str());
}

/**
 * @brief Deletes a game teleport by name.
 *
 * @param name The teleport name.
 * @return true if a teleport was removed; otherwise, false.
 */
bool ObjectMgr::DeleteGameTele(const std::string& name)
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wname);

    for (GameTeleMap::iterator itr = m_GameTeleMap.begin(); itr != m_GameTeleMap.end(); ++itr)
    {
        if (itr->second.wnameLow == wname)
        {
            WorldDatabase.PExecuteLog("DELETE FROM `game_tele` WHERE `name` = '%s'", itr->second.name.c_str());
            m_GameTeleMap.erase(itr);
            return true;
        }
    }

    return false;
}
