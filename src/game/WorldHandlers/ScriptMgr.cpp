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
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include "WaypointManager.h"
#include "World.h"
#include <DBCStores.h>
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "SQLStorages.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "WaypointMovementGenerator.h"
#include "Mail.h"
#if defined(CLASSIC)
#include "LFGMgr.h"
#endif

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif

#include <cstring> /* std::strcmp */

INSTANTIATE_SINGLETON_1(ScriptMgr);

ScriptMgr::ScriptMgr() : m_scheduledScripts(0), m_lock(0)
{
    m_dbScripts.resize(DBS_END);

    ScriptChainMap emptyMap;

    for (int t = DBS_START; t < DBS_END; ++t)
    {
        m_dbScripts[t] = emptyMap;
    }
}

ScriptMgr::~ScriptMgr()
{
    m_dbScripts.clear();
}








// /////////////////////////////////////////////////////////
//              DB SCRIPT ENGINE
// /////////////////////////////////////////////////////////










































/**
 * @brief Loads or reloads the named script library.
 *
 * @param libName The script library name.
 * @return ScriptLoadResult The library loading result.
 */
ScriptLoadResult ScriptMgr::LoadScriptLibrary(const char* libName)
{
#ifdef ENABLE_SD3
    if (std::strcmp(libName, "mangosscript") == 0)
    {
        SD3::FreeScriptLibrary();
        SD3::InitScriptLibrary();
        return SCRIPT_LOAD_OK;
    }
#endif

    return SCRIPT_LOAD_ERR_NOT_FOUND;
}

/**
 * @brief Unloads the currently active script library.
 */
void ScriptMgr::UnloadScriptLibrary()
{
#ifdef ENABLE_SD3
    SD3::FreeScriptLibrary();
#else
    return;
#endif
}

/**
 * @brief Collects event ids that can legally start database event scripts.
 *
 * @param eventIds The set that receives discovered event ids.
 */
void ScriptMgr::CollectPossibleEventIds(std::set<uint32>& eventIds)
{
    // Load all possible script entries from gameobjects
    for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
    {
        switch (itr->type)
        {
            case GAMEOBJECT_TYPE_GOOBER:
                eventIds.insert(itr->goober.eventId);
                break;
            case GAMEOBJECT_TYPE_CHEST:
                eventIds.insert(itr->chest.eventId);
                break;
            case GAMEOBJECT_TYPE_CAMERA:
                eventIds.insert(itr->camera.eventID);
                break;
            case GAMEOBJECT_TYPE_CAPTURE_POINT:
                eventIds.insert(itr->capturePoint.neutralEventID1);
                eventIds.insert(itr->capturePoint.neutralEventID2);
                eventIds.insert(itr->capturePoint.contestedEventID1);
                eventIds.insert(itr->capturePoint.contestedEventID2);
                eventIds.insert(itr->capturePoint.progressEventID1);
                eventIds.insert(itr->capturePoint.progressEventID2);
                eventIds.insert(itr->capturePoint.winEventID1);
                eventIds.insert(itr->capturePoint.winEventID2);
                break;
#if defined(WOTLK) || defined (CATA) || defined (MISTS)
            case GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING:
                eventIds.insert(itr->destructibleBuilding.damagedEvent);
                eventIds.insert(itr->destructibleBuilding.destroyedEvent);
                eventIds.insert(itr->destructibleBuilding.intactEvent);
                eventIds.insert(itr->destructibleBuilding.rebuildingEvent);
                break;
#endif
            default:
                break;
        }
    }

    // Load all possible script entries from spells
    for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(i);
        if (spell)
        {
            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
#if defined (CATA)
                SpellEffectEntry const* spellEffect = spell->GetSpellEffect(SpellEffectIndex(j));
                if (!spellEffect)
                {
                    continue;
                }

                if (spellEffect->Effect == SPELL_EFFECT_SEND_EVENT)
                {
                    if (spellEffect->EffectMiscValue)
                    {
                        eventIds.insert(spellEffect->EffectMiscValue);
                    }
                }
#else
                if (spell->Effect[j] == SPELL_EFFECT_SEND_EVENT)
                {
                    if (spell->EffectMiscValue[j])
                    {
                        eventIds.insert(spell->EffectMiscValue[j]);
                    }
                }
#endif
            }
        }
    }
#if defined(TBC) || defined (WOTLK) || defined (CATA)
    // Load all possible event entries from taxi path nodes
    for (size_t path_idx = 0; path_idx < sTaxiPathNodesByPath.size(); ++path_idx)
    {
        for (size_t node_idx = 0; node_idx < sTaxiPathNodesByPath[path_idx].size(); ++node_idx)
        {
            TaxiPathNodeEntry const& node = sTaxiPathNodesByPath[path_idx][node_idx];

            if (node.arrivalEventID)
            {
                eventIds.insert(node.arrivalEventID);
            }

            if (node.departureEventID)
            {
                eventIds.insert(node.departureEventID);
            }
        }
    }
#endif
}

// Starters for events
bool StartEvents_Event(Map* map, uint32 id, Object* source, Object* target, bool isStart/*=true*/, Unit* forwardToPvp/*=NULL*/)
{
    MANGOS_ASSERT(source);

    // Handle SD3 script
    if (sScriptMgr.OnProcessEvent(id, source, target, isStart))
    {
        return true;
    }

    // Handle PvP Calls
    if (forwardToPvp && source->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        BattleGround* bg = NULL;
        OutdoorPvP* opvp = NULL;
        if (forwardToPvp->GetTypeId() == TYPEID_PLAYER)
        {
            bg = ((Player*)forwardToPvp)->GetBattleGround();
            if (!bg)
            {
                opvp = sOutdoorPvPMgr.GetScript(((Player*)forwardToPvp)->GetCachedZoneId());
            }
        }
        else
        {
#if defined(CLASSIC)
            if (map->IsBattleGround())
#else
            if (map->IsBattleGroundOrArena())
#endif
            {
                bg = ((BattleGroundMap*)map)->GetBG();
            }
            else                                            // Use the go, because GOs don't move
            {
                opvp = sOutdoorPvPMgr.GetScript(((GameObject*)source)->GetZoneId());
            }
        }

        if (bg && bg->HandleEvent(id, static_cast<GameObject*>(source)))
        {
            return true;
        }

        if (opvp && opvp->HandleEvent(id, static_cast<GameObject*>(source)))
        {
            return true;
        }
    }

    Map::ScriptExecutionParam execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE_TARGET;
    if (source->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
    {
        execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE;
    }
    else if (target && target->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
    {
        execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET;
    }

    return map->ScriptsStart(DBS_ON_EVENT, id, source, target, execParam);
}

// Wrappers
uint32 GetScriptId(const char* name)
{
    return sScriptMgr.GetScriptId(name);
}

/**
 * @brief Returns the script name for a script id.
 *
 * @param id The internal script id.
 * @return char const* The matching script name.
 */
char const* GetScriptName(uint32 id)
{
    return sScriptMgr.GetScriptName(id);
}

/**
 * @brief Returns the number of registered script ids.
 *
 * @return uint32 The count of registered script ids.
 */
uint32 GetScriptIdsCount()
{
    return sScriptMgr.GetScriptIdsCount();
}

/**
 * @brief Sets the external waypoint table used by the waypoint manager.
 *
 * @param tableName The external waypoint table name.
 */
void SetExternalWaypointTable(char const* tableName)
{
    sWaypointMgr.SetExternalWPTable(tableName);
}

/**
 * @brief Adds a waypoint node from an external waypoint table.
 *
 * @param entry The creature entry owning the path.
 * @param pathId The path identifier.
 * @param pointId The waypoint point identifier.
 * @param x The waypoint X coordinate.
 * @param y The waypoint Y coordinate.
 * @param z The waypoint Z coordinate.
 * @param o The waypoint orientation.
 * @param waittime The wait time at the node.
 * @return true if the waypoint was added; otherwise false.
 */
bool AddWaypointFromExternal(uint32 entry, int32 pathId, uint32 pointId, float x, float y, float z, float o, uint32 waittime)
{
    return sWaypointMgr.AddExternalNode(entry, pathId, pointId, x, y, z, o, waittime);
}
