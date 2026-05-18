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

/**
 * @file AccountCommands.cpp
 * @brief Implementation of account management chat commands.
 *
 * This file contains chat command handlers for account-related operations including:
 * - Account information display
 * - Password management
 * - Account locking
 * - Account creation and deletion
 * - Character listing
 * - Account properties modification (addons, GM level, password)
 */

#include "World.h"
#include "Chat.h"
#include "AccountMgr.h"
#include "Database/DatabaseEnv.h"
#include "Player.h"


/**
 * @brief Displays the current account information and access level.
 *
 * @param args Command arguments (should be empty).
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleAccountCommand(char* args)
{
    // let show subcommands at unexpected data in args
    if (*args)
    {
        return false;
    }

    AccountTypes gmlevel = GetAccessLevel();
    PSendSysMessage(LANG_ACCOUNT_LEVEL, uint32(gmlevel));
    return true;
}

/**
 * @brief Changes the account password after verifying the old password.
 *
 * This command is only available through remote administration (RA) and requires
 * the old password to be correct before the new password is accepted.
 *
 * @param args Command arguments: old_password new_password new_password_confirm.
 * @returns True if the password was changed successfully, false otherwise.
 */
bool ChatHandler::HandleAccountPasswordCommand(char* args)
{
    // allow use from RA, but not from console (not have associated account id)
    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    // allow or quoted string with possible spaces or literal without spaces
    char* old_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass = ExtractQuotedOrLiteralArg(&args);
    char* new_pass_c = ExtractQuotedOrLiteralArg(&args);

    if (!old_pass || !new_pass || !new_pass_c)
    {
        return false;
    }

    std::string password_old = old_pass;
    std::string password_new = new_pass;
    std::string password_new_c = new_pass_c;

    if (password_new != password_new_c)
    {
        SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
        SetSentErrorMessage(true);
        return false;
    }

    if (!sAccountMgr.CheckPassword(GetAccountId(), password_old))
    {
        SendSysMessage(LANG_COMMAND_WRONGOLDPASSWORD);
        SetSentErrorMessage(true);
        return false;
    }

    AccountOpResult result = sAccountMgr.ChangePassword(GetAccountId(), password_new);

    switch (result)
    {
        case AOR_OK:
            SendSysMessage(LANG_COMMAND_PASSWORD);
            break;
        case AOR_PASS_TOO_LONG:
            SendSysMessage(LANG_PASSWORD_TOO_LONG);
            SetSentErrorMessage(true);
            return false;
        case AOR_NAME_NOT_EXIST:                            // not possible case, don't want get account name for output
        default:
            SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
            SetSentErrorMessage(true);
            return false;
    }

    // OK, but avoid normal report for hide passwords, but log use command for anyone
    LogCommand(".account password *** *** ***");
    SetSentErrorMessage(true);
    return false;
}

/**
 * @brief Locks or unlocks the current account to prevent or allow logins.
 *
 * This command is only available through remote administration (RA).
 *
 * @param args Command arguments: on/off value to lock or unlock the account.
 * @returns True if the lock state was changed successfully, false otherwise.
 */
bool ChatHandler::HandleAccountLockCommand(char* args)
{
    // allow use from RA, but not from console (not have associated account id)
    if (!GetAccountId())
    {
        SendSysMessage(LANG_RA_ONLY_COMMAND);
        SetSentErrorMessage(true);
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        LoginDatabase.PExecute("UPDATE `account` SET `locked` = '1' WHERE `id` = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKLOCKED);
    }
    else
    {
        LoginDatabase.PExecute("UPDATE `account` SET `locked` = '0' WHERE `id` = '%u'", GetAccountId());
        PSendSysMessage(LANG_COMMAND_ACCLOCKUNLOCKED);
    }

    return true;
}

/**
 * @brief Displays a list of accounts currently logged in to the realm.
 *
 * @param args Command arguments: optional limit (default 100) for maximum accounts to display.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleAccountOnlineListCommand(char* args)
{
    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    ///- Get the list of accounts ID logged to the realm
    //                                                 0   1         2        3        4
    QueryResult* result = LoginDatabase.PQuery("SELECT `id`, `username`, `last_ip`, `gmlevel`, `expansion` FROM `account` WHERE `active_realm_id` = %u", realmID);

    return ShowAccountListHelper(result, &limit);
}

/**
 * @brief Deletes an account and all associated characters in the realm.
 *
 * This command can only delete accounts with lower security level than the executor.
 *
 * @param args Command arguments: account name to delete.
 * @returns True if the account was deleted successfully, false otherwise.
 */
bool ChatHandler::HandleAccountDeleteCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string account_name;
    uint32 account_id = ExtractAccountId(&args, &account_name);
    if (!account_id)
    {
        return false;
    }

    /// Commands not recommended call from chat, but support anyway
    /// can delete only for account with less security
    /// This is also reject self apply in fact
    if (HasLowerSecurityAccount(NULL, account_id, true))
    {
        return false;
    }

    AccountOpResult result = sAccountMgr.DeleteAccount(account_id);
    switch (result)
    {
        case AOR_OK:
            PSendSysMessage(LANG_ACCOUNT_DELETED, account_name.c_str());
            break;
        case AOR_NAME_NOT_EXIST:
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
        case AOR_DB_INTERNAL_ERROR:
            PSendSysMessage(LANG_ACCOUNT_NOT_DELETED_SQL_ERROR, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
        default:
            PSendSysMessage(LANG_ACCOUNT_NOT_DELETED, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
    }

    return true;
}

/**
 * @brief Creates a new account with optional expansion level.
 *
 * @param args Command arguments: account_name password [expansion_level].
 * @returns True if the account was created successfully, false otherwise.
 */
bool ChatHandler::HandleAccountCreateCommand(char* args)
{
    ///- %Parse the command line arguments
    char* szAcc = ExtractQuotedOrLiteralArg(&args);
    char* szPassword = ExtractQuotedOrLiteralArg(&args);
    if (!szAcc || !szPassword)
    {
        return false;
    }

    // normalized in accmgr.CreateAccount
    std::string account_name = szAcc;
    std::string password = szPassword;

    AccountOpResult result;
    uint32 expansion = 0;
    if (ExtractUInt32(&args, expansion))
    {
        // No point in assigning to result as it's never used on this side of the if/else branch
        sAccountMgr.CreateAccount(account_name, password, expansion);
    }
    else
    {
        result = sAccountMgr.CreateAccount(account_name, password);
        switch (result)
        {
            case AOR_OK:
                PSendSysMessage(LANG_ACCOUNT_CREATED, account_name.c_str());
                break;
            case AOR_NAME_TOO_LONG:
                SendSysMessage(LANG_ACCOUNT_TOO_LONG);
                SetSentErrorMessage(true);
                return false;
            case AOR_NAME_ALREADY_EXIST:
                SendSysMessage(LANG_ACCOUNT_ALREADY_EXIST);
                SetSentErrorMessage(true);
                return false;
            case AOR_DB_INTERNAL_ERROR:
                PSendSysMessage(LANG_ACCOUNT_NOT_CREATED_SQL_ERROR, account_name.c_str());
                SetSentErrorMessage(true);
                return false;
            default:
                PSendSysMessage(LANG_ACCOUNT_NOT_CREATED, account_name.c_str());
                SetSentErrorMessage(true);
                return false;
        }
    }
    return true;
}

/**
 * @brief Lists all characters for a specified account.
 *
 * @param args Command arguments: optional account name or target player to query their account.
 * @returns True if the character list was displayed successfully, false otherwise.
 */
bool ChatHandler::HandleAccountCharactersCommand(char* args)
{
    ///- Get the command line arguments
    std::string account_name;
    Player* target = NULL;                                  // only for triggering use targeted player account
    uint32 account_id = ExtractAccountId(&args, &account_name, &target);
    if (!account_id)
    {
        return false;
    }

    ///- Get the characters for account id
    QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`, `name`, `race`, `class`, `level` FROM `characters` WHERE `account` = %u", account_id);

    return ShowPlayerListHelper(result);
}

/**
 * @brief Sets the expansion level for an account.
 *
 * @param args Command arguments: account_name expansion_level.
 * @returns True if the expansion level was set successfully, false otherwise.
 */
bool ChatHandler::HandleAccountSetAddonCommand(char* args)
{
    ///- Get the command line arguments
    char* accountStr = ExtractOptNotLastArg(&args);

    std::string account_name;
    uint32 account_id = ExtractAccountId(&accountStr, &account_name);
    if (!account_id)
    {
        return false;
    }

    // Let set addon state only for lesser (strong) security level
    // or to self account
    if (GetAccountId() && GetAccountId() != account_id && HasLowerSecurityAccount(NULL, account_id, true))
    {
        return false;
    }

    uint32 lev;
    if (!ExtractUInt32(&args, lev))
    {
        return false;
    }

    // No SQL injection
    LoginDatabase.PExecute("UPDATE `account` SET `expansion` = '%u' WHERE `id` = '%u'", lev, account_id);
    PSendSysMessage(LANG_ACCOUNT_SETADDON, account_name.c_str(), account_id, lev);
    return true;
}

/**
 * @brief Sets the GM level (security level) for an account.
 *
 * This command can only set security levels lower than the executor's level.
 * Self-application is not allowed.
 *
 * @param args Command arguments: account_name gm_level.
 * @returns True if the GM level was set successfully, false otherwise.
 */
bool ChatHandler::HandleAccountSetGmLevelCommand(char* args)
{
    char* accountStr = ExtractOptNotLastArg(&args);

    std::string targetAccountName;
    Player* targetPlayer = NULL;
    uint32 targetAccountId = ExtractAccountId(&accountStr, &targetAccountName, &targetPlayer);
    if (!targetAccountId)
    {
        return false;
    }

    /// only target player different from self allowed
    if (GetAccountId() == targetAccountId)
    {
        return false;
    }

    int32 gm;
    if (!ExtractInt32(&args, gm))
    {
        return false;
    }

    if (gm < SEC_PLAYER || gm > SEC_ADMINISTRATOR)
    {
        SendSysMessage(LANG_BAD_VALUE);
        SetSentErrorMessage(true);
        return false;
    }

    /// can set security level only for target with less security and to less security that we have
    /// This will reject self apply by specify account name
    if (HasLowerSecurityAccount(NULL, targetAccountId, true))
    {
        return false;
    }

    /// account can't set security to same or grater level, need more power GM or console
    AccountTypes plSecurity = GetAccessLevel();
    if (AccountTypes(gm) >= plSecurity)
    {
        SendSysMessage(LANG_YOURS_SECURITY_IS_LOW);
        SetSentErrorMessage(true);
        return false;
    }

    if (targetPlayer)
    {
        ChatHandler(targetPlayer).PSendSysMessage(LANG_YOURS_SECURITY_CHANGED, GetNameLink().c_str(), gm);
        targetPlayer->GetSession()->SetSecurity(AccountTypes(gm));
    }

    PSendSysMessage(LANG_YOU_CHANGE_SECURITY, targetAccountName.c_str(), gm);
    LoginDatabase.PExecute("UPDATE `account` SET `gmlevel` = '%i' WHERE `id` = '%u'", gm, targetAccountId);

    return true;
}

/**
 * @brief Sets the password for a specified account.
 *
 * This command can only set passwords for accounts with lower security level.
 * Both passwords must match to be accepted.
 *
 * @param args Command arguments: account_name new_password new_password_confirm.
 * @returns True if the password was set successfully, false otherwise.
 */
bool ChatHandler::HandleAccountSetPasswordCommand(char* args)
{
    ///- Get the command line arguments
    std::string account_name;
    uint32 targetAccountId = ExtractAccountId(&args, &account_name);
    if (!targetAccountId)
    {
        return false;
    }

    // allow or quoted string with possible spaces or literal without spaces
    char* szPassword1 = ExtractQuotedOrLiteralArg(&args);
    char* szPassword2 = ExtractQuotedOrLiteralArg(&args);
    if (!szPassword1 || !szPassword2)
    {
        return false;
    }

    /// can set password only for target with less security
    /// This is also reject self apply in fact
    if (HasLowerSecurityAccount(NULL, targetAccountId, true))
    {
        return false;
    }

    if (strcmp(szPassword1, szPassword2))
    {
        SendSysMessage(LANG_NEW_PASSWORDS_NOT_MATCH);
        SetSentErrorMessage(true);
        return false;
    }

    AccountOpResult result = sAccountMgr.ChangePassword(targetAccountId, szPassword1);

    switch (result)
    {
        case AOR_OK:
            SendSysMessage(LANG_COMMAND_PASSWORD);
            break;
        case AOR_NAME_NOT_EXIST:
            PSendSysMessage(LANG_ACCOUNT_NOT_EXIST, account_name.c_str());
            SetSentErrorMessage(true);
            return false;
        case AOR_PASS_TOO_LONG:
            SendSysMessage(LANG_PASSWORD_TOO_LONG);
            SetSentErrorMessage(true);
            return false;
        default:
            SendSysMessage(LANG_COMMAND_NOTCHANGEPASSWORD);
            SetSentErrorMessage(true);
            return false;
    }

    // OK, but avoid normal report for hide passwords, but log use command for anyone
    char msg[100];
    snprintf(msg, 100, ".account set password %s *** ***", account_name.c_str());
    LogCommand(msg);
    SetSentErrorMessage(true);
    return false;
}
