/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "ScriptMgr.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "ObjectMgr.h"
#include "WaypointManager.h"
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

#include "revision.h"

INSTANTIATE_SINGLETON_1(ScriptMgr);

ScriptMgr::ScriptMgr() : m_scheduledScripts(0), m_lock(0)
{
    m_dbScripts.resize(DBS_END);

    ScriptChainMap emptyMap;

    for (int t = DBS_START; t < DBS_END; ++t)
        m_dbScripts[t] = emptyMap;
}

ScriptMgr::~ScriptMgr()
{
    m_dbScripts.clear();
}

ScriptChainMap const* ScriptMgr::GetScriptChainMap(DBScriptType type)
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, _guard, m_lock, NULL)
    if ((type != DBS_INTERNAL) && type < DBS_END)
        return &m_dbScripts[type];

    return NULL;
}


// /////////////////////////////////////////////////////////
//              DB SCRIPTS (loaders of static data)
// /////////////////////////////////////////////////////////
// returns priority (0 == can not start script)
uint8 GetSpellStartDBScriptPriority(SpellEntry const* spellinfo, SpellEffectIndex effIdx)
{
    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_SCRIPT_EFFECT)
        { return 10; }

    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_DUMMY)
        { return 9; }

    // NonExisting triggered spells can also start DB-Spell-Scripts
    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_TRIGGER_SPELL && !sSpellStore.LookupEntry(spellinfo->EffectTriggerSpell[effIdx]))
        { return 5; }

    // NonExisting trigger missile spells can also start DB-Spell-Scripts
    if (spellinfo->Effect[effIdx] == SPELL_EFFECT_TRIGGER_MISSILE && !sSpellStore.LookupEntry(spellinfo->EffectTriggerSpell[effIdx]))
        { return 4; }

    // Can not start script
    return 0;
}

// Priorize: SCRIPT_EFFECT before DUMMY before Non-Existing triggered spell, for same priority the first effect with the priority triggers
bool ScriptMgr::CanSpellEffectStartDBScript(SpellEntry const* spellinfo, SpellEffectIndex effIdx)
{
    uint8 priority = GetSpellStartDBScriptPriority(spellinfo, effIdx);
    if (!priority)
        { return false; }

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        uint8 currentPriority = GetSpellStartDBScriptPriority(spellinfo, SpellEffectIndex(i));
        if (currentPriority < priority)                     // lower priority, continue checking
            { continue; }
        if (currentPriority > priority)                     // take other index with higher priority
            { return false; }
        if (i < effIdx)                                     // same priority at lower index
            { return false; }
    }

    return true;
}

void ScriptMgr::LoadScripts(DBScriptType type)
{
    if (IsScriptScheduled())                                // function don't must be called in time scripts use.
        { return; }

    m_dbScripts[type].clear();                                 // need for reload support

    //                                                 0   1      2        3         4          5            6              7           8        9         10        11        12 13 14 15
    QueryResult* result = WorldDatabase.PQuery("SELECT id, delay, command, datalong, datalong2, buddy_entry, search_radius, data_flags, dataint, dataint2, dataint3, dataint4, x, y, z, o FROM db_scripts WHERE script_type = %d ORDER BY script_guid ASC", type);

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
                    { continue; }
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
                    { sLog.outErrorDb("Table `db_scripts [type = %d]` using unsupported sound flags (datalong2: %u) in SCRIPT_COMMAND_PLAY_SOUND for script id %u, unsupported flags will be ignored", type, tmp.playSound.flags, tmp.id); }
                if ((tmp.playSound.flags & (1 | 2)) > 0 && (tmp.playSound.flags & (4 | 8)) > 0)
                    { sLog.outErrorDb("Table `db_scripts [type = %d]` uses sound flags (datalong2: %u) in SCRIPT_COMMAND_PLAY_SOUND for script id %u, combining (1|2) with (4|8) makes no sense", type, tmp.playSound.flags, tmp.id); }
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
                sLog.outErrorDb("Table `db_scripts [type = %d]` use unsupported SCRIPT_COMMAND_PLAY_MOVIE for script id %u",
                                type, tmp.id);
                continue;
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
                break;
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
                break;
            case SCRIPT_COMMAND_GO_LOCK_STATE:              // 27
            {
                if (// lock(0x01) and unlock(0x02) together
                    ((tmp.goLockState.lockState & 0x01) && (tmp.goLockState.lockState & 0x02)) ||
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
                break;
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
                            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                            {
                                if (spell->Effect[j] == SPELL_EFFECT_SEND_TAXI && spell->EffectMiscValue[j] == int32(tmp.sendTaxiPath.taxiPathId))
                                {
                                    taxiSpell = i;
                                    break;
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
                break;
            case SCRIPT_COMMAND_JOIN_LFG:                   // 33
                //Only currently used in Zero
                break;
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
                break;
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
            case SCRIPT_COMMAND_SET_FLY:                      // 39
            case SCRIPT_COMMAND_DESPAWN_GO:                   // 40
            case SCRIPT_COMMAND_RESPAWN:                      // 41
                break;
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
                break;
            case SCRIPT_COMMAND_UPDATE_TEMPLATE:              // 44
            {
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

void ScriptMgr::LoadDbScripts(DBScriptType t)
{

    std::set<uint32> eventIds;                              // Store possible event ids

    if (t == DBS_ON_EVENT)
      CollectPossibleEventIds(eventIds);

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
                  sLog.outErrorDb("Table `db_scripts [type = %d]` has not existing quest (Id: %u) as script id", t, itr->first);
                break;

            case DBS_ON_CREATURE_DEATH:
                if (!sObjectMgr.GetCreatureTemplate(itr->first))
                  sLog.outErrorDb("Table `db_scripts [type = %d]` has not existing creature (Entry: %u) as script id", t, itr->first);
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
                  sLog.outErrorDb("Table `db_scripts [type = %d]` has unsupported spell (Id: %u)", t, itr->first);
                break;
            }
            case DBS_ON_GO_USE:
                if (!sObjectMgr.GetGOData(itr->first))
                  sLog.outErrorDb("Table `db_scripts [type = %d]`, has not existing gameobject (GUID: %u) as script id", t, itr->first);
                break;

            case DBS_ON_GOT_USE:
                if (!sObjectMgr.GetGameObjectInfo(itr->first))
                  sLog.outErrorDb("Table `db_scripts [type = %d]` has not existing gameobject (Entry: %u) as script id", t, itr->first);
                break;

            case DBS_ON_EVENT:
            {
                std::set<uint32>::const_iterator itr2 = eventIds.find(itr->first);
                if (itr2 == eventIds.end())
                  sLog.outErrorDb("Table `db_scripts [type = %d]` has script (Id: %u) not referring to any fitting gameobject_template or any spell effect %u or path taxi node data", t, itr->first, SPELL_EFFECT_SEND_EVENT);
                break;
            }
            default:
                break;
        }
    }
}


void ScriptMgr::LoadDbScriptStrings()
{
    sObjectMgr.LoadMangosStrings(WorldDatabase, "db_script_string", MIN_DB_SCRIPT_STRING_ID, MAX_DB_SCRIPT_STRING_ID, true);

    std::set<int32> ids;

    for (int32 i = MIN_DB_SCRIPT_STRING_ID; i < MAX_DB_SCRIPT_STRING_ID; ++i)
        if (sObjectMgr.GetMangosStringLocale(i))
            { ids.insert(i); }

    CheckScriptTexts(ids);
    sWaypointMgr.CheckTextsExistance(ids);

    for (std::set<int32>::const_iterator itr = ids.begin(); itr != ids.end(); ++itr)
        { sLog.outErrorDb("Table `db_script_string` has unused string id %u", *itr); }
}

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
                          sLog.outErrorDb("Table `db_script_string` is missing string id %u, used in `db_script [type = %d]` table, id %u.", itrC->textId[i], t, itrCM->first);

                        if (ids.find(itrC->textId[i]) != ids.end())
                          ids.erase(itrC->textId[i]);
                    }
                }
            }
        }
    }
}

// /////////////////////////////////////////////////////////
//              DB SCRIPT ENGINE
// /////////////////////////////////////////////////////////

/// Helper function to get Object source or target for Script-Command
/// returns false iff an error happened
bool ScriptAction::GetScriptCommandObject(const ObjectGuid guid, bool includeItem, Object*& resultObject)
{
    resultObject = NULL;

    if (!guid)
        { return true; }

    switch (guid.GetHigh())
    {
        case HIGHGUID_UNIT:
            resultObject = m_map->GetCreature(guid);
            break;
        case HIGHGUID_PET:
            resultObject = m_map->GetPet(guid);
            break;
        case HIGHGUID_PLAYER:
            resultObject = m_map->GetPlayer(guid);
            break;
        case HIGHGUID_GAMEOBJECT:
            resultObject = m_map->GetGameObject(guid);
            break;
        case HIGHGUID_CORPSE:
            resultObject = sObjectAccessor.FindCorpse(guid);
            break;
        case HIGHGUID_ITEM:
            // case HIGHGUID_CONTAINER: ==HIGHGUID_ITEM
        {
            if (includeItem)
            {
                if (Player* player = m_map->GetPlayer(m_ownerGuid))
                    { resultObject = player->GetItemByGuid(guid); }
                break;
            }
            // else no break, but display error message
        }
        default:
            sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u with unsupported guid %s, skipping", m_type, m_script->id, m_script->command, guid.GetString().c_str());
            return false;
    }

    if (resultObject && !resultObject->IsInWorld())
        { resultObject = NULL; }

    return true;
}

/// Select source and target for a script command
/// Returns false iff an error happened
bool ScriptAction::GetScriptProcessTargets(WorldObject* pOrigSource, WorldObject* pOrigTarget, WorldObject*& pFinalSource, WorldObject*& pFinalTarget)
{
    WorldObject* pBuddy = NULL;

    if (m_script->buddyEntry)
    {
        if (m_script->data_flags & SCRIPT_FLAG_BUDDY_BY_GUID)
        {
            if (m_script->IsCreatureBuddy())
            {
                CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(m_script->buddyEntry);

                if (cinfo != NULL)
                {
                    pBuddy = m_map->GetCreature(cinfo->GetObjectGuid(m_script->searchRadiusOrGuid));

                    if (pBuddy && !((Creature*)pBuddy)->IsAlive())
                    {
                        sLog.outError(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u by guid %u but buddy is dead, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                        return false;
                    }
                }
                else
                {
                    sLog.outError(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has no buddy %u by guid %u, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                    return false;
                }
            }
            else
            {
                // GameObjectInfo const* ginfo = ObjectMgr::GetGameObjectInfo(m_script->buddyEntry);
                pBuddy = m_map->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, m_script->buddyEntry, m_script->searchRadiusOrGuid));
            }
            // TODO Maybe load related grid if not already done? How to handle multi-map case?
            if (!pBuddy)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u by guid %u not loaded in map %u (data-flags %u), skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid, m_map->GetId(), m_script->data_flags);
                return false;
            }
        }
        else                                                // Buddy by entry
        {
            if (!pOrigSource && !pOrigTarget)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without buddy %u, but no source for search available, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                return false;
            }

            // Prefer non-players as searcher
            WorldObject* pSearcher = pOrigSource ? pOrigSource : pOrigTarget;
            if (pSearcher->GetTypeId() == TYPEID_PLAYER && pOrigTarget && pOrigTarget->GetTypeId() != TYPEID_PLAYER)
                { pSearcher = pOrigTarget; }

            if (m_script->IsCreatureBuddy())
            {
                Creature* pCreatureBuddy = NULL;

                if (m_script->data_flags & SCRIPT_FLAG_BUDDY_IS_DESPAWNED)
                {
                    MaNGOS::AllCreaturesOfEntryInRangeCheck u_check(pSearcher, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                    MaNGOS::CreatureLastSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(pCreatureBuddy, u_check);
                    Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                }
                else
                {
                    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*pSearcher, m_script->buddyEntry, true, false, m_script->searchRadiusOrGuid, true);
                    MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pCreatureBuddy, u_check);

                    if (m_script->data_flags & SCRIPT_FLAG_BUDDY_IS_PET)
                        { Cell::VisitWorldObjects(pSearcher, searcher, m_script->searchRadiusOrGuid); }
                    else                                        // Normal Creature
                        { Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid); }
                }

                pBuddy = pCreatureBuddy;

                // TODO: Remove this extra check output after a while - it might have false effects
                if (!pBuddy && pSearcher->GetEntry() == m_script->buddyEntry)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: WARNING: Process table `db_scripts [type = %d]` id %u, command %u has no OTHER buddy %u found - maybe you need to update the script?", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                    pBuddy = pSearcher;
                }
            }
            else
            {
                GameObject* pGOBuddy = NULL;

                MaNGOS::NearestGameObjectEntryInObjectRangeCheck u_check(*pSearcher, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> searcher(pGOBuddy, u_check);

                Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                pBuddy = pGOBuddy;
            }

            if (!pBuddy)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u not found in range %u of searcher %s (data-flags %u), skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid, pSearcher->GetGuidStr().c_str(), m_script->data_flags);
                return false;
            }
        }
    }

    if (m_script->data_flags & SCRIPT_FLAG_BUDDY_AS_TARGET)
    {
        pFinalSource = pOrigSource;
        pFinalTarget = pBuddy;
    }
    else
    {
        pFinalSource = pBuddy ? pBuddy : pOrigSource;
        pFinalTarget = pOrigTarget;
    }

    if (m_script->data_flags & SCRIPT_FLAG_REVERSE_DIRECTION)
        { std::swap(pFinalSource, pFinalTarget); }

    if (m_script->data_flags & SCRIPT_FLAG_SOURCE_TARGETS_SELF)
        { pFinalTarget = pFinalSource; }

    return true;
}

/// Helper to log error information
bool ScriptAction::LogIfNotCreature(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_UNIT)
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-creature, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}
bool ScriptAction::LogIfNotUnit(WorldObject* pWorldObject)
{
    if (!pWorldObject || !pWorldObject->isType(TYPEMASK_UNIT))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-unit, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}
bool ScriptAction::LogIfNotGameObject(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_GAMEOBJECT)
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-gameobject, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}
bool ScriptAction::LogIfNotPlayer(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_PLAYER)
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-player, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

/// Helper to get a player if possible (target preferred)
Player* ScriptAction::GetPlayerTargetOrSourceAndLog(WorldObject* pSource, WorldObject* pTarget)
{
    if ((!pTarget || pTarget->GetTypeId() != TYPEID_PLAYER) && (!pSource || pSource->GetTypeId() != TYPEID_PLAYER))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non player, skipping.", m_type, m_script->id, m_script->command);
        return NULL;
    }

    return pTarget && pTarget->GetTypeId() == TYPEID_PLAYER ? (Player*)pTarget : (Player*)pSource;
}

/// Handle one Script Step
// Return true if and only if further parts of this script shall be skipped
bool ScriptAction::HandleScriptStep()
{
    WorldObject* pSource;
    WorldObject* pTarget;
    Object* pSourceOrItem;                                  // Stores a provided pSource (if exists as WorldObject) or source-item

    {
        // Add scope for source & target variables so that they are not used below
        Object* source = NULL;
        Object* target = NULL;
        if (!GetScriptCommandObject(m_sourceGuid, true, source))
            { return false; }
        if (!GetScriptCommandObject(m_targetGuid, false, target))
            { return false; }

        // Give some debug log output for easier use
        DEBUG_LOG("DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u for source %s (%sin world), target %s (%sin world)", m_type, m_script->id, m_script->command, m_sourceGuid.GetString().c_str(), source ? "" : "not ", m_targetGuid.GetString().c_str(), target ? "" : "not ");

        // Get expected source and target (if defined with buddy)
        pSource = source && source->isType(TYPEMASK_WORLDOBJECT) ? (WorldObject*)source : NULL;
        pTarget = target && target->isType(TYPEMASK_WORLDOBJECT) ? (WorldObject*)target : NULL;
        if (!GetScriptProcessTargets(pSource, pTarget, pSource, pTarget))
            { return false; }

        pSourceOrItem = pSource ? pSource : (source && source->isType(TYPEMASK_ITEM) ? source : NULL);
    }

    switch (m_script->command)
    {
        case SCRIPT_COMMAND_TALK:                           // 0
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u found no worldobject as source, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            Unit* unitTarget = pTarget && pTarget->isType(TYPEMASK_UNIT) ? static_cast<Unit*>(pTarget) : NULL;
            int32 textId = m_script->textId[0];

            // May have text for random
            if (m_script->textId[1])
            {
                int i = 2;
                for (; i < MAX_TEXT_ID; ++i)
                {
                    if (!m_script->textId[i])
                        { break; }
                }

                // Use one random
                textId = m_script->textId[urand(0, i - 1)];
            }

            if (!DoDisplayText(pSource, textId, unitTarget))
                { sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, could not display text %i properly", m_type, m_script->id, textId); }
            break;
        }
        case SCRIPT_COMMAND_EMOTE:                          // 1
        {
            if (LogIfNotUnit(pSource))
                { break; }

            std::vector<uint32> emotes;
            emotes.push_back(m_script->emote.emoteId);
            for (int i = 0; i < MAX_TEXT_ID; ++i)
            {
                if (!m_script->textId[i])
                    { break; }
                emotes.push_back(uint32(m_script->textId[i]));
            }

            ((Unit*)pSource)->HandleEmote(emotes[urand(0, emotes.size() - 1)]);
            break;
        }
        case SCRIPT_COMMAND_FIELD_SET:                      // 2
            if (!pSourceOrItem)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for NULL object.", m_type, m_script->id, m_script->command);
                break;
            }
            if (m_script->setField.fieldId <= OBJECT_FIELD_ENTRY || m_script->setField.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for wrong field %u (max count: %u) in %s.",
                                m_type, m_script->id, m_script->command, m_script->setField.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->SetUInt32Value(m_script->setField.fieldId, m_script->setField.fieldValue);
            break;
        case SCRIPT_COMMAND_MOVE_TO:                        // 3
        {
            if (LogIfNotUnit(pSource))
                { break; }

            // Just turn around
            if ((m_script->x == 0.0f && m_script->y == 0.0f && m_script->z == 0.0f) ||
                // Check point-to-point distance, hence revert effect of bounding radius
                ((Unit*)pSource)->IsWithinDist3d(m_script->x, m_script->y, m_script->z, 0.01f - ((Unit*)pSource)->GetObjectBoundingRadius()))
            {
                ((Unit*)pSource)->SetFacingTo(m_script->o);
                break;
            }

            // For command additional teleport the unit
            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                ((Unit*)pSource)->NearTeleportTo(m_script->x, m_script->y, m_script->z, m_script->o != 0.0f ? m_script->o : ((Unit*)pSource)->GetOrientation());
                break;
            }

            // Normal Movement
            if (m_script->moveTo.travelSpeed)
                { ((Unit*)pSource)->MonsterMoveWithSpeed(m_script->x, m_script->y, m_script->z, m_script->moveTo.travelSpeed * 0.01f); }
            else
            {
                ((Unit*)pSource)->GetMotionMaster()->Clear();
                ((Unit*)pSource)->GetMotionMaster()->MovePoint(0, m_script->x, m_script->y, m_script->z);
            }
            break;
        }
        case SCRIPT_COMMAND_FLAG_SET:                       // 4
            if (!pSourceOrItem)
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_SET (script id %u) call for NULL object.", m_script->id);
                break;
            }
            if (m_script->setFlag.fieldId <= OBJECT_FIELD_ENTRY || m_script->setFlag.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_SET (script id %u) call for wrong field %u (max count: %u) in %s.",
                                m_script->id, m_script->setFlag.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->SetFlag(m_script->setFlag.fieldId, m_script->setFlag.fieldValue);
            break;
        case SCRIPT_COMMAND_FLAG_REMOVE:                    // 5
            if (!pSourceOrItem)
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for NULL object.", m_script->id);
                break;
            }
            if (m_script->removeFlag.fieldId <= OBJECT_FIELD_ENTRY || m_script->removeFlag.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for wrong field %u (max count: %u) in %s.",
                                m_script->id, m_script->removeFlag.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->RemoveFlag(m_script->removeFlag.fieldId, m_script->removeFlag.fieldValue);
            break;
        case SCRIPT_COMMAND_TELEPORT_TO:                    // 6
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
                { break; }

            pPlayer->TeleportTo(m_script->teleportTo.mapId, m_script->x, m_script->y, m_script->z, m_script->o);
            break;
        }
        case SCRIPT_COMMAND_QUEST_EXPLORED:                 // 7
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
                { break; }

            WorldObject* pWorldObject = NULL;
            if (pSource && pSource->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
                { pWorldObject = pSource; }
            else if (pTarget && pTarget->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
                { pWorldObject = pTarget; }

            // if we have a distance, we must have a worldobject
            if (m_script->questExplored.distance != 0 && !pWorldObject)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without source worldobject, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            bool failQuest = false;
            // Creature must be alive for giving credit
            if (pWorldObject && pWorldObject->GetTypeId() == TYPEID_UNIT && !((Creature*)pWorldObject)->IsAlive())
                { failQuest = true; }
            else if (m_script->questExplored.distance != 0 && !pWorldObject->IsWithinDistInMap(pPlayer, float(m_script->questExplored.distance)))
                { failQuest = true; }

            // quest id and flags checked at script loading
            if (!failQuest)
                { pPlayer->AreaExploredOrEventHappens(m_script->questExplored.questId); }
            else
                { pPlayer->FailQuest(m_script->questExplored.questId); }

            break;
        }
        case SCRIPT_COMMAND_KILL_CREDIT:                    // 8
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
                { break; }

            uint32 creatureEntry = m_script->killCredit.creatureEntry;
            WorldObject* pRewardSource = pSource && pSource->GetTypeId() == TYPEID_UNIT ? pSource : (pTarget && pTarget->GetTypeId() == TYPEID_UNIT ? pTarget : NULL);

            // dynamic effect, take entry of reward Source
            if (!creatureEntry)
            {
                if (pRewardSource)
                    { creatureEntry =  pRewardSource->GetEntry(); }
                else
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called for dynamic killcredit without creature partner, skipping.", m_type, m_script->id, m_script->command);
                    break;
                }
            }

            if (m_script->killCredit.isGroupCredit)
            {
                WorldObject* pSearcher = pRewardSource ? pRewardSource : (pSource ? pSource : pTarget);
                if (pSearcher != pRewardSource)
                    { sLog.outDebug(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, SCRIPT_COMMAND_KILL_CREDIT called for groupCredit without creature as searcher, script might need adjustment.", m_type, m_script->id); }
                pPlayer->RewardPlayerAndGroupAtEvent(creatureEntry, pSearcher);
            }
            else
                { pPlayer->KilledMonsterCredit(creatureEntry, pRewardSource ? pRewardSource->GetObjectGuid() : ObjectGuid()); }

            break;
        }
        case SCRIPT_COMMAND_RESPAWN_GO:                     // 9
        {
            GameObject* pGo;
            if (m_script->respawnGo.goGuid)
            {
                GameObjectData const* goData = sObjectMgr.GetGOData(m_script->respawnGo.goGuid);
                if (!goData)
                    { break; }                                  // checked at load

                // TODO - This was a change, was before current map of source
                pGo = m_map->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->respawnGo.goGuid));
            }
            else
            {
                if (LogIfNotGameObject(pSource))
                    { break; }

                pGo = (GameObject*)pSource;
            }

            if (!pGo)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, m_script->respawnGo.goGuid, m_script->buddyEntry);
                break;
            }

            if (pGo->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE ||
                    pGo->GetGoType() == GAMEOBJECT_TYPE_DOOR)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u can not be used with gameobject of type %u (guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, uint32(pGo->GetGoType()), m_script->respawnGo.goGuid, m_script->buddyEntry);
                break;
            }

            if (pGo->isSpawned())
                { break; }                                      // gameobject already spawned
            
            uint32 time_to_despawn = m_script->respawnGo.despawnDelay < 5 ? 5 : m_script->respawnGo.despawnDelay;

            pGo->SetLootState(GO_READY);
            pGo->SetRespawnTime(time_to_despawn);           // despawn object in ? seconds
            pGo->Refresh();
            break;
        }
        case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:           // 10
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u found no worldobject as source, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            float x = m_script->x;
            float y = m_script->y;
            float z = m_script->z;
            float o = m_script->o;

            Creature* pCreature = pSource->SummonCreature(m_script->summonCreature.creatureEntry, x, y, z, o, m_script->summonCreature.despawnDelay ? TEMPSUMMON_TIMED_OOC_OR_DEAD_DESPAWN : TEMPSUMMON_DEAD_DESPAWN, m_script->summonCreature.despawnDelay, (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) ? true : false, m_script->textId[0] != 0);
            if (!pCreature)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for creature (entry: %u).", m_type, m_script->id, m_script->command, m_script->summonCreature.creatureEntry);
                break;
            }

            break;
        }
        case SCRIPT_COMMAND_OPEN_DOOR:                      // 11
        case SCRIPT_COMMAND_CLOSE_DOOR:                     // 12
        {
            GameObject* pDoor;
            uint32 time_to_reset = m_script->changeDoor.resetDelay < 15 ? 15 : m_script->changeDoor.resetDelay;

            if (m_script->changeDoor.goGuid)
            {
                GameObjectData const* goData = sObjectMgr.GetGOData(m_script->changeDoor.goGuid);
                if (!goData)                                // checked at load
                    { break; }

                // TODO - Was a change, before random map
                pDoor = m_map->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->changeDoor.goGuid));
            }
            else
            {
                if (LogIfNotGameObject(pSource))
                    { break; }

                pDoor = (GameObject*)pSource;
            }

            if (!pDoor)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, m_script->changeDoor.goGuid, m_script->buddyEntry);
                break;
            }

            if (pDoor->GetGoType() != GAMEOBJECT_TYPE_DOOR)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for non-door(GoType: %u).", m_type, m_script->id, m_script->command, pDoor->GetGoType());
                break;
            }

            if ((m_script->command == SCRIPT_COMMAND_OPEN_DOOR && pDoor->GetGoState() != GO_STATE_READY) ||
                (m_script->command == SCRIPT_COMMAND_CLOSE_DOOR && pDoor->GetGoState() == GO_STATE_READY))
                { break; }                                      // to be opened door already open, or to be closed door already closed

            pDoor->UseDoorOrButton(time_to_reset);

            if (pTarget && pTarget->isType(TYPEMASK_GAMEOBJECT) && ((GameObject*)pTarget)->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
                { ((GameObject*)pTarget)->UseDoorOrButton(time_to_reset); }

            break;
        }
        case SCRIPT_COMMAND_ACTIVATE_OBJECT:                // 13
        {
            if (LogIfNotUnit(pSource))
                { break; }
            if (LogIfNotGameObject(pTarget))
                { break; }

            ((GameObject*)pTarget)->Use((Unit*)pSource);
            break;
        }
        case SCRIPT_COMMAND_REMOVE_AURA:                    // 14
        {
            if (LogIfNotUnit(pSource))
                { break; }

            ((Unit*)pSource)->RemoveAurasDueToSpell(m_script->removeAura.spellId);
            break;
        }
        case SCRIPT_COMMAND_CAST_SPELL:                     // 15
        {
            if (LogIfNotUnit(pTarget))                      // TODO - Change when support for casting without victim will be supported
                { break; }

            // Select Spell
            uint32 spell = m_script->castSpell.spellId;
            uint32 filledCount = 0;
            while (filledCount < MAX_TEXT_ID && m_script->textId[filledCount])  // Count which dataint fields are filled
                ++filledCount;
            if (filledCount > 0)
                if (uint32 randomField = urand(0, filledCount))               // Random selection resulted in one of the dataint fields
                    spell = m_script->textId[randomField - 1];

            // TODO: when GO cast implemented, code below must be updated accordingly to also allow GO spell cast
            if (pSource && pSource->GetTypeId() == TYPEID_GAMEOBJECT)
            {
                ((Unit*)pTarget)->CastSpell(((Unit*)pTarget), spell, true, NULL, NULL, pSource->GetObjectGuid());
                { break; }
            }

            if (LogIfNotUnit(pSource))
                { break; }
            ((Unit*)pSource)->CastSpell(((Unit*)pTarget), spell, (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) != 0);

            break;
        }
        case SCRIPT_COMMAND_PLAY_SOUND:                     // 16
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u could not find proper source", m_type, m_script->id, m_script->command);
                break;
            }

            // bitmask: 0/1=target-player, 0/2=with distance dependent, 0/4=map wide, 0/8=zone wide
            Player* pSoundTarget = NULL;
            if (m_script->playSound.flags & 1)
            {
                pSoundTarget = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
                if (!pSoundTarget)
                    { break; }
            }

            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                pSource->PlayMusic(m_script->playSound.soundId, pSoundTarget);
            else
            {
                if (m_script->playSound.flags & 2)
                    { pSource->PlayDistanceSound(m_script->playSound.soundId, pSoundTarget); }
                else if (m_script->playSound.flags & (4 | 8))
                    { m_map->PlayDirectSoundToMap(m_script->playSound.soundId, (m_script->playSound.flags & 8) ? pSource->GetZoneId() : 0); }
                else
                    { pSource->PlayDirectSound(m_script->playSound.soundId, pSoundTarget); }
            }
            break;
        }
        case SCRIPT_COMMAND_CREATE_ITEM:                    // 17
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
                { break; }

            if (Item* pItem = pPlayer->StoreNewItemInInventorySlot(m_script->createItem.itemEntry, m_script->createItem.amount))
                { pPlayer->SendNewItem(pItem, m_script->createItem.amount, true, false); }

            break;
        }
        case SCRIPT_COMMAND_DESPAWN_SELF:                   // 18
        {
            // TODO - Remove this check after a while
            if (pTarget && pTarget->GetTypeId() != TYPEID_UNIT && pSource && pSource->GetTypeId() == TYPEID_UNIT)
            {
                sLog.outErrorDb("DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u target must be creature, but (only) source is, use data_flags to fix", m_type, m_script->id, m_script->command);
                pTarget = pSource;
            }

            if (LogIfNotCreature(pTarget))
                { break; }

            ((Creature*)pTarget)->ForcedDespawn(m_script->despawn.despawnDelay);

            break;
        }
        case SCRIPT_COMMAND_PLAY_MOVIE:                     // 19
        {
            break;                                      // must be skipped at loading
        }
        case SCRIPT_COMMAND_MOVEMENT:                       // 20
        {
            if (LogIfNotCreature(pSource))
                { break; }

            // Consider add additional checks for cases where creature should not change movementType
            // (pet? in combat? already using same MMgen as script try to apply?)

            switch (m_script->movement.movementType)
            {
                case IDLE_MOTION_TYPE:
                    ((Creature*)pSource)->GetMotionMaster()->MoveIdle();
                    break;
                case RANDOM_MOTION_TYPE:
                    if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                        { ((Creature*)pSource)->GetMotionMaster()->MoveRandomAroundPoint(pSource->GetPositionX(), pSource->GetPositionY(), pSource->GetPositionZ(), float(m_script->movement.wanderDistance)); }
                    else
                    {
                        float respX, respY, respZ, respO, wander_distance;
                        ((Creature*)pSource)->GetRespawnCoord(respX, respY, respZ, &respO, &wander_distance);
                        wander_distance = m_script->movement.wanderDistance ? m_script->movement.wanderDistance : wander_distance;
                        ((Creature*)pSource)->GetMotionMaster()->MoveRandomAroundPoint(respX, respY, respZ, wander_distance);
                    }
                    break;
                case WAYPOINT_MOTION_TYPE:
                    ((Creature*)pSource)->GetMotionMaster()->MoveWaypoint();
                    break;
            }

            break;
        }
        case SCRIPT_COMMAND_SET_ACTIVEOBJECT:               // 21
        {
            if (LogIfNotCreature(pSource))
                { break; }

            ((Creature*)pSource)->SetActiveObjectState(m_script->activeObject.activate);
            break;
        }
        case SCRIPT_COMMAND_SET_FACTION:                    // 22
        {
            if (LogIfNotCreature(pSource))
                { break; }

            if (m_script->faction.factionId)
                { ((Creature*)pSource)->SetFactionTemporary(m_script->faction.factionId, m_script->faction.flags); }
            else
                { ((Creature*)pSource)->ClearTemporaryFaction(); }

            break;
        }
        case SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL:        // 23
        {
            if (LogIfNotCreature(pSource))
                { break; }

            if (!m_script->morph.creatureOrModelEntry)
                { ((Creature*)pSource)->DeMorph(); }
            else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                { ((Creature*)pSource)->SetDisplayId(m_script->morph.creatureOrModelEntry); }
            else
            {
                CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_script->morph.creatureOrModelEntry);
                uint32 display_id = Creature::ChooseDisplayId(ci);

                ((Creature*)pSource)->SetDisplayId(display_id);
            }

            break;
        }
        case SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL:        // 24
        {
            if (LogIfNotCreature(pSource))
                { break; }

            if (!m_script->mount.creatureOrModelEntry)
                { ((Creature*)pSource)->Unmount(); }
            else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                { ((Creature*)pSource)->Mount(m_script->mount.creatureOrModelEntry); }
            else
            {
                CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_script->mount.creatureOrModelEntry);
                uint32 display_id = Creature::ChooseDisplayId(ci);

                ((Creature*)pSource)->Mount(display_id);
            }

            break;
        }
        case SCRIPT_COMMAND_SET_RUN:                        // 25
        {
            if (LogIfNotCreature(pSource))
                { break; }

            ((Creature*)pSource)->SetWalk(!m_script->run.run, true);

            break;
        }
        case SCRIPT_COMMAND_ATTACK_START:                   // 26
        {
            if (LogIfNotCreature(pSource))
                { break; }
            if (LogIfNotUnit(pTarget))
                { break; }

            Creature* pAttacker = static_cast<Creature*>(pSource);
            Unit* unitTarget = static_cast<Unit*>(pTarget);

            if (pAttacker->IsFriendlyTo(unitTarget))
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u attacker is friendly to target, can not attack (Attacker: %s, Target: %s)", m_type, m_script->id, m_script->command, pAttacker->GetGuidStr().c_str(), unitTarget->GetGuidStr().c_str());
                break;
            }

            pAttacker->AI()->AttackStart(unitTarget);

            break;
        }
        case SCRIPT_COMMAND_GO_LOCK_STATE:                  // 27
        {
            if (LogIfNotGameObject(pSource))
                { break; }

            GameObject* pGo = static_cast<GameObject*>(pSource);

            /* flag lockState
             * go_lock          0x01
             * go_unlock        0x02
             * go_nonInteract   0x04
             * go_Interact      0x08
             */

            // Lock or Unlock
            if (m_script->goLockState.lockState & 0x01)
                { pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED); }
            else if (m_script->goLockState.lockState & 0x02)
                { pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED); }
            // Set Non Interactable or Set Interactable
            if (m_script->goLockState.lockState & 0x04)
                { pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT); }
            else if (m_script->goLockState.lockState & 0x08)
                { pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT); }

            break;
        }
        case SCRIPT_COMMAND_STAND_STATE:                    // 28
        {
            if (LogIfNotCreature(pSource))
                { break; }

            // Must be safe cast to Unit* here
            ((Unit*)pSource)->SetStandState(m_script->standState.stand_state);
            break;
        }
        case SCRIPT_COMMAND_MODIFY_NPC_FLAGS:               // 29
        {
            if (LogIfNotCreature(pSource))
                { break; }

            // Add Flags
            if (m_script->npcFlag.change_flag & 0x01)
                { pSource->SetFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag); }
            // Remove Flags
            else if (m_script->npcFlag.change_flag & 0x02)
                { pSource->RemoveFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag); }
            // Toggle Flags
            else
            {
                if (pSource->HasFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag))
                    { pSource->RemoveFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag); }
                else
                    { pSource->SetFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag); }
            }

            break;
        }
        case SCRIPT_COMMAND_SEND_TAXI_PATH:                 // 30
        {
            // only Player
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
                { break; }

            pPlayer->ActivateTaxiPathTo(m_script->sendTaxiPath.taxiPathId);
            break;
        }
        case SCRIPT_COMMAND_TERMINATE_SCRIPT:               // 31
        {
            bool result = false;
            if (m_script->terminateScript.npcEntry)
            {
                WorldObject* pSearcher = pSource ? pSource : pTarget;
                if (pSearcher->GetTypeId() == TYPEID_PLAYER && pTarget && pTarget->GetTypeId() != TYPEID_PLAYER)
                    { pSearcher = pTarget; }

                Creature* pCreatureBuddy = NULL;
                MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*pSearcher, m_script->terminateScript.npcEntry, true, false, m_script->terminateScript.searchDist, true);
                MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pCreatureBuddy, u_check);
                Cell::VisitGridObjects(pSearcher, searcher, m_script->terminateScript.searchDist);

                if (!(m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) && !pCreatureBuddy)
                {
                    DEBUG_LOG("DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, terminate further steps of this script! (as searched other npc %u was not found alive)", m_type, m_script->id, m_script->terminateScript.npcEntry);
                    result = true;
                }
                else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL && pCreatureBuddy)
                {
                    DEBUG_LOG("DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, terminate further steps of this script! (as searched other npc %u was found alive)", m_type, m_script->id, m_script->terminateScript.npcEntry);
                    result = true;
                }
            }
            else
                { result = true; }

            if (result)                                    // Terminate further steps of this script
            {
                if (m_script->textId[0] && !LogIfNotCreature(pSource))
                {
                    Creature* cSource = static_cast<Creature*>(pSource);
                    if (cSource->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
                        { (static_cast<WaypointMovementGenerator<Creature>* >(cSource->GetMotionMaster()->top()))->AddToWaypointPauseTime(m_script->textId[0]); }
                }

                return true;
            }

            break;
        }
        case SCRIPT_COMMAND_PAUSE_WAYPOINTS:                // 32
        {
            if (LogIfNotCreature(pSource))
                { return false; }
            if (m_script->pauseWaypoint.doPause)
                { ((Creature*)pSource)->addUnitState(UNIT_STAT_WAYPOINT_PAUSED); }
            else
                { ((Creature*)pSource)->clearUnitState(UNIT_STAT_WAYPOINT_PAUSED); }
            break;
        }
        case SCRIPT_COMMAND_JOIN_LFG:                       // 33
        {
            //Not supported
            break;
        }
        case SCRIPT_COMMAND_TERMINATE_COND:                 // 34
        {
            Player* player = NULL;
            WorldObject* second = pSource;
            // First case: target is player
            if (pTarget && pTarget->GetTypeId() == TYPEID_PLAYER)
                { player = static_cast<Player*>(pTarget); }
            // Second case: source is player
            else if (pSource && pSource->GetTypeId() == TYPEID_PLAYER)
            {
                player = static_cast<Player*>(pSource);
                second = pTarget;
            }

            bool terminateResult;
            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                { terminateResult = !sObjectMgr.IsPlayerMeetToCondition(m_script->terminateCond.conditionId, player, m_map, second, CONDITION_FROM_DBSCRIPTS); }
            else
                { terminateResult = sObjectMgr.IsPlayerMeetToCondition(m_script->terminateCond.conditionId, player, m_map, second, CONDITION_FROM_DBSCRIPTS); }

            if (terminateResult && m_script->terminateCond.failQuest && player)
            {
                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember(); groupRef != NULL; groupRef = groupRef->next())
                    {
                        Player* member = groupRef->getSource();
                        if (member->GetQuestStatus(m_script->terminateCond.failQuest) == QUEST_STATUS_INCOMPLETE)
                            { member->FailQuest(m_script->terminateCond.failQuest); }
                    }
                }
                else
                {
                    if (player->GetQuestStatus(m_script->terminateCond.failQuest) == QUEST_STATUS_INCOMPLETE)
                        { player->FailQuest(m_script->terminateCond.failQuest); }
                }
            }
            return terminateResult;
        }
        case SCRIPT_COMMAND_SEND_AI_EVENT_AROUND:           // 35
        {
            if (LogIfNotCreature(pSource))
                return false;
            if (LogIfNotUnit(pTarget))
                break;

            ((Creature*)pSource)->AI()->SendAIEventAround(AIEventType(m_script->sendAIEvent.eventType), (Unit*)pTarget, 0, float(m_script->sendAIEvent.radius));
            break;
        }
        case SCRIPT_COMMAND_TURN_TO:                        // 36
        {
            if (LogIfNotUnit(pSource))
                { break; }
            //note for self: this command has different impl. and usage in other core(s)
            ((Unit*)pSource)->SetFacingTo(pSource->GetAngle(pTarget));
            break;
        }
        case SCRIPT_COMMAND_MOVE_DYNAMIC:                   // 37
        {
            if (LogIfNotCreature(pSource))
                return false;
            if (LogIfNotUnit(pTarget))
                return false;

            float x, y, z;
            if (m_script->moveDynamic.maxDist == 0)         // Move to pTarget
            {
                if (pTarget == pSource)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, _MOVE_DYNAMIC called with maxDist == 0, but resultingSource == resultingTarget (== %s)", m_type, m_script->id, pSource->GetGuidStr().c_str());
                    break;
                }
                pTarget->GetContactPoint(pSource, x, y, z);
            }
            else                                            // Calculate position
            {
                float orientation;
                if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                    orientation = pSource->GetOrientation() + m_script->o + 2 * M_PI_F;
                else
                    orientation = m_script->o;

                pSource->GetRandomPoint(pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ(), m_script->moveDynamic.maxDist, x, y, z,
                                        m_script->moveDynamic.minDist, (orientation == 0.0f ? NULL : &orientation));
                z = std::max(z, pTarget->GetPositionZ());
                pSource->UpdateAllowedPositionZ(x, y, z);
            }
            ((Creature*)pSource)->GetMotionMaster()->MovePoint(1, x, y, z);
            break;
        }
        case SCRIPT_COMMAND_SEND_MAIL:                      // 38
        {
            if (LogIfNotPlayer(pTarget))
                return false;
            if (!m_script->sendMail.altSender && LogIfNotCreature(pSource))
                return false;

            MailSender sender;
            if (m_script->sendMail.altSender)
                sender = MailSender(MAIL_CREATURE, m_script->sendMail.altSender);
            else
                sender = MailSender(pSource);
            uint32 deliverDelay = m_script->textId[0] > 0 ? (uint32)m_script->textId[0] : 0;

            MailDraft(m_script->sendMail.mailTemplateId).SendMailTo(static_cast<Player*>(pTarget), sender, MAIL_CHECK_MASK_HAS_BODY, deliverDelay);
            break;
        }
        case SCRIPT_COMMAND_SET_FLY:                        // 39
        {
            if (LogIfNotCreature(pSource))
                { break; }
            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                if (m_script->fly.enable)
                    { pSource->SetByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND); }
                else
                    { pSource->RemoveByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND); }
            }

            ((Creature*)pSource)->SetLevitate((m_script->fly.enable == 0) ? false : true);
            break;
        }
        case SCRIPT_COMMAND_DESPAWN_GO:                     // 40
        {
            if (LogIfNotGameObject(pTarget))
                { break; }
            ((GameObject*)pTarget)->SetLootState(GO_JUST_DEACTIVATED);
            break;
        }
        case SCRIPT_COMMAND_RESPAWN:                        // 41
        {
            if (LogIfNotCreature(pTarget))
                { break; }
            ((Creature*)pTarget)->Respawn();
            break;
        }
        case SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS:            // 42
        {
            if (LogIfNotCreature(pSource))
                { break; }

            Creature* pCSource = static_cast<Creature*>(pSource);

            // reset default
            if (m_script->setEquipment.resetDefault)
            {
                pCSource->LoadEquipment(pCSource->GetCreatureInfo()->EquipmentTemplateId, true);
                break;
            }

            // main hand
            if (m_script->textId[0] >= 0)
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, m_script->textId[0]);

            // off hand
            if (m_script->textId[1] >= 0)
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, m_script->textId[1]);

            // ranged
            if (m_script->textId[2] >= 0)
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, m_script->textId[2]);
            break;
        }
        case SCRIPT_COMMAND_RESET_GO:                       // 43
        {
            if (LogIfNotGameObject(pTarget))
                { break; }

            GameObject* pGoTarget = static_cast<GameObject*>(pTarget);

            switch (pGoTarget->GetGoType())
            {
                case GAMEOBJECT_TYPE_DOOR:
                case GAMEOBJECT_TYPE_BUTTON:
                    pGoTarget->ResetDoorOrButton();
                    break;
                default:
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed: GO(buddyEntry) %u is not a door or button", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                    break;
            }
            break;
        }
        case SCRIPT_COMMAND_UPDATE_TEMPLATE:                // 44
        {
            if (LogIfNotCreature(pSource))
                { break; }

            Creature* pCre = static_cast<Creature*>(pSource);

            if (pCre->GetEntry() != m_script->updateTemplate.entry)
                { pCre->UpdateEntry(m_script->updateTemplate.entry, m_script->updateTemplate.faction ? HORDE : ALLIANCE); }
            else
                { sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u: source creature already has the specified creature entry %u", m_type, m_script->id, m_script->command, m_script->updateTemplate.entry); }
            break;
        }
        default:
            sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u unknown command used.", m_type, m_script->id, m_script->command);
            break;
    }

    return false;
}

// /////////////////////////////////////////////////////////
//              Scripting Library Hooks
// /////////////////////////////////////////////////////////
void ScriptMgr::LoadScriptBinding()
{
#ifdef ENABLE_SD3
    for (int i = 0; i < SCRIPTED_MAX_TYPE; ++i)
        m_scriptBind[i].clear();

    QueryResult* result = WorldDatabase.PQuery("SELECT type, bind, ScriptName, data FROM script_binding");
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
                exists = mapEntry->IsBattleGround();
            break;
        case SCRIPTED_INSTANCE:
            if (MapEntry const* mapEntry = sMapStore.LookupEntry(uint32(id)))
                exists = mapEntry->IsDungeon();
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
            id |= uint32(data) << 24;   //incorporate spell effect number into the key

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

bool ScriptMgr::ReloadScriptBinding()
{
#ifdef _DEBUG
    m_bindMutex.acquire_write();
    LoadScriptBinding();
    m_bindMutex.release();
    return true;
#else
    return false;
#endif /* _DEBUG */
}

void ScriptMgr::LoadScriptNames()
{
    m_scriptNames.push_back("");
    QueryResult* result = WorldDatabase.Query("SELECT DISTINCT(ScriptName) FROM script_binding");

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

uint32 ScriptMgr::GetScriptId(const char* name) const
{
    // use binary search to find the script name in the sorted vector
    // assume "" is the first element
    if (!name)
        { return 0; }

    ScriptNameMap::const_iterator itr =
        std::lower_bound(m_scriptNames.begin(), m_scriptNames.end(), name);

    if (itr == m_scriptNames.end() || *itr != name)
        { return 0; }

    return uint32(itr - m_scriptNames.begin());
}

uint32 ScriptMgr::GetBoundScriptId(ScriptedObjectType entity, int32 entry)
{
#ifdef _DEBUG
    m_bindMutex.acquire_read();
#endif /* _DEBUG */
    uint32 id = 0;
    if (entity < SCRIPTED_MAX_TYPE)
    {
        EntryToScriptIdMap::iterator it = m_scriptBind[entity].find(entry);
        if (it != m_scriptBind[entity].end())
            id = it->second;
    }
    else
        sLog.outErrorScriptLib("asking a script for non-existing entity type %u!", entity);
#ifdef _DEBUG
    m_bindMutex.release();
#endif /* _DEBUG */
    return id;
}

char const* ScriptMgr::GetScriptLibraryVersion() const
{
#ifdef ENABLE_SD3
    return SD3::GetScriptLibraryVersion();
#else
    return NULL;
#endif
}

CreatureAI* ScriptMgr::GetCreatureAI(Creature* pCreature)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (CreatureAI* luaAI = sEluna->GetAI(pCreature))
        return luaAI;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GetCreatureAI(pCreature);
#else
    return NULL;
#endif
}

InstanceData* ScriptMgr::CreateInstanceData(Map* pMap)
{
#ifdef ENABLE_SD3
    return SD3::CreateInstanceData(pMap);
#else
    return NULL;
#endif
}

bool ScriptMgr::OnGossipHello(Player* pPlayer, Creature* pCreature)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnGossipHello(pPlayer, pCreature))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GossipHello(pPlayer, pCreature);
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipHello(Player* pPlayer, GameObject* pGameObject)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnGossipHello(pPlayer, pGameObject))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOGossipHello(pPlayer, pGameObject);
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 sender, uint32 action, const char* code)
{
#ifdef ENABLE_ELUNA
    if (code)
    {
        // Used by Eluna
        if (sEluna->OnGossipSelectCode(pPlayer, pCreature, sender, action, code))
            return true;
    }
    else
    {
        // Used by Eluna
        if (sEluna->OnGossipSelect(pPlayer, pCreature, sender, action))
            return true;
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    if (code)
        { return SD3::GossipSelectWithCode(pPlayer, pCreature, sender, action, code); }
    else
        { return SD3::GossipSelect(pPlayer, pCreature, sender, action); }
#else
    return false;
#endif
}

bool ScriptMgr::OnGossipSelect(Player* pPlayer, GameObject* pGameObject, uint32 sender, uint32 action, const char* code)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (code)
    {
        if (sEluna->OnGossipSelectCode(pPlayer, pGameObject, sender, action, code))
            return true;
    }
    else
    {
        if (sEluna->OnGossipSelect(pPlayer, pGameObject, sender, action))
            return true;
    }
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    if (code)
        { return SD3::GOGossipSelectWithCode(pPlayer, pGameObject, sender, action, code); }
    else
        { return SD3::GOGossipSelect(pPlayer, pGameObject, sender, action); }
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnQuestAccept(pPlayer, pCreature, pQuest))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::QuestAccept(pPlayer, pCreature, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestAccept(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnQuestAccept(pPlayer, pGameObject, pQuest))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOQuestAccept(pPlayer, pGameObject, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnQuestAccept(pPlayer, pItem, pQuest))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::ItemQuestAccept(pPlayer, pItem, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest, uint32 reward)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnQuestReward(pPlayer, pCreature, pQuest, reward))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::QuestRewarded(pPlayer, pCreature, pQuest);
#else
    return false;
#endif
}

bool ScriptMgr::OnQuestRewarded(Player* pPlayer, GameObject* pGameObject, Quest const* pQuest, uint32 reward)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnQuestReward(pPlayer, pGameObject, pQuest, reward))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOQuestRewarded(pPlayer, pGameObject, pQuest);
#else
    return false;
#endif
}

uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, Creature* pCreature)
{
#ifdef ENABLE_ELUNA
    sEluna->GetDialogStatus(pPlayer, pCreature);
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GetNPCDialogStatus(pPlayer, pCreature);
#else
    return DIALOG_STATUS_UNDEFINED;
#endif
}

uint32 ScriptMgr::GetDialogStatus(Player* pPlayer, GameObject* pGameObject)
{
#ifdef ENABLE_ELUNA
    sEluna->GetDialogStatus(pPlayer, pGameObject);
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GetGODialogStatus(pPlayer, pGameObject);
#else
    return DIALOG_STATUS_UNDEFINED;
#endif
}

bool ScriptMgr::OnGameObjectUse(Player* pPlayer, GameObject* pGameObject)
{
#ifdef ENABLE_ELUNA
    if (sEluna->OnGameObjectUse(pPlayer, pGameObject))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::GOUse(pPlayer, pGameObject);
#else
    return false;
#endif
}

bool ScriptMgr::OnItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (!sEluna->OnUse(pPlayer, pItem, targets))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::ItemUse(pPlayer, pItem, targets);
#else
    return false;
#endif
}

bool ScriptMgr::OnAreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnAreaTrigger(pPlayer, atEntry))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::AreaTrigger(pPlayer, atEntry);
#else
    return false;
#endif
}

bool ScriptMgr::OnProcessEvent(uint32 eventId, Object* pSource, Object* pTarget, bool isStart)
{
#ifdef ENABLE_SD3
    return SD3::ProcessEvent(eventId, pSource, pTarget, isStart);
#else
    return false;
#endif
}

bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (pTarget->ToCreature())
        if (sEluna->OnDummyEffect(pCaster, spellId, effIndex, pTarget->ToCreature()))
            return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::EffectDummyUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}
  
bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnDummyEffect(pCaster, spellId, effIndex, pTarget))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::EffectDummyGameObject(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}
  
bool ScriptMgr::OnEffectDummy(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (sEluna->OnDummyEffect(pCaster, spellId, effIndex, pTarget))
        return true;
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_SD3
    return SD3::EffectDummyItem(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

bool ScriptMgr::OnEffectScriptEffect(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Unit* pTarget, ObjectGuid originalCasterGuid)
{
#ifdef ENABLE_SD3
    return SD3::EffectScriptEffectUnit(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
#else
    return false;
#endif
}

bool ScriptMgr::OnAuraDummy(Aura const* pAura, bool apply)
{
#ifdef ENABLE_SD3
    return SD3::AuraDummy(pAura, apply);
#else
    return false;
#endif
}

ScriptLoadResult ScriptMgr::LoadScriptLibrary(const char* libName)
{
#ifdef ENABLE_SD3
    if (std::strcmp(libName, MANGOS_SCRIPT_NAME) == 0)
    {
        SD3::FreeScriptLibrary();
        SD3::InitScriptLibrary();
        return SCRIPT_LOAD_OK;
    }
#endif

    return SCRIPT_LOAD_ERR_NOT_FOUND;
}

void ScriptMgr::UnloadScriptLibrary()
{
#ifdef ENABLE_SD3
    SD3::FreeScriptLibrary();
#else
    return;
#endif
}

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
                if (spell->Effect[j] == SPELL_EFFECT_SEND_EVENT)
                {
                    if (spell->EffectMiscValue[j])
                        { eventIds.insert(spell->EffectMiscValue[j]); }
                }
            }
        }
    }
#if defined(TBC) || defined (WOTLK)
    // Load all possible event entries from taxi path nodes
    for (size_t path_idx = 0; path_idx < sTaxiPathNodesByPath.size(); ++path_idx)
    {
        for (size_t node_idx = 0; node_idx < sTaxiPathNodesByPath[path_idx].size(); ++node_idx)
        {
            TaxiPathNodeEntry const& node = sTaxiPathNodesByPath[path_idx][node_idx];

            if (node.arrivalEventID)
                eventIds.insert(node.arrivalEventID);

            if (node.departureEventID)
                eventIds.insert(node.departureEventID);
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
        { return true; }

    // Handle PvP Calls
    if (forwardToPvp && source->GetTypeId() == TYPEID_GAMEOBJECT)
    {
        BattleGround* bg = NULL;
        OutdoorPvP* opvp = NULL;
        if (forwardToPvp->GetTypeId() == TYPEID_PLAYER)
        {
            bg = ((Player*)forwardToPvp)->GetBattleGround();
            if (!bg)
                { opvp = sOutdoorPvPMgr.GetScript(((Player*)forwardToPvp)->GetCachedZoneId()); }
        }
        else
        {
#if defined(CLASSIC)
            if (map->IsBattleGround())
#else
            if (map->IsBattleGroundOrArena())
#endif
                { bg = ((BattleGroundMap*)map)->GetBG(); }
            else                                            // Use the go, because GOs don't move
                { opvp = sOutdoorPvPMgr.GetScript(((GameObject*)source)->GetZoneId()); }
        }

        if (bg && bg->HandleEvent(id, static_cast<GameObject*>(source)))
            { return true; }

        if (opvp && opvp->HandleEvent(id, static_cast<GameObject*>(source)))
            { return true; }
    }

    Map::ScriptExecutionParam execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE_TARGET;
    if (source->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
        { execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_SOURCE; }
    else if (target && target->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
        { execParam = Map::SCRIPT_EXEC_PARAM_UNIQUE_BY_TARGET; }

    return map->ScriptsStart(DBS_ON_EVENT, id, source, target, execParam);
}

uint32 GetScriptId(const char* name)
{
    return sScriptMgr.GetScriptId(name);
}

char const* GetScriptName(uint32 id)
{
    return sScriptMgr.GetScriptName(id);
}

uint32 GetScriptIdsCount()
{
    return sScriptMgr.GetScriptIdsCount();
}

void SetExternalWaypointTable(char const* tableName)
{
    sWaypointMgr.SetExternalWPTable(tableName);
}

bool AddWaypointFromExternal(uint32 entry, int32 pathId, uint32 pointId, float x, float y, float z, float o, uint32 waittime)
{
    return sWaypointMgr.AddExternalNode(entry, pathId, pointId, x, y, z, o, waittime);
}
