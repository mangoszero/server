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
 * @file AhServiceCommands.cpp
 * @brief mangosd CLI commands for the AH subprocess service.
 *
 * Provides the "ah console show|hide" command which sends an IPC_CONSOLE
 * frame to the running ah-service child process, toggling its console
 * window visibility (Windows) or logging the request (POSIX).
 *
 * Usage (mangosd console):
 *   ah console show    -- make the ah-service console window visible
 *   ah console hide    -- hide the ah-service console window
 */

#include "Chat.h"
#include "World.h"
#include "WorkerSupervisor.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"

// ---------------------------------------------------------------------------
// Helper: check that the supervisor is live and connected.
// Must be called from a ChatHandler member (has access to protected members).
// Returns true if the channel is ready.
// ---------------------------------------------------------------------------

static bool IsAhServiceConnected(WorkerSupervisor* sv)
{
    return sv != NULL && sv->Channel().Connected();
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

/**
 * @brief "ah console show" -- request the child to show its console window.
 */
bool ChatHandler::HandleAhServiceConsoleShowCommand(char* /*args*/)
{
    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (!IsAhServiceConnected(sv))
    {
        SendSysMessage("AH service is not running.");
        SetSentErrorMessage(true);
        return false;
    }

    IpcMessage msg;
    msg.op = IPC_CONSOLE;
    msg.body << static_cast<uint8>(1);
    sv->Channel().SendFrame(msg);
    SendSysMessage("AH service console show requested.");
    return true;
}

/**
 * @brief "ah console hide" -- request the child to hide its console window.
 */
bool ChatHandler::HandleAhServiceConsoleHideCommand(char* /*args*/)
{
    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (!IsAhServiceConnected(sv))
    {
        SendSysMessage("AH service is not running.");
        SetSentErrorMessage(true);
        return false;
    }

    IpcMessage msg;
    msg.op = IPC_CONSOLE;
    msg.body << static_cast<uint8>(0);
    sv->Channel().SendFrame(msg);
    SendSysMessage("AH service console hide requested.");
    return true;
}
