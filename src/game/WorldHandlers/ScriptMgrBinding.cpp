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
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include "SQLStorages.h"
#include "Policies/Singleton.h"
#include "WaypointManager.h"
#include "World.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "WaypointMovementGenerator.h"
#include "Mail.h"
#include <DBCStores.h>
#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif /* ENABLE_SD3 */
#ifdef CLASSIC
#include "LFGMgr.h"
#endif /* CLASSIC */
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

// /////////////////////////////////////////////////////////
//              Scripting Library Hooks
// /////////////////////////////////////////////////////////
void ScriptMgr::LoadScriptBinding()
{
#ifdef ENABLE_SD3
    for (int i = 0; i < SCRIPTED_MAX_TYPE; ++i)
    {
        m_scriptBind[i].clear();
    }

    QueryResult* result = WorldDatabase.PQuery("SELECT `type`, `bind`, `ScriptName`, `data` FROM `script_binding`");
    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded no script binding.");
        sLog.outString();
        return;
    }

    std::set<uint32> eventIds;                              // Store possible event ids, for checking
    CollectPossibleEventIds(eventIds);

    BarGoLink bar(result->GetRowCount());
    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint8 type = fields[0].GetUInt8();
        int32 id = fields[1].GetInt32();
        const char* scriptName = fields[2].GetString();
        uint8 data = fields[3].GetUInt8();

        if (type >= SCRIPTED_MAX_TYPE)
        {
            sLog.outErrorScriptLib("script_binding table contains a script for non-existent type %u (bind %d), ignoring.", type, id);
            continue;
        }
        uint32 scriptId = GetScriptId(scriptName);
        if (!scriptId)  //this should never happen! the script names are initialized from the same table
        {
            sLog.outErrorScriptLib("something is very bad with your script_binding table!");
            continue;
        }

        // checking if the scripted object actually exists
        bool exists = false;
        switch (type)
        {
            case SCRIPTED_UNIT:
                exists = id > 0 ? bool(sCreatureStorage.LookupEntry<CreatureInfo>(uint32(id))) : bool(sObjectMgr.GetCreatureData(uint32(-id)));
                break;
            case SCRIPTED_GAMEOBJECT:
                exists = id > 0 ? bool(sGOStorage.LookupEntry<GameObjectInfo>(uint32(id))) : bool(sObjectMgr.GetGOData(uint32(-id)));
                break;
            case SCRIPTED_ITEM:
                exists = bool(sItemStorage.LookupEntry<ItemPrototype>(uint32(id)));
                break;
            case SCRIPTED_AREATRIGGER:
                exists = bool(sAreaTriggerStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_SPELL:
            case SCRIPTED_AURASPELL:
                exists = bool(sSpellStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_MAPEVENT:
                exists = eventIds.count(uint32(id));
                break;
            case SCRIPTED_MAP:
                exists = bool(sMapStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_PVP_ZONE: // for now, no check on special zones
                exists = bool(sAreaStore.LookupEntry(uint32(id)));
                break;
            case SCRIPTED_BATTLEGROUND:
                if (MapEntry const* mapEntry = sMapStore.LookupEntry(uint32(id)))
                {
                    exists = mapEntry->IsBattleGround();
                }
                break;
            case SCRIPTED_INSTANCE:
                if (MapEntry const* mapEntry = sMapStore.LookupEntry(uint32(id)))
                {
                    exists = mapEntry->IsDungeon();
                }
                break;
            case SCRIPTED_CONDITION:
                exists = sConditionStorage.LookupEntry<PlayerCondition>(uint32(id));
                break;
            case SCRIPTED_ACHIEVEMENT:
                break;
        }

        if (!exists)
        {
            sLog.outErrorScriptLib("script type %u (%s) is bound to non-existing entry %d, ignoring.", type, scriptName, id);
            continue;
        }

        if (type == SCRIPTED_SPELL || type == SCRIPTED_AURASPELL)
        {
            id |= uint32(data) << 24;   //incorporate spell effect number into the key
        }

        m_scriptBind[type][id] = scriptId;
    }
    while (result->NextRow());

    delete result;
    sLog.outString("Of the total %u script bindings, loaded succesfully:", count);
    for (uint8 i = 0; i < SCRIPTED_MAX_TYPE; ++i)
    {
        if (m_scriptBind[i].size()) //ignore missing script types to shorten the log
        {
            sLog.outString(".. type %u: %u binds", i, uint32(m_scriptBind[i].size()));
            count -= m_scriptBind[i].size();
        }
    }
    sLog.outString("Thus, %u script binds are found bad.", count);

    sLog.outString();
#endif /* ENABLE_SD3 */
    return;
}

/**
 * @brief Reloads script bindings in debug builds.
 *
 * @return true if bindings were reloaded; otherwise false.
 */
bool ScriptMgr::ReloadScriptBinding()
{
#ifdef _DEBUG
    std::unique_lock<std::shared_mutex> guard(m_bindMutex);
    LoadScriptBinding();
    return true;
#else
    return false;
#endif /* _DEBUG */
}

/**
 * @brief Loads and sorts the distinct script names referenced by script bindings.
 */
void ScriptMgr::LoadScriptNames()
{
    m_scriptNames.push_back("");
    QueryResult* result = WorldDatabase.Query("SELECT DISTINCT(`ScriptName`) FROM `script_binding`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded empty set of Script Names!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;

    do
    {
        bar.step();
        m_scriptNames.push_back((*result)[0].GetString());
        ++count;
    }
    while (result->NextRow());
    delete result;

    std::sort(m_scriptNames.begin(), m_scriptNames.end());

    sLog.outString(">> Loaded %d unique Script Names", count);
    sLog.outString();
}

/**
 * @brief Resolves a script name to its internal script id.
 *
 * @param name The script name to search for.
 * @return uint32 The resolved script id, or 0 if not found.
 */
uint32 ScriptMgr::GetScriptId(const char* name) const
{
    // use binary search to find the script name in the sorted vector
    // assume "" is the first element
    if (!name)
    {
        return 0;
    }

    ScriptNameMap::const_iterator itr =
        std::lower_bound(m_scriptNames.begin(), m_scriptNames.end(), name);

    if (itr == m_scriptNames.end() || *itr != name)
    {
        return 0;
    }

    return uint32(itr - m_scriptNames.begin());
}

/**
 * @brief Returns the script id bound to a specific scripted entity entry.
 *
 * @param entity The scripted object type.
 * @param entry The object entry or binding key.
 * @return uint32 The bound script id, or 0 if none exists.
 */
uint32 ScriptMgr::GetBoundScriptId(ScriptedObjectType entity, int32 entry)
{
#ifdef _DEBUG
    std::shared_lock<std::shared_mutex> guard(m_bindMutex);
#endif /* _DEBUG */
    uint32 id = 0;
    if (entity < SCRIPTED_MAX_TYPE)
    {
        EntryToScriptIdMap::iterator it = m_scriptBind[entity].find(entry);
        if (it != m_scriptBind[entity].end())
        {
            id = it->second;
        }
    }
    else
    {
        sLog.outErrorScriptLib("asking a script for non-existing entity type %u!", entity);
    }
    return id;
}

/**
 * @brief Returns the version string for the loaded script library.
 *
 * @return char const* The script library version, or NULL when unavailable.
 */
char const* ScriptMgr::GetScriptLibraryVersion() const
{
#ifdef ENABLE_SD3
    return SD3::GetScriptLibraryVersion();
#else
    return NULL;
#endif
}
