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

#include <ace/Thread.h>
#include <ace/TSS_T.h>
#include <ace/Atomic_Op.h>
#include <assert.h>

namespace ACE_Based
{
    /**
     * @brief Base class for runnable tasks
     *
     * Runnable provides an interface for tasks that can be executed
     * in separate threads. Uses reference counting for automatic cleanup.
     */
    class Runnable
    {
        public:
            /**
             * @brief Virtual destructor
             */
            virtual ~Runnable() {}
            /**
             * @brief Main execution method for the task
             */
            virtual void run() = 0;

            /**
             * @brief Increment reference count
             */
            void incReference() { ++m_refs; }
            /**
             * @brief Decrement reference count, delete if zero
             */
            void decReference()
            {
                if (!--m_refs)
                {
                    delete this;
                }
            }
        private:
            ACE_Atomic_Op<ACE_Thread_Mutex, long> m_refs; /**< Reference counter */
    };

    /**
     * @brief Thread priority levels
     */
    enum Priority
    {
        Idle,
        Lowest,
        Low,
        Normal,
        High,
        Highest,
        Realtime
    };

#define MAXPRIORITYNUM (Realtime + 1)

    /**
     * @brief Thread priority management
     *
     * ThreadPriority maps platform-independent priority levels
     * to OS-specific priority values.
     */
    class ThreadPriority
    {
        public:
            /**
             * @brief Constructor - initializes priority mappings
             */
            ThreadPriority();
            /**
             * @brief Get OS-specific priority value
             * @param p Platform-independent priority level
             * @return OS-specific priority value
             */
            int getPriority(Priority p) const;

        private:
            int m_priority[MAXPRIORITYNUM]; /**< Priority value array */
    };

    /**
     * @brief
     *
     */
    class Thread
    {
        public:
            /**
             * @brief Default constructor
             */
            Thread();
            /**
             * @brief Constructor with runnable task
             * @param instance Runnable task to execute
             */
            explicit Thread(Runnable* instance);
            /**
             * @brief Destructor
             */
            ~Thread();

            /**
             * @brief Start the thread execution
             * @return True on success, false on failure
             */
            bool start();
            /**
             * @brief Wait for thread to finish
             * @return True on success, false on failure
             */
            bool wait();
            /**
             * @brief Destroy the thread
             */
            void destroy();

            /**
             * @brief Suspend thread execution
             */
            void suspend();
            /**
             * @brief Resume thread execution
             */
            void resume();

            /**
             * @brief Set thread priority
             * @param type Priority level
             */
            void setPriority(Priority type);

            /**
             * @brief Sleep for specified milliseconds
             * @param msecs Milliseconds to sleep
             */
            static void Sleep(unsigned long msecs);

        private:
            /**
             * @brief Copy constructor (disabled)
             */
            Thread(const Thread&);
            /**
             * @brief Assignment operator (disabled)
             */
            Thread& operator=(const Thread&);

            /**
             * @brief Thread entry point function
             * @param param Thread parameter
             * @return Thread return value
             */
            static ACE_THR_FUNC_RETURN ThreadTask(void* param);

            ACE_thread_t m_iThreadId; /**< Thread ID */
            ACE_hthread_t m_hThreadHandle; /**< Thread handle */
            Runnable* m_task; /**< Runnable task */

            /**
             * @brief Static priority mapping object
             */
            static ThreadPriority m_TpEnum; /**< Maps Priority enum to OS-specific values */
    };
}
#endif
