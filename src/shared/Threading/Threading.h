/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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
     * @brief
     *
     */
    class Runnable
    {
        public:
            /**
             * @brief
             *
             */
            virtual ~Runnable() {}
            /**
             * @brief
             *
             */
            virtual void run() = 0;

            /**
             * @brief
             *
             */
            void incReference() { ++m_refs; }
            /**
             * @brief
             *
             */
            void decReference()
            {
                if (!--m_refs)
                    { delete this; }
            }
        private:
            ACE_Atomic_Op<ACE_Thread_Mutex, long> m_refs; /**< TODO */
    };

    /**
     * @brief
     *
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
     * @brief
     *
     */
    class ThreadPriority
    {
        public:
            /**
             * @brief
             *
             */
            ThreadPriority();
            /**
             * @brief
             *
             * @param p
             * @return int
             */
            int getPriority(Priority p) const;

        private:
            int m_priority[MAXPRIORITYNUM]; /**< TODO */
    };

    /**
     * @brief
     *
     */
    class Thread
    {
        public:
            /**
             * @brief
             *
             */
            Thread();
            /**
             * @brief
             *
             * @param instance
             */
            explicit Thread(Runnable* instance);
            /**
             * @brief
             *
             */
            ~Thread();

            /**
             * @brief
             *
             * @return bool
             */
            bool start();
            /**
             * @brief
             *
             * @return bool
             */
            bool wait();
            /**
             * @brief
             *
             */
            void destroy();

            /**
             * @brief
             *
             */
            void suspend();
            /**
             * @brief
             *
             */
            void resume();

            /**
             * @brief
             *
             * @param type
             */
            void setPriority(Priority type);

            /**
             * @brief
             *
             * @param msecs
             */
            static void Sleep(unsigned long msecs);

        private:
            /**
             * @brief
             *
             * @param
             */
            Thread(const Thread&);
            /**
             * @brief
             *
             * @param
             * @return Thread &operator
             */
            Thread& operator=(const Thread&);

            /**
             * @brief
             *
             * @param param
             * @return ACE_THR_FUNC_RETURN
             */
            static ACE_THR_FUNC_RETURN ThreadTask(void* param);

            ACE_thread_t m_iThreadId; /**< TODO */
            ACE_hthread_t m_hThreadHandle; /**< TODO */
            Runnable* m_task; /**< TODO */

            /**
             * @brief
             *
             */
            static ThreadPriority m_TpEnum; /**< use this object to determine current OS thread priority values mapped to enum Priority{} */
    };
}
#endif
