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
 * @file GameTime.cpp
 * @brief Server game time tracking and management
 *
 * This file implements the GameTime namespace which provides centralized
 * time tracking for the game server. It maintains various time representations
 * used throughout the codebase for event scheduling, cooldowns, and uptime tracking.
 *
 * Time representations:
 * - StartTime: When the server process began
 * - GameTime: Current Unix timestamp (seconds since epoch)
 * - GameMSTime: Current time in milliseconds (for precise intervals)
 * - System/Steady clock points: For high-resolution timing
 *
 * All game time values are updated together in UpdateGameTimers() which is
 * called periodically (typically once per server tick).
 *
 * @see GameTime for the namespace interface
 * @see UpdateGameTimers() for the update mechanism
 */

#include "GameTime.h"
#include "Timer.h"

namespace GameTime
{

    /**
     * @var StartTime
     * @brief Server process start time (Unix timestamp)
     *
     * Set once at startup and never changes. Used for uptime calculations.
     */
    time_t const StartTime = time(nullptr);

    /**
     * @var GameTime
     * @brief Current game time (Unix timestamp in seconds)
     *
     * Updated each tick via UpdateGameTimers(). Used for most game
     * time calculations like spell cooldowns, aura durations, etc.
     */
    time_t GameTime = 0;

    /**
     * @var GameMSTime
     * @brief Current game time in milliseconds
     *
     * Updated each tick. Used for high-precision timing needs.
     * @note This wraps around after ~49 days (uint32 milliseconds)
     */
    uint32 GameMSTime = 0;

    /**
     * @var GameTimeSystemPoint
     * @brief System clock time point for wall-clock time
     *
     * Represents real-world time. Can be affected by system time changes.
     */
    std::chrono::system_clock::time_point GameTimeSystemPoint = std::chrono::system_clock::time_point::min();

    /**
     * @var GameTimeSteadyPoint
     * @brief Steady clock time point for monotonic timing
     *
     * Guaranteed to never go backwards. Best for measuring intervals.
     */
    std::chrono::steady_clock::time_point GameTimeSteadyPoint = std::chrono::steady_clock::time_point::min();

    /**
     * @brief Get server start time
     * @return Unix timestamp when server started
     */
    time_t GetStartTime()
    {
        return StartTime;
    }

    /**
     * @brief Get current game time
     * @return Current Unix timestamp (seconds)
     *
     * @see UpdateGameTimers() for how this is updated
     */
    time_t GetGameTime()
    {
        return GameTime;
    }

    /**
     * @brief Get current game time in milliseconds
     * @return Time in milliseconds (wraps after ~49 days)
     *
     * Uses getMSTime() which provides millisecond precision.
     */
    uint32 GetGameTimeMS()
    {
        return GameMSTime;
    }

    /**
     * @brief Get system clock time point
     * @return Current system clock time
     *
     * Suitable for wall-clock time operations. Not monotonic.
     */
    std::chrono::system_clock::time_point GetGameTimeSystemPoint()
    {
        return GameTimeSystemPoint;
    }

    /**
     * @brief Get steady clock time point
     * @return Current steady clock time
     *
     * Suitable for measuring intervals. Never goes backwards.
     */
    std::chrono::steady_clock::time_point GetGameTimeSteadyPoint()
    {
        return GameTimeSteadyPoint;
    }

    /**
     * @brief Get server uptime in seconds
     * @return Seconds since server started
     */
    uint32 GetUptime()
    {
        return uint32(GameTime - StartTime);
    }

    /**
     * @brief Update all game time values
     *
     * Called periodically (typically once per server tick) to refresh
     * all time values simultaneously. Ensures consistent time across
     * all game systems during a single tick.
     *
     * Updates:
     * - GameTime (seconds)
     * - GameMSTime (milliseconds)
     * - System clock point
     * - Steady clock point
     */
    void UpdateGameTimers()
    {
        GameTime = time(nullptr);
        GameMSTime = getMSTime();
        GameTimeSystemPoint = std::chrono::system_clock::now();
        GameTimeSteadyPoint = std::chrono::steady_clock::now();
    }
}
