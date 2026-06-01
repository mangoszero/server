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

#ifndef MANGOS_CONFUSEDMOVEMENTGENERATOR_H
#define MANGOS_CONFUSEDMOVEMENTGENERATOR_H

#include "MovementGenerator.h"

/**
 * @brief ConfusedMovementGenerator is a movement generator that makes a unit move in a confused manner.
 */
template<class T>
class ConfusedMovementGenerator
    : public MovementGeneratorMedium< T, ConfusedMovementGenerator<T> >
{
    public:
        /**
         * @brief Constructor for ConfusedMovementGenerator.
         */
        explicit ConfusedMovementGenerator() : i_nextMoveTime(0), i_x(0.0f), i_y(0.0f), i_z(0.0f) {}

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
        MovementGeneratorType GetMovementGeneratorType() const override { return CONFUSED_MOTION_TYPE; }

    private:
        TimeTracker i_nextMoveTime; ///< Time tracker for the next move.
        float i_x, i_y, i_z; ///< Coordinates for the next move.
};

#endif // MANGOS_CONFUSEDMOVEMENTGENERATOR_H
