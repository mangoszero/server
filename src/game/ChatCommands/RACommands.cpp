/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file RACommands.cpp
 * @brief Implementation of remote administration (RA) chat commands.
 *
 * This file contains chat command handlers for RA operations including:
 * - Remote account management
 * - Server administration via RA protocol
 * - RA-specific utilities
 */

#include "Chat.h"

/**
 * @brief Handler for HandleQuitCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleQuitCommand(char* /*args*/)
{
    // processed in RASocket
    SendSysMessage(LANG_QUIT_WRONG_USE_ERROR);
    return true;
}