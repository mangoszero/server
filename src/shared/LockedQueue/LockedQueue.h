/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2017  MaNGOS project <https://getmangos.eu>
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

#include <ace/Guard_T.h>
#include <ace/Thread_Mutex.h>
#include <deque>
#include <assert.h>
#include "Utilities/Errors.h"

namespace ACE_Based
{
    template < class T, class LockType, typename StorageType = std::deque<T> >
    /**
     * @brief
     *
     */
    class LockedQueue
    {
            LockType _lock; /**< Lock access to the queue. */

            StorageType _queue; /**< Storage backing the queue. */

            /*volatile*/ bool _canceled; /**< Cancellation flag. */

        public:

            /**
             * @brief Create a LockedQueue.
             *
             */
            LockedQueue()
                : _canceled(false)
            {
            }

            /**
             * @brief Destroy a LockedQueue.
             *
             */
            virtual ~LockedQueue()
            {
            }

            /**
             * @brief Adds an item to the queue.
             *
             * @param item
             */
            void add(const T& item)
            {
                ACE_Guard<LockType> g(this->_lock);
                _queue.push_back(item);
            }

            /**
             * @brief Gets the next result in the queue, if any.
             *
             * @param result
             * @return bool
             */
            bool next(T& result)
            {
                ACE_GUARD_RETURN(LockType, g, this->_lock, false);

                if (_queue.empty())
                    { return false; }

                result = _queue.front();
                _queue.pop_front();

                return true;
            }

            template<class Checker>
            /**
             * @brief
             *
             * @param result
             * @param check
             * @return bool
             */
            bool next(T& result, Checker& check)
            {
                ACE_GUARD_RETURN(LockType, g, this->_lock, false);

                if (_queue.empty())
                    { return false; }

                result = _queue.front();
                if (!check.Process(result))
                    { return false; }

                _queue.pop_front();
                return true;
            }

            /**
             * @brief Peeks at the top of the queue. Remember to unlock after use.
             *
             * @return T
             */
            T& peek()
            {
                lock();

                T& result = _queue.front();

                return result;
            }

            /**
             * @brief Cancels the queue.
             *
             */
            void cancel()
            {
                ACE_Guard<LockType> g(this->_lock);
                _canceled = true;
            }

            /**
             * @brief Checks if the queue is cancelled.
             *
             * @return bool
             */
            bool cancelled()
            {
                ACE_Guard<LockType> g(this->_lock);
                return _canceled;
            }

            /**
             * @brief Locks the queue for access.
             *
             */
            void lock()
            {
                this->_lock.acquire();
            }

            /**
             * @brief Unlocks the queue.
             *
             */
            void unlock()
            {
                this->_lock.release();
            }

            /**
             * @brief Checks if we're empty or not with locks held
             *
             * @return bool
             */
            bool empty()
            {
                ACE_Guard<LockType> g(this->_lock);
                return _queue.empty();
            }
    };
}
#endif
