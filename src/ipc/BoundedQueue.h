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
 */

#ifndef AH_IPC_BOUNDED_QUEUE_H
#define AH_IPC_BOUNDED_QUEUE_H

#include "Common.h"
#include "LockedQueue/LockedQueue.h"
#include <ace/Atomic_Op.h>
#include <ace/Thread_Mutex.h>

/**
 * @brief A capacity-bounded, non-blocking FIFO queue.
 *
 * Wraps ACE_Based::LockedQueue<T> with a hard capacity limit.
 * When the queue is at or above capacity, push() drops the newest
 * item (i.e. the incoming item), increments a dropped counter,
 * and returns false immediately - it NEVER blocks.
 *
 * A momentary +/-1 over capacity is acceptable; no additional lock
 * is taken to make the size-check-then-push atomic.
 *
 * @tparam T  The element type stored in the queue.
 */
template<class T>
class BoundedQueue
{
    public:
        /**
         * @brief Construct a BoundedQueue with the given capacity.
         *
         * @param cap Maximum number of elements the queue will hold.
         *            A capacity of 0 causes every push to be dropped.
         */
        explicit BoundedQueue(size_t cap)
            : m_cap(cap), m_count(0), m_dropped(0)
        {
        }

        /**
         * @brief Push an item onto the queue.
         *
         * Returns false and increments dropped() if the queue is full.
         * Never blocks.
         *
         * @param item The item to enqueue.
         * @return true on success, false if the item was dropped.
         */
        bool push(const T& item)
        {
            if (size() >= m_cap)
            {
                ++m_dropped;
                return false;
            }

            m_queue.add(item);
            ++m_count;
            return true;
        }

        /**
         * @brief Pop the oldest item from the queue.
         *
         * @param item Populated with the dequeued item on success.
         * @return true if an item was dequeued, false if the queue was empty.
         */
        bool pop(T& item)
        {
            if (!m_queue.next(item))
            {
                return false;
            }

            --m_count;
            return true;
        }

        /**
         * @brief Return the approximate number of items currently in the queue.
         *
         * The value is maintained via a separate atomic counter rather than
         * calling through the LockedQueue mutex on every size query, so it
         * may momentarily read +/-1 relative to the true queue depth.
         *
         * @return Approximate queue size.
         */
        size_t size() const
        {
            return static_cast<size_t>(m_count.value());
        }

        /**
         * @brief Return the total number of items dropped due to overflow.
         *
         * @return Cumulative drop count since construction.
         */
        size_t dropped() const
        {
            return static_cast<size_t>(m_dropped.value());
        }

    private:
        size_t                                        m_cap;
        ACE_Based::LockedQueue<T, ACE_Thread_Mutex>   m_queue;
        ACE_Atomic_Op<ACE_Thread_Mutex, long>         m_count;
        mutable ACE_Atomic_Op<ACE_Thread_Mutex, long> m_dropped;
};

#endif // AH_IPC_BOUNDED_QUEUE_H
