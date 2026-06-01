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

#ifndef GAMETIME_H
#define GAMETIME_H

#include "Define.h"

#include <chrono>

/**
 * @brief GameTime namespace provides time-related functions for the game server
 */
namespace GameTime
{

    /**
     * @brief Get the server start time
     * @return Server start time as Unix timestamp
     */
    time_t GetStartTime();

    /**
     * @brief Get the current server time (Unix timestamp in seconds)
     * @return Current server time as Unix timestamp
     */
    time_t GetGameTime();

    /**
     * @brief Get milliseconds since server start
     * @return Milliseconds elapsed since server start
     */
    uint32 GetGameTimeMS();

    /**
     * @brief Get current chrono system_clock time point
     * @return Current system_clock time point
     */
    std::chrono::system_clock::time_point GetGameTimeSystemPoint();

    /**
     * @brief Get current chrono steady_clock time point
     * @return Current steady_clock time point
     */
    std::chrono::steady_clock::time_point GetGameTimeSteadyPoint();

    /**
     * @brief Get server uptime in seconds
     * @return Server uptime in seconds
     */
    uint32 GetUptime();

    /**
     * @brief Update game timers
     */
    void UpdateGameTimers();
}

#endif
