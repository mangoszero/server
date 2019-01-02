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

#ifndef MANGOS_H_EVENTPROCESSOR
#define MANGOS_H_EVENTPROCESSOR

#include "Platform/Define.h"

#include <map>

/**
 * @brief Note. All times are in milliseconds here.
 *
 */
class BasicEvent
{
    public:

        /**
         * @brief
         *
         */
        BasicEvent()
            : to_Abort(false)
        {
        }

        /**
         * @brief override destructor to perform some actions on event removal
         *
         */
        virtual ~BasicEvent()
        {
        };


        /**
         * @brief this method executes when the event is triggered
         *
         * @param uint64 e_time is execution time
         * @param uint32 p_time is update interval
         * @return bool return false if event does not want to be deleted
         */
        virtual bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) { return true; }

        /**
         * @brief this event can be safely deleted
         *
         * @return bool
         */
        virtual bool IsDeletable() const { return true; }

        /**
         * @brief this method executes when the event is aborted
         *
         * @param uint64
         */
        virtual void Abort(uint64 /*e_time*/) {}

        bool to_Abort;                                      /**< set by externals when the event is aborted, aborted events don't execute and get Abort call when deleted */

        // these can be used for time offset control
        uint64 m_addTime;                                   /**< time when the event was added to queue, filled by event handler */
        uint64 m_execTime;                                  /**< planned time of next execution, filled by event handler */
};

/**
 * @brief
 *
 */
typedef std::multimap<uint64, BasicEvent*> EventList;

/**
 * @brief
 *
 */
class EventProcessor
{
    public:

        /**
         * @brief
         *
         */
        EventProcessor();
        /**
         * @brief
         *
         */
        ~EventProcessor();

        /**
         * @brief
         *
         * @param p_time
         */
        void Update(uint32 p_time);
        /**
         * @brief
         *
         * @param force
         */
        void KillAllEvents(bool force);
        /**
         * @brief
         *
         * @param Event
         * @param e_time
         * @param set_addtime
         */
        void AddEvent(BasicEvent* Event, uint64 e_time, bool set_addtime = true);
        /**
         * @brief
         *
         * @param t_offset
         * @return uint64
         */
        uint64 CalculateTime(uint64 t_offset);

    protected:

        uint64 m_time; /**< TODO */
        EventList m_events; /**< TODO */
        bool m_aborting; /**< TODO */
};

#endif
