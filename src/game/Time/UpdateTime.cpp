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
 * @file UpdateTime.cpp
 * @brief Server update/tick time tracking and performance monitoring
 *
 * This file implements the UpdateTime class which tracks server performance
 * by measuring the time between updates (ticks). It maintains statistics like
 * average update time, maximum update time, and historical data for analysis.
 *
 * Metrics tracked:
 * - Average update time (rolling window)
 * - Time-weighted average (weighted by duration)
 * - Maximum update time (all time and per-period)
 * - Historical data table for trend analysis
 *
 * WorldUpdateTime extends this for global server update tracking with
 * configurable logging thresholds.
 *
 * @see UpdateTime for the base metrics class
 * @see WorldUpdateTime for server-wide tracking
 */

#include "UpdateTime.h"

#include "Timer.h"
#include "Config.h"
#include "Log.h"

/**
 * @var sWorldUpdateTime
 * @brief Global server update time tracker
 *
 * Singleton instance used for monitoring overall server performance.
 */
WorldUpdateTime sWorldUpdateTime;

/**
 * @brief Construct UpdateTime tracker
 *
 * Initializes all metrics to zero. The data table is sized by the template
 * parameter (default 100 entries for historical tracking).
 */
UpdateTime::UpdateTime() : _averageUpdateTime(0), _totalUpdateTime(0), _updateTimeTableIndex(0), _maxUpdateTime(0),
    _maxUpdateTimeOfLastTable(0), _maxUpdateTimeOfCurrentTable(0), _updateTimeDataTable() { }

/**
 * @brief Get simple average update time
 * @return Average update time in milliseconds
 *
 * Calculates simple mean over the data table.
 */
uint32 UpdateTime::GetAverageUpdateTime() const
{
    return _averageUpdateTime;
}

/**
 * @brief Get time-weighted average update time
 * @return Weighted average in milliseconds
 *
 * Uses squared values as weights, giving more influence to longer updates.
 * This better represents user experience (longer updates are more noticeable).
 */
uint32 UpdateTime::GetTimeWeightedAverageUpdateTime() const
{
    uint32 sum = 0, weightsum = 0;
    for (uint32 diff : _updateTimeDataTable)
    {
        sum += diff * diff;
        weightsum += diff;
    }

    return sum / weightsum;
}

/**
 * @brief Get maximum update time ever recorded
 * @return Maximum update time in milliseconds
 *
 * Tracks the highest single update time since creation.
 */
uint32 UpdateTime::GetMaxUpdateTime() const
{
    return _maxUpdateTime;
}

/**
 * @brief Get maximum update time for current period
 * @return Maximum update time in milliseconds
 *
 * Returns the maximum of current and last table periods.
 */
uint32 UpdateTime::GetMaxUpdatTimeOfCurrentTable() const
{
    return std::max(_maxUpdateTimeOfCurrentTable, _maxUpdateTimeOfLastTable);
}

/**
 * @brief Get the most recent update time
 * @return Last update time in milliseconds
 *
 * Returns the time difference from the most recent update.
 */
uint32 UpdateTime::GetLastUpdateTime() const
{
    return _updateTimeDataTable[_updateTimeTableIndex != 0 ? _updateTimeTableIndex - 1 : _updateTimeDataTable.size() - 1];
}

/**
 * @brief Record a new update time measurement
 * @param diff Time difference since last update (milliseconds)
 *
 * Updates rolling statistics with the new measurement:
 * - Adds to data table (circular buffer)
 * - Updates max time tracking
 * - Recalculates average
 * - Rotates table when full
 */
void UpdateTime::UpdateWithDiff(uint32 diff)
{
    _totalUpdateTime = _totalUpdateTime - _updateTimeDataTable[_updateTimeTableIndex] + diff;
    _updateTimeDataTable[_updateTimeTableIndex] = diff;

    if (diff > _maxUpdateTime)
    {
        _maxUpdateTime = diff;
    }

    if (diff > _maxUpdateTimeOfCurrentTable)
    {
        _maxUpdateTimeOfCurrentTable = diff;
    }

    if (++_updateTimeTableIndex >= _updateTimeDataTable.size())
    {
        _updateTimeTableIndex = 0;
        _maxUpdateTimeOfLastTable = _maxUpdateTimeOfCurrentTable;
        _maxUpdateTimeOfCurrentTable = 0;
    }

    if (_updateTimeDataTable[_updateTimeDataTable.size() - 1])
    {
        _averageUpdateTime = _totalUpdateTime / _updateTimeDataTable.size();
    }
    else if (_updateTimeTableIndex)
    {
        _averageUpdateTime = _totalUpdateTime / _updateTimeTableIndex;
    }
}

/**
 * @brief Reset the update time recording timer
 *
 * Marks current time for subsequent duration measurements.
 * @see _RecordUpdateTimeDuration()
 */
void UpdateTime::RecordUpdateTimeReset()
{
    _recordedTime = getMSTime();
}

/**
 * @brief Log update duration if it exceeds threshold
 * @param text Description of the operation being measured
 * @param minUpdateTime Minimum time to trigger logging (milliseconds)
 *
 * Measures time since last RecordUpdateTimeReset() call. If duration
 * exceeds threshold, logs an error with the duration.
 */
void UpdateTime::_RecordUpdateTimeDuration(std::string const& text, uint32 minUpdateTime)
{
    uint32 thisTime = getMSTime();
    uint32 diff = getMSTimeDiff(_recordedTime, thisTime);

    if (diff > minUpdateTime)
    {
        sLog.outError("Record Update Time of %s: %u", text.c_str(), diff);
    }

    _recordedTime = thisTime;
}

/**
 * @brief Load logging configuration
 *
 * Sets default values for update time logging:
 * - Log interval: 60 seconds
 * - Minimum update time to log: 100ms
 *
 * @todo Move these to configuration file
 */
void WorldUpdateTime::LoadFromConfig()
{
    _recordUpdateTimeInverval = 60000;
    _recordUpdateTimeMin = 100;
}

/**
 * @brief Set update time logging interval
 * @param t Interval in milliseconds
 */
void WorldUpdateTime::SetRecordUpdateTimeInterval(uint32 t)
{
    _recordUpdateTimeInverval = t;
}

/**
 * @brief Log periodic server performance update
 * @param gameTimeMs Current game time in milliseconds
 * @param diff Time since last update
 * @param sessionCount Number of online players
 *
 * Logs server update performance periodically if update time exceeds
 * minimum threshold. Includes average update time and player count.
 */
void WorldUpdateTime::RecordUpdateTime(uint32 gameTimeMs, uint32 diff, uint32 sessionCount)
{
    if (_recordUpdateTimeInverval > 0 && diff > _recordUpdateTimeMin)
    {
        if (getMSTimeDiff(_lastRecordTime, gameTimeMs) > _recordUpdateTimeInverval)
        {
            sLog.outString("Update time diff: %u. Players online: %u", GetAverageUpdateTime(), sessionCount);
            _lastRecordTime = gameTimeMs;
        }
    }
}

/**
 * @brief Log specific operation duration
 * @param text Description of the operation
 *
 * Uses the configured minimum threshold from LoadFromConfig().
 */
void WorldUpdateTime::RecordUpdateTimeDuration(std::string const& text)
{
    _RecordUpdateTimeDuration(text, _recordUpdateTimeMin);
}
