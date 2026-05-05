/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

WardenCheckMgr::WardenCheckMgr() : m_lock(0), CheckStore(), CheckResultStore() { }

WardenCheckMgr::~WardenCheckMgr()
{
    for (CheckMap::iterator it = CheckStore.begin(); it != CheckStore.end(); ++it)
    {
        delete it->second;
    }

    for (CheckResultMap::iterator it = CheckResultStore.begin(); it != CheckResultStore.end(); ++it)
    {
        delete it->second;
    }

    CheckStore.clear();
    CheckResultStore.clear();
}

void WardenCheckMgr::LoadWardenChecks()
{
    // Check if Warden is enabled by config before loading anything
    if (!sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED) && !sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED))
    {
        sLog.outString(">> Warden disabled, loading checks skipped.");
        return;
    }

    // Detect whether the optional `groupid` column exists in this deployment's
    // schema. If it does, include it in the SELECT so rotation can use it.
    bool hasGroupId = false;
    {
        QueryResult* probe = WorldDatabase.Query("SHOW COLUMNS FROM `warden` LIKE 'groupid'");
        if (probe)
        {
            hasGroupId = true;
            delete probe;
        }
    }

    QueryResult *result = NULL;
    if (hasGroupId)
    {                                             //  0     1        2       3       4         5          6         7      8          9
        result = WorldDatabase.Query("SELECT `id`, `build`, `type`, `data`, `result`, `address`, `length`, `str`, `comment`, `groupid` FROM `warden` ORDER BY `build` ASC, `id` ASC");
    }
    else
    {                                             //  0     1        2       3       4         5          6         7      8
        result = WorldDatabase.Query("SELECT `id`, `build`, `type`, `data`, `result`, `address`, `length`, `str`, `comment` FROM `warden` ORDER BY `build` ASC, `id` ASC");
    }

    if (!result)
    {
        sLog.outString("[Warden]: >> Loaded 0 warden data and results");
        return;
    }

    uint32 count = 0;
    Field* fields;

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
        uint16 groupId          = hasGroupId ? fields[9].GetUInt16() : 0;

        WardenCheck* wardenCheck = new WardenCheck();
        wardenCheck->Type = checkType;
        wardenCheck->CheckId = id;
        wardenCheck->GroupId = groupId;

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

        if (checkType == MEM_CHECK || checkType == PAGE_CHECK_A || checkType == PAGE_CHECK_B || checkType == PROC_CHECK)
        {
            wardenCheck->Address = address;
            wardenCheck->Length = length;
        }

        // PROC_CHECK support missing
        if (checkType == MEM_CHECK || checkType == MPQ_CHECK || checkType == LUA_STR_CHECK || checkType == DRIVER_CHECK || checkType == MODULE_CHECK)
        {
            wardenCheck->Str = str;
        }

        CheckStore.insert(std::pair<uint16, WardenCheck*>(build, wardenCheck));

        if (checkType == MPQ_CHECK || checkType == MEM_CHECK)
        {
            WardenCheckResult* wr = new WardenCheckResult();
            wr->Id = id;
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
            CheckResultStore.insert(std::pair<uint16, WardenCheckResult*>(build, wr));
        }

        if (comment.empty())
        {
            wardenCheck->Comment = "";
        }
        else
        {
            wardenCheck->Comment = comment;
        }

        ++count;
    } while (result->NextRow());

    sLog.outString(">> Loaded %u warden checks.", count);

    delete result;
}

void WardenCheckMgr::LoadWardenOverrides()
{
    // Check if Warden is enabled by config before loading anything
    if (!sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED) && !sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED))
    {
        sLog.outString(">> Warden disabled, loading check overrides skipped.");
        return;
    }

    //                                                    0         1
    QueryResult* result = CharacterDatabase.Query("SELECT `wardenId`, `action` FROM `warden_action`");

    if (!result)
    {
        sLog.outString(">> Loaded 0 Warden action overrides. DB table `warden_action` is empty!");
        return;
    }

    uint32 count = 0;

    ACE_WRITE_GUARD(LOCK, g, m_lock)

    do
    {
        Field* fields = result->Fetch();

        uint16 checkId = fields[0].GetUInt16();
        uint8  action  = fields[1].GetUInt8();

        // Check if action value is in range (0-2, see WardenActions enum)
        if (action > WARDEN_ACTION_BAN)
        {
            sLog.outWarden("Warden check override action out of range (ID: %u, action: %u)", checkId, action);
        }
        else
        {
            bool found = false;
            for (CheckMap::iterator it = CheckStore.begin(); it != CheckStore.end(); ++it)
            {
                if (it->second->CheckId == checkId)
                {
                    it->second->Action = WardenActions(action);
                    ++count;
                    found = true;
                }
            }
            if (!found)
                sLog.outWarden("Warden check action override for non-existing check (ID: %u, action: %u), skipped", checkId, action);
        }
    }
    while (result->NextRow());

    sLog.outString(">> Loaded %u warden action overrides.", count);
}

WardenCheck* WardenCheckMgr::GetWardenDataById(uint16 build, uint16 id)
{
    WardenCheck* result = NULL;

    ACE_READ_GUARD_RETURN(LOCK, g, m_lock, result)
    for (CheckMap::iterator it = CheckStore.lower_bound(build); it != CheckStore.upper_bound(build); ++it)
    {
        if (it->second->CheckId == id)
        {
            result = it->second;
        }
    }

    return result;
}

WardenCheckResult* WardenCheckMgr::GetWardenResultById(uint16 build, uint16 id)
{
    WardenCheckResult* result = NULL;

    ACE_READ_GUARD_RETURN(LOCK, g, m_lock, result)
    for (CheckResultMap::iterator it = CheckResultStore.lower_bound(build); it != CheckResultStore.upper_bound(build); ++it)
    {
        if (it->second->Id == id)
        {
            result = it->second;
        }
    }

    return result;
}

// MEM_CHECK and MODULE_CHECK go through the dedicated memory queue. Every other
// check type goes through the "other" queue. The two queues must be disjoint so
// the same check id is never sent twice in the same cycle.
static bool IsMemoryQueueCheck(uint8 type)
{
    return type == MEM_CHECK || type == MODULE_CHECK;
}

void WardenCheckMgr::GetWardenCheckIds(bool isMemCheck, uint16 build, std::list<uint16>& idl)
{
    idl.clear(); //just to be sure

    // Bucket by groupid so we can interleave when consuming. groupid 0 is treated
    // as "no group" - one bucket per check.
    typedef std::map<uint16, std::vector<uint16> > GroupBuckets;
    GroupBuckets buckets;
    std::vector<uint16> ungrouped;

    {
        ACE_READ_GUARD(LOCK, g, m_lock)
        for (CheckMap::iterator it = CheckStore.lower_bound(build); it != CheckStore.upper_bound(build); ++it)
        {
            const uint8 type = it->second->Type;
            const bool isMem = IsMemoryQueueCheck(type);

            if (isMemCheck)
            {
                if (!isMem)
                {
                    continue;
                }
            }
            else
            {
                // Skip TIMING_CHECK - WardenWin::RequestData() appends a
                // synthetic TIMING_CHECK per cycle.
                if (isMem || type == TIMING_CHECK)
                {
                    continue;
                }
            }

            if (it->second->GroupId == 0)
            {
                ungrouped.push_back(it->second->CheckId);
            }
            else
            {
                buckets[it->second->GroupId].push_back(it->second->CheckId);
            }
        }
    }

    // Round-robin across grouped buckets so checks belonging to the same group
    // do not all get consumed back-to-back in a single cycle. This minimizes
    // repeated coverage of similar checks.
    bool madeProgress;
    do
    {
        madeProgress = false;
        for (GroupBuckets::iterator it = buckets.begin(); it != buckets.end(); ++it)
        {
            if (!it->second.empty())
            {
                idl.push_back(it->second.back());
                it->second.pop_back();
                madeProgress = true;
            }
        }
    } while (madeProgress);

    // Ungrouped checks tail (insertion order).
    for (std::vector<uint16>::iterator it = ungrouped.begin(); it != ungrouped.end(); ++it)
    {
        idl.push_back(*it);
    }
}
