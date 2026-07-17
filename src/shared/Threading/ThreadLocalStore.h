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
#include <mutex>
#include <thread>

namespace MaNGOS
{
    /**
     * @brief One instance of T per thread, per store object.
     *
     * Note that a plain `thread_local` will not serve
     * here: the server runs three Database objects (world, characters, login) and each
     * needs its own per-thread helper, which a single static thread_local cannot
     * express — the object identity is part of the key.
     *
     * std::map (rather than unordered_map) is deliberate: its references are stable
     * across inserts, so a T& handed out to one thread stays valid when another thread
     * later registers itself. Access takes a lock, which is fine for the transaction
     * bookkeeping this holds — it is not on a per-packet path.
     */
    template<typename T>
    class ThreadLocalStore
    {
        public:

            /// Access the calling thread's instance, creating it on first use.
            T& get()
            {
                const std::thread::id self = std::this_thread::get_id();

                std::lock_guard<std::mutex> guard(m_mutex);
                return m_slots[self];
            }

            T* operator->() { return &get(); }

        private:

            std::mutex                   m_mutex;
            std::map<std::thread::id, T> m_slots;
    };
}

#endif
