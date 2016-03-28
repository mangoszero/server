/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2016  MaNGOS project <https://getmangos.eu>
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
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

#include "Common.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "WardenCheckMgr.h"
#include "Warden.h"

WardenCheckMgr::WardenCheckMgr() { }

WardenCheckMgr::~WardenCheckMgr()
{
    for (CheckMap::iterator it = CheckStore.begin(); it != CheckStore.end(); ++it)
    {
        for (CheckContainer::iterator itr = it->second->begin(); itr != it->second->end(); ++itr)
            delete itr->second;
        delete it->second;
    }

    for (CheckResultMap::iterator it = CheckResultStore.begin(); it != CheckResultStore.end(); ++it)
    {
        for (CheckResultContainer::iterator itr = it->second->begin(); itr != it->second->end(); ++itr)
            delete itr->second;
        delete it->second;
    }
}

void WardenCheckMgr::LoadWardenChecks()
{
    // Check if Warden is enabled by config before loading anything
    if (!sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED) && !sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED))
    {
        sLog.outWarden(">> Warden disabled, loading checks skipped.");
        return;
    }

    QueryResult *result = WorldDatabase.Query("SELECT DISTINCT build FROM warden");

    if (!result)
    {
        sLog.outString("[Warden]: >> Loaded 0 warden data and results");
        return;
    }

    Field* fields;
    
    do
    {
        fields = result->Fetch();
        uint16 build = fields[0].GetUInt16();
        CheckContainer* ck = new CheckContainer();
        CheckResultContainer* ckr = new CheckResultContainer();
        CheckStore[build] = ck;
        CheckResultStore[build] = ckr;
        
    } while (result->NextRow());

    delete result;

    //                                    0  1      2     3     4       5        6      7     8
    result = WorldDatabase.Query("SELECT id, build, type, data, result, address, length, str, comment FROM warden ORDER BY build ASC, id ASC");
    // no need to check. done in SELECT DISTINCT before

    uint32 count = 0;

    do
    {
        fields = result->Fetch();

        uint16 id               = fields[0].GetUInt16();
        uint16 build            = fields[1].GetUInt16();
        uint8 checkType         = fields[2].GetUInt8();
        std::string data        = fields[3].GetString();
        std::string checkResult = fields[4].GetString();
        uint32 address          = fields[5].GetUInt32();
        uint8 length            = fields[6].GetUInt8();
        std::string str         = fields[7].GetString();
        std::string comment     = fields[8].GetString();

        WardenCheck* wardenCheck = new WardenCheck();
        wardenCheck->Type = checkType;
        wardenCheck->CheckId = id;

        // Initialize action with default action from config
        wardenCheck->Action = WardenActions(sWorld.getConfig(CONFIG_UINT32_WARDEN_CLIENT_FAIL_ACTION));

        if (checkType == PAGE_CHECK_A || checkType == PAGE_CHECK_B || checkType == DRIVER_CHECK)
        {
            wardenCheck->Data.SetHexStr(data.c_str());
            int len = data.size() / 2;

            if (wardenCheck->Data.GetNumBytes() < len)
            {
                uint8 temp[24];
                memset(temp, 0, len);
                memcpy(temp, wardenCheck->Data.AsByteArray(), wardenCheck->Data.GetNumBytes());
                std::reverse(temp, temp + len);
                wardenCheck->Data.SetBinary((uint8*)temp, len);
            }
        }

        if (checkType == MEM_CHECK || checkType == MODULE_CHECK)
            MemChecksIdPool.push_back(id);
        else
            OtherChecksIdPool.push_back(id);

        if (checkType == MEM_CHECK || checkType == PAGE_CHECK_A || checkType == PAGE_CHECK_B || checkType == PROC_CHECK)
        {
            wardenCheck->Address = address;
            wardenCheck->Length = length;
        }

        // PROC_CHECK support missing
        if (checkType == MEM_CHECK || checkType == MPQ_CHECK || checkType == LUA_STR_CHECK || checkType == DRIVER_CHECK || checkType == MODULE_CHECK)
            wardenCheck->Str = str;

        CheckStore[build]->insert(std::pair<uint16,WardenCheck*>(id, wardenCheck));

        if (checkType == MPQ_CHECK || checkType == MEM_CHECK)
        {
            WardenCheckResult* wr = new WardenCheckResult();
            wr->Result.SetHexStr(checkResult.c_str());
            int len = checkResult.size() / 2;
            if (wr->Result.GetNumBytes() < len)
            {
                uint8 *temp = new uint8[len];
                memset(temp, 0, len);
                memcpy(temp, wr->Result.AsByteArray(), wr->Result.GetNumBytes());
                std::reverse(temp, temp + len);
                wr->Result.SetBinary((uint8*)temp, len);
                delete[] temp;
            }
            CheckResultStore[build]->insert(std::pair<uint16, WardenCheckResult*>(id,wr));
        }

        if (comment.empty())
            wardenCheck->Comment = "";
        else
            wardenCheck->Comment = comment;

        ++count;
    } while (result->NextRow());

    sLog.outWarden(">> Loaded %u warden checks.", count);

    delete result;
}

void WardenCheckMgr::LoadWardenOverrides()
{
    // Check if Warden is enabled by config before loading anything
    if (!sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED) && !sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED))
    {
        sLog.outWarden(">> Warden disabled, loading check overrides skipped.");
        return;
    }

    //                                                      0        1
    QueryResult* result = CharacterDatabase.Query("SELECT wardenId, action FROM warden_action");

    if (!result)
    {
        sLog.outWarden(">> Loaded 0 Warden action overrides. DB table `warden_action` is empty!");
        return;
    }

    uint32 count = 0;

    ACE_WRITE_GUARD(ACE_RW_Mutex, g, _checkStoreLock);

    do
    {
        Field* fields = result->Fetch();

        uint16 checkId = fields[0].GetUInt16();
        uint8  action  = fields[1].GetUInt8();

        // Check if action value is in range (0-2, see WardenActions enum)
        if (action > WARDEN_ACTION_BAN)
            sLog.outWarden("Warden check override action out of range (ID: %u, action: %u)", checkId, action);

        // Check if check actually exists before accessing the CheckStore vector
        else 
        {
            for (CheckMap::iterator it = CheckStore.begin(); it != CheckStore.end(); ++it)
            {
                CheckContainer::iterator ir = it->second->find(checkId);
                if (ir == it->second->end())
                    sLog.outWarden("Warden check action override for non-existing check (ID: %u, action: %u), skipped", checkId, action);
                else
                {
                    ir->second->Action = WardenActions(action);
                    ++count;
                }
            }
        }
    }
    while (result->NextRow());

    sLog.outWarden(">> Loaded %u warden action overrides.", count);
}

WardenCheck* WardenCheckMgr::GetWardenDataById(uint16 build, uint16 id)
{
    WardenCheck* result = NULL;
    CheckMap::iterator it = CheckStore.find(build);
    if (it != CheckStore.end())
    {
        CheckContainer::iterator ir = it->second->find(id);
        if (ir != it->second->end())
            result = ir->second;
    }

    return result;
}

WardenCheckResult* WardenCheckMgr::GetWardenResultById(uint16 build, uint16 id)
{
    WardenCheckResult* result = NULL;
    CheckResultMap::iterator it = CheckResultStore.find(build);
    if (it != CheckResultStore.end())
    {
        CheckResultContainer::iterator ir = it->second->find(id);
        if (ir != it->second->end())
            result = ir->second;
    }

    return result;
}
