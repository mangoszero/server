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

#ifndef MANGOS_TIMER_H
#define MANGOS_TIMER_H

#include "Common.h"
#include "Duration.h"
#include <ctime>

// New Method
inline std::chrono::steady_clock::time_point GetApplicationStartTime()
{
    using namespace std::chrono;

    static const steady_clock::time_point ApplicationStartTime = steady_clock::now();

    return ApplicationStartTime;
}

inline uint32 getMSTime()
{
    using namespace std::chrono;

    return uint32(duration_cast<milliseconds>(steady_clock::now() - GetApplicationStartTime()).count());
}

inline uint32 getMSTimeDiff(uint32 oldMSTime, uint32 newMSTime)
{
    // getMSTime() have limited data range and this is case when it overflow in this tick
    if (oldMSTime > newMSTime)
    {
        return (0xFFFFFFFF - oldMSTime) + newMSTime;
    }
    else
    {
        return newMSTime - oldMSTime;
    }
}

inline uint32 getMSTimeDiff(uint32 oldMSTime, std::chrono::steady_clock::time_point newTime)
{
    using namespace std::chrono;

    uint32 newMSTime = uint32(duration_cast<milliseconds>(newTime - GetApplicationStartTime()).count());

    return getMSTimeDiff(oldMSTime, newMSTime);
}

inline uint32 GetMSTimeDiffToNow(uint32 oldMSTime)
{
    return getMSTimeDiff(oldMSTime, getMSTime());
}

inline uint32 GetUnixTimeStamp()
{
    time_t nowMS = std::time(nullptr);

    return nowMS;
}

struct IntervalTimer
{
    public:
        /**
         * @brief
         *
         */
        IntervalTimer() : _interval(0), _current(0) {}

        /**
         * @brief
         *
         * @param diff
         */
    void Update(time_t diff)
    {
        _current += diff;
        if (_current < 0)
        {
            _current = 0;
        }
    }
        /**
         * @brief
         *
         * @return bool
         */
    bool Passed()
    {
        return _current >= _interval;
    }
        /**
         * @brief
         *
         */
    void Reset()
    {
        if (_current >= _interval)
        {
            _current %= _interval;
        }
    }

    void SetCurrent(time_t current)
    {
        _current = current;
    }

    void SetInterval(time_t interval)
    {
        _interval = interval;
    }

    time_t GetInterval() const
    {
        return _interval;
    }

    time_t GetCurrent() const
    {
        return _current;
    }

    private:
        time_t _interval; /**< TODO */
        time_t _current; /**< TODO */
};

/**
 * @brief
 *
 */
struct TimeTracker
{
public:
    TimeTracker(int32 expiry = 0) : _expiryTime(expiry) { }
    TimeTracker(Milliseconds expiry) : _expiryTime(expiry) { }

    void Update(int32 diff)
    {
        Update(Milliseconds(diff));
    }

    void Update(Milliseconds diff)
    {
        _expiryTime -= diff;
    }

    bool Passed() const
    {
        return _expiryTime <= Seconds(0);
    }

    void Reset(int32 expiry)
    {
        Reset(Milliseconds(expiry));
    }

    void Reset(Milliseconds expiry)
    {
        _expiryTime = expiry;
    }

    Milliseconds GetExpiry() const
    {
        return _expiryTime;
    }

private:
    Milliseconds _expiryTime;
};

struct PeriodicTimer
{
public:
    PeriodicTimer(int32 period, int32 start_time) :
        i_period(period), i_expireTime(start_time) { }

    bool Update(const uint32 diff)
    {
        if ((i_expireTime -= diff) > 0)
        {
            return false;
        }

        i_expireTime += i_period > int32(diff) ? i_period : diff;
        return true;
    }

    void SetPeriodic(int32 period, int32 start_time)
    {
        i_expireTime = start_time;
        i_period = period;
    }

    // Tracker interface
    void TUpdate(int32 diff) { i_expireTime -= diff; }
    bool TPassed() const { return i_expireTime <= 0; }
    void TReset(int32 diff, int32 period) { i_expireTime += period > diff ? period : diff; }

private:
    int32 i_period;
    int32 i_expireTime;
};

#endif
