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
 * @file BattleGroundMgr.cpp
 * @brief Implementation of the battleground manager and queue system.
 *
 * This file contains the implementation of the BattleGroundMgr singleton class and
 * the BattleGroundQueue class, which handle:
 * - Battleground instance creation and management
 * - Player queue management and matching
 * - Team balancing for battleground invitations
 * - Average wait time calculations
 * - Bracket-based queue organization
 * - Premade group matching
 */



#include "BattleGroundMgr.h"
#include "Common.h"
#include "SharedDefines.h"
#include "Player.h"
#include "BattleGroundAV.h"
#include "BattleGroundAB.h"
#include "BattleGroundWS.h"
#include "MapManager.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Chat.h"
#include "World.h"
#include "WorldPacket.h"
#include "GameEventMgr.h"
#include "Formulas.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "Policies/Singleton.h"
#include "Language.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Converts a battleground type ID to a queue type ID.
 *
 * Maps a battleground type ID to its corresponding queue type ID. Different queue types
 * have separate queues in the matchmaking system.
 *
 * @param bgTypeId The battleground type ID.
 * @return The corresponding queue type ID, or BATTLEGROUND_QUEUE_NONE if invalid.
 */
BattleGroundQueueTypeId BattleGroundMgr::BGQueueTypeId(BattleGroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_WS:
            return BATTLEGROUND_QUEUE_WS;
        case BATTLEGROUND_AB:
            return BATTLEGROUND_QUEUE_AB;
        case BATTLEGROUND_AV:
            return BATTLEGROUND_QUEUE_AV;
        default:
            return BATTLEGROUND_QUEUE_NONE;
    }
}

/**
 * @brief Converts a battleground queue type to its template battleground type.
 *
 * Maps queue identifiers back to the battleground template type used to create
 * or reference battleground instances.
 *
 * @param bgQueueTypeId The battleground queue type identifier.
 * @return The corresponding battleground type identifier.
 */
BattleGroundTypeId BattleGroundMgr::BGTemplateId(BattleGroundQueueTypeId bgQueueTypeId)
{
    switch (bgQueueTypeId)
    {
        case BATTLEGROUND_QUEUE_WS:
            return BATTLEGROUND_WS;
        case BATTLEGROUND_QUEUE_AB:
            return BATTLEGROUND_AB;
        case BATTLEGROUND_QUEUE_AV:
            return BATTLEGROUND_AV;
        default:
            return BattleGroundTypeId(0);                   // used for unknown template (it exist and do nothing)
    }
}

/**
 * @brief Toggles battleground debug testing mode.
 *
 * Enables or disables testing mode and broadcasts the status change to the world.
 */
void BattleGroundMgr::ToggleTesting()
{
    m_Testing = !m_Testing;
    if (m_Testing)
    {
        sWorld.SendWorldText(LANG_DEBUG_BG_ON);
    }
    else
    {
        sWorld.SendWorldText(LANG_DEBUG_BG_OFF);
    }
}

/**
 * @brief Schedules a queue update for a specific battleground queue.
 *
 * Adds a queue update to the scheduler so that the next world update cycle will
 * process matchmaking and invitations for this queue. Multiple requests for the same
 * queue are consolidated to avoid duplicate processing.
 *
 * @param bgQueueTypeId The battleground queue type to update.
 * @param bgTypeId The battleground type.
 * @param bracket_id The bracket to update.
 */
void BattleGroundMgr::ScheduleQueueUpdate(BattleGroundQueueTypeId bgQueueTypeId, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{
    // combine bgQueueTypeId, bgTypeId and bracket_id into a single schedule id
    uint32 schedule_id = (bgQueueTypeId << 16) | (bgTypeId << 8) | bracket_id;
    bool found = false;
    for (uint8 i = 0; i < m_QueueUpdateScheduler.size(); ++i)
    {
        if (m_QueueUpdateScheduler[i] == schedule_id)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        m_QueueUpdateScheduler.push_back(schedule_id);
    }
}

/**
 * @brief Gets the premature finish timer duration.
 *
 * Returns the configured duration in milliseconds after which a battleground can be
 * finished prematurely if one team is significantly outnumbered or defeated.
 *
 * @return The premature finish timer duration in milliseconds.
 */
uint32 BattleGroundMgr::GetPrematureFinishTime() const
{
    return sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER);
}

/**
 * @brief Loads battle master creature entries from the database.
 *
 * Populates the battle master map from the `battlemaster_entry` database table,
 * which maps creature entries to their respective battleground types.
 */
void BattleGroundMgr::LoadBattleMastersEntry()
{
    mBattleMastersMap.clear();                              // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`bg_template` FROM `battlemaster_entry`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 battlemaster entries - table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 bgTypeId  = fields[1].GetUInt32();
        if (bgTypeId >= MAX_BATTLEGROUND_TYPE_ID)
        {
            sLog.outErrorDb("Table `battlemaster_entry` contain entry %u for nonexistent battleground type %u, ignored.", entry, bgTypeId);
            continue;
        }

        mBattleMastersMap[entry] = BattleGroundTypeId(bgTypeId);
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u battlemaster entries", count);
    sLog.outString();
}

/**
 * @brief Converts a battleground type to its weekend holiday ID.
 *
 * Maps battleground types to their associated "Call to Arms" weekend holiday events that
 * provide bonus rewards for participating in that battleground type.
 *
 * @param bgTypeId The battleground type to convert.
 * @return The corresponding holiday ID, or HOLIDAY_NONE if not a recognized type.
 */
HolidayIds BattleGroundMgr::BGTypeToWeekendHolidayId(BattleGroundTypeId bgTypeId)
{
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV: return HOLIDAY_CALL_TO_ARMS_AV;
        case BATTLEGROUND_WS: return HOLIDAY_CALL_TO_ARMS_WS;
        case BATTLEGROUND_AB: return HOLIDAY_CALL_TO_ARMS_AB;
        default: return HOLIDAY_NONE;
    }
}

/**
 * @brief Converts a battleground type to its weekend holiday ID.
 *
 * Maps battleground types to their associated "Call to Arms" weekend holiday events.
 *
 * @param holiday The holiday ID to convert.
 * @return The corresponding battleground type, or BATTLEGROUND_TYPE_NONE if invalid.
 */
BattleGroundTypeId BattleGroundMgr::WeekendHolidayIdToBGType(HolidayIds holiday)
{
    switch (holiday)
    {
        case HOLIDAY_CALL_TO_ARMS_AV: return BATTLEGROUND_AV;
        case HOLIDAY_CALL_TO_ARMS_WS: return BATTLEGROUND_WS;
        case HOLIDAY_CALL_TO_ARMS_AB: return BATTLEGROUND_AB;
        default: return BATTLEGROUND_TYPE_NONE;
    }
}

/**
 * @brief Checks if a battleground type is active for the weekend.
 *
 * Determines whether the specified battleground type has an active "Call to Arms"
 * weekend event that provides bonus experience and reputation.
 *
 * @param bgTypeId The battleground type to check.
 * @return true if the battleground is currently featured for the weekend, false otherwise.
 */
bool BattleGroundMgr::IsBGWeekend(BattleGroundTypeId bgTypeId)
{
    return sGameEventMgr.IsActiveHoliday(BGTypeToWeekendHolidayId(bgTypeId));
}

/**
 * @brief Loads battleground event indexes from the database.
 *
 * Populates the game object and creature event index maps from the database,
 * associating spawned objects and creatures with their battleground events.
 * This enables proper spawning and despawning of objectives during battles.
 */
void BattleGroundMgr::LoadBattleEventIndexes()
{
    BattleGroundEventIdx events;
    events.event1 = BG_EVENT_NONE;
    events.event2 = BG_EVENT_NONE;
    m_GameObjectBattleEventIndexMap.clear();             // need for reload case
    m_GameObjectBattleEventIndexMap[-1] = events;
    m_CreatureBattleEventIndexMap.clear();               // need for reload case
    m_CreatureBattleEventIndexMap[-1] = events;

    uint32 count = 0;

    QueryResult* result =
    //                                      0             1               2                      3                        4              5                      6
        WorldDatabase.Query("SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1` AS `ev1`, `data`.`ev2` AS ev2, `data`.`map` AS m, `data`.`guid2`, `description`.`map`, "
    //                  7                       8                       9
        "`description`.`event1`, `description`.`event2`, `description`.`description` "
        "FROM "
        "(SELECT '1' AS typ, `a`.`guid` AS `guid1`, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
        "FROM `gameobject_battleground` AS a "
        "LEFT OUTER JOIN `gameobject` AS b ON `a`.`guid` = `b`.`guid` "
        "UNION "
        "SELECT '2' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
        "FROM `creature_battleground` AS a "
        "LEFT OUTER JOIN `creature` AS b ON `a`.`guid` = `b`.`guid` "
        ") data "
        "RIGHT OUTER JOIN `battleground_events` AS `description` ON `data`.`map` = `description`.`map` "
        "AND `data`.`ev1` = `description`.`event1` AND `data`.`ev2` = `description`.`event2` "
    //  full outer join doesn't work in mysql :-/ so just UNION-select the same again and add a left outer join
        "UNION "
        "SELECT `data`.`typ`, `data`.`guid1`, `data`.`ev1`, `data`.`ev2`, `data`.`map`, `data`.`guid2`, `description`.`map`, "
        "`description`.`event1`, `description`.`event2`, `description`.`description` "
        "FROM "
        "(SELECT '1' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
        "FROM `gameobject_battleground` AS a "
        "LEFT OUTER JOIN `gameobject` AS b ON `a`.`guid` = `b`.`guid` "
        "UNION "
        "SELECT '2' AS typ, `a`.`guid` AS guid1, `a`.`event1` AS ev1, `a`.`event2` AS ev2, `b`.`map` AS map, `b`.`guid` AS guid2 "
        "FROM `creature_battleground` AS a "
        "LEFT OUTER JOIN `creature` AS b ON `a`.`guid` = `b`.`guid` "
        ") data "
        "LEFT OUTER JOIN `battleground_events` AS `description` ON `data`.`map` = `description`.`map` "
        "AND `data`.`ev1` = `description`.`event1` AND `data`.`ev2` = `description`.`event2` "
        "ORDER BY `m`, `ev1`, `ev2`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 battleground eventindexes.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        if (fields[2].GetUInt8() == BG_EVENT_NONE || fields[3].GetUInt8() == BG_EVENT_NONE)
        {
            continue; // we don't need to add those to the eventmap
        }

        bool gameobject         = (fields[0].GetUInt8() == 1);
        uint32 dbTableGuidLow   = fields[1].GetUInt32();
        events.event1           = fields[2].GetUInt8();
        events.event2           = fields[3].GetUInt8();
        uint32 map              = fields[4].GetUInt32();

        uint32 desc_map = fields[6].GetUInt32();
        uint8 desc_event1 = fields[7].GetUInt8();
        uint8 desc_event2 = fields[8].GetUInt8();
        const char* description = fields[9].GetString();

        // checking for NULL - through right outer join this will mean following:
        if (fields[5].GetUInt32() != dbTableGuidLow)
        {
            sLog.outErrorDb("BattleGroundEvent: %s with nonexistent guid %u for event: map:%u, event1:%u, event2:%u (\"%s\")",
                (gameobject) ? "gameobject" : "creature", dbTableGuidLow, map, events.event1, events.event2, description);
            continue;
        }

        // checking for NULL - through full outer join this can mean 2 things:
        if (desc_map != map)
        {
            // there is an event missing
            if (dbTableGuidLow == 0)
            {
                sLog.outErrorDb("BattleGroundEvent: missing db-data for map:%u, event1:%u, event2:%u (\"%s\")", desc_map, desc_event1, desc_event2, description);
                continue;
            }
            // we have an event which shouldn't exist
            else
            {
                sLog.outErrorDb("BattleGroundEvent: %s with guid %u is registered, for a nonexistent event: map:%u, event1:%u, event2:%u",
                    (gameobject) ? "gameobject" : "creature", dbTableGuidLow, map, events.event1, events.event2);
                continue;
            }
        }

        if (gameobject)
        {
            m_GameObjectBattleEventIndexMap[dbTableGuidLow] = events;
        }
        else
        {
            m_CreatureBattleEventIndexMap[dbTableGuidLow] = events;
        }

        ++count;
    }
    while (result->NextRow());

    sLog.outString(">> Loaded %u battleground eventindexes", count);
    sLog.outString();
    delete result;
}
