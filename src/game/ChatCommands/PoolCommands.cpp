/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Chat.h"
#include "Language.h"


void ChatHandler::ShowPoolListHelper(uint16 pool_id)
{
    PoolTemplateData const& pool_template = sPoolMgr.GetPoolTemplate(pool_id);
    if (m_session)
        PSendSysMessage(LANG_POOL_ENTRY_LIST_CHAT,
                        pool_id, pool_id, pool_template.description.c_str(), pool_template.AutoSpawn ? 1 : 0, pool_template.MaxLimit,
                        sPoolMgr.GetPoolCreatures(pool_id).size(), sPoolMgr.GetPoolGameObjects(pool_id).size(), sPoolMgr.GetPoolPools(pool_id).size());
    else
        PSendSysMessage(LANG_POOL_ENTRY_LIST_CONSOLE,
                        pool_id, pool_template.description.c_str(), pool_template.AutoSpawn ? 1 : 0, pool_template.MaxLimit,
                        sPoolMgr.GetPoolCreatures(pool_id).size(), sPoolMgr.GetPoolGameObjects(pool_id).size(), sPoolMgr.GetPoolPools(pool_id).size());
}

bool ChatHandler::HandlePoolListCommand(char* /*args*/)
{
    Player* player = m_session->GetPlayer();

    MapPersistentState* mapState = player->GetMap()->GetPersistentState();

    if (!mapState->GetMapEntry()->Instanceable())
    {
        PSendSysMessage(LANG_POOL_LIST_NON_INSTANCE, mapState->GetMapEntry()->name[GetSessionDbcLocale()], mapState->GetMapId());
        SetSentErrorMessage(false);
        return false;
    }

    uint32 counter = 0;

    // spawn pools for expected map or for not initialized shared pools state for non-instanceable maps
    for (uint16 pool_id = 0; pool_id < sPoolMgr.GetMaxPoolId(); ++pool_id)
    {
        if (sPoolMgr.GetPoolTemplate(pool_id).CanBeSpawnedAtMap(mapState->GetMapEntry()))
        {
            ShowPoolListHelper(pool_id);
            ++counter;
        }
    }

    if (counter == 0)
    {
        PSendSysMessage(LANG_NO_POOL_FOR_MAP, mapState->GetMapEntry()->name[GetSessionDbcLocale()], mapState->GetMapId());
    }

    return true;
}

bool ChatHandler::HandlePoolSpawnsCommand(char* args)
{
    Player* player = m_session->GetPlayer();

    MapPersistentState* mapState = player->GetMap()->GetPersistentState();

    // shared continent pools data expected too big for show
    uint32 pool_id = 0;
    if (!ExtractUint32KeyFromLink(&args, "Hpool", pool_id) && !mapState->GetMapEntry()->Instanceable())
    {
        PSendSysMessage(LANG_POOL_SPAWNS_NON_INSTANCE, mapState->GetMapEntry()->name[GetSessionDbcLocale()], mapState->GetMapId());
        SetSentErrorMessage(false);
        return false;
    }

    SpawnedPoolData const& spawns = mapState->GetSpawnedPoolData();

    SpawnedPoolObjects const& crSpawns = spawns.GetSpawnedCreatures();
    for (SpawnedPoolObjects::const_iterator itr = crSpawns.begin(); itr != crSpawns.end(); ++itr)
        if (!pool_id || pool_id == sPoolMgr.IsPartOfAPool<Creature>(*itr))
            if (CreatureData const* data = sObjectMgr.GetCreatureData(*itr))
                if (CreatureInfo const* info = ObjectMgr::GetCreatureTemplate(data->id))
                    PSendSysMessage(LANG_CREATURE_LIST_CHAT, *itr, PrepareStringNpcOrGoSpawnInformation<Creature>(*itr).c_str(),
                                    *itr, info->Name, data->posX, data->posY, data->posZ, data->mapid);

    SpawnedPoolObjects const& goSpawns = spawns.GetSpawnedGameobjects();
    for (SpawnedPoolObjects::const_iterator itr = goSpawns.begin(); itr != goSpawns.end(); ++itr)
        if (!pool_id || pool_id == sPoolMgr.IsPartOfAPool<GameObject>(*itr))
            if (GameObjectData const* data = sObjectMgr.GetGOData(*itr))
                if (GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(data->id))
                    PSendSysMessage(LANG_GO_LIST_CHAT, *itr, PrepareStringNpcOrGoSpawnInformation<GameObject>(*itr).c_str(),
                                    *itr, info->name, data->posX, data->posY, data->posZ, data->mapid);

    return true;
}

bool ChatHandler::HandlePoolInfoCommand(char* args)
{
    // id or [name] Shift-click form |color|Hpool:id|h[name]|h|r
    uint32 pool_id;
    if (!ExtractUint32KeyFromLink(&args, "Hpool", pool_id))
    {
        return false;
    }

    if (pool_id > sPoolMgr.GetMaxPoolId())
    {
        PSendSysMessage(LANG_POOL_ENTRY_LOWER_MAX_POOL, pool_id, sPoolMgr.GetMaxPoolId());
        return true;
    }

    Player* player = m_session ? m_session->GetPlayer() : NULL;

    MapPersistentState* mapState = player ? player->GetMap()->GetPersistentState() : NULL;
    SpawnedPoolData const* spawns = mapState ? &mapState->GetSpawnedPoolData() : NULL;

    std::string active_str = GetMangosString(LANG_ACTIVE);

    PoolTemplateData const& pool_template = sPoolMgr.GetPoolTemplate(pool_id);
    uint32 mother_pool_id = sPoolMgr.IsPartOfAPool<Pool>(pool_id);
    if (!mother_pool_id)
    {
        PSendSysMessage(LANG_POOL_INFO_HEADER, pool_id, pool_template.AutoSpawn, pool_template.MaxLimit);
    }
    else
    {
        PoolTemplateData const& mother_template = sPoolMgr.GetPoolTemplate(mother_pool_id);
        if (m_session)
            PSendSysMessage(LANG_POOL_INFO_HEADER_CHAT, pool_id, mother_pool_id, mother_pool_id, mother_template.description.c_str(),
                            pool_template.AutoSpawn, pool_template.MaxLimit);
        else
            PSendSysMessage(LANG_POOL_INFO_HEADER_CONSOLE, pool_id, mother_pool_id, mother_template.description.c_str(),
                            pool_template.AutoSpawn, pool_template.MaxLimit);
    }

    PoolGroup<Creature> const& poolCreatures = sPoolMgr.GetPoolCreatures(pool_id);
    SpawnedPoolObjects const* crSpawns = spawns ? &spawns->GetSpawnedCreatures() : NULL;

    PoolObjectList const& poolCreaturesEx = poolCreatures.GetExplicitlyChanced();
    if (!poolCreaturesEx.empty())
    {
        SendSysMessage(LANG_POOL_CHANCE_CREATURE_LIST_HEADER);
        for (PoolObjectList::const_iterator itr = poolCreaturesEx.begin(); itr != poolCreaturesEx.end(); ++itr)
        {
            if (CreatureData const* data = sObjectMgr.GetCreatureData(itr->guid))
            {
                if (CreatureInfo const* info = ObjectMgr::GetCreatureTemplate(data->id))
                {
                    char const* active = crSpawns && crSpawns->find(itr->guid) != crSpawns->end() ? active_str.c_str() : "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_CHANCE_CREATURE_LIST_CHAT, itr->guid, PrepareStringNpcOrGoSpawnInformation<Creature>(itr->guid).c_str(),
                                        itr->guid, info->Name, data->posX, data->posY, data->posZ, data->mapid, itr->chance, active);
                    else
                        PSendSysMessage(LANG_POOL_CHANCE_CREATURE_LIST_CONSOLE, itr->guid, PrepareStringNpcOrGoSpawnInformation<Creature>(itr->guid).c_str(),
                                        info->Name, data->posX, data->posY, data->posZ, data->mapid, itr->chance, active);
                }
            }
        }
    }

    PoolObjectList const& poolCreaturesEq = poolCreatures.GetEqualChanced();
    if (!poolCreaturesEq.empty())
    {
        SendSysMessage(LANG_POOL_CREATURE_LIST_HEADER);
        for (PoolObjectList::const_iterator itr = poolCreaturesEq.begin(); itr != poolCreaturesEq.end(); ++itr)
        {
            if (CreatureData const* data = sObjectMgr.GetCreatureData(itr->guid))
            {
                if (CreatureInfo const* info = ObjectMgr::GetCreatureTemplate(data->id))
                {
                    char const* active = crSpawns && crSpawns->find(itr->guid) != crSpawns->end() ? active_str.c_str() : "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_CREATURE_LIST_CHAT, itr->guid, PrepareStringNpcOrGoSpawnInformation<Creature>(itr->guid).c_str(),
                                        itr->guid, info->Name, data->posX, data->posY, data->posZ, data->mapid, active);
                    else
                        PSendSysMessage(LANG_POOL_CREATURE_LIST_CONSOLE, itr->guid, PrepareStringNpcOrGoSpawnInformation<Creature>(itr->guid).c_str(),
                                        info->Name, data->posX, data->posY, data->posZ, data->mapid, active);
                }
            }
        }
    }

    PoolGroup<GameObject> const& poolGameObjects = sPoolMgr.GetPoolGameObjects(pool_id);
    SpawnedPoolObjects const* goSpawns = spawns ? &spawns->GetSpawnedGameobjects() : NULL;

    PoolObjectList const& poolGameObjectsEx = poolGameObjects.GetExplicitlyChanced();
    if (!poolGameObjectsEx.empty())
    {
        SendSysMessage(LANG_POOL_CHANCE_GO_LIST_HEADER);
        for (PoolObjectList::const_iterator itr = poolGameObjectsEx.begin(); itr != poolGameObjectsEx.end(); ++itr)
        {
            if (GameObjectData const* data = sObjectMgr.GetGOData(itr->guid))
            {
                if (GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(data->id))
                {
                    char const* active = goSpawns && goSpawns->find(itr->guid) != goSpawns->end() ? active_str.c_str() : "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_CHANCE_GO_LIST_CHAT, itr->guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(itr->guid).c_str(),
                                        itr->guid, info->name, data->posX, data->posY, data->posZ, data->mapid, itr->chance, active);
                    else
                        PSendSysMessage(LANG_POOL_CHANCE_GO_LIST_CONSOLE, itr->guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(itr->guid).c_str(),
                                        info->name, data->posX, data->posY, data->posZ, data->mapid, itr->chance, active);
                }
            }
        }
    }

    PoolObjectList const& poolGameObjectsEq = poolGameObjects.GetEqualChanced();
    if (!poolGameObjectsEq.empty())
    {
        SendSysMessage(LANG_POOL_GO_LIST_HEADER);
        for (PoolObjectList::const_iterator itr = poolGameObjectsEq.begin(); itr != poolGameObjectsEq.end(); ++itr)
        {
            if (GameObjectData const* data = sObjectMgr.GetGOData(itr->guid))
            {
                if (GameObjectInfo const* info = ObjectMgr::GetGameObjectInfo(data->id))
                {
                    char const* active = goSpawns && goSpawns->find(itr->guid) != goSpawns->end() ? active_str.c_str() : "";
                    if (m_session)
                        PSendSysMessage(LANG_POOL_GO_LIST_CHAT, itr->guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(itr->guid).c_str(),
                                        itr->guid, info->name, data->posX, data->posY, data->posZ, data->mapid, active);
                    else
                        PSendSysMessage(LANG_POOL_GO_LIST_CONSOLE, itr->guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(itr->guid).c_str(),
                                        info->name, data->posX, data->posY, data->posZ, data->mapid, active);
                }
            }
        }
    }

    PoolGroup<Pool> const& poolPools = sPoolMgr.GetPoolPools(pool_id);
    SpawnedPoolPools const* poolSpawns = spawns ? &spawns->GetSpawnedPools() : NULL;

    PoolObjectList const& poolPoolsEx = poolPools.GetExplicitlyChanced();
    if (!poolPoolsEx.empty())
    {
        SendSysMessage(LANG_POOL_CHANCE_POOL_LIST_HEADER);
        for (PoolObjectList::const_iterator itr = poolPoolsEx.begin(); itr != poolPoolsEx.end(); ++itr)
        {
            PoolTemplateData const& itr_template = sPoolMgr.GetPoolTemplate(itr->guid);
            char const* active = poolSpawns && poolSpawns->find(itr->guid) != poolSpawns->end() ? active_str.c_str() : "";
            if (m_session)
                PSendSysMessage(LANG_POOL_CHANCE_POOL_LIST_CHAT, itr->guid,
                                itr->guid, itr_template.description.c_str(), itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                                sPoolMgr.GetPoolCreatures(itr->guid).size(), sPoolMgr.GetPoolGameObjects(itr->guid).size(), sPoolMgr.GetPoolPools(itr->guid).size(),
                                itr->chance, active);
            else
                PSendSysMessage(LANG_POOL_CHANCE_POOL_LIST_CONSOLE, itr->guid,
                                itr_template.description.c_str(), itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                                sPoolMgr.GetPoolCreatures(itr->guid).size(), sPoolMgr.GetPoolGameObjects(itr->guid).size(), sPoolMgr.GetPoolPools(itr->guid).size(),
                                itr->chance, active);
        }
    }

    PoolObjectList const& poolPoolsEq = poolPools.GetEqualChanced();
    if (!poolPoolsEq.empty())
    {
        SendSysMessage(LANG_POOL_POOL_LIST_HEADER);
        for (PoolObjectList::const_iterator itr = poolPoolsEq.begin(); itr != poolPoolsEq.end(); ++itr)
        {
            PoolTemplateData const& itr_template = sPoolMgr.GetPoolTemplate(itr->guid);
            char const* active = poolSpawns && poolSpawns->find(itr->guid) != poolSpawns->end() ? active_str.c_str() : "";
            if (m_session)
                PSendSysMessage(LANG_POOL_POOL_LIST_CHAT, itr->guid,
                                itr->guid, itr_template.description.c_str(), itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                                sPoolMgr.GetPoolCreatures(itr->guid).size(), sPoolMgr.GetPoolGameObjects(itr->guid).size(), sPoolMgr.GetPoolPools(itr->guid).size(),
                                active);
            else
                PSendSysMessage(LANG_POOL_POOL_LIST_CONSOLE, itr->guid,
                                itr_template.description.c_str(), itr_template.AutoSpawn ? 1 : 0, itr_template.MaxLimit,
                                sPoolMgr.GetPoolCreatures(itr->guid).size(), sPoolMgr.GetPoolGameObjects(itr->guid).size(), sPoolMgr.GetPoolPools(itr->guid).size(),
                                active);
        }
    }
    return true;
}
