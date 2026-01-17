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

#include "Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Language.h"

 /**********************************************************************
     CommandTable : selectCommandTable
 /***********************************************************************/

bool ChatHandler::HandleSelectPlayerCommand(char* args)
{
    if (!*args)
    {
        SendSysMessage("Usage: .select player <player_name>");
        SetSentErrorMessage(true);
        return false;
    }

    std::string playerName = ExtractPlayerNameFromLink(&args);
    if (playerName.empty())
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // Normalize player name
    normalizePlayerName(playerName);

    // Try to find online player first
    Player* target = sObjectMgr.GetPlayer(playerName.c_str());
    ObjectGuid targetGuid;

    if (target)
    {
        targetGuid = target->GetObjectGuid();
    }
    else
    {
        // Try to find offline player
        targetGuid = sObjectMgr.GetPlayerGuidByName(playerName);
        if (!targetGuid)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }

    // Store selection for console
    if (!m_session)
    {
        uint32 accountId = GetAccountId();
        m_consoleSelectedPlayers[accountId] = targetGuid;

        PSendSysMessage("Selected player: %s (GUID: %u)", playerName.c_str(), targetGuid.GetCounter());
    }
    else
    {
        SendSysMessage("This command is intended for console use. In-game, use target selection instead.");
    }

    return true;
}

bool ChatHandler::HandleSelectClearCommand(char* /*args*/)
{
    if (!m_session)
    {
        uint32 accountId = GetAccountId();
        auto itr = m_consoleSelectedPlayers.find(accountId);

        if (itr != m_consoleSelectedPlayers.end())
        {
            m_consoleSelectedPlayers.erase(itr);
            SendSysMessage("Console player selection cleared.");
        }
        else
        {
            SendSysMessage("No player currently selected.");
        }
    }
    else
    {
        SendSysMessage("This command is intended for console use.");
    }
    return true;

}
