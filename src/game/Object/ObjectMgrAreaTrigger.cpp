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
#include "DBCStores.h"
#include "Log.h"
#include "ProgressBar.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "SQLStorages.h"
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
 * @brief Loads area trigger teleport destinations and access requirements.
 */
void ObjectMgr::LoadAreaTriggerTeleports()
{
    mAreaTriggers.clear();                                  // need for reload case

    uint32 count = 0;

    //                                                0         1                2                       3                  4                  5                                6                    7
    QueryResult* result = WorldDatabase.Query("SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation`, `status_failed_mangos_string_id`, `condition_id` FROM `areatrigger_teleport`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u area trigger teleport definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        ++count;

        uint32 Trigger_ID = fields[0].GetUInt32();

        AreaTrigger at;

        at.target_mapId                 = fields[1].GetUInt32();
        at.target_X                     = fields[2].GetFloat();
        at.target_Y                     = fields[3].GetFloat();
        at.target_Z                     = fields[4].GetFloat();
        at.target_Orientation           = fields[5].GetFloat();
        at.failed_text_mangos_string_id = fields[6].GetUInt32();
        at.condition                    = fields[7].GetUInt16();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has area trigger (ID:%u) not listed in `AreaTrigger.dbc`.", Trigger_ID);
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(at.target_mapId);
        if (!mapEntry)
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has nonexistent target map (ID: %u) for Area trigger (ID:%u).", at.target_mapId, Trigger_ID);
            continue;
        }

        if (at.target_X == 0 && at.target_Y == 0 && at.target_Z == 0)
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has area trigger (ID:%u) without target coordinates.", Trigger_ID);
            continue;
        }

        if (at.condition && !sConditionStorage.LookupEntry<PlayerCondition>(at.condition))
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has nonexistent condition (ID:%u) for Area trigger (ID:%u).", at.condition, Trigger_ID);
            continue;
        }

        mAreaTriggers[Trigger_ID] = at;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u area trigger teleport definitions", count);
    sLog.outString();
}

/**
 * Searches for the areatrigger which teleports players out of the given map (only direct to continent)
 */
AreaTrigger const* ObjectMgr::GetGoBackTrigger(uint32 map_id) const
{
    InstanceTemplate const* temp = GetInstanceTemplate(map_id);
    if (!temp)
    {
        return NULL;
    }

    // Try to find one that teleports to the map we want to enter
    std::list<AreaTrigger const*> ghostTrigger;
    AreaTrigger const* compareTrigger = NULL;
    for (AreaTriggerMap::const_iterator itr = mAreaTriggers.begin(); itr != mAreaTriggers.end(); ++itr)
    {
        if (itr->second.target_mapId == uint32(temp->ghostEntranceMap))
        {
            ghostTrigger.push_back(&itr->second);
            // First run, only consider AreaTrigger that teleport in the proper map
            if ((!compareTrigger || itr->second.IsLessOrEqualThan(compareTrigger)) && sAreaTriggerStore.LookupEntry(itr->first)->mapid == map_id)
            {
                if (itr->second.IsMinimal())
                {
                    return &itr->second;
                }

                compareTrigger = &itr->second;
            }
        }
    }
    if (compareTrigger)
    {
        return compareTrigger;
    }

    // Second attempt: take one fitting
    for (std::list<AreaTrigger const*>::const_iterator itr = ghostTrigger.begin(); itr != ghostTrigger.end(); ++itr)
    {
        if (!compareTrigger || (*itr)->IsLessOrEqualThan(compareTrigger))
        {
            if ((*itr)->IsMinimal())
            {
                return *itr;
            }

            compareTrigger = *itr;
        }
    }
    return compareTrigger;
}

/**
 * Searches for the areatrigger which teleports players to the given map
 */
AreaTrigger const* ObjectMgr::GetMapEntranceTrigger(uint32 Map) const
{
    AreaTrigger const* compareTrigger = NULL;
    MapEntry const* mEntry = sMapStore.LookupEntry(Map);

    for (AreaTriggerMap::const_iterator itr = mAreaTriggers.begin(); itr != mAreaTriggers.end(); ++itr)
    {
        if (itr->second.target_mapId == Map)
        {
            if (mEntry->Instanceable())
            {
                // Remark that IsLessOrEqualThan is no total order, and a->IsLeQ(b) != !b->IsLeQ(a)
                if (!compareTrigger || compareTrigger->IsLessOrEqualThan(&itr->second))
                {
                    compareTrigger = &itr->second;
                }
            }
            else
            {
                if (!compareTrigger || itr->second.IsLessOrEqualThan(compareTrigger))
                {
                    if (itr->second.IsMinimal())
                    {
                        return &itr->second;
                    }

                    compareTrigger = &itr->second;
                }
            }
        }
    }
    return compareTrigger;
}
