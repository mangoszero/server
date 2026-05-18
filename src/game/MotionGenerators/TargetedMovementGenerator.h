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

#ifndef MANGOS_TARGETEDMOVEMENTGENERATOR_H
#define MANGOS_TARGETEDMOVEMENTGENERATOR_H

#include "MovementGenerator.h"
#include "FollowerReference.h"
#include "G3D/Vector3.h"
#include "PathFinder.h" // Include the header file for PathFinder

class PathFinder;

/**
 * @brief Base class for targeted movement generators.
 */
class TargetedMovementGeneratorBase
{
    public:
        /**
         * @brief Constructor for TargetedMovementGeneratorBase.
         * @param target Reference to the target unit.
         */
        TargetedMovementGeneratorBase(Unit& target) { i_target.link(&target, this); }

        /**
         * @brief Stops following the target.
         */
        void stopFollowing() { }

    protected:
        FollowerReference i_target; ///< Reference to the target unit.
};

/**
 * @brief Template class for medium targeted movement generators.
 * @tparam T Type of the unit (Player or Creature).
 * @tparam D Derived class type.
 */
template<class T, typename D>
class TargetedMovementGeneratorMedium
    : public MovementGeneratorMedium< T, D >, public TargetedMovementGeneratorBase
{
    protected:
        /**
         * @brief Constructor for TargetedMovementGeneratorMedium.
         * @param target Reference to the target unit.
         * @param offset Offset distance from the target.
         * @param angle Angle to maintain from the target.
         */
        TargetedMovementGeneratorMedium(Unit& target, float offset, float angle) :
            TargetedMovementGeneratorBase(target),
            i_recheckDistance(0),
            i_offset(offset), i_angle(angle),
            m_speedChanged(false), i_targetReached(false),
            i_path(NULL)
        {
        }

        /**
         * @brief Destructor for TargetedMovementGeneratorMedium.
         */
        ~TargetedMovementGeneratorMedium() { delete i_path; }

    public:
        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(T&, const uint32&);

        /**
         * @brief Checks if the target is reachable.
         * @return True if the target is reachable, false otherwise.
         */
        bool IsReachable() const;

        /**
         * @brief Gets the target unit.
         * @return Pointer to the target unit.
         */
        Unit* GetTarget() const { return i_target.getTarget(); }

        /**
         * @brief Called when the unit's speed changes.
         */
        void unitSpeedChanged() { m_speedChanged = true; }

    protected:
        /**
         * @brief Sets the target location for the unit.
         * @param owner Reference to the unit.
         * @param updateDestination Whether to update the destination.
         */
        void _setTargetLocation(T&, bool updateDestination);

        /**
         * @brief Checks if a new position is required.
         * @param owner Reference to the unit.
         * @param x X-coordinate of the position.
         * @param y Y-coordinate of the position.
         * @param z Z-coordinate of the position.
         * @return True if a new position is required, false otherwise.
         */
        bool RequiresNewPosition(T& owner, float x, float y, float z) const;

        /**
         * @brief Gets the dynamic target distance.
         * @param owner Reference to the unit.
         * @param forRangeCheck Whether the distance is for range check.
         * @return The dynamic target distance.
         */
        virtual float GetDynamicTargetDistance(T& /*owner*/, bool /*forRangeCheck*/) const { return i_offset; }

        TimeTracker i_recheckDistance; ///< Time tracker for rechecking distance.
        float i_offset; ///< Offset distance from the target.
        float i_angle; ///< Angle to maintain from the target.
        G3D::Vector3 m_prevTargetPos; ///< Previous target position.
        bool m_speedChanged : 1; ///< Indicates if the speed has changed.
        bool i_targetReached : 1; ///< Indicates if the target has been reached.
        PathFinder* i_path; ///< Path finder for the movement.
};

/**
 * @brief ChaseMovementGenerator is a movement generator that makes a unit chase a target.
 * @tparam T Type of the unit (Player or Creature).
 */
template<class T>
class ChaseMovementGenerator : public TargetedMovementGeneratorMedium<T, ChaseMovementGenerator<T> >
{
    public:
        /**
         * @brief Constructor for ChaseMovementGenerator.
         * @param target Reference to the target unit.
         * @param offset Offset distance from the target.
         * @param angle Angle to maintain from the target.
         */
        ChaseMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGeneratorMedium<T, ChaseMovementGenerator<T> >(target, offset, angle) {}

        /**
         * @brief Destructor for ChaseMovementGenerator.
         */
        ~ChaseMovementGenerator() {}

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return CHASE_MOTION_TYPE; }

        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(T&);

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(T&);

        /**
         * @brief Interrupts the movement generator.
         * @param owner Reference to the unit.
         */
        void Interrupt(T&);

        /**
         * @brief Resets the movement generator.
         * @param owner Reference to the unit.
         */
        void Reset(T&);

        /**
         * @brief Clears the unit's move state.
         * @param u Reference to the unit.
         */
        static void _clearUnitStateMove(T& u);

        /**
         * @brief Adds the unit's move state.
         * @param u Reference to the unit.
         */
        static void _addUnitStateMove(T& u);

        /**
         * @brief Checks if walking is enabled.
         * @return False, as walking is not enabled for chase movement.
         */
        bool EnableWalking() const { return false;}

        /**
         * @brief Checks if the target is lost.
         * @param u Reference to the unit.
         * @return True if the target is lost, false otherwise.
         */
        bool _lostTarget(T& u) const;

        /**
         * @brief Called when the target is reached.
         * @param owner Reference to the unit.
         */
        void _reachTarget(T&);

    protected:
        /**
         * @brief Gets the dynamic target distance.
         * @param owner Reference to the unit.
         * @param forRangeCheck Whether the distance is for range check.
         * @return The dynamic target distance.
         */
        float GetDynamicTargetDistance(T& owner, bool forRangeCheck) const override;
};

/**
 * @brief FollowMovementGenerator is a movement generator that makes a unit follow a target.
 * @tparam T Type of the unit (Player or Creature).
 */
template<class T>
class FollowMovementGenerator : public TargetedMovementGeneratorMedium<T, FollowMovementGenerator<T> >
{
    public:
        /**
         * @brief Constructor for FollowMovementGenerator.
         * @param target Reference to the target unit.
         */
        FollowMovementGenerator(Unit& target)
            : TargetedMovementGeneratorMedium<T, FollowMovementGenerator<T> >(target) {}

        /**
         * @brief Constructor for FollowMovementGenerator with offset and angle.
         * @param target Reference to the target unit.
         * @param offset Offset distance from the target.
         * @param angle Angle to maintain from the target.
         */
        FollowMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGeneratorMedium<T, FollowMovementGenerator<T> >(target, offset, angle) {}

        /**
         * @brief Destructor for FollowMovementGenerator.
         */
        ~FollowMovementGenerator() {}

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return FOLLOW_MOTION_TYPE; }

        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(T&);

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(T&);

        /**
         * @brief Interrupts the movement generator.
         * @param owner Reference to the unit.
         */
        void Interrupt(T&);

        /**
         * @brief Resets the movement generator.
         * @param owner Reference to the unit.
         */
        void Reset(T&);

        /**
         * @brief Clears the unit's move state.
         * @param u Reference to the unit.
         */
        static void _clearUnitStateMove(T& u);

        /**
         * @brief Adds the unit's move state.
         * @param u Reference to the unit.
         */
        static void _addUnitStateMove(T& u);

        /**
         * @brief Checks if walking is enabled.
         * @return True if walking is enabled, false otherwise.
         */
        bool EnableWalking() const;

        /**
         * @brief Checks if the target is lost.
         * @param owner Reference to the unit.
         * @return False, as the target is not lost for follow movement.
         */
        bool _lostTarget(T&) const { return false; }

        /**
         * @brief Called when the target is reached.
         * @param owner Reference to the unit.
         */
        void _reachTarget(T&) {}

    private:
        /**
         * @brief Updates the unit's speed.
         * @param owner Reference to the unit.
         */
        void _updateSpeed(T& u);

    protected:
        /**
         * @brief Gets the dynamic target distance.
         * @param owner Reference to the unit.
         * @param forRangeCheck Whether the distance is for range check.
         * @return The dynamic target distance.
         */
        float GetDynamicTargetDistance(T& owner, bool forRangeCheck) const override;
};

#endif // MANGOS_TARGETEDMOVEMENTGENERATOR_H
