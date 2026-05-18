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

#ifndef MANGOSSERVER_TYPEDEFS_H
#define MANGOSSERVER_TYPEDEFS_H

#include "Common.h"

namespace G3D
{
    class Vector2;
    class Vector3;
    class Vector4;
}

namespace Movement
{
    using G3D::Vector2;
    using G3D::Vector3;
    using G3D::Vector4;

    /**
     * @brief Converts seconds to milliseconds.
     * @param sec The time in seconds.
     * @return uint32 The time in milliseconds.
     */
    inline uint32 SecToMS(float sec)
    {
        return static_cast<uint32>(sec * 1000.f);
    }

    /**
     * @brief Converts milliseconds to seconds.
     * @param ms The time in milliseconds.
     * @return float The time in seconds.
     */
    inline float MSToSec(uint32 ms)
    {
        return ms / 1000.f;
    }

    template<class T, T limit>
    /**
     * @brief A counter class that generates unique IDs up to a specified limit.
     */
    class counter
    {
        public:
            /**
             * @brief Constructor for the counter class.
             */
            counter() { init();}

            /**
             * @brief Increases the counter value.
             */
            void Increase()
            {
                if (m_counter == limit)
                {
                    init();
                }
                else
                {
                    ++m_counter;
                }
            }

            /**
             * @brief Generates a new ID.
             * @return T The new ID.
             */
            T NewId() { Increase(); return m_counter;}
            /**
             * @brief Gets the current counter value.
             * @return T The current counter value.
             */
            T getCurrent() const { return m_counter;}

        private:
            /**
             * @brief Initializes the counter to zero.
             */
            void init() { m_counter = 0; }
            T m_counter; /**< The current counter value. */
    };

    /**
     * @brief Typedef for a 32-bit unsigned integer counter.
     */
    typedef counter<uint32, 0xFFFFFFFF> UInt32Counter;
}

#endif // MANGOSSERVER_TYPEDEFS_H
