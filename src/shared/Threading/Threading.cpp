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
 * @file Threading.cpp
 * @brief std::thread-based threading implementation
 *
 * A minimal, platform-independent threading abstraction built directly on the
 * C++ standard library (std::thread):
 *
 * - Runnable-based task execution
 * - Thread lifecycle management (start, wait)
 * - Reference counting for task safety
 *
 * @see Thread for the main thread class
 * @see Runnable for the task interface
 */

#include "Threading.h"

#include "Utilities/Errors.h"
#include <thread>

namespace MaNGOS
{
    Thread::Thread()
        : m_task(nullptr)
    {
    }

    Thread::Thread(Runnable* instance)
        : m_task(instance)
    {
        if (m_task)
        {
            // The Thread's own reference, released in the destructor. Every call site
            // constructs a Thread expecting it to already be running.
            m_task->incReference();

            const bool started = start();
            MANGOS_ASSERT(started);
        }
    }

    Thread::~Thread()
    {
        wait();

        if (m_task)
        {
            m_task->decReference();  // usually the last one: reclaims the Runnable
            m_task = nullptr;
        }
    }

    bool Thread::start()
    {
        if (!m_task || m_thread.joinable())
        {
            return false;
        }

        Runnable* task = m_task;
        task->incReference();        // the running thread's own reference

        m_thread = std::thread([task]
        {
            task->run();
            task->decReference();
        });
        return true;
    }

    bool Thread::wait()
    {
        if (!m_thread.joinable())
        {
            return false;
        }

        m_thread.join();
        return true;
    }
}
