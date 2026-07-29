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

#ifndef MANGOS_THREADLOCALSTORE_H
#define MANGOS_THREADLOCALSTORE_H

#include <map>
#include <thread>

namespace MaNGOS
{
    template<typename T>
    class ThreadLocalStore
    {
        public:
            /// Access the calling thread's instance, creating it on first use.
            T& get()
            {
                // The map lives in THREAD-LOCAL storage and is keyed by store, not the
                // other way round. That way a slot dies with the thread that made it --
                // the previous shape was one shared map keyed by std::thread::id, which
                // never removed anything and so handed a reused id the dead thread's
                // state. Keying by `this` is what lets the three DatabaseType instances
                // each keep their own slot without a bare `thread_local T` collapsing
                // them into one.
                thread_local std::map<const ThreadLocalStore*, T> slots;
                return slots[this];
            }

            T* operator->() { return &get(); }
    };
}

#endif
