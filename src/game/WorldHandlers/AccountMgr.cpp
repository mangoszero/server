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

#include "AccountMgr.h"
#include "Database/DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Util.h"
#include "Auth/Sha1.h"

extern DatabaseType LoginDatabase;

AccountMgr::AccountMgr() { }

AccountMgr::~AccountMgr() { }

AccountMgr* AccountMgr::Instance()
{
    static AccountMgr Instance;
    return &Instance;
}

AccountOpResult AccountMgr::CreateAccount(std::string username, std::string password)
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
    {
        return AccountOpResult::AOR_NAME_TOO_LONG;  // username's too long
    }

    if (utf8length(password) > MAX_PASSWORD_STR)
    {
        return AccountOpResult::AOR_PASS_TOO_LONG;  // password too long
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(password);

    if (GetId(username))
    {
        return AccountOpResult::AOR_NAME_ALREADY_EXIST; // username does already exist
    }

    if (!LoginDatabase.PExecute("INSERT INTO `account` (`username`,`sha_pass_hash`,`joindate`) VALUES ('%s','%s', NOW())", username.c_str(), CalculateShaPassHash(username, password).c_str()))
    {
        return AccountOpResult::AOR_DB_INTERNAL_ERROR;  // unexpected error
    }

    LoginDatabase.Execute("INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`) SELECT `realmlist`.`id`, `account`.`id`, 0 FROM `realmlist`,`account` LEFT JOIN `realmcharacters` ON `acctid`=`account`.`id` WHERE `acctid` IS NULL");

    return AccountOpResult::AOR_OK; // everything's fine
}

AccountOpResult AccountMgr::DeleteAccount(uint32 accountId)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u'", accountId);
    if (!result)
    {
        return AccountOpResult::AOR_NAME_NOT_EXIST; // account doesn't exist
    }
    delete result;

    // existing characters list
    result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account`='%u'", accountId);
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guidlo = fields[0].GetUInt32();
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidlo);

            // kick if player currently
            sObjectAccessor.KickPlayer(guid);
            Player::DeleteFromDB(guid, accountId, false);       // no need to update realm characters
        }
        while (result->NextRow());

        delete result;
    }

    // table realm specific but common for all characters of account for realm
    CharacterDatabase.PExecute("DELETE FROM `character_tutorial` WHERE `account` = '%u'", accountId);

    LoginDatabase.BeginTransaction();

    bool res =
        LoginDatabase.PExecute("DELETE FROM `account` WHERE `id`='%u'", accountId) &&
        LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`='%u'", accountId);

    LoginDatabase.CommitTransaction();

    if (!res)
    {
        return AccountOpResult::AOR_DB_INTERNAL_ERROR;  // unexpected error;
    }

    return AccountOpResult::AOR_OK;
}

AccountOpResult AccountMgr::ChangeUsername(uint32 accountId, std::string newUsername, std::string newPassword)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u'", accountId);
    if (!result)
    {
        return AccountOpResult::AOR_NAME_NOT_EXIST; // account doesn't exist
    }
    delete result;

    if (utf8length(newUsername) > MAX_ACCOUNT_STR)
    {
        return AccountOpResult::AOR_NAME_TOO_LONG;
    }

    if (utf8length(newPassword) > MAX_PASSWORD_STR)
    {
        return AccountOpResult::AOR_PASS_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(newUsername);
    Utf8ToUpperOnlyLatin(newPassword);

    std::string safe_newUsername = newUsername;
    LoginDatabase.escape_string(safe_newUsername);

    if (!LoginDatabase.PExecute("UPDATE `account` SET `v`='0',`s`='0',`username`='%s',`sha_pass_hash`='%s' WHERE `id`='%u'", safe_newUsername.c_str(),
        CalculateShaPassHash(newUsername, newPassword).c_str(), accountId))
    {
        return AccountOpResult::AOR_DB_INTERNAL_ERROR;  // unexpected error
    }

    return AccountOpResult::AOR_OK;
}

AccountOpResult AccountMgr::ChangePassword(uint32 accountId, std::string newPassword)
{
    std::string username;

    if (!GetName(accountId, username))
    {
        return AccountOpResult::AOR_NAME_NOT_EXIST; // account doesn't exist
    }

    if (utf8length(newPassword) > MAX_PASSWORD_STR)
    {
        return AccountOpResult::AOR_PASS_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(newPassword);

    // also reset s and v to force update at next realmd login
    if (!LoginDatabase.PExecute("UPDATE `account` SET `v`='0', `s`='0', `sha_pass_hash`='%s' WHERE `id`='%u'",
        CalculateShaPassHash(username, newPassword).c_str(), accountId))
    {
        return AccountOpResult::AOR_DB_INTERNAL_ERROR;  // unexpected error
    }

    return AccountOpResult::AOR_OK;
}

uint32 AccountMgr::GetId(std::string username)
{
    LoginDatabase.escape_string(username);
    QueryResult* result = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", username.c_str());

    return (result) ? (*result)[0].GetUInt32() : 0;
}

AccountTypes AccountMgr::GetSecurity(uint32 accountId)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT `gmlevel` FROM `account` WHERE `id` = '%u'", accountId);
    if (result)
    {
        AccountTypes sec = AccountTypes((*result)[0].GetInt32());
        delete result;
        return sec;
    }

    return SEC_PLAYER;
}

bool AccountMgr::GetName(uint32 accountId, std::string& username)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT `username` FROM `account` WHERE `id` = '%u'", accountId);
    if (result)
    {
        username = (*result)[0].GetCppString();
        delete result;
        return true;
    }

    return false;
}

uint32 AccountMgr::GetCharactersCount(uint32 accountId)
{
    // check character count
    QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);
    if (result)
    {
        Field* fields = result->Fetch();
        uint32 charcount = fields[0].GetUInt32();
        delete result;
        return charcount;
    }
    else
    {
        return 0;
    }
}

bool AccountMgr::CheckPassword(uint32 accountId, std::string password)
{
    std::string username;
    if (!GetName(accountId, username))
    {
        return false;
    }

    Utf8ToUpperOnlyLatin(password);
    Utf8ToUpperOnlyLatin(username);

    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u' AND `sha_pass_hash`='%s'", accountId, CalculateShaPassHash(username, password).c_str());
    if (result)
    {
        delete result;
        return true;
    }

    return false;
}

std::string AccountMgr::CalculateShaPassHash(std::string& username, std::string& password)
{
    Sha1Hash sha;
    sha.Initialize();
    sha.UpdateData(username);
    sha.UpdateData(":");
    sha.UpdateData(password);
    sha.Finalize();

    std::string encoded;
    hexEncodeByteArray(sha.GetDigest(), sha.GetLength(), encoded);

    return encoded;
}
