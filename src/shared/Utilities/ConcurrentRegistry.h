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

#ifndef MANGOS_CONCURRENTREGISTRY_H
#define MANGOS_CONCURRENTREGISTRY_H

#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <utility>

namespace MaNGOS
{
    /**
     * @brief A key -> pointer index guarded by a reader/writer lock.
     *
     * Knows nothing about the game: the only thing it asks of the stored type is
     * that it can be keyed. Lookups take the lock shared, so the map-update
     * threads read concurrently; only Insert and Remove are exclusive.
     *
     * Deliberately `final` and non-virtual. Its predecessor (HashMapHolder) was
     * inherited from in order to extend Insert/Remove -- but with no virtual
     * functions at all, the derived versions merely *hid* the base ones, so any
     * access through a base reference silently skipped the extra bookkeeping, and
     * the non-virtual destructor meant deleting through a base pointer leaked
     * everything the derived destructor was supposed to release. Use this by
     * composition; if you need behaviour around it, wrap it, do not inherit it.
     *
     * @tparam Key    Key type (ObjectGuid in the game code).
     * @tparam T      Pointed-to type. Ownership stays with the caller.
     */
    template <typename Key, typename T>
    class ConcurrentRegistry final
    {
        public:

            typedef std::unordered_map<Key, T*> MapType;

            ConcurrentRegistry() = default;

            ConcurrentRegistry(const ConcurrentRegistry&) = delete;
            ConcurrentRegistry& operator=(const ConcurrentRegistry&) = delete;

            void Insert(const Key& key, T* value)
            {
                std::unique_lock<std::shared_mutex> guard(m_lock);
                m_map[key] = value;
            }

            void Remove(const Key& key)
            {
                std::unique_lock<std::shared_mutex> guard(m_lock);
                m_map.erase(key);
            }

            /// Look one up, or nullptr.
            T* Find(const Key& key) const
            {
                std::shared_lock<std::shared_mutex> guard(m_lock);
                const auto itr = m_map.find(key);
                return itr != m_map.end() ? itr->second : nullptr;
            }

            /// First entry satisfying pred(key, value), or nullptr.
            template <typename F>
            T* FindWith(F&& pred) const
            {
                std::shared_lock<std::shared_mutex> guard(m_lock);
                for (const auto& itr : m_map)
                {
                    if (pred(itr.first, itr.second))
                    {
                        return itr.second;
                    }
                }
                return nullptr;
            }

            /// Run work(value) over every entry, under the shared lock.
            template <typename F>
            void ForEach(F&& work) const
            {
                std::shared_lock<std::shared_mutex> guard(m_lock);
                for (const auto& itr : m_map)
                {
                    work(itr.second);
                }
            }

            /// Hand the caller the raw map under an exclusive lock, for teardown
            /// and other bulk operations that have to mutate while iterating.
            template <typename F>
            void WithExclusive(F&& work)
            {
                std::unique_lock<std::shared_mutex> guard(m_lock);
                work(m_map);
            }

            size_t Size() const
            {
                std::shared_lock<std::shared_mutex> guard(m_lock);
                return m_map.size();
            }

        private:

            mutable std::shared_mutex m_lock;
            MapType                   m_map;
    };
}

#endif
