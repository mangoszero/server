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
 * @file MapUpdater.h
 * @brief Worker pool that ticks maps in parallel.
 *
 * The world thread hands each map's Update() to this pool via schedule_update(), then
 * blocks in wait() until the whole tick has been processed.
 */

#ifndef _MAP_UPDATER_H_INCLUDED
#define _MAP_UPDATER_H_INCLUDED

#include "Platform/Define.h"

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

class Map;

/**
 * @brief Schedules map updates across a pool of worker threads.
 */
class MapUpdater
{
    public:

        MapUpdater();
        ~MapUpdater();

        MapUpdater(const MapUpdater&) = delete;
        MapUpdater& operator=(const MapUpdater&) = delete;

        /**
         * @brief Queue map.Update(diff) for a worker.
         * @return 0 on success, -1 if the pool is not running.
         */
        int schedule_update(Map& map, uint32 diff);

        /**
         * @brief Block until every scheduled update has finished.
         *
         * This is the tick barrier: the world thread must not advance until every map
         * queued this tick has been updated.
         *
         * @return Always 0.
         */
        int wait();

        /**
         * @brief Start @p num_threads workers.
         * @return 0 on success, -1 on failure.
         */
        int activate(size_t num_threads);

        /**
         * @brief Drain outstanding updates, then stop and join the workers.
         * @return Always 0.
         */
        int deactivate();

        /// True while worker threads are running.
        bool activated();

    private:

        /// One queued map tick.
        typedef std::pair<Map*, uint32> Task;

        /// Worker body: run tasks until stopped and the queue has drained.
        void workerLoop();

        std::vector<std::thread> m_workers;
        std::queue<Task>         m_tasks;

        std::mutex              m_mutex;      ///< Guards m_tasks, m_pending and m_stop
        std::condition_variable m_taskAdded;  ///< Wakes a worker when work arrives
        std::condition_variable m_taskDone;   ///< Wakes wait() once m_pending hits zero

        size_t m_pending; ///< Scheduled but not yet finished updates
        bool   m_stop;    ///< Set by deactivate() to retire the workers
};

#endif //_MAP_UPDATER_H_INCLUDED
