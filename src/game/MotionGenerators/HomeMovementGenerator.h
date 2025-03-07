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

#ifndef MANGOS_HOMEMOVEMENTGENERATOR_H
#define MANGOS_HOMEMOVEMENTGENERATOR_H

#include "MovementGenerator.h"

class Creature;

/**
 * @brief HomeMovementGenerator is a movement generator that returns a creature to its home position.
 */
template < class T >
class HomeMovementGenerator;

template <>
class HomeMovementGenerator<Creature>
    : public MovementGeneratorMedium< Creature, HomeMovementGenerator<Creature> >
{
    public:
        /**
         * @brief Constructor for HomeMovementGenerator.
         */
        HomeMovementGenerator() : arrived(false) {}

        /**
         * @brief Destructor for HomeMovementGenerator.
         */
        ~HomeMovementGenerator() {}

        /**
         * @brief Initializes the movement generator.
         * @param creature Reference to the creature.
         */
        void Initialize(Creature&);

        /**
         * @brief Finalizes the movement generator.
         * @param creature Reference to the creature.
         */
        void Finalize(Creature&);

        /**
         * @brief Interrupts the movement generator.
         * @param creature Reference to the creature.
         */
        void Interrupt(Creature&) {}

        /**
         * @brief Resets the movement generator.
         * @param creature Reference to the creature.
         */
        void Reset(Creature&);

        /**
         * @brief Updates the movement generator.
         * @param creature Reference to the creature.
         * @param diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(Creature&, const uint32&);

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return HOME_MOTION_TYPE; }

    private:
        /**
         * @brief Sets the target location for the creature.
         * @param creature Reference to the creature.
         */
        void _setTargetLocation(Creature&);

        bool arrived; ///< Indicates whether the creature has arrived at its home position.
};

#endif // MANGOS_HOMEMOVEMENTGENERATOR_H
