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

/**
 * @brief Returns the script chain map for a database script type.
 *
 * @param type The database script type.
 * @return ScriptChainMap const* The corresponding script chain map, or NULL for unsupported types.
 */
ScriptChainMap const* ScriptMgr::GetScriptChainMap(DBScriptType type)
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, _guard, m_lock, NULL)
    if ((type != DBS_INTERNAL) && type < DBS_END)
    {
        return &m_dbScripts[type];
    }

    return NULL;
}

// /////////////////////////////////////////////////////////
//              DB SCRIPTS (loaders of static data)
// /////////////////////////////////////////////////////////
// returns priority (0 == can not start script)
uint8 GetSpellStartDBScriptPriority(SpellEntry const* spellinfo, SpellEffectIndex effIdx)
{
#if defined (CATA)
    SpellEffectEntry const* spellEffect = spellinfo->GetSpellEffect(effIdx);
    if (!spellEffect)
    {
        return 0;
    }
#endif
#if defined (CATA)
    if (spellEffect->Effect == SPELL_EFFECT_SCRIPT_EFFECT)
#else
    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_SCRIPT_EFFECT)
#endif
    {
        return 10;
    }

#if defined (CATA)
    if (spellEffect->Effect == SPELL_EFFECT_DUMMY)
#else
    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_DUMMY)
#endif
    {
        return 9;
    }

    // NonExisting triggered spells can also start DB-Spell-Scripts
#if defined (CATA)
    if (spellEffect->Effect == SPELL_EFFECT_TRIGGER_SPELL && !sSpellStore.LookupEntry(spellEffect->EffectTriggerSpell))
#else
    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_TRIGGER_SPELL && !sSpellStore.LookupEntry(spellinfo->EffectTriggerSpell[effIdx]))
#endif
    {
        return 5;
    }

    // NonExisting trigger missile spells can also start DB-Spell-Scripts
#if defined (CATA)
    if (spellEffect->Effect == SPELL_EFFECT_TRIGGER_MISSILE && !sSpellStore.LookupEntry(spellEffect->EffectTriggerSpell))
#else
    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_TRIGGER_MISSILE && !sSpellStore.LookupEntry(spellinfo->EffectTriggerSpell[effIdx]))
#endif
    {
        return 4;
    }

    // Can not start script
    return 0;
}

// Priorize: SCRIPT_EFFECT before DUMMY before Non-Existing triggered spell, for same priority the first effect with the priority triggers
bool ScriptMgr::CanSpellEffectStartDBScript(SpellEntry const* spellinfo, SpellEffectIndex effIdx)
{
    uint8 priority = GetSpellStartDBScriptPriority(spellinfo, effIdx);
    if (!priority)
    {
        return false;
    }

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint8 currentPriority = GetSpellStartDBScriptPriority(spellinfo, SpellEffectIndex(i));
        if (currentPriority < priority)                     // lower priority, continue checking
        {
            continue;
        }
        if (currentPriority > priority)                     // take other index with higher priority
        {
            return false;
        }
        if (i < effIdx)                                     // same priority at lower index
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Loads and validates raw db_script records for a specific script type.
 *
 * @param type The database script type to load.
 */
void ScriptMgr::LoadScripts(DBScriptType type)
{
    if (IsScriptScheduled())                                // function don't must be called in time scripts use.
    {
        return;
    }

    m_dbScripts[type].clear();                                 // need for reload support

    //                                                 0   1      2        3         4          5            6              7           8        9         10        11        12 13 14 15
    QueryResult* result = WorldDatabase.PQuery("SELECT `id`, `delay`, `command`, `datalong`, `datalong2`, `buddy_entry`, `search_radius`, `data_flags`, `dataint`, `dataint2`, `dataint3`, `dataint4`, `x`, `y`, `z`, `o` FROM `db_scripts` WHERE `script_type` = %d ORDER BY `script_guid` ASC", type);

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u script definitions from `db_scripts [type %d]` table", count, type);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        ScriptInfo tmp;
        tmp.id           = fields[0].GetUInt32();
        tmp.delay        = fields[1].GetUInt32();
        tmp.command      = fields[2].GetUInt32();
        tmp.raw.data[0]  = fields[3].GetUInt32();
        tmp.raw.data[1]  = fields[4].GetUInt32();
        tmp.buddyEntry   = fields[5].GetUInt32();
        tmp.searchRadiusOrGuid = fields[6].GetUInt32();
        tmp.data_flags   = fields[7].GetUInt8();
        tmp.textId[0]    = fields[8].GetInt32();
        tmp.textId[1]    = fields[9].GetInt32();
        tmp.textId[2]    = fields[10].GetInt32();
        tmp.textId[3]    = fields[11].GetInt32();
        tmp.x            = fields[12].GetFloat();
        tmp.y            = fields[13].GetFloat();
        tmp.z            = fields[14].GetFloat();
        tmp.o            = fields[15].GetFloat();

        // generic command args check
        if (tmp.buddyEntry && !(tmp.data_flags & SCRIPT_FLAG_BUDDY_BY_GUID))
        {
            if (tmp.IsCreatureBuddy() && !ObjectMgr::GetCreatureTemplate(tmp.buddyEntry))
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has buddyEntry = %u in command %u for script id %u, but this creature_template does not exist, skipping.", type, tmp.buddyEntry, tmp.command, tmp.id);
                continue;
            }
            else if (!tmp.IsCreatureBuddy() && !ObjectMgr::GetGameObjectInfo(tmp.buddyEntry))
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has buddyEntry = %u in command %u for script id %u, but this gameobject_template does not exist, skipping.", type, tmp.buddyEntry, tmp.command, tmp.id);
                continue;
            }
            if (!tmp.searchRadiusOrGuid)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has searchRadius = 0 in command %u for script id %u for buddy %u, skipping.", type, tmp.command, tmp.id, tmp.buddyEntry);
                continue;
            }
        }

        if (tmp.data_flags)                                 // Check flags
        {
            if (tmp.data_flags & ~MAX_SCRIPT_FLAG_VALID)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid data_flags %u in command %u for script id %u, skipping.", type, tmp.data_flags, tmp.command, tmp.id);
                continue;
            }
            if (!tmp.HasAdditionalScriptFlag() && tmp.data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid data_flags %u in command %u for script id %u, skipping.", type, tmp.data_flags, tmp.command, tmp.id);
                continue;
            }
            if (tmp.data_flags & SCRIPT_FLAG_BUDDY_AS_TARGET && ! tmp.buddyEntry)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has buddy required in data_flags %u in command %u for script id %u, but no buddy defined, skipping.", type, tmp.data_flags, tmp.command, tmp.id);
                continue;
            }
            if (tmp.data_flags & SCRIPT_FLAG_BUDDY_BY_GUID) // Check guid
            {
                if (tmp.IsCreatureBuddy())
                {
                    CreatureData const* data = sObjectMgr.GetCreatureData(tmp.searchRadiusOrGuid);
                    if (!data)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]`, script %u has buddy defined by guid (SCRIPT_FLAG_BUDDY_BY_GUID %u set) but no npc spawned with guid %u, skipping.", type, tmp.id, SCRIPT_FLAG_BUDDY_BY_GUID,  tmp.searchRadiusOrGuid);
                        continue;
                    }
                    if (data->id != tmp.buddyEntry)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has buddy defined by guid (SCRIPT_FLAG_BUDDY_BY_GUID %u set) but spawned npc with guid %u has entry %u, expected buddy_entry is %u, skipping.", type, SCRIPT_FLAG_BUDDY_BY_GUID,  tmp.searchRadiusOrGuid, data->id, tmp.buddyEntry);
                        continue;
                    }
                }
                else
                {
                    GameObjectData const* data = sObjectMgr.GetGOData(tmp.searchRadiusOrGuid);
                    if (!data)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has go-buddy defined by guid (SCRIPT_FLAG_BUDDY_BY_GUID %u set) but no go spawned with guid %u, skipping.", type, SCRIPT_FLAG_BUDDY_BY_GUID,  tmp.searchRadiusOrGuid);
                        continue;
                    }
                    if (data->id != tmp.buddyEntry)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has go-buddy defined by guid (SCRIPT_FLAG_BUDDY_BY_GUID %u set) but spawned go with guid %u has entry %u, expected buddy_entry is %u, skipping.", type, SCRIPT_FLAG_BUDDY_BY_GUID,  tmp.searchRadiusOrGuid, data->id, tmp.buddyEntry);
                        continue;
                    }
                }
            }
        }

        switch (tmp.command)
        {
            case SCRIPT_COMMAND_TALK:                       // 0
            {
                if (tmp.textId[0] == 0)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid talk text id (dataint = %i) in SCRIPT_COMMAND_TALK for script id %u", type, tmp.textId[0], tmp.id);
                    continue;
                }

                for (int i = 0; i < MAX_TEXT_ID; ++i)
                {
                    if (tmp.textId[i] && (tmp.textId[i] < MIN_DB_SCRIPT_STRING_ID || tmp.textId[i] >= MAX_DB_SCRIPT_STRING_ID))
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has out of range text_id%u (dataint = %i expected %u-%u) in SCRIPT_COMMAND_TALK for script id %u", type, i + 1, tmp.textId[i], MIN_DB_SCRIPT_STRING_ID, MAX_DB_SCRIPT_STRING_ID, tmp.id);
                        continue;
                    }
                }

                // if (!GetMangosStringLocale(tmp.dataint)) will be checked after db_script_string loading
                break;
            }
            case SCRIPT_COMMAND_EMOTE:                      // 1
            {
                if (!sEmotesStore.LookupEntry(tmp.emote.emoteId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid emote id (datalong = %u) in SCRIPT_COMMAND_EMOTE for script id %u", type, tmp.emote.emoteId, tmp.id);
                    continue;
                }
                for (int i = 0; i < MAX_TEXT_ID; ++i)
                {
                    if (tmp.textId[i] && !sEmotesStore.LookupEntry(tmp.textId[i]))
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid emote id (text_id%u = %u) in SCRIPT_COMMAND_EMOTE for script id %u", type, i + 1, tmp.textId[i], tmp.id);
                        continue;
                    }
                }
                break;
            }
            case SCRIPT_COMMAND_FIELD_SET:                  // 2
            case SCRIPT_COMMAND_MOVE_TO:                    // 3
            case SCRIPT_COMMAND_FLAG_SET:                   // 4
            case SCRIPT_COMMAND_FLAG_REMOVE:                // 5
                break;
            case SCRIPT_COMMAND_TELEPORT_TO:                // 6
            {
                if (!sMapStore.LookupEntry(tmp.teleportTo.mapId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid map (Id: %u) in SCRIPT_COMMAND_TELEPORT_TO for script id %u", type, tmp.teleportTo.mapId, tmp.id);
                    continue;
                }

                if (!MaNGOS::IsValidMapCoord(tmp.x, tmp.y, tmp.z, tmp.o))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid coordinates (X: %f Y: %f) in SCRIPT_COMMAND_TELEPORT_TO for script id %u", type, tmp.x, tmp.y, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_QUEST_EXPLORED:             // 7
            {
                Quest const* quest = sObjectMgr.GetQuestTemplate(tmp.questExplored.questId);
                if (!quest)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid quest (ID: %u) in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u", type, tmp.questExplored.questId, tmp.id);
                    continue;
                }

                if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has quest (ID: %u) in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u, but quest not have flag QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT in quest flags. Script command or quest flags wrong. Quest modified to require objective.", type, tmp.questExplored.questId, tmp.id);

                    // this will prevent quest completing without objective
                    const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);

                    // continue; - quest objective requirement set and command can be allowed
                }

                if (float(tmp.questExplored.distance) > DEFAULT_VISIBILITY_DISTANCE)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has too large distance (%u) for exploring objective complete in `datalong2` in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u",
                        type, tmp.questExplored.distance, tmp.id);
                    continue;
                }

                if (tmp.questExplored.distance && float(tmp.questExplored.distance) > DEFAULT_VISIBILITY_DISTANCE)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has too large distance (%u) for exploring objective complete in `datalong2` in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u, max distance is %f or 0 for disable distance check",
                        type, tmp.questExplored.distance, tmp.id, DEFAULT_VISIBILITY_DISTANCE);
                    continue;
                }

                if (tmp.questExplored.distance && float(tmp.questExplored.distance) < INTERACTION_DISTANCE)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has too small distance (%u) for exploring objective complete in `datalong2` in SCRIPT_COMMAND_QUEST_EXPLORED in `datalong` for script id %u, min distance is %f or 0 for disable distance check",
                        type, tmp.questExplored.distance, tmp.id, INTERACTION_DISTANCE);
                    continue;
                }

                break;
            }
            case SCRIPT_COMMAND_KILL_CREDIT:                // 8
            {
                if (tmp.killCredit.creatureEntry && !ObjectMgr::GetCreatureTemplate(tmp.killCredit.creatureEntry))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid creature (Entry: %u) in SCRIPT_COMMAND_KILL_CREDIT for script id %u", type, tmp.killCredit.creatureEntry, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_RESPAWN_GO:                 // 9
            {
                uint32 goEntry;
                if (!tmp.GetGOGuid())
                {
                    if (!tmp.buddyEntry)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has no gameobject nor buddy defined in SCRIPT_COMMAND_RESPAWN_GO for script id %u", type, tmp.id);
                        continue;
                    }
                    goEntry = tmp.buddyEntry;
                }
                else
                {
                    GameObjectData const* data = sObjectMgr.GetGOData(tmp.GetGOGuid());
                    if (!data)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid gameobject (GUID: %u) in SCRIPT_COMMAND_RESPAWN_GO for script id %u", type, tmp.GetGOGuid(), tmp.id);
                        continue;
                    }
                    goEntry = data->id;
                }

                GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(goEntry);
                if (!info)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has gameobject with invalid entry (GUID: %u Entry: %u) in SCRIPT_COMMAND_RESPAWN_GO for script id %u", type, tmp.GetGOGuid(), goEntry, tmp.id);
                    continue;
                }

                if (info->type == GAMEOBJECT_TYPE_FISHINGNODE ||
                    info->type == GAMEOBJECT_TYPE_FISHINGHOLE ||
                    info->type == GAMEOBJECT_TYPE_DOOR)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` have gameobject type (%u) unsupported by command SCRIPT_COMMAND_RESPAWN_GO for script id %u", type, info->type, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:       // 10
            {
                if (!MaNGOS::IsValidMapCoord(tmp.x, tmp.y, tmp.z, tmp.o))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid coordinates (X: %f Y: %f) in SCRIPT_COMMAND_TEMP_SUMMON_CREATURE for script id %u", type, tmp.x, tmp.y, tmp.id);
                    continue;
                }

                if (!ObjectMgr::GetCreatureTemplate(tmp.summonCreature.creatureEntry))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid creature (Entry: %u) in SCRIPT_COMMAND_TEMP_SUMMON_CREATURE for script id %u", type, tmp.summonCreature.creatureEntry, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_OPEN_DOOR:                  // 11
            case SCRIPT_COMMAND_CLOSE_DOOR:                 // 12
            {
                uint32 goEntry;
                if (!tmp.GetGOGuid())
                {
                    if (!tmp.buddyEntry)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has no gameobject nor buddy defined in %s for script id %u", type, (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ? "SCRIPT_COMMAND_OPEN_DOOR" : "SCRIPT_COMMAND_CLOSE_DOOR"), tmp.id);
                        continue;
                    }
                    goEntry = tmp.buddyEntry;
                }
                else
                {
                    GameObjectData const* data = sObjectMgr.GetGOData(tmp.GetGOGuid());
                    if (!data)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid gameobject (GUID: %u) in %s for script id %u", type, tmp.GetGOGuid(), (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ? "SCRIPT_COMMAND_OPEN_DOOR" : "SCRIPT_COMMAND_CLOSE_DOOR"), tmp.id);
                        continue;
                    }
                    goEntry = data->id;
                }

                GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(goEntry);
                if (!info)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has gameobject with invalid entry (GUID: %u Entry: %u) in %s for script id %u", type, tmp.GetGOGuid(), goEntry, (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ? "SCRIPT_COMMAND_OPEN_DOOR" : "SCRIPT_COMMAND_CLOSE_DOOR"), tmp.id);
                    continue;
                }

                if (info->type != GAMEOBJECT_TYPE_DOOR)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has gameobject type (%u) non supported by command %s for script id %u", type, info->id, (tmp.command == SCRIPT_COMMAND_OPEN_DOOR ? "SCRIPT_COMMAND_OPEN_DOOR" : "SCRIPT_COMMAND_CLOSE_DOOR"), tmp.id);
                    continue;
                }

                break;
            }
            case SCRIPT_COMMAND_ACTIVATE_OBJECT:            // 13
                break;
            case SCRIPT_COMMAND_REMOVE_AURA:                // 14
            {
                if (!sSpellStore.LookupEntry(tmp.removeAura.spellId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` using nonexistent spell (id: %u) in SCRIPT_COMMAND_REMOVE_AURA or SCRIPT_COMMAND_CAST_SPELL for script id %u",
                        type, tmp.removeAura.spellId, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_CAST_SPELL:                 // 15
            {
                if (!sSpellStore.LookupEntry(tmp.castSpell.spellId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` using nonexistent spell (id: %u) in SCRIPT_COMMAND_REMOVE_AURA or SCRIPT_COMMAND_CAST_SPELL for script id %u",
                        type, tmp.castSpell.spellId, tmp.id);
                    continue;
                }
                bool hasErrored = false;
                for (uint8 i = 0; i < MAX_TEXT_ID; ++i)
                {
                    if (tmp.textId[i] && !sSpellStore.LookupEntry(uint32(tmp.textId[i])))
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` using nonexistent spell (id: %u) in SCRIPT_COMMAND_CAST_SPELL for script id %u, dataint%u",
                            type, uint32(tmp.textId[i]), tmp.id, i + 1);
                        hasErrored = true;
                    }
                }
                if (hasErrored)
                {
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_PLAY_SOUND:                 // 16
            {
                if (!sSoundEntriesStore.LookupEntry(tmp.playSound.soundId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` using nonexistent sound (id: %u) in SCRIPT_COMMAND_PLAY_SOUND for script id %u",
                        type, tmp.playSound.soundId, tmp.id);
                    continue;
                }
                // bitmask: 0/1=target-player, 0/2=with distance dependent, 0/4=map wide, 0/8=zone wide
                if (tmp.playSound.flags & ~(1 | 2 | 4 | 8))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` using unsupported sound flags (datalong2: %u) in SCRIPT_COMMAND_PLAY_SOUND for script id %u, unsupported flags will be ignored", type, tmp.playSound.flags, tmp.id);
                }
                if ((tmp.playSound.flags & (1 | 2)) > 0 && (tmp.playSound.flags & (4 | 8)) > 0)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` uses sound flags (datalong2: %u) in SCRIPT_COMMAND_PLAY_SOUND for script id %u, combining (1|2) with (4|8) makes no sense", type, tmp.playSound.flags, tmp.id);
                }
                break;
            }
            case SCRIPT_COMMAND_CREATE_ITEM:                // 17
            {
                if (!ObjectMgr::GetItemPrototype(tmp.createItem.itemEntry))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has nonexistent item (entry: %u) in SCRIPT_COMMAND_CREATE_ITEM for script id %u",
                        type, tmp.createItem.itemEntry, tmp.id);
                    continue;
                }
                if (!tmp.createItem.amount)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` SCRIPT_COMMAND_CREATE_ITEM but amount is %u for script id %u",
                        type, tmp.createItem.amount, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_DESPAWN_SELF:               // 18
            {
                // for later, we might consider despawn by database guid, and define in datalong2 as option to despawn self.
                break;
            }
            case SCRIPT_COMMAND_PLAY_MOVIE:                 // 19
            {
#if defined(CLASSIC)
                sLog.outErrorDb("Table `db_scripts [type = %d]` use unsupported SCRIPT_COMMAND_PLAY_MOVIE for script id %u",
                    type, tmp.id);
                continue;
#else
                if (!sMovieStore.LookupEntry(tmp.playMovie.movieId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` use non-existing movie_id (id: %u) in SCRIPT_COMMAND_PLAY_MOVIE for script id %u",
                        type, tmp.playMovie.movieId, tmp.id);
                    continue;
                }
                break;
#endif
            }
            case SCRIPT_COMMAND_MOVEMENT:                   // 20
            {
                if (tmp.movement.movementType >= MAX_DB_MOTION_TYPE)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` SCRIPT_COMMAND_MOVEMENT has invalid MovementType %u for script id %u",
                        type, tmp.movement.movementType, tmp.id);
                    continue;
                }

                break;
            }
            case SCRIPT_COMMAND_SET_ACTIVEOBJECT:           // 21
            {
                break;
            }
            case SCRIPT_COMMAND_SET_FACTION:                // 22
            {
                if (tmp.faction.factionId && !sFactionTemplateStore.LookupEntry(tmp.faction.factionId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_SET_FACTION for script id %u, but this faction-template does not exist.", type, tmp.faction.factionId, tmp.id);
                    continue;
                }

                break;
            }
            case SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL:    // 23
            {
                if (tmp.data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                {
                    if (tmp.morph.creatureOrModelEntry && !sCreatureDisplayInfoStore.LookupEntry(tmp.morph.creatureOrModelEntry))
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL for script id %u, but this model does not exist.", type, tmp.morph.creatureOrModelEntry, tmp.id);
                        continue;
                    }
                }
                else
                {
                    if (tmp.morph.creatureOrModelEntry && !ObjectMgr::GetCreatureTemplate(tmp.morph.creatureOrModelEntry))
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL for script id %u, but this creature_template does not exist.", type, tmp.morph.creatureOrModelEntry, tmp.id);
                        continue;
                    }
                }

                break;
            }
            case SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL:    // 24
            {
                if (tmp.data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                {
                    if (tmp.mount.creatureOrModelEntry && !sCreatureDisplayInfoStore.LookupEntry(tmp.mount.creatureOrModelEntry))
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL for script id %u, but this model does not exist.", type, tmp.mount.creatureOrModelEntry, tmp.id);
                        continue;
                    }
                }
                else
                {
                    if (tmp.mount.creatureOrModelEntry && !ObjectMgr::GetCreatureTemplate(tmp.mount.creatureOrModelEntry))
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL for script id %u, but this creature_template does not exist.", type, tmp.mount.creatureOrModelEntry, tmp.id);
                        continue;
                    }
                }

                break;
            }
            case SCRIPT_COMMAND_SET_RUN:                    // 25
            case SCRIPT_COMMAND_ATTACK_START:               // 26
            {
                break;
            }
            case SCRIPT_COMMAND_GO_LOCK_STATE:              // 27
            {
                // lock(0x01) and unlock(0x02) together
                if (((tmp.goLockState.lockState & 0x01) && (tmp.goLockState.lockState & 0x02)) ||
                    // non-interact (0x4) and interact (0x08) together
                    ((tmp.goLockState.lockState & 0x04) && (tmp.goLockState.lockState & 0x08)) ||
                    // no setting
                    !tmp.goLockState.lockState ||
                    // invalid number
                    tmp.goLockState.lockState >= 0x10)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid lock state (datalong = %u) in SCRIPT_COMMAND_GO_LOCK_STATE for script id %u.", type, tmp.goLockState.lockState, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_STAND_STATE:                // 28
            {
                if (tmp.standState.stand_state >= MAX_UNIT_STAND_STATE)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid stand state (datalong = %u) in SCRIPT_COMMAND_STAND_STATE for script id %u", type, tmp.standState.stand_state, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_MODIFY_NPC_FLAGS:           // 29
            {
                break;
            }
            case SCRIPT_COMMAND_SEND_TAXI_PATH:             // 30
            {
                if (!sTaxiPathStore.LookupEntry(tmp.sendTaxiPath.taxiPathId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_SEND_TAXI_PATH for script id %u, but this taxi path does not exist.", type, tmp.sendTaxiPath.taxiPathId, tmp.id);
                    continue;
                }
                // Check if this taxi path can be triggered with a spell
                if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
                {
                    uint32 taxiSpell = 0;
                    for (uint32 i = 1; i < sSpellStore.GetNumRows() && taxiSpell == 0; ++i)
                    {
                        if (SpellEntry const* spell = sSpellStore.LookupEntry(i))
                        {
                            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                            {
#if defined (CATA)
                                SpellEffectEntry const* spellEffect = spell->GetSpellEffect(SpellEffectIndex(j));
                                if (!spellEffect)
                                {
                                    continue;
                                }

                                if (spellEffect->Effect == SPELL_EFFECT_SEND_TAXI && spellEffect->EffectMiscValue == tmp.sendTaxiPath.taxiPathId)
                                {
                                    taxiSpell = i;
                                    break;
                                }
#else
                                if (spell->Effect[j] == SPELL_EFFECT_SEND_TAXI && spell->EffectMiscValue[j] == int32(tmp.sendTaxiPath.taxiPathId))
                                {
                                    taxiSpell = i;
                                    break;
                                }
#endif
                            }
                        }
                    }

                    if (taxiSpell)
                    {
                        sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_SEND_TAXI_PATH for script id %u, but this taxi path can be triggered by spell %u.", type, tmp.sendTaxiPath.taxiPathId, tmp.id, taxiSpell);
                        continue;
                    }
                }
                break;
            }
            case SCRIPT_COMMAND_TERMINATE_SCRIPT:           // 31
            {
                if (tmp.terminateScript.npcEntry && !ObjectMgr::GetCreatureTemplate(tmp.terminateScript.npcEntry))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_TERMINATE_SCRIPT for script id %u, but this npc entry does not exist.", type, tmp.sendTaxiPath.taxiPathId, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_PAUSE_WAYPOINTS:            // 32
            {
                break;
            }
            case SCRIPT_COMMAND_JOIN_LFG:                   // 33
            {
                //Only currently used in Zero
                break;
            }
            case SCRIPT_COMMAND_TERMINATE_COND:             // 34
            {
                if (!sConditionStorage.LookupEntry<PlayerCondition>(tmp.terminateCond.conditionId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_TERMINATE_COND for script id %u, but this condition_id does not exist.", type, tmp.terminateCond.conditionId, tmp.id);
                    continue;
                }
                if (tmp.terminateCond.failQuest && !sObjectMgr.GetQuestTemplate(tmp.terminateCond.failQuest))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong2 = %u in SCRIPT_COMMAND_TERMINATE_COND for script id %u, but this questId does not exist.", type, tmp.terminateCond.failQuest, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_SEND_AI_EVENT_AROUND:       // 35
            {
                if (tmp.sendAIEvent.eventType >= MAXIMAL_AI_EVENT_EVENTAI)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid AI event (datalong = %u) in SCRIPT_COMMAND_SEND_AI_EVENT for script id %u", type, tmp.sendAIEvent.eventType, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_TURN_TO:                    // 36
            {
                break;
            }
            case SCRIPT_COMMAND_MOVE_DYNAMIC:               // 37
            {
                if (tmp.moveDynamic.maxDist < tmp.moveDynamic.minDist)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid min-dist (datalong2 = %u) less than max-dist (datalon = %u) in SCRIPT_COMMAND_MOVE_DYNAMIC for script id %u", type, tmp.moveDynamic.minDist, tmp.moveDynamic.maxDist, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_SEND_MAIL:                  // 38
            {
                if (!sMailTemplateStore.LookupEntry(tmp.sendMail.mailTemplateId))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid mailTemplateId (datalong = %u) in SCRIPT_COMMAND_SEND_MAIL for script id %u", type, tmp.sendMail.mailTemplateId, tmp.id);
                    continue;
                }
                if (tmp.sendMail.altSender && !ObjectMgr::GetCreatureTemplate(tmp.sendMail.altSender))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid alternativeSender (datalong2 = %u) in SCRIPT_COMMAND_SEND_MAIL for script id %u", type, tmp.sendMail.altSender, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_CHANGE_ENTRY:              // 39
            {
                if (tmp.changeEntry.creatureEntry && !ObjectMgr::GetCreatureTemplate(tmp.changeEntry.creatureEntry))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_CHANGE_ENTRY for script id %u, but this creature_template does not exist.", type, tmp.changeEntry.creatureEntry, tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_DESPAWN_GO:                   // 40
            {
                if (!tmp.despawnGo.goGuid)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has no gameobject defined in SCRIPT_COMMAND_DESPAWN_GO for script id %u", type, tmp.id);
                    continue;
                }

                GameObjectData const* data = sObjectMgr.GetGOData(tmp.despawnGo.goGuid);
                if (!data)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid gameobject (GUID: %u) in SCRIPT_COMMAND_RESPAWN_GO for script id %u", type, tmp.despawnGo.goGuid, tmp.id);
                    continue;
                }

                break;
            }
            case SCRIPT_COMMAND_RESPAWN:                      // 41
            {
                break;
            }
            case SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS:          // 42
            {
                if (tmp.textId[0] < 0 || tmp.textId[1] < 0 || tmp.textId[2] < 0)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid equipment slot (dataint = %u, dataint2 = %u dataint3 = %u) in SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS for script id %u", type, tmp.textId[0], tmp.textId[1], tmp.textId[2], tmp.id);
                    continue;
                }
                break;
            }
            case SCRIPT_COMMAND_RESET_GO:                     // 43
            {
                break;
            }
            case SCRIPT_COMMAND_UPDATE_TEMPLATE:              // 44
            {
#if defined(CLASSIC) || defined(TBC) || defined(WOTLK)
                if (tmp.updateTemplate.entry && !ObjectMgr::GetCreatureTemplate(tmp.updateTemplate.entry))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_UPDATE_TEMPLATE for script id %u, but this creature_template does not exist.", type, tmp.updateTemplate.entry, tmp.id);
                    continue;
                }

                if (tmp.updateTemplate.faction > 1)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` uses invalid faction team (datalong2 = %u, must be 0 or 1) in SCRIPT_COMMAND_UPDATE_TEMPLATE for script id %u", type, tmp.updateTemplate.faction, tmp.id);
                    continue;
                }
#else
                if (!sCreatureStorage.LookupEntry<CreatureInfo>(tmp.updateTemplate.entry))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has datalong = %u in SCRIPT_COMMAND_UPDATE_TEMPLATE for script id %u, but this creature_template does not exist.", type, tmp.updateTemplate.entry, tmp.id);
                    continue;
                }

                if (tmp.updateTemplate.faction != 0 && tmp.updateTemplate.faction != 1)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` uses invalid faction team (datalong2 = %u, must be 0 or 1) in SCRIPT_COMMAND_UPDATE_TEMPLATE for script id %u", type, tmp.updateTemplate.faction, tmp.id);
                    continue;
                }
#endif
                break;
            }
            case SCRIPT_COMMAND_XP_USER:                      // 53
            {
                break;
            }
            case SCRIPT_COMMAND_SET_FLY:                      // 59
            {
                break;
            }
            default:
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` uses unknown command %u, skipping.", type, tmp.command);
                continue;
            }
        }

        if (m_dbScripts[type].find(tmp.id) == m_dbScripts[type].end())
        {
            ScriptChain emptyVec;
            m_dbScripts[type][tmp.id] = emptyVec;
        }
        m_dbScripts[type][tmp.id].push_back(tmp);

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u script definitions from `db_scripts [type = %d]` table", count, type);
    sLog.outString();
}

/**
 * @brief Loads db scripts for a type and validates that their script ids refer to existing objects.
 *
 * @param t The database script type to load.
 */
void ScriptMgr::LoadDbScripts(DBScriptType t)
{
    std::set<uint32> eventIds;                              // Store possible event ids

    if (t == DBS_ON_EVENT)
    {
        CollectPossibleEventIds(eventIds);
    }

    {
        ACE_GUARD(ACE_Thread_Mutex, _g, m_lock)
        LoadScripts(t);
    }

    ScriptChainMap& scm = m_dbScripts[t];

    for (ScriptChainMap::const_iterator itr = scm.begin(); itr != scm.end(); ++itr)
    {
        switch (t)
        {
            case DBS_ON_QUEST_START:
            case DBS_ON_QUEST_END:
                if (!sObjectMgr.GetQuestTemplate(itr->first))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has not existing quest (Id: %u) as script id", t, itr->first);
                }
                break;

            case DBS_ON_CREATURE_DEATH:
                if (!sObjectMgr.GetCreatureTemplate(itr->first))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has not existing creature (Entry: %u) as script id", t, itr->first);
                }
                break;

            case DBS_ON_SPELL:
            {
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);
                if (!spellInfo)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has not existing spell (Id: %u) as script id", t, itr->first);
                    continue;
                }

                // check for correct spellEffect
                bool found = false;
                for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                {
                    if (GetSpellStartDBScriptPriority(spellInfo, SpellEffectIndex(i)))
                    {
                        found =  true;
                        break;
                    }
                }
                if (!found)
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has unsupported spell (Id: %u)", t, itr->first);
                }
                break;
            }
            case DBS_ON_GO_USE:
                if (!sObjectMgr.GetGOData(itr->first))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]`, has not existing gameobject (GUID: %u) as script id", t, itr->first);
                }
                break;

            case DBS_ON_GOT_USE:
                if (!sObjectMgr.GetGameObjectInfo(itr->first))
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has not existing gameobject (Entry: %u) as script id", t, itr->first);
                }
                break;

            case DBS_ON_EVENT:
            {
                std::set<uint32>::const_iterator itr2 = eventIds.find(itr->first);
                if (itr2 == eventIds.end())
                {
                    sLog.outErrorDb("Table `db_scripts [type = %d]` has script (Id: %u) not referring to any fitting gameobject_template or any spell effect %u or path taxi node data", t, itr->first, SPELL_EFFECT_SEND_EVENT);
                }
                break;
            }
            default:
                break;
        }
    }
}

/**
 * @brief Loads db_script_string records and checks their usage from scripts and waypoints.
 */
void ScriptMgr::LoadDbScriptStrings()
{
    sObjectMgr.LoadMangosStrings(WorldDatabase, "db_script_string", MIN_DB_SCRIPT_STRING_ID, MAX_DB_SCRIPT_STRING_ID, true);

    std::set<int32> ids;

    for (int32 i = MIN_DB_SCRIPT_STRING_ID; i < MAX_DB_SCRIPT_STRING_ID; ++i)
    {
        if (sObjectMgr.GetMangosStringLocale(i))
        {
            ids.insert(i);
        }
    }

    CheckScriptTexts(ids);
    sWaypointMgr.CheckTextsExistance(ids);

    for (std::set<int32>::const_iterator itr = ids.begin(); itr != ids.end(); ++itr)
    {
        sLog.outErrorDb("Table `db_script_string` has unused string id %u", *itr);
    }
}

/**
 * @brief Validates script text ids referenced by db scripts and removes used ids from the provided set.
 *
 * @param ids The set of loaded string ids that will be trimmed as usages are found.
 */
void ScriptMgr::CheckScriptTexts(std::set<int32>& ids)
{
    for (int t = DBS_START; t < DBS_END; ++t)
    {
        for (ScriptChainMap::const_iterator itrCM = m_dbScripts[t].begin(); itrCM != m_dbScripts[t].end(); ++itrCM)
        {
            for (ScriptChain::const_iterator itrC = itrCM->second.begin(); itrC != itrCM->second.end(); ++itrC)
            {
                if (itrC->command == SCRIPT_COMMAND_TALK)
                {
                    for (int i = 0; i < MAX_TEXT_ID; ++i)
                    {
                        if (itrC->textId[i] && !sObjectMgr.GetMangosStringLocale(itrC->textId[i]))
                        {
                            sLog.outErrorDb("Table `db_script_string` is missing string id %u, used in `db_script [type = %d]` table, id %u.", itrC->textId[i], t, itrCM->first);
                        }

                        if (ids.find(itrC->textId[i]) != ids.end())
                        {
                            ids.erase(itrC->textId[i]);
                        }
                    }
                }
            }
        }
    }
}

// /////////////////////////////////////////////////////////
//              DB SCRIPT ENGINE
// /////////////////////////////////////////////////////////















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
