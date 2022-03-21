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


#include "UpdateTime.h"

#include "Timer.h"
#include "Config.h"
#include "Log.h"

WorldUpdateTime sWorldUpdateTime;

UpdateTime::UpdateTime() : _averageUpdateTime(0), _totalUpdateTime(0), _updateTimeTableIndex(0), _maxUpdateTime(0),
    _maxUpdateTimeOfLastTable(0), _maxUpdateTimeOfCurrentTable(0), _updateTimeDataTable() { }

uint32 UpdateTime::GetAverageUpdateTime() const
{
    return _averageUpdateTime;
}

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

uint32 UpdateTime::GetMaxUpdateTime() const
{
    return _maxUpdateTime;
}

uint32 UpdateTime::GetMaxUpdatTimeOfCurrentTable() const
{
    return std::max(_maxUpdateTimeOfCurrentTable, _maxUpdateTimeOfLastTable);
}

uint32 UpdateTime::GetLastUpdateTime() const
{
    return _updateTimeDataTable[_updateTimeTableIndex != 0 ? _updateTimeTableIndex - 1 : _updateTimeDataTable.size() - 1];
}

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

void UpdateTime::RecordUpdateTimeReset()
{
    _recordedTime = getMSTime();
}

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

void WorldUpdateTime::LoadFromConfig()
{
    // ToDo: move to configuration
    _recordUpdateTimeInverval = 60000;
    _recordUpdateTimeMin = 100;
}

void WorldUpdateTime::SetRecordUpdateTimeInterval(uint32 t)
{
    _recordUpdateTimeInverval = t;
}

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

void WorldUpdateTime::RecordUpdateTimeDuration(std::string const& text)
{
    _RecordUpdateTimeDuration(text, _recordUpdateTimeMin);
}
