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

#ifndef MANGOSSERVER_MOVESPLINEINIT_ARGS_H
#define MANGOSSERVER_MOVESPLINEINIT_ARGS_H

#include "MoveSplineFlag.h"
#include <G3D/Vector3.h>

class Unit;

namespace Movement
{
    /**
     * @brief
     *
     */
    typedef std::vector<Vector3> PointsArray;

    /**
     * @brief
     *
     */
    union FacingInfo
    {
        /**
         * @brief
         *
         */
        struct
        {
            float x, y, z; /**< TODO */
        } f; /**< TODO */
        uint64  target; /**< TODO */
        float   angle; /**< TODO */

        /**
         * @brief
         *
         * @param o
         */
        FacingInfo(float o) : angle(o) {}
        /**
         * @brief
         *
         * @param t
         */
        FacingInfo(uint64 t) : target(t) {}
        /**
         * @brief
         *
         */
        FacingInfo() {}
    };

    /**
     * @brief
     *
     */
    struct MoveSplineInitArgs
    {
            /**
             * @brief
             *
             * @param path_capacity
             */
            MoveSplineInitArgs(size_t path_capacity = 16) : path_Idx_offset(0),
                velocity(0.f), splineId(0)
            {
                path.reserve(path_capacity);
            }

            PointsArray path; /**< TODO */
            FacingInfo facing; /**< TODO */
            MoveSplineFlag flags; /**< TODO */
            int32 path_Idx_offset; /**< TODO */
            float velocity; /**< TODO */
            uint32 splineId; /**< TODO */

            /**
             * @brief Returns true to show that the arguments were configured correctly and MoveSpline initialization will succeed.
             *
             * @param unit
             * @return bool
             */
            bool Validate(Unit* unit) const;
        private:
            /**
             * @brief
             *
             * @return bool
             */
            bool _checkPathBounds() const;
    };
}

#endif // MANGOSSERVER_MOVESPLINEINIT_ARGS_H
