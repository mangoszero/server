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
#include <deque>
#include <mutex>
#include <utility>

/**
 * @brief A capacity-bounded, non-blocking FIFO queue.
 *
 * A hard capacity limit applies both by element COUNT and (optionally) by
 * total queued BYTES. When EITHER bound would be exceeded, push() drops the
 * newest item (i.e. the incoming item), increments a dropped counter, and
 * returns false immediately - it NEVER blocks.
 *
 * The byte bound defends against a hostile producer that sends a small
 * number of very large frames: a count-only cap would let
 * count x max-frame-size bytes queue before the consumer drains. Callers
 * supply each item's byte cost to push(); a byteCap of 0 disables the
 * byte bound (count-only, the legacy behaviour).
 *
 * CONCURRENCY / INVARIANT: a SINGLE mutex (@ref m_mutex) guards the whole of
 * every operation - the container mutation AND the count/byte counters move
 * together under that one lock. The earlier design mutated an
 * MaNGOS::LockedQueue (with its own internal mutex) and then bumped
 * SEPARATE atomic counters OUTSIDE that mutex, so a producer preempted between
 * the enqueue and the counter increment could be overtaken by a consumer's
 * decrement, permanently drifting the counters away from the real contents.
 * A drifted count/bytes could hit the frame or byte cap while the queue was
 * near-empty and then drop EVERY legitimate frame (a silent inbound stall).
 * Folding the counters under the same lock makes that drift impossible: the
 * counters ALWAYS equal the actual contents.
 *
 * No method calls another locked method, and no callback runs under the lock,
 * so there is no nested-locking / self-deadlock path.
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
         * The cap check, the enqueue, and the counter updates all happen under
         * the SAME lock, so the count/byte totals can never drift from the
         * actual contents (see the class-level concurrency note).
         *
         * @param item  The item to enqueue.
         * @param bytes The item's byte cost for the byte budget.
         * @return true on success, false if the item was dropped.
         */
        bool push(const T& item, size_t bytes)
        {
            std::lock_guard<std::mutex> guard(m_mutex);

            if (m_count >= m_cap)
            {
                ++m_dropped;
                return false;
            }

            if (m_byteCap != 0 && m_bytes + bytes > m_byteCap)
            {
                ++m_dropped;
                return false;
            }

            // Store the byte cost alongside the item so pop()/clear() can
            // decrement the running byte total by the EXACT amount this item
            // added - all under this same lock.
            m_queue.push_back(std::make_pair(item, bytes));
            ++m_count;
            m_bytes += bytes;
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
            std::lock_guard<std::mutex> guard(m_mutex);

            if (m_queue.empty())
            {
                return false;
            }

            item = m_queue.front().first;
            const size_t bytes = m_queue.front().second;
            m_queue.pop_front();
            --m_count;
            m_bytes -= bytes;
            return true;
        }

        /**
         * @brief Drain and discard every queued item (thread-safe).
         *
         * Used to purge frames left by a dead child before the next child's
         * frames are consumed. The clear and the counter reset happen together
         * under the lock, so the post-condition (empty queue, zero counters)
         * holds atomically against any concurrent producer/consumer.
         *
         * @return Number of items discarded.
         */
        size_t clear()
        {
            std::lock_guard<std::mutex> guard(m_mutex);

            const size_t removed = m_queue.size();
            m_queue.clear();
            m_count = 0;
            m_bytes = 0;
            return removed;
        }

        /**
         * @brief Return the number of items currently in the queue.
         *
         * Maintained under the same lock as every mutation, so it always
         * matches the actual contents (no +/-1 drift).
         *
         * @return Queue size.
         */
        size_t size() const
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            return m_count;
        }

        /**
         * @brief Return the total number of items dropped due to overflow.
         *
         * @return Cumulative drop count since construction.
         */
        size_t dropped() const
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            return m_dropped;
        }

        /**
         * @brief Return the total queued bytes.
         *
         * Maintained under the same lock as every mutation, so it always
         * matches the sum of the byte cost of all queued items.
         *
         * @return Sum of the byte cost of all queued items.
         */
        size_t bytes() const
        {
            std::lock_guard<std::mutex> guard(m_mutex);
            return m_bytes;
        }

    private:
        /// Single mutex guarding the container AND the counters together, so
        /// the count/byte totals can never drift from the actual contents.
        mutable std::mutex                       m_mutex;

        /// Item plus its byte cost, so the byte total can be decremented by the
        /// exact per-item amount on pop()/clear() (T alone carries no canonical
        /// size).
        std::deque<std::pair<T, size_t> >        m_queue;

        size_t                                   m_cap;
        size_t                                   m_byteCap;
        size_t                                   m_count;
        size_t                                   m_bytes;
        size_t                                   m_dropped;
};

#endif // AH_IPC_BOUNDED_QUEUE_H
