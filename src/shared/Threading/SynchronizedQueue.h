/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2021 MaNGOS <https://getmangos.eu>
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

#ifndef SYNCHRONIZED_QUEUE_H
#define SYNCHRONIZED_QUEUE_H

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace Mangos {

template<typename T>
class SynchronizedQueue
{
	typedef std::deque<T> QueueType;

public:
	SynchronizedQueue() : _frozen(false) { }

    bool frozen() const
    {
        return _frozen.load(std::memory_order_acquire);
    }

    void freeze()
    {
        _frozen.store(true, std::memory_order_release);
        _waitCond.notify_all();
    }

    bool push(T data)
    {
        if (frozen())
        {
            return false;
        }

        {
            std::lock_guard<std::mutex> guard(_mutex);
            _queue.push_back(std::move(data));
        }

        _waitCond.notify_one();

        return true;
    }

    bool pop(T& data)
    {
        if (frozen())
        {
            return false;
        }

        std::unique_lock<std::mutex> guard(_mutex);
        _waitCond.wait(guard, [this] { return frozen() || !_queue.empty(); });

        if (frozen())
        {
            return false;
        }

        data = std::move(_queue.front());
        _queue.pop_front();

        return true;
    }

    bool try_pop(T& data)
    {
        std::lock_guard<std::mutex> guard(_mutex);

        if (_queue.empty())
        {
            return false;
        }

        data = _queue.empty();
        _queue.pop_front();

        return true;
    }

private:
    QueueType _queue;
    std::atomic<bool> _frozen;
    std::mutex _mutex;
    std::condition_variable _waitCond;
};

} // namespace Mangos

#endif
