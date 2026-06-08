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

#include "EventProcessor.h"

/**
 * @brief Construct a new Event Processor::Event Processor object
 * Initializes member variables m_time and m_aborting.
 */
EventProcessor::EventProcessor()
{
    m_time = 0;
    m_aborting = false;
}

/**
 * @brief Destroy the Event Processor::Event Processor object
 * Calls KillAllEvents with force set to true.
 */
EventProcessor::~EventProcessor()
{
    KillAllEvents(true);
}

/**
 * @brief Updates the event processor with the given time.
 *
 * @param p_time Time to update the event processor with.
 */
void EventProcessor::Update(uint32 p_time)
{
    // update time
    m_time += p_time;

    // main event loop
    EventList::iterator i;
    while (((i = m_events.begin()) != m_events.end()) && i->first <= m_time)
    {
        // get and remove event from queue
        BasicEvent* Event = i->second;
        m_events.erase(i);

        if (!Event->to_Abort)
        {
            if (Event->Execute(m_time, p_time))
            {
                // completely destroy event if it is not re-added
                delete Event;
            }
        }
        else
        {
            Event->Abort(m_time);
            delete Event;
        }
    }
}

/**
 * @brief Kills all events in the event processor.
 *
 * @param force If true, forces the deletion of all events.
 */
void EventProcessor::KillAllEvents(bool force)
{
    // prevent event insertions
    m_aborting = true;

    // first, abort all existing events
    for (EventList::iterator i = m_events.begin(); i != m_events.end();)
    {
        EventList::iterator i_old = i;
        ++i;

        i_old->second->to_Abort = true;
        i_old->second->Abort(m_time);
        if (force || i_old->second->IsDeletable())
        {
            delete i_old->second;

            if (!force)                                     // need per-element cleanup
            {
                m_events.erase(i_old);
            }
        }
    }

    // fast clear event list (in force case)
    if (force)
    {
        m_events.clear();
    }
}

/**
 * @brief Adds an event to the event processor.
 *
 * @param Event Pointer to the event to add.
 * @param e_time Execution time of the event.
 * @param set_addtime If true, sets the add time of the event.
 */
void EventProcessor::AddEvent(BasicEvent* Event, uint64 e_time, bool set_addtime)
{
    if (set_addtime)
    {
        Event->m_addTime = m_time;
    }

    Event->m_execTime = e_time;
    m_events.insert(std::pair<uint64, BasicEvent*>(e_time, Event));
}

/**
 * @brief Calculates the time with the given offset.
 *
 * @param t_offset Time offset to add.
 * @return uint64 Calculated time.
 */
uint64 EventProcessor::CalculateTime(uint64 t_offset) const
{
    return m_time + t_offset;
}
