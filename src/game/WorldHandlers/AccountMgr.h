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

#ifndef MANGOS_H_ACCMGR
#define MANGOS_H_ACCMGR

#include "Common.h"

/**
 * @brief Account operation result enumeration
 *
 * An enumeration of the possible results of an account operation.
 */
enum AccountOpResult
{
    AOR_OK,                 ///< Success
    AOR_NAME_TOO_LONG,      ///< Name too long
    AOR_PASS_TOO_LONG,      ///< Password too long
    AOR_NAME_ALREADY_EXIST, ///< Name already exists
    AOR_NAME_NOT_EXIST,     ///< Name does not exist
    AOR_DB_INTERNAL_ERROR   ///< Database internal error
};

#define MAX_ACCOUNT_STR 16
#define MAX_PASSWORD_STR 16

/**
 * @brief Account manager class
 *
 * A class that is used to manage accounts.
 */
class AccountMgr
{
    public:
        /**
         * @brief Constructor
         */
        AccountMgr();

        /**
         * @brief Destructor
         */
        ~AccountMgr();

        /**
         * @brief Create account
         * @param username Username
         * @param password Password
         * @return Account operation result
         */
        AccountOpResult CreateAccount(std::string username, std::string password);

        /**
         * @brief Create account with expansion
         * @param username Username
         * @param password Password
         * @param expansion Expansion level
         * @return Account operation result
         */
        AccountOpResult CreateAccount(std::string username, std::string password, uint32 expansion);

        /**
         * @brief Delete account
         * @param accid Account ID
         * @return Account operation result
         */
        AccountOpResult DeleteAccount(uint32 accid);

        /**
         * @brief Change username
         * @param accid Account ID
         * @param new_uname New username
         * @param new_passwd New password
         * @return Account operation result
         */
        AccountOpResult ChangeUsername(uint32 accid, std::string new_uname, std::string new_passwd);

        /**
         * @brief Change password
         * @param accid Account ID
         * @param new_passwd New password
         * @return Account operation result
         */
        AccountOpResult ChangePassword(uint32 accid, std::string new_passwd);

        /**
         * @brief Check password
         * @param accid Account ID
         * @param passwd Password to check
         * @return True if password matches
         */
        bool CheckPassword(uint32 accid, std::string passwd);

        /**
         * @brief Get account ID by username
         * @param username Username
         * @return Account ID
         */
        uint32 GetId(std::string username);

        /**
         * @brief Get security level
         * @param acc_id Account ID
         * @return Account security type
         */
        AccountTypes GetSecurity(uint32 acc_id);

        /**
         * @brief Get account name
         * @param acc_id Account ID
         * @param name Output name
         * @return True if successful
         */
        bool GetName(uint32 acc_id, std::string& name);

        /**
         * @brief Get character count
         * @param acc_id Account ID
         * @return Character count
         */
        uint32 GetCharactersCount(uint32 acc_id);

        /**
         * @brief Calculate SHA password hash
         * @param name Account name
         * @param password Password
         * @return SHA hash string
         */
        std::string CalculateShaPassHash(std::string& name, std::string& password);
};

/* A macro that creates a global variable called `sAccountMgr` that is an instance of the `AccountMgr` class. */
#define sAccountMgr MaNGOS::Singleton<AccountMgr>::Instance()
#endif
