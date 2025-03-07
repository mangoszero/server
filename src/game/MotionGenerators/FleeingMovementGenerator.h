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

#ifndef MANGOS_FLEEINGMOVEMENTGENERATOR_H
#define MANGOS_FLEEINGMOVEMENTGENERATOR_H

#include "MovementGenerator.h"
#include "ObjectGuid.h"

/**
 * @brief FleeingMovementGenerator is a movement generator that makes a unit flee from a specified target.
 */
template<class T>
class FleeingMovementGenerator
    : public MovementGeneratorMedium< T, FleeingMovementGenerator<T> >
{
    public:
        /**
         * @brief Constructor for FleeingMovementGenerator.
         * @param fright The GUID of the target to flee from.
         */
        FleeingMovementGenerator(ObjectGuid fright) : i_frightGuid(fright), i_nextCheckTime(0) {}

        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(T& owner);

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(T& owner);

        /**
         * @brief Interrupts the movement generator.
         * @param owner Reference to the unit.
         */
        void Interrupt(T& owner);

        /**
         * @brief Resets the movement generator.
         * @param owner Reference to the unit.
         */
        void Reset(T& owner);

        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(T& owner, const uint32& diff);

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return FLEEING_MOTION_TYPE; }

    private:
        /**
         * @brief Sets the target location for the unit to flee to.
         * @param owner Reference to the unit.
         */
        void _setTargetLocation(T& owner);

        /**
         * @brief Gets a point for the unit to flee to.
         * @param owner Reference to the unit.
         * @param x Reference to the x-coordinate.
         * @param y Reference to the y-coordinate.
         * @param z Reference to the z-coordinate.
         * @return True if the point was successfully obtained, false otherwise.
         */
        bool _getPoint(T& owner, float& x, float& y, float& z);

        ObjectGuid i_frightGuid; ///< The GUID of the target to flee from.
        TimeTracker i_nextCheckTime; ///< Time tracker for the next check.
};

/**
 * @brief TimedFleeingMovementGenerator is a movement generator that makes a unit flee from a specified target for a specified time.
 */
class TimedFleeingMovementGenerator
    : public FleeingMovementGenerator<Creature>
{
    public:
        /**
         * @brief Constructor for TimedFleeingMovementGenerator.
         * @param fright The GUID of the target to flee from.
         * @param time The total time to flee.
         */
        TimedFleeingMovementGenerator(ObjectGuid fright, uint32 time) :
            FleeingMovementGenerator<Creature>(fright),
            i_totalFleeTime(time) {}

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return TIMED_FLEEING_MOTION_TYPE; }

        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(Unit& owner, const uint32& diff) override;

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(Unit& owner) override;

    private:
        TimeTracker i_totalFleeTime; ///< Total time to flee.
};

#endif // MANGOS_FLEEINGMOVEMENTGENERATOR_H
