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

        public:
            LockedQueue(): _lock(), _queue()
            {
            }

            virtual ~LockedQueue()
            {
            }

            void add(const T& item)
            {
                ACE_GUARD (LockType, g, this->_lock);
                _queue.push_back(item);
            }

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

            bool empty()
            {
                ACE_GUARD_RETURN (LockType, g, this->_lock, false);
                return _queue.empty();
            }
    };
}
#endif
