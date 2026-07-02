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
#include "DBCStores.h"
#include "SQLStorages.h"
#include "MapManager.h"
#include "GridDefines.h"
#include "DisableMgr.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
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
#include "ItemEnchantmentMgr.h"

/**
 * @brief Loads gameobject spawn records and validates their database data.
 */
void ObjectMgr::LoadGameObjects()
{
    QueryResult* result = WorldDatabase.Query(
        //                        0                    1                  2                   3                          4                          5             6
            "SELECT `gameobject`.`guid`, `gameobject`.`id`, `gameobject`.`map`, `gameobject`.`position_x`, `gameobject`.`position_y`, `gameobject`.`position_z`, `gameobject`.`orientation`, "
        //                 7                         8                         9                         10                        11                            12              13
            "`gameobject`.`rotation0`, `gameobject`.`rotation1`, `gameobject`.`rotation2`, `gameobject`.`rotation3`, `gameobject`.`spawntimesecs`, `gameobject`.`animprogress`, `gameobject`.`state`, "
        //                            14                         15                                       16
            "`game_event_gameobject`.`event`, `pool_gameobject`.`pool_entry`, `pool_gameobject_template`.`pool_entry` "
            "FROM `gameobject` "
            "LEFT OUTER JOIN `game_event_gameobject` ON `gameobject`.`guid` = `game_event_gameobject`.`guid` "
            "LEFT OUTER JOIN `pool_gameobject` ON `gameobject`.`guid` = `pool_gameobject`.`guid` "
            "LEFT OUTER JOIN `pool_gameobject_template` ON `gameobject`.`id` = `pool_gameobject_template`.`id`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 gameobjects. DB table `gameobject` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 local_transports = 0;
    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 guid         = fields[ 0].GetUInt32();
        uint32 entry        = fields[ 1].GetUInt32();

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_GAMEOBJECT_SPAWN, entry, NULL, 0, guid))
        {
            sLog.outDebug("Gameobject guid %u (entry %u) spawning is disabled.", guid, entry);
            continue;
        }

        GameObjectInfo const* gInfo = GetGameObjectInfo(entry);
        if (!gInfo)
        {
            sLog.outErrorDb("Table `gameobject` has gameobject (GUID: %u) with non existing gameobject entry %u, skipped.", guid, entry);
            continue;
        }

        if (gInfo->displayId && !sGameObjectDisplayInfoStore.LookupEntry(gInfo->displayId))
        {
            sLog.outErrorDb("Gameobject (GUID: %u Entry %u GoType: %u) have invalid displayId (%u), not loaded.", guid, entry, gInfo->type, gInfo->displayId);
            continue;
        }

        GameObjectData& data = mGameObjectDataMap[guid];

        data.id             = entry;
        data.mapid          = fields[ 2].GetUInt32();
        data.posX           = fields[ 3].GetFloat();
        data.posY           = fields[ 4].GetFloat();
        data.posZ           = fields[ 5].GetFloat();
        data.orientation    = fields[ 6].GetFloat();
        data.rotation0      = fields[ 7].GetFloat();
        data.rotation1      = fields[ 8].GetFloat();
        data.rotation2      = fields[ 9].GetFloat();
        data.rotation3      = fields[10].GetFloat();
        data.spawntimesecs  = fields[11].GetInt32();
        data.animprogress   = fields[12].GetUInt32();
        uint32 go_state     = fields[13].GetUInt32();
        int16 gameEvent     = fields[14].GetInt16();
        int16 GuidPoolId    = fields[15].GetInt16();
        int16 EntryPoolId   = fields[16].GetInt16();

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.mapid);
        if (!mapEntry)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) that spawned at nonexistent map (Id: %u), skip", guid, data.id, data.mapid);
            continue;
        }

        if (data.spawntimesecs == 0 && gInfo->IsDespawnAtAction())
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with `spawntimesecs` (0) value, but gameobejct marked as despawnable at action.", guid, data.id);
        }

        if (go_state >= MAX_GO_STATE)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid `state` (%u) value, skip", guid, data.id, go_state);
            continue;
        }
        data.go_state       = GOState(go_state);

        if (data.rotation0 < -1.0f || data.rotation0 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation0 (%f) value, skip", guid, data.id, data.rotation0);
            continue;
        }

        if (data.rotation1 < -1.0f || data.rotation1 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation1 (%f) value, skip", guid, data.id, data.rotation1);
            continue;
        }

        if (data.rotation2 < -1.0f || data.rotation2 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation2 (%f) value, skip", guid, data.id, data.rotation2);
            continue;
        }

        if (data.rotation3 < -1.0f || data.rotation3 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation3 (%f) value, skip", guid, data.id, data.rotation3);
            continue;
        }

        if (!MapManager::IsValidMapCoord(data.mapid, data.posX, data.posY, data.posZ, data.orientation))
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid coordinates, skip", guid, data.id);
            continue;
        }

        if (gInfo->type != GAMEOBJECT_TYPE_TRANSPORT && gameEvent == 0 && GuidPoolId == 0 && EntryPoolId == 0) // if not this is to be managed by GameEvent System or Pool system
        {
            AddGameobjectToGrid(guid, &data);
        }

        if (gInfo->type == GAMEOBJECT_TYPE_TRANSPORT)
        {
            m_localTransports.insert(LocalTransportGuidsOnMap::value_type(data.mapid, guid));
            ++local_transports;
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %zu gameobjects", mGameObjectDataMap.size());
    sLog.outString(">>> Loaded %u local transport objects", local_transports);
}

/**
 * @brief Adds a gameobject spawn GUID to the grid lookup for its map cell.
 *
 * @param guid The gameobject spawn GUID.
 * @param data The gameobject spawn data.
 */
void ObjectMgr::AddGameobjectToGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids& cell_guids = mMapObjectGuids[data->mapid][cell_id];
    cell_guids.gameobjects.insert(guid);
}

/**
 * @brief Removes a gameobject spawn GUID from the grid lookup for its map cell.
 *
 * @param guid The gameobject spawn GUID.
 * @param data The gameobject spawn data.
 */
void ObjectMgr::RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids& cell_guids = mMapObjectGuids[data->mapid][cell_id];
    cell_guids.gameobjects.erase(guid);
}
