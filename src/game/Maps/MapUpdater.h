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

#ifndef _MAP_UPDATER_H_INCLUDED
#define _MAP_UPDATER_H_INCLUDED

#include <ace/Thread_Mutex.h>
#include <ace/Condition_Thread_Mutex.h>

#include "DelayExecutor.h"

class Map;

/**
 * @brief The MapUpdater class is responsible for managing map update requests.
 */
class MapUpdater
{
    public:
        /**
         * @brief Constructor for MapUpdater.
         */
        MapUpdater();

        /**
         * @brief Destructor for MapUpdater.
         */
        virtual ~MapUpdater();

        friend class MapUpdateRequest;

        /**
         * @brief Schedules a map update.
         * @param map Reference to the map to be updated.
         * @param diff Time difference for the update.
         * @return Result of the scheduling.
         */
        int schedule_update(Map& map, ACE_UINT32 diff);

        /**
         * @brief Waits for all pending requests to be processed.
         * @return Always returns 0.
         */
        int wait();

        /**
         * @brief Activates the map updater with the specified number of threads.
         * @param num_threads Number of threads to activate.
         * @return Result of the activation.
         */
        int activate(size_t num_threads);

        /**
         * @brief Deactivates the map updater.
         * @return Result of the deactivation.
         */
        int deactivate();

        /**
         * @brief Checks if the map updater is activated.
         * @return True if activated, false otherwise.
         */
        bool activated();

    private:
        DelayExecutor m_executor; ///< Executor for handling delayed tasks.
        ACE_Thread_Mutex m_mutex; ///< Mutex for synchronizing access to pending requests.
        ACE_Condition_Thread_Mutex m_condition; ///< Condition variable for signaling when requests are processed.
        size_t pending_requests; ///< Number of pending update requests.

        /**
         * @brief Called when a map update is finished.
         */
        void update_finished();
};

#endif //_MAP_UPDATER_H_INCLUDED
