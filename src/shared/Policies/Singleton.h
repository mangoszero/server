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

#ifndef MANGOS_SINGLETON_H
#define MANGOS_SINGLETON_H

namespace MaNGOS
{
    /**
     * @brief Lazily-constructed process-wide singleton.
     *
     * Derive and befriend, then reach the instance through the usual accessor macro:
     * @code
     *     class World : public MaNGOS::Singleton<World>
     *     {
     *             friend class MaNGOS::Singleton<World>;
     *             World();      // may stay private
     *             ~World();     // likewise
     *     };
     *     #define sWorld MaNGOS::Singleton<World>::Instance()
     * @endcode
     *
     * A function-local static is initialised exactly once, thread-safely, with no
     * mutex of our own.
     */
    template<typename T>
    class Singleton
    {
        public:

            /**
             * @brief Get the singleton instance, constructing it on first use.
             *
             * Initialisation is thread-safe and happens exactly once: concurrent
             * callers block until the winner finishes ([stmt.dcl]/4). Destruction runs
             * at exit, in reverse order of first use.
             */
            static T& Instance()
            {
                static T instance;
                return instance;
            }

            Singleton(const Singleton&) = delete;
            Singleton& operator=(const Singleton&) = delete;

        protected:

            Singleton() = default;
            ~Singleton() = default;
    };
}

// Compatibility shims. The Meyers singleton above needs no explicit template
// instantiation, so these former INSTANTIATE_SINGLETON_* macros expand to nothing.
// They are kept only so the historical `INSTANTIATE_SINGLETON_1(Foo);` lines dotted
// through the .cpp files still compile; new code should not use them.
#define INSTANTIATE_SINGLETON_1(TYPE)
#define INSTANTIATE_SINGLETON_2(TYPE, THREADINGMODEL)
#define INSTANTIATE_SINGLETON_3(TYPE, THREADINGMODEL, CREATIONPOLICY)
#define INSTANTIATE_SINGLETON_4(TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME)

#endif
