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
 * @file MapUpdater.cpp
 * @brief Implementation of the parallel map-update worker pool.
 */

#include "MapUpdater.h"

#include "Map.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <mutex>
#include <thread>

MapUpdater::MapUpdater()
    : m_pending(0), m_stop(false)
{
}

MapUpdater::~MapUpdater()
{
    deactivate();
}

int MapUpdater::activate(size_t num_threads)
{
    if (num_threads == 0 || activated())
    {
        return -1;
    }

    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_stop = false;
    }

    m_workers.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i)
    {
        m_workers.emplace_back([this] { workerLoop(); });
    }

    return 0;
}

int MapUpdater::deactivate()
{
    if (!activated())
    {
        return 0;
    }

    sLog.outString("[shutdown] MapUpdater::deactivate: draining pending map updates (pending=%zu)", m_pending);

    // Drain first: a map must not be left half-updated, and Map::Update touches world
    // state that is torn down right after this returns.
    wait();

    sLog.outString("[shutdown] MapUpdater::deactivate: pending drained; joining worker threads");

    {
        std::lock_guard<std::mutex> guard(m_mutex);
        m_stop = true;
    }
    m_taskAdded.notify_all();

    for (std::thread& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
    m_workers.clear();

    sLog.outString("[shutdown] MapUpdater::deactivate: worker threads joined");
    return 0;
}

bool MapUpdater::activated()
{
    return !m_workers.empty();
}

int MapUpdater::schedule_update(Map& map, uint32 diff)
{
    std::unique_lock<std::mutex> guard(m_mutex);

    if (m_stop || m_workers.empty())
    {
        sLog.outError("MapUpdater::schedule_update: pool is not running, map %u not updated", map.GetId());
        return -1;
    }

    m_tasks.push(Task(&map, diff));
    ++m_pending;

    guard.unlock();
    m_taskAdded.notify_one();

    return 0;
}

int MapUpdater::wait()
{
    std::unique_lock<std::mutex> guard(m_mutex);

    m_taskDone.wait(guard, [this] { return m_pending == 0; });

    return 0;
}

void MapUpdater::workerLoop()
{
    for (;;)
    {
        Task task(nullptr, 0);

        {
            std::unique_lock<std::mutex> guard(m_mutex);

            m_taskAdded.wait(guard, [this] { return m_stop || !m_tasks.empty(); });

            // Only retire once the queue is genuinely empty, so a stop racing with a
            // still-queued tick cannot drop that map's update on the floor.
            if (m_tasks.empty())
            {
                if (m_stop)
                {
                    return;
                }
                continue;
            }

            task = m_tasks.front();
            m_tasks.pop();
        }

        task.first->Update(task.second);

        {
            std::lock_guard<std::mutex> guard(m_mutex);
            --m_pending;
        }

        // Outside the lock: wait() only ever cares about the count reaching zero, and
        // notifying while holding the mutex would just make the waiter block again.
        m_taskDone.notify_all();
    }
}
