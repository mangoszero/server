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

#ifndef MANGOS_IDLEMOVEMENTGENERATOR_H
#define MANGOS_IDLEMOVEMENTGENERATOR_H

#include "MovementGenerator.h"

/**
 * @brief IdleMovementGenerator is a movement generator that does nothing.
 */
class IdleMovementGenerator : public MovementGenerator
{
    public:
        /**
         * @brief Initializes the movement generator.
         * @param unit Reference to the unit.
         */
        void Initialize(Unit&) override {}

        /**
         * @brief Finalizes the movement generator.
         * @param unit Reference to the unit.
         */
        void Finalize(Unit&) override {}

        /**
         * @brief Interrupts the movement generator.
         * @param unit Reference to the unit.
         */
        void Interrupt(Unit&) override {}

        /**
         * @brief Resets the movement generator.
         * @param unit Reference to the unit.
         */
        void Reset(Unit&) override;

        /**
         * @brief Updates the movement generator.
         * @param unit Reference to the unit.
         * @param diff Time difference.
         * @return Always returns true.
         */
        bool Update(Unit&, const uint32&) override { return true; }

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return IDLE_MOTION_TYPE; }
};

/**
 * @brief Global instance of IdleMovementGenerator.
 */
extern IdleMovementGenerator si_idleMovement;

/**
 * @brief DistractMovementGenerator is a movement generator that distracts the unit for a specified time.
 */
class DistractMovementGenerator : public MovementGenerator
{
    public:
        /**
         * @brief Constructor for DistractMovementGenerator.
         * @param timer Time to distract the unit.
         */
        explicit DistractMovementGenerator(uint32 timer) : m_timer(timer) {}

        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(Unit& owner) override;

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(Unit& owner) override;

        /**
         * @brief Interrupts the movement generator.
         * @param unit Reference to the unit.
         */
        void Interrupt(Unit&) override;

        /**
         * @brief Resets the movement generator.
         * @param unit Reference to the unit.
         */
        void Reset(Unit&) override;

        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param time_diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(Unit& owner, const uint32& time_diff) override;

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return DISTRACT_MOTION_TYPE; }

    private:
        uint32 m_timer; ///< Time to distract the unit.
};

/**
 * @brief AssistanceDistractMovementGenerator is a movement generator that distracts the unit for assistance.
 */
class AssistanceDistractMovementGenerator : public DistractMovementGenerator
{
    public:
        /**
         * @brief Constructor for AssistanceDistractMovementGenerator.
         * @param timer Time to distract the unit.
         */
        AssistanceDistractMovementGenerator(uint32 timer) :
            DistractMovementGenerator(timer) {}

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return ASSISTANCE_DISTRACT_MOTION_TYPE; }

        /**
         * @brief Finalizes the movement generator.
         * @param unit Reference to the unit.
         */
        void Finalize(Unit& unit) override;
};

#endif // MANGOS_IDLEMOVEMENTGENERATOR_H
