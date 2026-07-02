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
 * @file Chat.cpp
 * @brief Chat system implementation
 *
 * This file implements the chat system including:
 * - Message formatting and color codes
 * - Shift-link parsing (item, spell, quest links)
 * - Channel message routing
 * - Whisper, say, yell, emote handling
 * - GM command parsing and execution
 * - Language filtering
 *
 * The chat system supports various message types with different
 * visibility ranges and formatting requirements.
 *
 * @see ChatHandler for command handling
 * @see Channel for channel chat
 */



#include "Chat.h"
#include "Language.h"
#include "WorldSession.h"
#include "CommandMgr.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "AccountMgr.h"
#include "SpellMgr.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "AuctionHouseBot/AuctionHouseBot.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Displays the available subcommands for a command table.
 *
 * @param table The subcommand table to display.
 * @param cmd The parent command name.
 * @return true if any subcommands were shown; otherwise false.
 */
bool ChatHandler::ShowHelpForSubCommands(ChatCommand* table, char const* cmd)
{
    std::string list;
    for (uint32 i = 0; table[i].Name != NULL; ++i)
    {
        // must be available (ignore handler existence for show command with possible available subcommands
        if (!isAvailable(table[i]))
        {
            continue;
        }

        if (m_session)
        {
            list += "\n    ";
        }
        else
        {
            list += "\n\r    ";
        }

        list += table[i].Name;

        if (table[i].ChildCommands)
        {
            list += " ...";
        }
    }

    if (list.empty())
    {
        return false;
    }

    if (table == getCommandTable())
    {
        SendSysMessage(LANG_AVIABLE_CMD);
        SendSysMessage(list.c_str());
    }
    else
    {
        PSendSysMessage(LANG_SUBCMDS_LIST, cmd);
        SendSysMessage(list.c_str());
    }
    return true;
}

/**
 * @brief Displays help text and subcommands for a command path.
 *
 * @param table The command table to search.
 * @param cmd The command path to resolve.
 * @return true if help content was shown; otherwise false.
 */
bool ChatHandler::ShowHelpForCommand(ChatCommand* table, const char* cmd)
{
    char const* oldCmd = cmd;
    ChatCommand* command = NULL;
    ChatCommand* parentCommand = NULL;

    ChatCommand* showCommand = NULL;
    ChatCommand* childCommands = NULL;

    ChatCommandSearchResult res = FindCommand(table, cmd, command, &parentCommand);

    switch (res)
    {
        case CHAT_COMMAND_OK:
        {
            // for "" subcommand use parent command if any for subcommands list output
            if (strlen(command->Name) == 0 && parentCommand)
            {
                showCommand = parentCommand;
            }
            else
            {
                showCommand = command;
            }

            childCommands = showCommand->ChildCommands;
            break;
        }
        case CHAT_COMMAND_UNKNOWN_SUBCOMMAND:
            showCommand = command;
            childCommands = showCommand->ChildCommands;
            break;
        case CHAT_COMMAND_UNKNOWN:
            // not show command list at error in first level command find fail
            childCommands = table != getCommandTable() || strlen(oldCmd) == 0 ? table : NULL;
            command = NULL;
            break;
    }

    if (command && !command->Help.empty())
    {
        std::string helpText = command->Help;

        // Attemp to localize help text if not in CLI mode
        if (m_session)
        {
            int loc_idx = m_session->GetSessionDbLocaleIndex();
            sCommandMgr.GetCommandHelpLocaleString(command->Id, loc_idx, &helpText);
        }

        SendSysMessage(helpText.c_str());
    }

    if (childCommands)
    {
        if (ShowHelpForSubCommands(childCommands, showCommand ? showCommand->Name : ""))
        {
            return true;
        }
    }

    if (command && command->Help.empty())
    {
        SendSysMessage(LANG_NO_HELP_CMD);
    }

    return command || childCommands;
}
