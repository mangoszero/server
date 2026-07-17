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

#ifndef THREADING_H
#define THREADING_H

#include <atomic>
#include <chrono>
#include <thread>

namespace MaNGOS
{
    /**
     * @brief Base class for a task that runs on its own thread.
     *
     * Reference-counted, so the Runnable can be handed to a Thread and forgotten:
     * the creator holds one reference, the running thread holds another for the
     * duration of run(), and whichever drops last deletes it.
     */
    class Runnable
    {
        public:

            virtual ~Runnable() {}

            /// Body of the task; runs on the spawned thread.
            virtual void run() = 0;

            void incReference() { ++m_refs; }

            void decReference()
            {
                if (--m_refs == 0)
                {
                    delete this;
                }
            }

        private:

            std::atomic<long> m_refs{0}; ///< Reference counter
    };

    /**
     * @brief A joinable OS thread running a Runnable.
     *
     * Constructing with a Runnable starts it immediately. Deleting the Thread joins
     * it and then drops the Thread's reference to the task — so `delete someThread`
     * is a "stop and reclaim", and by the time it returns the task is gone. Callers
     * are expected to have already told the Runnable to finish (its run() must
     * return), otherwise the join blocks forever.
     */
    class Thread
    {
        public:

            Thread();
            explicit Thread(Runnable* instance);
            ~Thread();

            Thread(const Thread&) = delete;
            Thread& operator=(const Thread&) = delete;

            /// Spawn the thread. Returns false if it is already running or has no task.
            bool start();

            /// Join the thread. Returns false if it was not running.
            bool wait();

            static void Sleep(unsigned long msecs)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(msecs));
            }

        private:

            std::thread m_thread; ///< The OS thread; joinable while running
            Runnable*   m_task;   ///< Task executed by m_thread (we hold one reference)
    };
}

#endif
