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
     * @brief
     *
     * @param sec
     * @return uint32
     */
    inline uint32 SecToMS(float sec)
    {
        return static_cast<uint32>(sec * 1000.f);
    }

    /**
     * @brief
     *
     * @param ms
     * @return float
     */
    inline float MSToSec(uint32 ms)
    {
        return ms / 1000.f;
    }

    template<class T, T limit>
    /**
     * @brief
     *
     */
    class counter
    {
        public:
            /**
             * @brief
             *
             */
            counter() { init();}


            /**
             * @brief
             *
             */
            void Increase()
            {
                if (m_counter == limit)
                    { init(); }
                else
                    { ++m_counter; }
            }

            /**
             * @brief
             *
             * @return T
             */
            T NewId() { Increase(); return m_counter;}
            /**
             * @brief
             *
             * @return T
             */
            T getCurrent() const { return m_counter;}

        private:
            /**
             * @brief
             *
             */
            void init() { m_counter = 0; }
            T m_counter; /**< TODO */
    };

    /**
     * @brief
     *
     */
    typedef counter<uint32, 0xFFFFFFFF> UInt32Counter;
}

#endif // MANGOSSERVER_TYPEDEFS_H
