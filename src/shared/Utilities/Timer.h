/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "Common/Common.h"
#include <ace/OS_NS_sys_time.h>

/**
 * @brief
 *
 */
class WorldTimer
{
    public:

        /**
         * @brief get current server time
         *
         * @return uint32
         */
        static uint32 getMSTime();

        /**
         * @brief get time difference between two timestamps
         *
         * @param oldMSTime
         * @param newMSTime
         * @return uint32
         */
        static inline uint32 getMSTimeDiff(const uint32& oldMSTime, const uint32& newMSTime)
        {
            if (oldMSTime > newMSTime)
            {
                const uint32 diff_1 = (uint32(0xFFFFFFFF) - oldMSTime) + newMSTime;
                const uint32 diff_2 = oldMSTime - newMSTime;

                return std::min(diff_1, diff_2);
            }

            return newMSTime - oldMSTime;
        }

        /**
         * @brief get last world tick time
         *
         * @return uint32
         */
        static  uint32 tickTime();
        /**
         * @brief get previous world tick time
         *
         * @return uint32
         */
        static  uint32 tickPrevTime();
        /**
         * @brief tick world timer
         *
         * @return uint32
         */
        static  uint32 tick();

    private:
        /**
         * @brief
         *
         */
        WorldTimer();
        /**
         * @brief
         *
         * @param
         */
        WorldTimer(const WorldTimer&);

        /**
         * @brief analogue to getMSTime() but it persists m_SystemTickTime
         *
         * @param savetime
         * @return uint32
         */
        static uint32 getMSTime_internal();

        static  uint32 m_iTime; /**< TODO */
        static  uint32 m_iPrevTime; /**< TODO */
};

/**
 * @brief
 *
 */
class IntervalTimer
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
                { _current = 0; }
        }
        /**
         * @brief
         *
         * @return bool
         */
        bool Passed() const { return _current >= _interval; }
        /**
         * @brief
         *
         */
        void Reset()
        {
            if (_current >= _interval)
                { _current -= _interval; }
        }

        /**
         * @brief
         *
         * @param current
         */
        void SetCurrent(time_t current) { _current = current; }
        /**
         * @brief
         *
         * @param interval
         */
        void SetInterval(time_t interval) { _interval = interval; }
        /**
         * @brief
         *
         * @return time_t
         */
        time_t GetInterval() const { return _interval; }
        /**
         * @brief
         *
         * @return time_t
         */
        time_t GetCurrent() const { return _current; }

    private:
        time_t _interval; /**< TODO */
        time_t _current; /**< TODO */
};

/**
 * @brief
 *
 */
class ShortIntervalTimer
{
    public:
        /**
         * @brief
         *
         */
        ShortIntervalTimer() : _interval(0), _current(0) {}

        /**
         * @brief
         *
         * @param diff
         */
        void Update(uint32 diff)
        {
            _current += diff;
        }

        /**
         * @brief
         *
         * @return bool
         */
        bool Passed() const { return _current >= _interval; }
        /**
         * @brief
         *
         */
        void Reset()
        {
            if (_current >= _interval)
                { _current -= _interval; }
        }

        /**
         * @brief
         *
         * @param current
         */
        void SetCurrent(uint32 current) { _current = current; }
        /**
         * @brief
         *
         * @param interval
         */
        void SetInterval(uint32 interval) { _interval = interval; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetInterval() const { return _interval; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetCurrent() const { return _current; }

    private:
        uint32 _interval; /**< TODO */
        uint32 _current; /**< TODO */
};

/**
 * @brief
 *
 */
struct TimeTracker
{
    public:
        /**
         * @brief
         *
         * @param expiry
         */
        TimeTracker(time_t expiry) : i_expiryTime(expiry) {}
        /**
         * @brief
         *
         * @param diff
         */
        void Update(time_t diff) { i_expiryTime -= diff; }
        /**
         * @brief
         *
         * @return bool
         */
        bool Passed() const { return (i_expiryTime <= 0); }
        /**
         * @brief
         *
         * @param interval
         */
        void Reset(time_t interval) { i_expiryTime = interval; }
        /**
         * @brief
         *
         * @return time_t
         */
        time_t GetExpiry() const { return i_expiryTime; }

    private:
        time_t i_expiryTime; /**< TODO */
};

/**
 * @brief
 *
 */
struct ShortTimeTracker
{
    public:
        /**
         * @brief
         *
         * @param expiry
         */
        ShortTimeTracker(int32 expiry = 0) : i_expiryTime(expiry) {}
        /**
         * @brief
         *
         * @param diff
         */
        void Update(int32 diff) { i_expiryTime -= diff; }
        /**
         * @brief
         *
         * @return bool
         */
        bool Passed() const { return (i_expiryTime <= 0); }
        /**
         * @brief
         *
         * @param interval
         */
        void Reset(int32 interval) { i_expiryTime = interval; }
        /**
         * @brief
         *
         * @return int32
         */
        int32 GetExpiry() const { return i_expiryTime; }

    private:
        int32 i_expiryTime; /**< TODO */
};

#endif
