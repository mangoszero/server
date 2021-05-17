/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2021 MaNGOS <https://getmangos.eu>
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

enum AccountOpResult
{
    AOR_OK,
    AOR_NAME_TOO_LONG,
    AOR_PASS_TOO_LONG,
    AOR_NAME_ALREADY_EXIST,
    AOR_NAME_NOT_EXIST,
    AOR_DB_INTERNAL_ERROR
};

#define MAX_ACCOUNT_STR  16
#define MAX_PASSWORD_STR 16
#define MAX_EMAIL_STR 64

class AccountMgr
{
    private:
        AccountMgr();
        ~AccountMgr();

    public:
        static AccountMgr* Instance();

        static AccountOpResult CreateAccount(std::string username, std::string password);
        static AccountOpResult DeleteAccount(uint32 accid);
        static AccountOpResult ChangeUsername(uint32 accid, std::string new_uname, std::string new_passwd);
        static AccountOpResult ChangePassword(uint32 accid, std::string new_passwd);
        static bool CheckPassword(uint32 accid, std::string passwd);

        static uint32 GetId(std::string username);
        static AccountTypes GetSecurity(uint32 acc_id);
        static bool GetName(uint32 acc_id, std::string& name);
        static uint32 GetCharactersCount(uint32 acc_id);
        static std::string CalculateShaPassHash(std::string& name, std::string& password);
};

#define sAccountMgr AccountMgr::Instance()

#endif
