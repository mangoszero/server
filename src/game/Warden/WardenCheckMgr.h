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

#ifndef _WARDENCHECKMGR_H
#define _WARDENCHECKMGR_H

#include <map>
#include "BigNumber.h"

enum WardenActions
{
    WARDEN_ACTION_LOG,
    WARDEN_ACTION_KICK,
    WARDEN_ACTION_BAN
};

struct WardenCheck
{
    uint8 Type;
    BigNumber Data;
    uint32 Address;                                         // PROC_CHECK, MEM_CHECK, PAGE_CHECK
    uint8 Length;                                           // PROC_CHECK, MEM_CHECK, PAGE_CHECK
    std::string Str;                                        // LUA, MPQ, DRIVER
    std::string Comment;
    uint16 CheckId;
    enum WardenActions Action;
};

struct WardenCheckResult
{
    BigNumber Result;                                       // MEM_CHECK
};

class WardenCheckMgr
{
    private:
        WardenCheckMgr();
        ~WardenCheckMgr();

    public:
        static WardenCheckMgr* instance()
        {
            static WardenCheckMgr instance;
            return &instance;
        }

        WardenCheck* GetWardenDataById(uint16 build, uint16 Id);
        WardenCheckResult* GetWardenResultById(uint16 build, uint16 Id);

        std::vector<uint16> MemChecksIdPool;
        std::vector<uint16> OtherChecksIdPool;

        void LoadWardenChecks();
        void LoadWardenOverrides();

        ACE_RW_Mutex _checkStoreLock;

    private:
        uint32 inline ComposeMultiCheckKey(uint16 clientBuild, uint16 checkID) { return (uint32(checkID) << 16) | clientBuild; }

        // We have a linear key without any gaps, so we use vector for fast access
        typedef std::vector<WardenCheck*> CheckContainer;
        typedef std::map<uint32, WardenCheckResult*> CheckResultContainer;

        CheckContainer CheckStore;
        CheckResultContainer CheckResultStore;

        // here we have just few checks, vector is not appropriate; key is from ComposeMultiCheckKey
        typedef std::map<uint32 /*MultiCheckKey*/, WardenCheck*> MultiCheckContainer;
        typedef std::map<uint32 /*MultiCheckKey*/, WardenCheckResult*> MultiResultContainer;

        MultiCheckContainer MCheckStore;
        MultiResultContainer MCheckResultStore;
};

#define sWardenCheckMgr WardenCheckMgr::instance()

#endif
