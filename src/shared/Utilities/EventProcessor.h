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
         * @brief Construct a new Basic Event object
         * Initializes member variables to_Abort, m_addTime, and m_execTime.
         */
        BasicEvent()
            : to_Abort(false), m_addTime(0), m_execTime(0) // Initialize member variables
        {
        }

        /**
         * @brief Destroy the Basic Event object
         * Override destructor to perform some actions on event removal.
         */
        virtual ~BasicEvent()
        {
        };

        /**
         * @brief This method executes when the event is triggered
         *
         * @param e_time Execution time
         * @param p_time Update interval
         * @return bool Return false if event does not want to be deleted
         */
        virtual bool Execute(uint64 /*e_time*/, uint32 /*p_time*/) { return true; }

        /**
         * @brief This event can be safely deleted
         *
         * @return bool
         */
        virtual bool IsDeletable() const { return true; }

        /**
         * @brief This method executes when the event is aborted
         *
         * @param e_time Execution time
         */
        virtual void Abort(uint64 /*e_time*/) {}

        bool to_Abort; /**< Set by externals when the event is aborted, aborted events don't execute and get Abort call when deleted */

        // These can be used for time offset control
        uint64 m_addTime; /**< Time when the event was added to queue, filled by event handler */
        uint64 m_execTime; /**< Planned time of next execution, filled by event handler */
};

/**
 * @brief Typedef for a multimap of events
 *
 */
typedef std::multimap<uint64, BasicEvent*> EventList;

/**
 * @brief Event Processor class
 *
 */
class EventProcessor
{
    public:
        /**
         * @brief Construct a new Event Processor object
         * Initializes member variables m_time and m_aborting.
         */
        EventProcessor();

        /**
         * @brief Destroy the Event Processor object
         * Calls KillAllEvents with force set to true.
         */
        ~EventProcessor();

        /**
         * @brief Updates the event processor with the given time
         *
         * @param p_time Time to update the event processor with
         */
        void Update(uint32 p_time);

        /**
         * @brief Kills all events in the event processor
         *
         * @param force If true, forces the deletion of all events
         */
        void KillAllEvents(bool force);

        /**
         * @brief Adds an event to the event processor
         *
         * @param Event Pointer to the event to add
         * @param e_time Execution time of the event
         * @param set_addtime If true, sets the add time of the event
         */
        void AddEvent(BasicEvent* Event, uint64 e_time, bool set_addtime = true);

        /**
         * @brief Calculates the time with the given offset
         *
         * @param t_offset Time offset to add
         * @return uint64 Calculated time
         */
        uint64 CalculateTime(uint64 t_offset) const;

    protected:
        uint64 m_time; /**< Current time in milliseconds */
        EventList m_events; /**< List of events */
        bool m_aborting; /**< Flag indicating if the event processor is aborting */
};

#endif
