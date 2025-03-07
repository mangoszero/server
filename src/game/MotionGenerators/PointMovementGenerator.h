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

#ifndef MANGOS_POINTMOVEMENTGENERATOR_H
#define MANGOS_POINTMOVEMENTGENERATOR_H

#include "MovementGenerator.h"

/**
 * @brief PointMovementGenerator is a movement generator that makes a unit move to a specific point.
 * @tparam T Type of the unit (Player or Creature).
 */
template<class T>
class PointMovementGenerator
    : public MovementGeneratorMedium< T, PointMovementGenerator<T> >
{
    public:
        /**
         * @brief Constructor for PointMovementGenerator.
         * @param _id ID of the movement.
         * @param _x X-coordinate of the destination.
         * @param _y Y-coordinate of the destination.
         * @param _z Z-coordinate of the destination.
         * @param _generatePath Whether to generate a path to the destination.
         */
        PointMovementGenerator(uint32 _id, float _x, float _y, float _z, bool _generatePath) :
            id(_id), i_x(_x), i_y(_y), i_z(_z), m_generatePath(_generatePath) {}

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
         * @brief Informs the unit about the movement.
         * @param owner Reference to the unit.
         */
        void MovementInform(T& owner);

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return POINT_MOTION_TYPE; }

        /**
         * @brief Gets the destination coordinates.
         * @param x Reference to the X-coordinate.
         * @param y Reference to the Y-coordinate.
         * @param z Reference to the Z-coordinate.
         * @return True if the destination coordinates were successfully obtained, false otherwise.
         */
        bool GetDestination(float& x, float& y, float& z) const { x = i_x; y = i_y; z = i_z; return true; }

    protected:
        uint32 id; ///< ID of the movement.
        float i_x, i_y, i_z; ///< Coordinates of the destination.
        bool m_generatePath; ///< Whether to generate a path to the destination.
};

/**
 * @brief AssistanceMovementGenerator is a movement generator that makes a creature move to assist another unit.
 */
class AssistanceMovementGenerator
    : public PointMovementGenerator<Creature>
{
    public:
        /**
         * @brief Constructor for AssistanceMovementGenerator.
         * @param _x X-coordinate of the destination.
         * @param _y Y-coordinate of the destination.
         * @param _z Z-coordinate of the destination.
         */
        AssistanceMovementGenerator(float _x, float _y, float _z) :
            PointMovementGenerator<Creature>(0, _x, _y, _z, true) {}

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return ASSISTANCE_MOTION_TYPE; }

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
};

/**
 * @brief EffectMovementGenerator is a movement generator that does almost nothing, just prevents previous movegen from interrupting the current effect.
 */
class EffectMovementGenerator : public MovementGenerator
{
    public:
        /**
         * @brief Constructor for EffectMovementGenerator.
         * @param Id ID of the effect.
         */
        explicit EffectMovementGenerator(uint32 Id) : m_Id(Id) {}

        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(Unit&) override {}

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(Unit& owner) override;

        /**
         * @brief Interrupts the movement generator.
         * @param owner Reference to the unit.
         */
        void Interrupt(Unit&) override {}

        /**
         * @brief Resets the movement generator.
         * @param owner Reference to the unit.
         */
        void Reset(Unit&) override {}

        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(Unit& owner, const uint32& diff) override;

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return EFFECT_MOTION_TYPE; }

    private:
        uint32 m_Id; ///< ID of the effect.
};

/**
 * @brief FlyOrLandMovementGenerator is a movement generator that makes a creature fly or land.
 */
class FlyOrLandMovementGenerator : public PointMovementGenerator<Creature>
{
    public:
        /**
         * @brief Constructor for FlyOrLandMovementGenerator.
         * @param _id ID of the movement.
         * @param _x X-coordinate of the destination.
         * @param _y Y-coordinate of the destination.
         * @param _z Z-coordinate of the destination.
         * @param liftOff Whether the creature should lift off or land.
         */
        FlyOrLandMovementGenerator(uint32 _id, float _x, float _y, float _z, bool liftOff) :
            PointMovementGenerator<Creature>(_id, _x, _y, _z, false),
            m_liftOff(liftOff) {}

        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(Unit& owner) override;

    private:
        bool m_liftOff; ///< Whether the creature should lift off or land.
};

#endif // MANGOS_POINTMOVEMENTGENERATOR_H
