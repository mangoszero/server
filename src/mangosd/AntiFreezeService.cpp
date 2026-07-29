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

#include <mutex>
#include <thread>
#include "AntiFreezeService.h"

#include "Log.h"
#include "Timer.h"
#include "World.h"

#include <chrono>
#include <cstdlib>

AntiFreezeService::AntiFreezeService(uint32 maxStuckMs)
    : m_maxStuckMs(maxStuckMs),
      m_stop(false)
{
}

AntiFreezeService::~AntiFreezeService()
{
    RequestStop();
    Join();
}

void AntiFreezeService::Start()
{
    if (m_maxStuckMs == 0)
    {
        return;     // disabled; never spawn the thread at all
    }

    sLog.outString("Anti-freeze watchdog armed (%u seconds max stuck time)",
                   m_maxStuckMs / 1000);

    m_thread = std::thread([this] { Run(); });
}

void AntiFreezeService::RequestStop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stop.store(true, std::memory_order_release);
    }
    m_wake.notify_all();
}

void AntiFreezeService::Join()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void AntiFreezeService::Run()
{
    // Snapshot of the counter and when it last moved.
    uint32 lastLoops  = World::m_worldLoopCounter.load(std::memory_order_relaxed);
    uint32 lastChange = getMSTime();

    for (;;)
    {
        // Wait a second, but wake immediately on shutdown rather than making
        // every stop pay up to a full second of latency.
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_wake.wait_for(lock, std::chrono::seconds(1),
                            [this] { return m_stop.load(std::memory_order_acquire); });
        }

        if (m_stop.load(std::memory_order_acquire) || World::IsStopped())
        {
            break;
        }

        const uint32 now   = getMSTime();
        const uint32 loops = World::m_worldLoopCounter.load(std::memory_order_relaxed);

        if (loops != lastLoops)
        {
            lastLoops  = loops;
            lastChange = now;
            continue;
        }

        if (getMSTimeDiff(lastChange, now) > m_maxStuckMs)
        {
            // Deliberately abort rather than return. The world thread is wedged,
            // so an orderly shutdown would itself hang waiting on it; the only
            // thing left that works is to die and let the supervisor restart us.
            sLog.outError("World thread has not advanced for %u seconds -- "
                          "terminating so the server can be restarted.",
                          m_maxStuckMs / 1000);
            Log::WaitBeforeContinueIfNeed();
            std::abort();
        }
    }

    sLog.outString("Anti-freeze watchdog stopped.");
}
