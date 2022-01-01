/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Chat.h"
#include "Language.h"
#include "World.h"
#include "PlayerDump.h"

 /**********************************************************************
     CommandTable : commandTable
 /***********************************************************************/

bool ChatHandler::HandleHelpCommand(char* args)
{
    if (!*args)
    {
        ShowHelpForCommand(getCommandTable(), "help");
        ShowHelpForCommand(getCommandTable(), "");
    }
    else
    {
        if (!ShowHelpForCommand(getCommandTable(), args))
        {
            SendSysMessage(LANG_NO_CMD);
        }
    }

    return true;
}

bool ChatHandler::HandleCommandsCommand(char* /*args*/)
{
    ShowHelpForCommand(getCommandTable(), "");
    return true;
}

bool ChatHandler::HandleGUIDCommand(char* /*args*/)
{
    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();

    if (!guid)
    {
        SendSysMessage(LANG_NO_SELECTION);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_OBJECT_GUID, guid.GetString().c_str());
    return true;
}

bool ChatHandler::HandleLoadScriptsCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    switch (sScriptMgr.LoadScriptLibrary(args))
    {
        case SCRIPT_LOAD_OK:
            sWorld.SendWorldText(LANG_SCRIPTS_RELOADED_ANNOUNCE);
            SendSysMessage(LANG_SCRIPTS_RELOADED_OK);
            break;
        case SCRIPT_LOAD_ERR_NOT_FOUND:
            SendSysMessage(LANG_SCRIPTS_NOT_FOUND);
            break;
        case SCRIPT_LOAD_ERR_WRONG_API:
            SendSysMessage(LANG_SCRIPTS_WRONG_API);
            break;
        case SCRIPT_LOAD_ERR_OUTDATED:
            SendSysMessage(LANG_SCRIPTS_OUTDATED);
            break;
    }

    return true;
}

bool ChatHandler::HandlePDumpLoadCommand(char* args)
{
    char* file = ExtractQuotedOrLiteralArg(&args);
    if (!file)
    {
        return false;
    }

    std::string account_name;
    uint32 account_id = ExtractAccountId(&args, &account_name);
    if (!account_id)
    {
        return false;
    }

    char* name_str = ExtractLiteralArg(&args);

    uint32 lowguid = 0;
    std::string name;

    if (name_str)
    {
        name = name_str;
        // normalize the name if specified and check if it exists
        if (!normalizePlayerName(name))
        {
            PSendSysMessage(LANG_INVALID_CHARACTER_NAME);
            SetSentErrorMessage(true);
            return false;
        }

        if (ObjectMgr::CheckPlayerName(name, true) != CHAR_NAME_SUCCESS)
        {
            PSendSysMessage(LANG_INVALID_CHARACTER_NAME);
            SetSentErrorMessage(true);
            return false;
        }

        if (*args)
        {
            if (!ExtractUInt32(&args, lowguid))
            {
                return false;
            }

            if (!lowguid)
            {
                PSendSysMessage(LANG_INVALID_CHARACTER_GUID);
                SetSentErrorMessage(true);
                return false;
            }

            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, lowguid);

            if (sObjectMgr.GetPlayerAccountIdByGUID(guid))
            {
                PSendSysMessage(LANG_CHARACTER_GUID_IN_USE, lowguid);
                SetSentErrorMessage(true);
                return false;
            }
        }
    }

    switch (PlayerDumpReader().LoadDump(file, account_id, name, lowguid))
    {
        case DUMP_SUCCESS:
            PSendSysMessage(LANG_COMMAND_IMPORT_SUCCESS);
            break;
        case DUMP_FILE_OPEN_ERROR:
            PSendSysMessage(LANG_FILE_OPEN_FAIL, file);
            SetSentErrorMessage(true);
            return false;
        case DUMP_FILE_BROKEN:
            PSendSysMessage(LANG_DUMP_BROKEN, file);
            SetSentErrorMessage(true);
            return false;
        case DUMP_TOO_MANY_CHARS:
            PSendSysMessage(LANG_ACCOUNT_CHARACTER_LIST_FULL, account_name.c_str(), account_id);
            SetSentErrorMessage(true);
            return false;
        default:
            PSendSysMessage(LANG_COMMAND_IMPORT_FAILED);
            SetSentErrorMessage(true);
            return false;
    }

    return true;
}

bool ChatHandler::HandlePDumpWriteCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* file = ExtractQuotedOrLiteralArg(&args);
    if (!file)
    {
        return false;
    }

    char* p2 = ExtractLiteralArg(&args);

    uint32 lowguid;
    ObjectGuid guid;
    // character name can't start from number
    if (!ExtractUInt32(&p2, lowguid))
    {
        std::string name = ExtractPlayerNameFromLink(&p2);
        if (name.empty())
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        guid = sObjectMgr.GetPlayerGuidByName(name);
        if (!guid)
        {
            PSendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }

        lowguid = guid.GetCounter();
    }
    else
    {
        guid = ObjectGuid(HIGHGUID_PLAYER, lowguid);
    }

    if (!sObjectMgr.GetPlayerAccountIdByGUID(guid))
    {
        PSendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    switch (PlayerDumpWriter().WriteDump(file, lowguid))
    {
        case DUMP_SUCCESS:
            PSendSysMessage(LANG_COMMAND_EXPORT_SUCCESS);
            break;
        case DUMP_FILE_OPEN_ERROR:
            PSendSysMessage(LANG_FILE_OPEN_FAIL, file);
            SetSentErrorMessage(true);
            return false;
        default:
            PSendSysMessage(LANG_COMMAND_EXPORT_FAILED);
            SetSentErrorMessage(true);
            return false;
    }

    return true;
}
