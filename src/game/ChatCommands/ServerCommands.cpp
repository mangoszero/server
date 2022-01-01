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
#include "Config.h"
#include "SystemConfig.h"
#include "UpdateTime.h"
#include "revision.h"

 /**********************************************************************
     CommandTable : serverCommandTable
 /***********************************************************************/


bool ChatHandler::HandleServerInfoCommand(char* /*args*/)
{
    uint32 activeClientsNum = sWorld.GetActiveSessionCount();
    uint32 queuedClientsNum = sWorld.GetQueuedSessionCount();
    uint32 maxActiveClientsNum = sWorld.GetMaxActiveSessionCount();
    uint32 maxQueuedClientsNum = sWorld.GetMaxQueuedSessionCount();
    std::string str = secsToTimeString(sWorld.GetUptime());
    uint32 updateTime = sWorldUpdateTime.GetLastUpdateTime();

    char const* full;
    full = REVISION_NR;
    SendSysMessage(full);

    if (sScriptMgr.IsScriptLibraryLoaded())
    {
        char const* ver = sScriptMgr.GetScriptLibraryVersion();
        if (ver && *ver)
        {
            PSendSysMessage(LANG_USING_SCRIPT_LIB, ver);
        }
        else
        {
            SendSysMessage(LANG_USING_SCRIPT_LIB_UNKNOWN);
        }
    }
    else
    {
        SendSysMessage(LANG_USING_SCRIPT_LIB_NONE);
    }

    PSendSysMessage(LANG_USING_WORLD_DB, sWorld.GetDBVersion());
    PSendSysMessage(LANG_CONNECTED_USERS, activeClientsNum, maxActiveClientsNum, queuedClientsNum, maxQueuedClientsNum);
    PSendSysMessage(LANG_UPTIME, str.c_str());
    PSendSysMessage("World Delay: %u", updateTime); // ToDo: move to language string

    return true;
}

/// Display the 'Message of the day' for the realm
bool ChatHandler::HandleServerMotdCommand(char* /*args*/)
{
    PSendSysMessage(LANG_MOTD_CURRENT, sWorld.GetMotd());
    return true;
}

bool ChatHandler::HandleServerShutDownCancelCommand(char* /*args*/)
{
    sWorld.ShutdownCancel();
    return true;
}

bool ChatHandler::HandleServerShutDownCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(NULL, "");

    int32 time = atoi(timeStr);

    // Prevent interpret wrong arg value as 0 secs shutdown time
    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        // Handle atoi() errors
        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        // Exit code should be in range of 0-125, 126-255 is used
        // in many shells for their own return codes and code > 255
        // is not supported in many others
        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_STOP, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_STOP, SHUTDOWN_EXIT_CODE);
    }

    return true;
}

bool ChatHandler::HandleServerRestartCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(NULL, "");

    int32 time = atoi(timeStr);

    //  Prevent interpret wrong arg value as 0 secs shutdown time
    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        // Handle atoi() errors
        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        // Exit code should be in range of 0-125, 126-255 is used
        // in many shells for their own return codes and code > 255
        // is not supported in many others
        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_RESTART, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_RESTART, RESTART_EXIT_CODE);
    }

    return true;
}

bool ChatHandler::HandleServerIdleRestartCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(NULL, "");

    int32 time = atoi(timeStr);

    //  Prevent interpret wrong arg value as 0 secs shutdown time
    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        // Handle atoi() errors
        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        // Exit code should be in range of 0-125, 126-255 is used
        // in many shells for their own return codes and code > 255
        // is not supported in many others
        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, SHUTDOWN_EXIT_CODE);
    }

    return true;
}

bool ChatHandler::HandleServerIdleShutDownCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    char* timeStr = strtok((char*)args, " ");
    char* exitCodeStr = strtok(NULL, "");

    int32 time = atoi(timeStr);

    //  Prevent interpret wrong arg value as 0 secs shutdown time
    if ((time == 0 && (timeStr[0] != '0' || timeStr[1] != '\0')) || time < 0)
    {
        return false;
    }

    if (exitCodeStr)
    {
        int32 exitCode = atoi(exitCodeStr);

        // Handle atoi() errors
        if (exitCode == 0 && (exitCodeStr[0] != '0' || exitCodeStr[1] != '\0'))
        {
            return false;
        }

        // Exit code should be in range of 0-125, 126-255 is used
        // in many shells for their own return codes and code > 255
        // is not supported in many others
        if (exitCode < 0 || exitCode > 125)
        {
            return false;
        }

        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, exitCode);
    }
    else
    {
        sWorld.ShutdownServ(time, SHUTDOWN_MASK_IDLE, RESTART_EXIT_CODE);
    }

    return true;
}

/// Exit the realm
bool ChatHandler::HandleServerExitCommand(char* /*args*/)
{
    SendSysMessage(LANG_COMMAND_EXIT);
    World::StopNow(SHUTDOWN_EXIT_CODE);
    return true;
}

/// Set the filters of logging
bool ChatHandler::HandleServerLogFilterCommand(char* args)
{
    if (!*args)
    {
        SendSysMessage(LANG_LOG_FILTERS_STATE_HEADER);
        for (int i = 0; i < LOG_FILTER_COUNT; ++i)
            if (*logFilterData[i].name)
            {
                PSendSysMessage("  %-20s = %s", logFilterData[i].name, GetOnOffStr(sLog.HasLogFilter(1 << i)));
            }
        return true;
    }

    char* filtername = ExtractLiteralArg(&args);
    if (!filtername)
    {
        return false;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (strncmp(filtername, "all", 4) == 0)
    {
        sLog.SetLogFilter(LogFilters(0xFFFFFFFF), value);
        PSendSysMessage(LANG_ALL_LOG_FILTERS_SET_TO_S, GetOnOffStr(value));
        return true;
    }

    for (int i = 0; i < LOG_FILTER_COUNT; ++i)
    {
        if (!*logFilterData[i].name)
        {
            continue;
        }

        if (!strncmp(filtername, logFilterData[i].name, strlen(filtername)))
        {
            sLog.SetLogFilter(LogFilters(1 << i), value);
            PSendSysMessage("  %-20s = %s", logFilterData[i].name, GetOnOffStr(value));
            return true;
        }
    }

    return false;
}

/// Set the level of logging
bool ChatHandler::HandleServerLogLevelCommand(char* args)
{
    if (!*args)
    {
        PSendSysMessage("Log level: %u", sLog.GetLogLevel());
        return true;
    }

    sLog.SetLogLevel(args);
    return true;
}

/// Triggering corpses expire check in world
bool ChatHandler::HandleServerCorpsesCommand(char* /*args*/)
{
    sObjectAccessor.RemoveOldCorpses();
    return true;
}

bool ChatHandler::HandleServerResetAllRaidCommand(char* args)
{
    PSendSysMessage("Global raid instances reset, all players in raid instances will be teleported to homebind!");
    sMapPersistentStateMgr.GetScheduler().ResetAllRaid();
    return true;
}

/// Define the 'Message of the day' for the realm
bool ChatHandler::HandleServerSetMotdCommand(char* args)
{
    sWorld.SetMotd(args);
    PSendSysMessage(LANG_MOTD_NEW, args);
    return true;
}

bool ChatHandler::HandleServerPLimitCommand(char* args)
{
    if (*args)
    {
        char* param = ExtractLiteralArg(&args);
        if (!param)
        {
            return false;
        }

        int l = strlen(param);

        int val;
        if (strncmp(param, "player", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_PLAYER);
        }
        else if (strncmp(param, "moderator", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_MODERATOR);
        }
        else if (strncmp(param, "gamemaster", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_GAMEMASTER);
        }
        else if (strncmp(param, "administrator", l) == 0)
        {
            sWorld.SetPlayerLimit(-SEC_ADMINISTRATOR);
        }
        else if (strncmp(param, "reset", l) == 0)
        {
            sWorld.SetPlayerLimit(sConfig.GetIntDefault("PlayerLimit", DEFAULT_PLAYER_LIMIT));
        }
        else if (ExtractInt32(&param, val))
        {
            if (val < -SEC_ADMINISTRATOR)
            {
                val = -SEC_ADMINISTRATOR;
            }

            sWorld.SetPlayerLimit(val);
        }
        else
        {
            return false;
        }

        // kick all low security level players
        if (sWorld.GetPlayerAmountLimit() > SEC_PLAYER)
        {
            sWorld.KickAllLess(sWorld.GetPlayerSecurityLimit());
        }
    }

    uint32 pLimit = sWorld.GetPlayerAmountLimit();
    AccountTypes allowedAccountType = sWorld.GetPlayerSecurityLimit();
    char const* secName;
    switch (allowedAccountType)
    {
        case SEC_PLAYER:        secName = "Player";        break;
        case SEC_MODERATOR:     secName = "Moderator";     break;
        case SEC_GAMEMASTER:    secName = "Gamemaster";    break;
        case SEC_ADMINISTRATOR: secName = "Administrator"; break;
        default:                secName = "<unknown>";     break;
    }

    PSendSysMessage("Player limits: amount %u, min. security level %s.", pLimit, secName);

    return true;
}
