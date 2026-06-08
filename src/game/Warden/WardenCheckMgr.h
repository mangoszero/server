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

#ifndef _WARDENCHECKMGR_H
#define _WARDENCHECKMGR_H

#include <map>
#include "BigNumber.h"

/**
 * @brief Warden actions enumeration
 */
enum WardenActions
{
    WARDEN_ACTION_LOG,  ///< Log action
    WARDEN_ACTION_KICK, ///< Kick action
    WARDEN_ACTION_BAN   ///< Ban action
};

/**
 * @brief Warden check structure
 */
struct WardenCheck
{
    uint8 Type; ///< Check type
    BigNumber Data; ///< Data
    uint32 Address; ///< Address (PROC_CHECK, MEM_CHECK, PAGE_CHECK)
    uint8 Length; ///< Length (PROC_CHECK, MEM_CHECK, PAGE_CHECK)
    std::string Str; ///< String (LUA, MPQ, DRIVER)
    std::string Comment; ///< Comment
    uint16 CheckId; ///< Check ID
    enum WardenActions Action; ///< Action to take
};

/**
 * @brief Warden check result structure
 */
struct WardenCheckResult
{
    uint16 Id; ///< ID
    BigNumber Result; ///< Result (MEM_CHECK)
};

/**
 * @brief Warden check manager class
 */
class WardenCheckMgr
{
    private:
        /**
         * @brief Constructor
         */
        WardenCheckMgr();

        /**
         * @brief Destructor
         */
        ~WardenCheckMgr();

    public:
        /**
         * @brief Get instance
         * @return Warden check manager instance
         */
        static WardenCheckMgr* instance()
        {
            static WardenCheckMgr instance;
            return &instance;
        }

        /**
         * @brief Get warden data by ID
         * @param build Client build
         * @param id Check ID
         * @return Warden check
         */
        WardenCheck* GetWardenDataById(uint16 /*build*/, uint16 /*id*/);

        /**
         * @brief Get warden result by ID
         * @param build Client build
         * @param id Check ID
         * @return Warden check result
         */
        WardenCheckResult* GetWardenResultById(uint16 /*build*/, uint16 /*id*/);

        /**
         * @brief Get warden check IDs
         * @param isMemCheck True if memory check
         * @param build Client build
         * @param list Output list of check IDs
         */
        void GetWardenCheckIds(bool isMemCheck /* true = MEM */, uint16 build, std::list<uint16>& list);

        /**
         * @brief Load warden checks
         */
        void LoadWardenChecks();

        /**
         * @brief Load warden overrides
         */
        void LoadWardenOverrides();

    private:
        typedef ACE_RW_Thread_Mutex LOCK;
        typedef std::multimap<uint16, WardenCheck*> CheckMap;
        typedef std::multimap<uint16, WardenCheckResult*> CheckResultMap;

        LOCK m_lock; ///< Lock
        CheckMap CheckStore; ///< Check store
        CheckResultMap CheckResultStore; ///< Check result store
};

#define sWardenCheckMgr WardenCheckMgr::instance()

#endif
