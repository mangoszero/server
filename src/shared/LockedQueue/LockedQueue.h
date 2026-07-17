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

#ifndef LOCKEDQUEUE_H
#define LOCKEDQUEUE_H

#include <deque>
#include <mutex>

namespace MaNGOS
{
    /**
     * @brief A simple thread-safe FIFO queue.
     *
     * The former lock template parameter is gone: the queue owns a std::mutex
     * and serialises itself, so call sites name only the element (and optionally the
     * backing container) type.
     *
     * @tparam T           Element type.
     * @tparam StorageType Underlying container (deque by default).
     */
    template <class T, typename StorageType = std::deque<T> >
    class LockedQueue
    {
        public:

            LockedQueue() = default;
            virtual ~LockedQueue() = default;

            /// Append an item to the back of the queue.
            void add(const T& item)
            {
                std::lock_guard<std::mutex> guard(_lock);
                _queue.push_back(item);
            }

            /// Pop the front item into @p result. Returns false if the queue is empty.
            bool next(T& result)
            {
                std::lock_guard<std::mutex> guard(_lock);

                if (_queue.empty())
                {
                    return false;
                }

                result = _queue.front();
                _queue.pop_front();

                return true;
            }

            /**
             * @brief Pop the front item only if @p check accepts it.
             *
             * Returns false — leaving the item queued — if the queue is empty or the
             * checker rejects it.
             */
            template<class Checker>
            bool next(T& result, Checker& check)
            {
                std::lock_guard<std::mutex> guard(_lock);

                if (_queue.empty())
                {
                    return false;
                }

                result = _queue.front();
                if (!check.Process(result))
                {
                    return false;
                }

                _queue.pop_front();
                return true;
            }

            /// True when the queue holds no elements (lock held).
            bool empty()
            {
                std::lock_guard<std::mutex> guard(_lock);
                return _queue.empty();
            }

            /// Number of elements currently queued (lock held).
            size_t size()
            {
                std::lock_guard<std::mutex> guard(_lock);
                return _queue.size();
            }

        private:

            std::mutex  _lock;   ///< Serialises access to the queue
            StorageType _queue;  ///< Storage backing the queue
    };
}
#endif
