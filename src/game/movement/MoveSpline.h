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

#ifndef MANGOSSERVER_MOVEPLINE_H
#define MANGOSSERVER_MOVEPLINE_H

#include "spline.h"
#include "MoveSplineInitArgs.h"

namespace Movement
{
    enum MonsterMoveType
    {
        MonsterMoveNormal = 0,
        MonsterMoveStop = 1,
        MonsterMoveFacingSpot = 2,
        MonsterMoveFacingTarget = 3,
        MonsterMoveFacingAngle = 4
    };

    struct Location : public Vector3
    {
        /**
         * @brief
         *
         */
        Location() : orientation(0) {}
        /**
         * @brief
         *
         * @param x
         * @param y
         * @param z
         * @param o
         */
        Location(float x, float y, float z, float o) : Vector3(x, y, z), orientation(o) {}
        /**
         * @brief
         *
         * @param v
         */
        Location(const Vector3& v) : Vector3(v), orientation(0) {}
        /**
         * @brief
         *
         * @param v
         * @param o
         */
        Location(const Vector3& v, float o) : Vector3(v), orientation(o) {}

        float orientation; /**< TODO */
    };

    /**
     * @brief MoveSpline represents smooth catmullrom or linear curve and point that moves belong it
     *
     * curve can be cyclic - in this case movement will be cyclic
     * point can have vertical acceleration motion componemt(used in fall, parabolic movement)
     *
     */
    class MoveSpline
    {
        public:
            /**
             * @brief
             *
             */
            typedef Spline<int32> MySpline;
            /**
             * @brief
             *
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
            MySpline        spline; /**< TODO */

            FacingInfo      facing; /**< TODO */

            uint32          m_Id; /**< TODO */

            MoveSplineFlag  splineflags; /**< TODO */

            int32           time_passed; /**< TODO */
            int32           point_Idx; /**< TODO */
            int32           point_Idx_offset; /**< TODO */

            /**
             * @brief
             *
             * @param args
             */
            void init_spline(const MoveSplineInitArgs& args);
        protected:

            /**
             * @brief
             *
             * @return const MySpline::ControlArray
             */
            const MySpline::ControlArray& getPath() const { return spline.getPoints();}
            /**
             * @brief
             *
             * @param el
             */
            void computeFallElevation(float& el) const;

            /**
             * @brief
             *
             * @param ms_time_diff
             * @return UpdateResult
             */
            UpdateResult _updateState(int32& ms_time_diff);
            /**
             * @brief
             *
             * @return int32
             */
            int32 next_timestamp() const { return spline.length(point_Idx + 1);}
            /**
             * @brief
             *
             * @return int32
             */
            int32 segment_time_elapsed() const { return next_timestamp() - time_passed;}
            /**
             * @brief
             *
             * @return int32
             */
            int32 timeElapsed() const { return Duration() - time_passed;}
            /**
             * @brief
             *
             * @return int32
             */
            int32 timePassed() const { return time_passed;}

        public:
            /**
             * @brief
             *
             * @return const MySpline
             */
            const MySpline& _Spline() const { return spline;}
            /**
             * @brief
             *
             * @return int32
             */
            int32 _currentSplineIdx() const { return point_Idx;}
            /**
             * @brief
             *
             */
            void _Finalize();
            /**
             * @brief
             *
             */
            void _Interrupt() { splineflags.done = true;}

        public:

            /**
             * @brief
             *
             * @param
             */
            void Initialize(const MoveSplineInitArgs&);
            /**
             * @brief
             *
             * @return bool
             */
            bool Initialized() const { return !spline.empty();}

            /**
             * @brief
             *
             */
            explicit MoveSpline();

            template<class UpdateHandler>
            /**
             * @brief
             *
             * @param difftime
             * @param handler
             */
            void updateState(int32 difftime, UpdateHandler& handler)
            {
                MANGOS_ASSERT(Initialized());
                do
                    { handler(_updateState(difftime)); }
                while (difftime > 0);
            }

            /**
             * @brief
             *
             * @param difftime
             */
            void updateState(int32 difftime)
            {
                MANGOS_ASSERT(Initialized());
                do { _updateState(difftime); }
                while (difftime > 0);
            }

            /**
             * @brief
             *
             * @return Location
             */
            Location ComputePosition() const;

            /**
             * @brief
             *
             * @return uint32
             */
            uint32 GetId() const { return m_Id;}
            /**
             * @brief
             *
             * @return bool
             */
            bool Finalized() const { return splineflags.done; }
            /**
             * @brief
             *
             * @return bool
             */
            bool isCyclic() const { return splineflags.cyclic;}
            /**
             * @brief
             *
             * @return const Vector3
             */
            const Vector3 FinalDestination() const { return Initialized() ? spline.getPoint(spline.last()) : Vector3();}
            /**
             * @brief
             *
             * @return const Vector3
             */
            const Vector3 CurrentDestination() const { return Initialized() ? spline.getPoint(point_Idx + 1) : Vector3();}
            /**
             * @brief
             *
             * @return int32
             */
            int32 currentPathIdx() const;

            /**
             * @brief
             *
             * @return int32
             */
            int32 Duration() const { return spline.length();}

            /**
             * @brief
             *
             * @return std::string
             */
            std::string ToString() const;
    };
}
#endif // MANGOSSERVER_MOVEPLINE_H
