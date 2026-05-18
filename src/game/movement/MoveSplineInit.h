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

#ifndef MANGOSSERVER_MOVESPLINEINIT_H
#define MANGOSSERVER_MOVESPLINEINIT_H

#include "MoveSplineInitArgs.h"
#include "PathFinder.h"

class Unit;

namespace Movement
{
    /**
     * @brief Initializes and launches spline movement
     *
     */
    class MoveSplineInit
    {
        public:

            /**
             * @brief Constructor that initializes the MoveSplineInit with a reference to a Unit.
             * @param m Reference to the Unit to be moved.
             */
            explicit MoveSplineInit(Unit& m);

            /**
             * @brief Final pass of initialization that launches spline movement.
             * @return int32 duration - estimated travel time
             */
            int32 Launch();

            /**
             * @brief Stops any creature movement.
             */
            void Stop();

            /**
             * @brief Adds final facing animation.
             * Sets unit's facing to specified point/angle after all path done.
             * You can have only one final facing: previous will be overridden.
             * @param angle The angle to face.
             */
            void SetFacing(float angle);

            /**
             * @brief Sets unit's facing to a specified point after all path done.
             * @param point The point to face.
             */
            void SetFacing(Vector3 const& point);

            /**
             * @brief Sets unit's facing to a specified target after all path done.
             * @param target The target to face.
             */
            void SetFacing(const Unit* target);

            /**
             * @brief Initializes movement by path.
             * @param path Array of points, shouldn't be empty.
             * @param pointId Id of first point of the path. Example: when the third path point is done, it will notify that pointId + 3 is done.
             */
            void MovebyPath(const PointsArray& path, int32 pointId = 0);

            /**
             * @brief Initializes simple A to B motion, A is the current unit's position, B is the destination.
             * @param destination The destination point.
             * @param generatePath Whether to generate a path.
             * @param forceDestination Whether to force the destination.
             * @param maxPathRange The maximum path range.
             */
            void MoveTo(const Vector3& destination, bool generatePath = false, bool forceDestination = false, float maxPathRange = 0.0f);

            /**
             * @brief Initializes simple A to B motion, A is the current unit's position, B is the destination.
             * @param x The x-coordinate of the destination.
             * @param y The y-coordinate of the destination.
             * @param z The z-coordinate of the destination.
             * @param generatePath Whether to generate a path.
             * @param forceDestination Whether to force the destination.
             * @param maxPathRange The maximum path range.
             */
            void MoveTo(float x, float y, float z, bool generatePath = false, bool forceDestination = false, float maxPathRange = 0.0f);

            /**
             * @brief Sets the Id of the first point of the path.
             * When the N-th path point is done, the listener will notify that pointId + N is done.
             * Needed for waypoint movement where the path is split into parts.
             * @param pointId The Id of the first point of the path.
             */
            void SetFirstPointId(int32 pointId) { args.path_Idx_offset = pointId; }

            /**
             * @brief Enables CatmullRom spline interpolation mode, enables flying animation.
             * Disabled by default.
             */
            void SetFly();

            /**
             * @brief Enables walk mode. Disabled by default.
             * @param enable Whether to enable walk mode.
             */
            void SetWalk(bool enable);

            /**
             * @brief Makes movement cyclic. Disabled by default.
             */
            void SetCyclic();

            /**
             * @brief Enables falling mode. Disabled by default.
             */
            void SetFall();

            /**
             * @brief Sets the velocity (in case you want to have custom movement velocity).
             * If not set, speed will be selected based on the unit's speeds and current movement mode.
             * Has no effect if falling mode is enabled.
             * @param velocity The velocity, shouldn't be negative.
             */
            void SetVelocity(float velocity);

            /**
             * @brief Gets the path points array.
             * @return PointsArray The path points array.
             */
            PointsArray& Path() { return args.path; }
        protected:

            MoveSplineInitArgs args; /**< Arguments for initializing the spline movement. */
            Unit&  unit; /**< Reference to the unit to be moved. */
    };

    /**
     * @brief Enables CatmullRom spline interpolation mode, enables flying animation.
     * Disabled by default.
     */
    inline void MoveSplineInit::SetFly() { args.flags.flying = true;}

    /**
     * @brief Enables walk mode. Disabled by default.
     * @param enable Whether to enable walk mode.
     */
    inline void MoveSplineInit::SetWalk(bool enable) { args.flags.runmode = !enable;}

    /**
     * @brief Makes movement cyclic. Disabled by default.
     */
    inline void MoveSplineInit::SetCyclic() { args.flags.cyclic = true;}

    /**
     * @brief Enables falling mode. Disabled by default.
     */
    inline void MoveSplineInit::SetFall() { args.flags.falling = true;}

    /**
     * @brief Sets the velocity (in case you want to have custom movement velocity).
     * If not set, speed will be selected based on the unit's speeds and current movement mode.
     * Has no effect if falling mode is enabled.
     * @param vel The velocity, shouldn't be negative.
     */
    inline void MoveSplineInit::SetVelocity(float vel) { args.velocity = vel;}

    /**
     * @brief Initializes movement by path.
     * @param controls Array of points, shouldn't be empty.
     * @param path_offset Id of the first point of the path.
     */
    inline void MoveSplineInit::MovebyPath(const PointsArray& controls, int32 path_offset)
    {
        args.path_Idx_offset = path_offset;
        args.path.assign(controls.begin(), controls.end());
    }

    /**
     * @brief Initializes simple A to B motion, A is the current unit's position, B is the destination.
     * @param x The x-coordinate of the destination.
     * @param y The y-coordinate of the destination.
     * @param z The z-coordinate of the destination.
     * @param generatePath Whether to generate a path.
     * @param forceDestination Whether to force the destination.
     * @param maxPathRange The maximum path range.
     */
    inline void MoveSplineInit::MoveTo(float x, float y, float z, bool generatePath, bool forceDestination, float maxPathRange)
    {
        Vector3 v(x, y, z);
        MoveTo(v, generatePath, forceDestination, maxPathRange);
    }

    /**
     * @brief Initializes simple A to B motion, A is the current unit's position, B is the destination.
     * @param dest The destination point.
     * @param generatePath Whether to generate a path.
     * @param forceDestination Whether to force the destination.
     * @param maxPathRange The maximum path range.
     */
    inline void MoveSplineInit::MoveTo(const Vector3& dest, bool generatePath, bool forceDestination, float maxPathRange)
    {
        if (generatePath)
        {
            PathFinder path(&unit);
            if (maxPathRange > 0.0f)
            {
                path.setPathLengthLimit(maxPathRange);
            }
            path.calculate(dest.x, dest.y, dest.z, forceDestination);
            if (!(path.getPathType() & PATHFIND_NOPATH))
            {
                MovebyPath(path.getPath());
                return;
            }
        }
        args.path_Idx_offset = 0;
        args.path.resize(2);
        args.path[1] = dest;
    }

    /**
     * @brief Sets unit's facing to a specified point after all path done.
     * @param spot The point to face.
     */
    inline void MoveSplineInit::SetFacing(Vector3 const& spot)
    {
        args.facing.f.x = spot.x;
        args.facing.f.y = spot.y;
        args.facing.f.z = spot.z;
        args.flags.EnableFacingPoint();
    }
}

#endif // MANGOSSERVER_MOVESPLINEINIT_H
