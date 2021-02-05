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

#include "ThreadPoolMgr.h"

ThreadPoolMgr::ThreadPoolMgr() : _requestCount(0) { }

void ThreadPoolMgr::Start(std::size_t numThreads)
{
	_threads.reserve(numThreads);
	for (std::size_t i = 0; i < numThreads; ++i)
	{
		_threads.emplace_back(&ThreadPoolMgr::threadFunc, this);
	}
}

void ThreadPoolMgr::Stop()
{
	if (!_queue.frozen())
	{
		_queue.freeze();
		for (auto& thread : _threads)
		{
			thread.join();
		}
	}
}

void ThreadPoolMgr::Wait()
{
	GuardType guard(_lock);
	_waitCond.wait(guard, [this] { return _requestCount == 0; });
}

void ThreadPoolMgr::threadFunc()
{
	FunctorType func;
	while (_queue.pop(func))
	{
		func();
		GuardType guard(_lock);
		if (--_requestCount == 0)
		{
			_waitCond.notify_all();
		}
	}
}
