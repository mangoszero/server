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

#ifndef MANGOS_RANDOMMOTIONGENERATOR_H
#define MANGOS_RANDOMMOTIONGENERATOR_H

#include "MovementGenerator.h"

// Define chance for creature to not stop after reaching a waypoint
#define MOVEMENT_RANDOM_MMGEN_CHANCE_NO_BREAK 30

/**
 * @brief RandomMovementGenerator is a movement generator that makes a unit move randomly within a specified radius.
 * @tparam T Type of the unit (Player or Creature).
 */
template<class T>
class RandomMovementGenerator
    : public MovementGeneratorMedium< T, RandomMovementGenerator<T> >
{
    public:
        /**
         * @brief Constructor for RandomMovementGenerator.
         * @param creature Reference to the creature.
         */
        explicit RandomMovementGenerator(const Creature& creature);

        /**
         * @brief Constructor for RandomMovementGenerator with specified coordinates and radius.
         * @param x X-coordinate of the center.
         * @param y Y-coordinate of the center.
         * @param z Z-coordinate of the center.
         * @param radius Radius within which the unit will move.
         * @param verticalZ Vertical offset for the movement.
         */
        explicit RandomMovementGenerator(float x, float y, float z, float radius, float verticalZ = 0.0f);

        /**
         * @brief Sets a random location for the unit to move to.
         * @param owner Reference to the unit.
         */
        void _setRandomLocation(T& owner);

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
        MovementGeneratorType GetMovementGeneratorType() const override { return RANDOM_MOTION_TYPE; }

    private:
        TimeTracker i_nextMoveTime; ///< Time tracker for the next move.
        float i_x, i_y, i_z; ///< Coordinates of the center.
        float i_radius; ///< Radius within which the unit will move.
        float i_verticalZ; ///< Vertical offset for the movement.
};

#endif // MANGOS_RANDOMMOTIONGENERATOR_H
