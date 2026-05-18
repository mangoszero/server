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

#ifndef UPDATETIME_H
#define UPDATETIME_H

#include "Common.h"
#include "Timer.h"

#include <array>
#include <string>

#define AVG_DIFF_COUNT 500

/**
 * @brief UpdateTime class tracks and calculates update time statistics
 */
class UpdateTime
{
    using DiffTableArray = std::array<uint32, AVG_DIFF_COUNT>;

public:
    /**
     * @brief Get average update time
     * @return Average update time in milliseconds
     */
    uint32 GetAverageUpdateTime() const;

    /**
     * @brief Get time-weighted average update time
     * @return Time-weighted average update time in milliseconds
     */
    uint32 GetTimeWeightedAverageUpdateTime() const;

    /**
     * @brief Get maximum update time recorded
     * @return Maximum update time in milliseconds
     */
    uint32 GetMaxUpdateTime() const;

    /**
     * @brief Get maximum update time of current table
     * @return Maximum update time of current table in milliseconds
     */
    uint32 GetMaxUpdatTimeOfCurrentTable() const;

    /**
     * @brief Get last update time
     * @return Last update time in milliseconds
     */
    uint32 GetLastUpdateTime() const;

    /**
     * @brief Update statistics with time difference
     * @param diff Time difference in milliseconds
     */
    void UpdateWithDiff(uint32 diff);

    /**
     * @brief Reset update time recording
     */
    void RecordUpdateTimeReset();

protected:
    /**
     * @brief Constructor
     */
    UpdateTime();

    /**
     * @brief Record update time duration
     * @param text Description text for the record
     * @param minUpdateTime Minimum update time threshold
     */
    void _RecordUpdateTimeDuration(std::string const& text, uint32 minUpdateTime);

private:
    DiffTableArray _updateTimeDataTable; ///< Table of update time data
    uint32 _averageUpdateTime; ///< Average update time
    uint32 _totalUpdateTime; ///< Total update time
    uint32 _updateTimeTableIndex; ///< Current index in update time table
    uint32 _maxUpdateTime; ///< Maximum update time recorded
    uint32 _maxUpdateTimeOfLastTable; ///< Maximum update time of last table
    uint32 _maxUpdateTimeOfCurrentTable; ///< Maximum update time of current table

    uint32 _recordedTime; ///< Recorded time
};

/**
 * @brief WorldUpdateTime extends UpdateTime for world-specific update time tracking
 */
class WorldUpdateTime : public UpdateTime
{
public:
    /**
     * @brief Constructor
     */
    WorldUpdateTime() : UpdateTime(), _recordUpdateTimeInverval(0), _recordUpdateTimeMin(0), _lastRecordTime(0) { }

    /**
     * @brief Load configuration from config file
     */
    void LoadFromConfig();

    /**
     * @brief Set record update time interval
     * @param t Interval in milliseconds
     */
    void SetRecordUpdateTimeInterval(uint32 t);

    /**
     * @brief Record update time with game time and session count
     * @param gameTimeMs Game time in milliseconds
     * @param diff Time difference in milliseconds
     * @param sessionCount Number of active sessions
     */
    void RecordUpdateTime(uint32 gameTimeMs, uint32 diff, uint32 sessionCount);

    /**
     * @brief Record update time duration with description
     * @param text Description text for the record
     */
    void RecordUpdateTimeDuration(std::string const& text);

private:
    uint32 _recordUpdateTimeInverval; ///< Record update time interval
    uint32 _recordUpdateTimeMin; ///< Minimum record update time
    uint32 _lastRecordTime; ///< Last record time
};

/**
 * @brief Global WorldUpdateTime instance
 */
extern WorldUpdateTime sWorldUpdateTime;

#endif
