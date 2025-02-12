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

#include "AccountMgr.h"
#include "Database/DatabaseEnv.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Policies/Singleton.h"
#include "Util.h"
#include "Auth/Sha1.h"

extern DatabaseType LoginDatabase;

INSTANTIATE_SINGLETON_1(AccountMgr);

/**
 * The AccountMgr constructor
 */
AccountMgr::AccountMgr()
{}

/**
 * The AccountMgr destructor
 */
AccountMgr::~AccountMgr()
{}

/**
 * It creates an account
 *
 * @param username The username of the account to create.
 * @param password The password you want to set for the account.
 *
 * @return AOR_OK
 */
AccountOpResult AccountMgr::CreateAccount(std::string username, std::string password)
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
    {
        return AOR_NAME_TOO_LONG;                            // username's too long
    }

    if (utf8length(password) > MAX_PASSWORD_STR)
    {
        return AOR_PASS_TOO_LONG;                            // password too long
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(password);

    if (GetId(username))
    {
        {
            return AOR_NAME_ALREADY_EXIST;                   // username does already exist
        }
    }

    if (!LoginDatabase.PExecute("INSERT INTO `account` (`username`,`sha_pass_hash`,`joindate`) VALUES ('%s','%s',NOW())", username.c_str(), CalculateShaPassHash(username, password).c_str()))
    {
        return AOR_DB_INTERNAL_ERROR;                        // unexpected error
    }
    LoginDatabase.Execute("INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`) SELECT `realmlist`.`id`, `account`.`id`, 0 FROM `realmlist`,`account` LEFT JOIN `realmcharacters` ON `acctid`=`account`.`id` WHERE `acctid` IS NULL");

    return AOR_OK;                                           // everything's fine
}

/**
 * It creates an account
 *
 * @param username The username of the account to create.
 * @param password The password you want to set for the account.
 * @param expansion 0 = Classic, 1 = TBC, 2 = WOTLK, 3 = Cataclysm
 *
 * @return AOR_OK
 */
AccountOpResult AccountMgr::CreateAccount(std::string username, std::string password, uint32 expansion)
{
    if (utf8length(username) > MAX_ACCOUNT_STR)
    {
        return AOR_NAME_TOO_LONG;                           // username's too long
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(password);

    if (GetId(username))
    {
        return AOR_NAME_ALREADY_EXIST;                       // username does already exist
    }

    if (!LoginDatabase.PExecute("INSERT INTO `account`(`username`,`sha_pass_hash`,`joindate`,`expansion`) VALUES('%s','%s',NOW(),'%u')", username.c_str(), CalculateShaPassHash(username, password).c_str(), expansion))
    {
        return AOR_DB_INTERNAL_ERROR;                       // unexpected error
    }
    LoginDatabase.Execute("INSERT INTO `realmcharacters` (`realmid`, `acctid`, `numchars`) SELECT `realmlist`.`id`, `account`.`id`, 0 FROM `realmlist`,`account` LEFT JOIN `realmcharacters` ON `acctid`=`account`.`id` WHERE `acctid` IS NULL");

    return AOR_OK;                                          // everything's fine
}

/**
 * It deletes an account from the database
 *
 * @param accid The account ID of the account to delete.
 *
 * @return AOR_OK
 */
AccountOpResult AccountMgr::DeleteAccount(uint32 accid)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u'", accid);
    if (!result)
    {
        return AOR_NAME_NOT_EXIST;                           // account doesn't exist
    }
    delete result;

    // existing characters list
    result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account`='%u'", accid);
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guidlo = fields[0].GetUInt32();
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidlo);

            // kick if player currently
            sObjectAccessor.KickPlayer(guid);
            Player::DeleteFromDB(guid, accid, false);       // no need to update realm characters
        }
        while (result->NextRow());

        delete result;
    }

    // table realm specific but common for all characters of account for realm
    CharacterDatabase.PExecute("DELETE FROM `character_tutorial` WHERE `account` = '%u'", accid);

    LoginDatabase.BeginTransaction();

    bool res =
        LoginDatabase.PExecute("DELETE FROM `account` WHERE `id`='%u'", accid) &&
        LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`='%u'", accid);

    LoginDatabase.CommitTransaction();

    if (!res)
    {
        return AOR_DB_INTERNAL_ERROR;                        // unexpected error;
    }

    return AOR_OK;
}

/**
 * It changes the username and password of an account
 *
 * @param accid The account ID of the account you want to change the username of.
 * @param new_uname The new username
 * @param new_passwd The new password for the account.
 *
 * @return AOR_OK
 */
AccountOpResult AccountMgr::ChangeUsername(uint32 accid, std::string new_uname, std::string new_passwd)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u'", accid);
    if (!result)
    {
        return AOR_NAME_NOT_EXIST;                           // account doesn't exist
    }
    delete result;

    if (utf8length(new_uname) > MAX_ACCOUNT_STR)
    {
        return AOR_NAME_TOO_LONG;
    }

    if (utf8length(new_passwd) > MAX_PASSWORD_STR)
    {
        return AOR_PASS_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(new_uname);
    Utf8ToUpperOnlyLatin(new_passwd);

    std::string safe_new_uname = new_uname;
    LoginDatabase.escape_string(safe_new_uname);

    if (!LoginDatabase.PExecute("UPDATE `account` SET `v`='0',`s`='0',`username`='%s',`sha_pass_hash`='%s' WHERE `id`='%u'", safe_new_uname.c_str(),
                                CalculateShaPassHash(new_uname, new_passwd).c_str(), accid))
                                {
                                    return AOR_DB_INTERNAL_ERROR;                        // unexpected error
                                }

    return AOR_OK;
}

/**
 * It takes a username and password, and updates the database with the new password
 *
 * @param accid The account ID of the account you want to change the password of.
 * @param new_passwd The new password to set for the account.
 *
 * @return AOR_OK
 */
AccountOpResult AccountMgr::ChangePassword(uint32 accid, std::string new_passwd)
{
    std::string username;

    if (!GetName(accid, username))
    {
        return AOR_NAME_NOT_EXIST;                           // account doesn't exist
    }

    if (utf8length(new_passwd) > MAX_PASSWORD_STR)
    {
        return AOR_PASS_TOO_LONG;
    }

    Utf8ToUpperOnlyLatin(username);
    Utf8ToUpperOnlyLatin(new_passwd);

    // also reset s and v to force update at next realmd login
    if (!LoginDatabase.PExecute("UPDATE `account` SET `v`='0', `s`='0', `sha_pass_hash`='%s' WHERE `id`='%u'",
                                CalculateShaPassHash(username, new_passwd).c_str(), accid))
                                {
                                    return AOR_DB_INTERNAL_ERROR;                        // unexpected error
                                }

    return AOR_OK;
}

/**
 * It returns the account ID of the account with the given username.
 *
 * @param username The username of the account you want to get the ID of.
 *
 * @return The account id of the account with the username that was passed in.
 */
uint32 AccountMgr::GetId(std::string username)
{
    LoginDatabase.escape_string(username);
    QueryResult* result = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", username.c_str());
    if (!result)
    {
        return 0;
    }
    else
    {
        uint32 id = (*result)[0].GetUInt32();
        delete result;
        return id;
    }
}

/**
 * It returns the security level of the account with the given account ID
 *
 * @param acc_id The account ID of the account you want to get the security level of.
 *
 * @return The security level of the account.
 */
AccountTypes AccountMgr::GetSecurity(uint32 acc_id)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT `gmlevel` FROM `account` WHERE `id` = '%u'", acc_id);
    if (result)
    {
        AccountTypes sec = AccountTypes((*result)[0].GetInt32());
        delete result;
        return sec;
    }

    return SEC_PLAYER;
}

/**
 * It gets the account name from the database
 *
 * @param acc_id The account ID of the account you want to get the name of.
 * @param name The name of the account to be checked.
 *
 * @return The name of the account.
 */
bool AccountMgr::GetName(uint32 acc_id, std::string& name)
{
    QueryResult* result = LoginDatabase.PQuery("SELECT `username` FROM `account` WHERE `id` = '%u'", acc_id);
    if (result)
    {
        name = (*result)[0].GetCppString();
        delete result;
        return true;
    }

    return false;
}

/**
 * It returns the number of characters on an account.
 *
 * @param acc_id The account ID of the account you want to check.
 *
 * @return The number of characters on the account.
 */
uint32 AccountMgr::GetCharactersCount(uint32 acc_id)
{
    // check character count
    QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", acc_id);
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

/**
 * It takes a username and password, and returns true if the password is correct
 *
 * @param accid The account ID of the account you want to check the password for.
 * @param passwd The password that the user entered.
 *
 * @return The account id of the account that is being logged in.
 */
bool AccountMgr::CheckPassword(uint32 accid, std::string passwd)
{
    std::string username;
    if (!GetName(accid, username))
    {
        return false;
    }

    Utf8ToUpperOnlyLatin(passwd);
    Utf8ToUpperOnlyLatin(username);

    QueryResult* result = LoginDatabase.PQuery("SELECT 1 FROM `account` WHERE `id`='%u' AND `sha_pass_hash`='%s'", accid, CalculateShaPassHash(username, passwd).c_str());
    if (result)
    {
        delete result;
        return true;
    }

    return false;
}

/**
 * It takes a username and password, concatenates them with a colon, and then hashes the result with
 * SHA1
 *
 * @param name The account name
 * @param password The password you want to use for the account.
 *
 * @return The SHA1 hash of the username and password.
 */
std::string AccountMgr::CalculateShaPassHash(std::string& name, std::string& password)
{
    Sha1Hash sha;
    sha.Initialize();
    sha.UpdateData(name);
    sha.UpdateData(":");
    sha.UpdateData(password);
    sha.Finalize();

    std::string encoded;
    hexEncodeByteArray(sha.GetDigest(), sha.GetLength(), encoded);

    return encoded;
}
