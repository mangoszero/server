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

#include "MapUpdater.h"
#include "DelayExecutor.h"
#include "Map.h"
#include "DatabaseEnv.h"

#include <ace/Guard_T.h>
#include <ace/Method_Request.h>

/**
 * @brief A request to update a map.
 */
class MapUpdateRequest : public ACE_Method_Request
{
    private:
        Map& m_map; ///< Reference to the map to be updated.
        MapUpdater& m_updater; ///< Reference to the map updater.
        ACE_UINT32 m_diff; ///< Time difference for the update.

    public:
        /**
         * @brief Constructor for MapUpdateRequest.
         * @param m Reference to the map.
         * @param u Reference to the map updater.
         * @param d Time difference for the update.
         */
        MapUpdateRequest(Map& m, MapUpdater& u, ACE_UINT32 d)
            : m_map(m), m_updater(u), m_diff(d)
        {
        }

        /**
         * @brief Executes the map update request.
         * @return Always returns 0.
         */
        virtual int call()
        {
            m_map.Update(m_diff);
            m_updater.update_finished();
            return 0;
        }
};

/**
 * @brief Constructor for MapUpdater.
 */
MapUpdater::MapUpdater():
m_executor(), m_mutex(), m_condition(m_mutex), pending_requests(0)
{
}

/**
 * @brief Destructor for MapUpdater.
 */
MapUpdater::~MapUpdater()
{
    deactivate();
}

/**
 * @brief Activates the map updater with the specified number of threads.
 * @param num_threads Number of threads to activate.
 * @return Result of the activation.
 */
int MapUpdater::activate(size_t num_threads)
{
    return m_executor._activate((int)num_threads);
}

/**
 * @brief Deactivates the map updater.
 * @return Result of the deactivation.
 */
int MapUpdater::deactivate()
{
    wait();
    return m_executor.deactivate();
}

/**
 * @brief Waits for all pending requests to be processed.
 * @return Always returns 0.
 */
int MapUpdater::wait()
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, m_mutex, -1);

    while (pending_requests > 0)
        m_condition.wait();

    return 0;
}

/**
 * @brief Schedules a map update.
 * @param map Reference to the map to be updated.
 * @param diff Time difference for the update.
 * @return Result of the scheduling.
 */
int MapUpdater::schedule_update(Map& map, ACE_UINT32 diff)
{
    ACE_GUARD_RETURN(ACE_Thread_Mutex, guard, m_mutex, -1);

    ++pending_requests;

    if (m_executor.execute(new MapUpdateRequest(map, *this, diff)) == -1)
    {
        ACE_DEBUG((LM_ERROR, ACE_TEXT("(%t) \n"), ACE_TEXT("Failed to schedule Map Update")));

        --pending_requests;
        return -1;
    }

    return 0;
}

/**
 * @brief Checks if the map updater is activated.
 * @return True if activated, false otherwise.
 */
bool MapUpdater::activated()
{
    return m_executor.activated();
}

/**
 * @brief Called when a map update is finished.
 */
void MapUpdater::update_finished()
{
    ACE_GUARD(ACE_Thread_Mutex, guard, m_mutex);

    if (pending_requests == 0)
    {
        ACE_ERROR((LM_ERROR, ACE_TEXT("(%t)\n"), ACE_TEXT("MapUpdater::update_finished BUG, report to devs")));
        return;
    }

    --pending_requests;

    m_condition.broadcast();
}
