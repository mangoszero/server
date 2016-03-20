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
    for (uint16 i = 0; i < CheckStore.size(); ++i)
        delete CheckStore[i];

    for (CheckResultContainer::iterator itr = CheckResultStore.begin(); itr != CheckResultStore.end(); ++itr)
        delete itr->second;
}

void WardenCheckMgr::LoadWardenChecks()
{
    // Check if Warden is enabled by config before loading anything
    if (!sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED) && !sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED))
    {
        sLog.outWarden(">> Warden disabled, loading checks skipped.");
        return;
    }

    QueryResult *result = WorldDatabase.Query("SELECT MAX(id) FROM warden_checks");

    if (!result)
    {
        sLog.outWarden(">> Loaded 0 Warden checks. DB table `warden_checks` is empty!");
        return;
    }

    Field* fields = result->Fetch();

    uint16 maxCheckId = fields[0].GetUInt16();

    CheckStore.resize(maxCheckId + 1);

    delete result;
    //                                    0    1     2     3        4       5      6      7
    result = WorldDatabase.Query("SELECT id, type, data, result, address, length, str, comment FROM warden_checks ORDER BY id ASC");

    uint32 count = 0;

    if (!result)
    {
        sLog.outString("[Warden]: >> Loaded %u warden data and results", count);
        return;
    }

    do
    {
        fields = result->Fetch();

        uint16 id = fields[0].GetUInt16();
        uint8 checkType = fields[1].GetUInt8();
        std::string data = fields[2].GetString();
        std::string checkResult = fields[3].GetString();
        uint32 address = fields[4].GetUInt32();
        uint8 length = fields[5].GetUInt8();
        std::string str = fields[6].GetString();
        std::string comment = fields[7].GetString();

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

        CheckStore[id] = wardenCheck;

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
            CheckResultStore[id] = wr;
        }

        if (comment.empty())
            wardenCheck->Comment = "";
        else
            wardenCheck->Comment = comment;

        ++count;
    } while (result->NextRow());

    sLog.outWarden(">> Loaded %u warden checks.", count);

    delete result;
    //                                    0      1     2       3        4       5    6
    result = WorldDatabase.Query("SELECT id, build, data, result, address, length, str FROM warden_build_specific ORDER BY id ASC");

    if (!result)
    {
        sLog.outString("[Warden]: >> Loaded 0 warden client build-specific data.");
        return;
    }

    count = 0;
    do
    {
        fields = result->Fetch();

        uint16 id = fields[0].GetUInt16();
        if (id >= CheckStore.size())
        {
            sLog.outWarden("ERROR: Build-specific, check is missing in warden_checks, skipping id %u.", id);
            continue;
        }

        uint16 build = fields[1].GetUInt16();
        if (build == DEFAULT_CLIENT_BUILD)
        {
            sLog.outWarden("ERROR: Build-specific table may not contain checks for default %u build, skipping id %u.", DEFAULT_CLIENT_BUILD, id);
            continue;
        }

        //std::string data = fields[2].GetString(); //unused for now
        std::string checkResult = fields[3].GetString();
        uint32 address = fields[4].GetUInt32();
        uint8 length = fields[5].GetUInt8();
        std::string str = fields[6].GetString();

        WardenCheck* wardenCheck = new WardenCheck();
        wardenCheck->CheckId = id;
        wardenCheck->Type = CheckStore[id]->Type;

        WardenCheckResult* wr = new WardenCheckResult();
        switch (wardenCheck->Type)
        {
        case MEM_CHECK:
            wardenCheck->Address = address;
            wardenCheck->Length = length;
            wardenCheck->Str = str;
            wr->Result.SetHexStr(checkResult.c_str());
            {
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
            }
            break;
        default:
            sLog.outWarden("The check type %u is considered as build-independent, skipping id %u.", wardenCheck->Type, id);
            delete wr;
            delete wardenCheck;
            continue;
        }

        MCheckStore[ComposeMultiCheckKey(build, id)] = wardenCheck;
        MCheckResultStore[ComposeMultiCheckKey(build, id)] = wr;
        ++count;

    } while (result->NextRow());

    sLog.outString(">> Loaded %u warden client build-specific check overrides.", count);
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
        else if (checkId > CheckStore.size())
            sLog.outWarden("Warden check action override for non-existing check (ID: %u, action: %u), skipped", checkId, action);
        else
        {
            CheckStore[checkId]->Action = WardenActions(action);
            ++count;
        }
    }
    while (result->NextRow());

    sLog.outWarden(">> Loaded %u warden action overrides.", count);
}

WardenCheck* WardenCheckMgr::GetWardenDataById(uint16 build, uint16 Id)
{
    if (Id < CheckStore.size())
    {
        if (build != DEFAULT_CLIENT_BUILD)
        {
            MultiCheckContainer::const_iterator it = MCheckStore.find(ComposeMultiCheckKey(build, Id));
            if (it != MCheckStore.end())
                return it->second;
        }
        return CheckStore[Id];
    }

    return NULL;
}

WardenCheckResult* WardenCheckMgr::GetWardenResultById(uint16 build, uint16 Id)
{
    if (build != DEFAULT_CLIENT_BUILD)
    {
        MultiResultContainer::const_iterator it = MCheckResultStore.find(ComposeMultiCheckKey(build, Id));
        if (it != MCheckResultStore.end())
            return it->second;
    }

    CheckResultContainer::const_iterator itr = CheckResultStore.find(Id);
    if (itr != CheckResultStore.end())
        return itr->second;

    return NULL;
}
