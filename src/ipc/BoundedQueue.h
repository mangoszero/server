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
 * Wraps ACE_Based::LockedQueue<T> with a hard capacity limit, both by
 * element COUNT and (optionally) by total queued BYTES. When EITHER bound
 * would be exceeded, push() drops the newest item (i.e. the incoming item),
 * increments a dropped counter, and returns false immediately - it NEVER
 * blocks.
 *
 * The byte bound defends against a hostile producer that sends a small
 * number of very large frames: a count-only cap would let
 * count x max-frame-size bytes queue before the consumer drains. Callers
 * supply each item's byte cost to push(); a byteCap of 0 disables the
 * byte bound (count-only, the legacy behaviour).
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
         * @param cap     Maximum number of elements the queue will hold.
         *                A capacity of 0 causes every push to be dropped.
         * @param byteCap Maximum total queued bytes (0 = no byte bound).
         */
        explicit BoundedQueue(size_t cap, size_t byteCap = 0)
            : m_cap(cap), m_byteCap(byteCap),
              m_count(0), m_bytes(0), m_dropped(0)
        {
        }

        /**
         * @brief Push an item onto the queue (count bound only).
         *
         * Equivalent to push(item, 0): the item contributes nothing to the
         * byte total, so only the element-count bound applies. Retained for
         * callers that do not track per-item byte cost.
         *
         * @param item The item to enqueue.
         * @return true on success, false if the item was dropped.
         */
        bool push(const T& item)
        {
            return push(item, 0);
        }

        /**
         * @brief Push an item onto the queue, accounting @p bytes against the
         *        byte budget.
         *
         * Returns false and increments dropped() if EITHER the element-count
         * cap or the byte cap (when enabled) would be exceeded. Never blocks.
         *
         * @param item  The item to enqueue.
         * @param bytes The item's byte cost for the byte budget.
         * @return true on success, false if the item was dropped.
         */
        bool push(const T& item, size_t bytes)
        {
            if (size() >= m_cap)
            {
                ++m_dropped;
                return false;
            }

            if (m_byteCap != 0 &&
                static_cast<size_t>(m_bytes.value()) + bytes > m_byteCap)
            {
                ++m_dropped;
                return false;
            }

            // Store the byte cost alongside the item so pop()/clear() can
            // decrement the running byte total accurately - a count-only
            // decrement would let the total drift up under steady traffic and
            // eventually lock out the byte bound for legitimate frames.
            m_queue.add(Entry(item, bytes));
            ++m_count;
            m_bytes += static_cast<long>(bytes);
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
            Entry e;
            if (!m_queue.next(e))
            {
                return false;
            }

            item = e.item;
            --m_count;
            m_bytes -= static_cast<long>(e.bytes);
            return true;
        }

        /**
         * @brief Drain and discard every queued item (thread-safe).
         *
         * Used to purge frames left by a dead child before the next child's
         * frames are consumed. Pops until empty so the underlying
         * LockedQueue's own mutex serialises against a concurrent producer.
         *
         * @return Number of items discarded.
         */
        size_t clear()
        {
            size_t removed = 0;
            Entry e;
            while (m_queue.next(e))
            {
                --m_count;
                m_bytes -= static_cast<long>(e.bytes);
                ++removed;
            }
            return removed;
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

        /**
         * @brief Return the approximate total queued bytes.
         *
         * @return Approximate sum of the byte cost of all queued items.
         */
        size_t bytes() const
        {
            const long v = m_bytes.value();
            return v > 0 ? static_cast<size_t>(v) : 0;
        }

    private:
        /// Item plus its byte cost, so the byte total can be decremented
        /// accurately on pop()/clear() (T alone carries no canonical size).
        struct Entry
        {
            T      item;
            size_t bytes;

            Entry() : item(), bytes(0) {}
            Entry(const T& i, size_t b) : item(i), bytes(b) {}
        };

        size_t                                          m_cap;
        size_t                                          m_byteCap;
        ACE_Based::LockedQueue<Entry, ACE_Thread_Mutex> m_queue;
        ACE_Atomic_Op<ACE_Thread_Mutex, long>           m_count;
        ACE_Atomic_Op<ACE_Thread_Mutex, long>           m_bytes;
        mutable ACE_Atomic_Op<ACE_Thread_Mutex, long>   m_dropped;
};

#endif // AH_IPC_BOUNDED_QUEUE_H
