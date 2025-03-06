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

#ifndef MANGOSSERVER_MOVESPLINEINIT_ARGS_H
#define MANGOSSERVER_MOVESPLINEINIT_ARGS_H

#include "MoveSplineFlag.h"
#include <G3D/Vector3.h>

class Unit;

namespace Movement
{
    /**
     * @brief A typedef for a vector of Vector3 points representing a path.
     */
    typedef std::vector<Vector3> PointsArray;

    /**
     * @brief A union for storing facing information.
     */
    union FacingInfo
    {
        /**
         * @brief A struct for storing coordinates.
         */
        struct
        {
            float x, y, z; /**< Coordinates for facing a point. */
        } f; /**< Facing coordinates. */
        uint64  target; /**< GUID of the target to face. */
        float   angle; /**< Angle to face. */

        /**
         * @brief Constructor for facing an angle.
         * @param o The angle to face.
         */
        FacingInfo(float o) : angle(o) {}
        /**
         * @brief Constructor for facing a target.
         * @param t The GUID of the target to face.
         */
        FacingInfo(uint64 t) : target(t) {}
        /**
         * @brief Default constructor.
         */
        FacingInfo() : target(0) {}
    };

    /**
     * @brief A struct for initializing MoveSpline arguments.
     */
    struct MoveSplineInitArgs
    {
            /**
             * @brief Constructor for MoveSplineInitArgs.
             * @param path_capacity The initial capacity of the path vector.
             */
            MoveSplineInitArgs(size_t path_capacity = 16) : path_Idx_offset(0),
                velocity(0.f), splineId(0), facing(), flags()
            {
                path.reserve(path_capacity);
            }

            PointsArray path; /**< The path points for the spline. */
            FacingInfo facing; /**< The facing information. */
            MoveSplineFlag flags; /**< The flags for the spline. */
            int32 path_Idx_offset; /**< The path index offset. */
            float velocity; /**< The velocity of the spline movement. */
            uint32 splineId; /**< The ID of the spline. */

            /**
             * @brief Validates the MoveSplineInitArgs.
             * @param unit The unit to validate against.
             * @return bool True if the arguments are valid, false otherwise.
             */
            bool Validate(Unit* unit) const;
        private:
            /**
             * @brief Checks if the path bounds are valid.
             * @return bool True if the path bounds are valid, false otherwise.
             */
            bool _checkPathBounds() const;
    };
}

#endif // MANGOSSERVER_MOVESPLINEINIT_ARGS_H
