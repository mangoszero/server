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

#ifndef MANGOS_MOTIONMASTER_H
#define MANGOS_MOTIONMASTER_H

#include "Common.h"
#include <stack>
#include <vector>

class MovementGenerator;
class Unit;

// Creature Entry ID used for waypoints show, visible only for GMs
#define VISUAL_WAYPOINT 1

// Values 0 ... MAX_DB_MOTION_TYPE-1 used in DB
enum MovementGeneratorType
{
    IDLE_MOTION_TYPE                = 0,                    // IdleMovementGenerator.h
    RANDOM_MOTION_TYPE              = 1,                    // RandomMovementGenerator.h
    WAYPOINT_MOTION_TYPE            = 2,                    // WaypointMovementGenerator.h
    MAX_DB_MOTION_TYPE              = 3,                    // *** this and below motion types can't be set in DB.

    CONFUSED_MOTION_TYPE            = 4,                    // ConfusedMovementGenerator.h
    CHASE_MOTION_TYPE               = 5,                    // TargetedMovementGenerator.h
    HOME_MOTION_TYPE                = 6,                    // HomeMovementGenerator.h
    FLIGHT_MOTION_TYPE              = 7,                    // WaypointMovementGenerator.h
    POINT_MOTION_TYPE               = 8,                    // PointMovementGenerator.h
    FLEEING_MOTION_TYPE             = 9,                    // FleeingMovementGenerator.h
    DISTRACT_MOTION_TYPE            = 10,                   // IdleMovementGenerator.h
    ASSISTANCE_MOTION_TYPE          = 11,                   // PointMovementGenerator.h (first part of flee for assistance)
    ASSISTANCE_DISTRACT_MOTION_TYPE = 12,                   // IdleMovementGenerator.h (second part of flee for assistance)
    TIMED_FLEEING_MOTION_TYPE       = 13,                   // FleeingMovementGenerator.h (alt.second part of flee for assistance)
    FOLLOW_MOTION_TYPE              = 14,                   // TargetedMovementGenerator.h
    EFFECT_MOTION_TYPE              = 15,

    EXTERNAL_WAYPOINT_MOVE          = 256,                  // Only used in CreatureAI::MovementInform when a waypoint is reached. The pathId >= 0 is added as additional value
    EXTERNAL_WAYPOINT_MOVE_START    = 512,                  // Only used in CreatureAI::MovementInform when a waypoint is started. The pathId >= 0 is added as additional value
    EXTERNAL_WAYPOINT_FINISHED_LAST = 1024,                 // Only used in CreatureAI::MovementInform when the wait time of the last wp is finished. The pathId >= 0 is added as additional value
};

enum MMCleanFlag
{
    MMCF_NONE   = 0,
    MMCF_UPDATE = 1,                                        // Clear or Expire called from update
    MMCF_RESET  = 2                                         // Flag if need top()->Reset()
};

/**
 * @brief MotionMaster is responsible for managing the movement generators for a unit.
 */
class MotionMaster : private std::stack<MovementGenerator*>
{
    private:
        typedef std::stack<MovementGenerator*> Impl;
        typedef std::vector<MovementGenerator*> ExpireList;

    public:
        /**
         * @brief Constructor for MotionMaster.
         * @param unit Pointer to the unit.
         */
        explicit MotionMaster(Unit* unit) : m_owner(unit), m_expList(NULL), m_cleanFlag(MMCF_NONE) {}

        /**
         * @brief Destructor for MotionMaster.
         */
        ~MotionMaster();

        /**
         * @brief Initializes the MotionMaster.
         */
        void Initialize();

        /**
         * @brief Gets the current movement generator.
         * @return Pointer to the current movement generator.
         */
        MovementGenerator const* GetCurrent() const { return top(); }

        using Impl::top;
        using Impl::empty;

        typedef Impl::container_type::const_iterator const_iterator;
        const_iterator begin() const { return Impl::c.begin(); }
        const_iterator end() const { return Impl::c.end(); }

        /**
         * @brief Updates the motion of the unit.
         * @param diff Time difference.
         */
        void UpdateMotion(uint32 diff);

        /**
         * @brief Clears the movement generators.
         * @param reset Whether to reset the movement generators.
         * @param all Whether to clear all movement generators.
         */
        void Clear(bool reset = true, bool all = false)
        {
            if (m_cleanFlag & MMCF_UPDATE)
            {
                DelayedClean(reset, all);
            }
            else
            {
                DirectClean(reset, all);
            }
        }

        /**
         * @brief Expires the current movement generator.
         * @param reset Whether to reset the movement generator.
         */
        void MovementExpired(bool reset = true)
        {
            if (m_cleanFlag & MMCF_UPDATE)
            {
                DelayedExpire(reset);
            }
            else
            {
                DirectExpire(reset);
            }
        }

        /**
         * @brief Moves the unit to idle state.
         */
        void MoveIdle();

        /**
         * @brief Moves the unit randomly around a point.
         * @param x X-coordinate of the center point.
         * @param y Y-coordinate of the center point.
         * @param z Z-coordinate of the center point.
         * @param radius Radius of the random movement.
         * @param verticalZ Vertical offset for the movement.
         */
        void MoveRandomAroundPoint(float x, float y, float z, float radius, float verticalZ = 0.0f);

        /**
         * @brief Moves the unit to its home position.
         */
        void MoveTargetedHome();

        /**
         * @brief Makes the unit follow a target.
         * @param target Pointer to the target unit.
         * @param dist Distance to maintain from the target.
         * @param angle Angle to maintain from the target.
         */
        void MoveFollow(Unit* target, float dist, float angle);

        /**
         * @brief Makes the unit chase a target.
         * @param target Pointer to the target unit.
         * @param dist Distance to maintain from the target.
         * @param angle Angle to maintain from the target.
         */
        void MoveChase(Unit* target, float dist = 0.0f, float angle = 0.0f);

        /**
         * @brief Makes the unit move in a confused manner.
         */
        void MoveConfused();

        /**
         * @brief Makes the unit flee from an enemy.
         * @param enemy Pointer to the enemy unit.
         * @param timeLimit Time limit for the fleeing movement.
         */
        void MoveFleeing(Unit* enemy, uint32 timeLimit = 0);

        /**
         * @brief Moves the unit to a specific point.
         * @param id ID of the movement.
         * @param x X-coordinate of the destination.
         * @param y Y-coordinate of the destination.
         * @param z Z-coordinate of the destination.
         * @param generatePath Whether to generate a path to the destination.
         */
        void MovePoint(uint32 id, float x, float y, float z, bool generatePath = true);

        /**
         * @brief Makes the unit seek assistance at a specific point.
         * @param x X-coordinate of the assistance point.
         * @param y Y-coordinate of the assistance point.
         * @param z Z-coordinate of the assistance point.
         */
        void MoveSeekAssistance(float x, float y, float z);

        /**
         * @brief Makes the unit seek assistance and then distract.
         * @param timer Time for the distraction.
         */
        void MoveSeekAssistanceDistract(uint32 timer);

        /**
         * @brief Moves the unit along a waypoint path.
         * @param id ID of the waypoint path.
         * @param source Source of the waypoint path.
         * @param initialDelay Initial delay before starting the movement.
         * @param overwriteEntry Entry to overwrite.
         */
        void MoveWaypoint(int32 id = 0, uint32 source = 0, uint32 initialDelay = 0, uint32 overwriteEntry = 0);

        /**
         * @brief Moves the unit along a taxi flight path.
         * @param path ID of the flight path.
         * @param pathnode Node of the flight path.
         */
        void MoveTaxiFlight(uint32 path, uint32 pathnode);

        /**
         * @brief Makes the unit distract for a specified time.
         * @param timeLimit Time limit for the distraction.
         */
        void MoveDistract(uint32 timeLimit);

        /**
         * @brief Makes the unit fall.
         */
        void MoveFall();

        /**
         * @brief Makes the unit fly or land.
         * @param id ID of the movement.
         * @param x X-coordinate of the destination.
         * @param y Y-coordinate of the destination.
         * @param z Z-coordinate of the destination.
         * @param liftOff Whether the unit should lift off or land.
         */
        void MoveFlyOrLand(uint32 id, float x, float y, float z, bool liftOff);

        /**
         * @brief Gets the type of the current movement generator.
         * @return The type of the current movement generator.
         */
        MovementGeneratorType GetCurrentMovementGeneratorType() const;

        /**
         * @brief Propagates the speed change to the movement generators.
         */
        void PropagateSpeedChange();

        /**
         * @brief Sets the next waypoint for the unit.
         * @param pointId ID of the next waypoint.
         * @return True if the next waypoint was successfully set, false otherwise.
         */
        bool SetNextWaypoint(uint32 pointId);

        /**
         * @brief Gets the last reached waypoint.
         * @return The ID of the last reached waypoint.
         */
        uint32 getLastReachedWaypoint() const;

        /**
         * @brief Gets the waypoint path information.
         * @param oss Output stream to store the waypoint path information.
         */
        void GetWaypointPathInformation(std::ostringstream& oss) const;

        /**
         * @brief Gets the destination coordinates.
         * @param x Reference to the X-coordinate.
         * @param y Reference to the Y-coordinate.
         * @param z Reference to the Z-coordinate.
         * @return True if the destination coordinates were successfully obtained, false otherwise.
         */
        bool GetDestination(float& x, float& y, float& z);

    private:
        /**
         * @brief Mutates the movement generator.
         * @param m Pointer to the movement generator.
         */
        void Mutate(MovementGenerator* m);                  // Use Move* functions instead

        /**
         * @brief Directly clears the movement generators.
         * @param reset Whether to reset the movement generators.
         * @param all Whether to clear all movement generators.
         */
        void DirectClean(bool reset, bool all);

        /**
         * @brief Delays the clearing of the movement generators.
         * @param reset Whether to reset the movement generators.
         * @param all Whether to clear all movement generators.
         */
        void DelayedClean(bool reset, bool all);

        /**
         * @brief Directly expires the current movement generator.
         * @param reset Whether to reset the movement generator.
         */
        void DirectExpire(bool reset);

        /**
         * @brief Delays the expiration of the current movement generator.
         * @param reset Whether to reset the movement generator.
         */
        void DelayedExpire(bool reset);

        Unit*       m_owner; ///< Pointer to the owner unit.
        ExpireList* m_expList; ///< List of expired movement generators.
        uint8       m_cleanFlag; ///< Flag for cleaning the movement generators.
};

#endif // MANGOS_MOTIONMASTER_H
