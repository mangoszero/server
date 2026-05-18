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

#ifndef MANGOSSERVER_MOVEPLINE_H
#define MANGOSSERVER_MOVEPLINE_H

#include "spline.h"
#include "MoveSplineInitArgs.h"

namespace Movement
{
    /**
     * @brief Enum representing different types of monster movement.
     */
    enum MonsterMoveType
    {
        MonsterMoveNormal = 0,
        MonsterMoveStop = 1,
        MonsterMoveFacingSpot = 2,
        MonsterMoveFacingTarget = 3,
        MonsterMoveFacingAngle = 4
    };

    /**
     * @brief Struct representing a location with orientation.
     */
    struct Location : public Vector3
    {
        /**
         * @brief Default constructor initializing orientation to 0.
         */
        Location() : orientation(0) {}

        /**
         * @brief Constructor initializing location and orientation.
         * @param x X-coordinate.
         * @param y Y-coordinate.
         * @param z Z-coordinate.
         * @param o Orientation.
         */
        Location(float x, float y, float z, float o) : Vector3(x, y, z), orientation(o) {}

        /**
         * @brief Constructor initializing location from a Vector3.
         * @param v Vector3 representing the location.
         */
        Location(const Vector3& v) : Vector3(v), orientation(0) {}

        /**
         * @brief Constructor initializing location from a Vector3 and orientation.
         * @param v Vector3 representing the location.
         * @param o Orientation.
         */
        Location(const Vector3& v, float o) : Vector3(v), orientation(o) {}

        float orientation; /**< Orientation of the location. */
    };

    /**
     * @brief MoveSpline represents smooth Catmull-Rom or linear curve and point that moves along it.
     *
     * The curve can be cyclic - in this case, movement will be cyclic.
     * The point can have vertical acceleration motion component (used in fall, parabolic movement).
     */
    class MoveSpline
    {
        public:
            /**
             * @brief Typedef for the spline type used.
             */
            typedef Spline<int32> MySpline;

            /**
             * @brief Enum representing the result of an update.
             */
            enum UpdateResult
            {
                Result_None         = 0x01,
                Result_Arrived      = 0x02,
                Result_NextCycle    = 0x04,
                Result_NextSegment  = 0x08,
            };

            friend class PacketBuilder;

        protected:
            MySpline        spline; /**< The spline representing the path. */
            FacingInfo      facing; /**< Information about the facing direction. */
            uint32          m_Id; /**< ID of the spline. */
            MoveSplineFlag  splineflags; /**< Flags representing the state of the spline. */
            int32           time_passed; /**< Time passed since the start of the movement. */
            int32           point_Idx; /**< Current point index in the spline. */
            int32           point_Idx_offset; /**< Offset for the point index. */

            /**
             * @brief Initializes the spline with the given arguments.
             * @param args The arguments for initializing the spline.
             */
            void init_spline(const MoveSplineInitArgs& args);

        protected:
            /**
             * @brief Gets the path points of the spline.
             * @return const MySpline::ControlArray& The path points of the spline.
             */
            const MySpline::ControlArray& getPath() const { return spline.getPoints();}

            /**
             * @brief Computes the fall elevation.
             * @param el The elevation to compute.
             */
            void computeFallElevation(float& el) const;

            /**
             * @brief Updates the state of the spline.
             * @param ms_time_diff The time difference in milliseconds.
             * @return UpdateResult The result of the update.
             */
            UpdateResult _updateState(int32& ms_time_diff);

            /**
             * @brief Gets the next timestamp in the spline.
             * @return int32 The next timestamp.
             */
            int32 next_timestamp() const { return spline.length(point_Idx + 1);}

            /**
             * @brief Gets the time elapsed in the current segment.
             * @return int32 The time elapsed in the current segment.
             */
            int32 segment_time_elapsed() const { return next_timestamp() - time_passed;}

            /**
             * @brief Gets the total time elapsed.
             * @return int32 The total time elapsed.
             */
            int32 timeElapsed() const { return Duration() - time_passed;}

            /**
             * @brief Gets the time passed since the start of the movement.
             * @return int32 The time passed since the start of the movement.
             */
            int32 timePassed() const { return time_passed;}

        public:
            /**
             * @brief Gets the spline.
             * @return const MySpline& The spline.
             */
            const MySpline& _Spline() const { return spline;}

            /**
             * @brief Gets the current spline index.
             * @return int32 The current spline index.
             */
            int32 _currentSplineIdx() const { return point_Idx;}

            /**
             * @brief Finalizes the spline.
             */
            void _Finalize();

            /**
             * @brief Interrupts the spline.
             */
            void _Interrupt() { splineflags.done = true;}

        public:
            /**
             * @brief Initializes the spline with the given arguments.
             * @param args The arguments for initializing the spline.
             */
            void Initialize(const MoveSplineInitArgs& args);

            /**
             * @brief Checks if the spline is initialized.
             * @return bool True if the spline is initialized, false otherwise.
             */
            bool Initialized() const { return !spline.empty();}

            /**
             * @brief Default constructor for MoveSpline.
             */
            explicit MoveSpline();

            /**
             * @brief Updates the state of the spline with a handler.
             * @param difftime The time difference in milliseconds.
             * @param handler The handler to process the update result.
             */
            template<class UpdateHandler>
            void updateState(int32 difftime, UpdateHandler& handler)
            {
                MANGOS_ASSERT(Initialized());
                do
                {
                    handler(_updateState(difftime));
                }
                while (difftime > 0);
            }

            /**
             * @brief Updates the state of the spline.
             * @param difftime The time difference in milliseconds.
             */
            void updateState(int32 difftime)
            {
                MANGOS_ASSERT(Initialized());
                do { _updateState(difftime); }
                while (difftime > 0);
            }

            /**
             * @brief Computes the current position on the spline.
             * @return Location The current position on the spline.
             */
            Location ComputePosition() const;

            /**
             * @brief Gets the ID of the spline.
             * @return uint32 The ID of the spline.
             */
            uint32 GetId() const { return m_Id;}

            /**
             * @brief Checks if the spline is finalized.
             * @return bool True if the spline is finalized, false otherwise.
             */
            bool Finalized() const { return splineflags.done; }

            /**
             * @brief Checks if the spline is cyclic.
             * @return bool True if the spline is cyclic, false otherwise.
             */
            bool isCyclic() const { return splineflags.cyclic;}

            /**
             * @brief Gets the final destination of the spline.
             * @return const Vector3 The final destination of the spline.
             */
            const Vector3 FinalDestination() const { return Initialized() ? spline.getPoint(spline.last()) : Vector3();}

            /**
             * @brief Gets the current destination of the spline.
             * @return const Vector3 The current destination of the spline.
             */
            const Vector3 CurrentDestination() const { return Initialized() ? spline.getPoint(point_Idx + 1) : Vector3();}

            /**
             * @brief Gets the current path index.
             * @return int32 The current path index.
             */
            int32 currentPathIdx() const;

            /**
             * @brief Gets the duration of the spline.
             * @return int32 The duration of the spline.
             */
            int32 Duration() const { return spline.length();}

            /**
             * @brief Converts the spline to a string representation.
             * @return std::string The string representation of the spline.
             */
            std::string ToString() const;
    };
}
#endif // MANGOSSERVER_MOVEPLINE_H
