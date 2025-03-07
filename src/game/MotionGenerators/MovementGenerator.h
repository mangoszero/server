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

#ifndef MANGOS_MOVEMENTGENERATOR_H
#define MANGOS_MOVEMENTGENERATOR_H

#include "Platform/Define.h"
#include "Dynamic/FactoryHolder.h"
#include "MotionMaster.h"
#include "Timer.h"

class Unit;
class Creature;
class Player;

/**
 * @brief Base class for all movement generators.
 */
class MovementGenerator
{
    public:
        virtual ~MovementGenerator();

        /**
         * @brief Called before adding movement generator to motion stack.
         * @param owner Reference to the unit.
         */
        virtual void Initialize(Unit&) = 0;

        /**
         * @brief Called after removing movement generator from motion stack.
         * @param owner Reference to the unit.
         */
        virtual void Finalize(Unit&) = 0;

        /**
         * @brief Called before losing top position (before pushing new movement generator above).
         * @param owner Reference to the unit.
         */
        virtual void Interrupt(Unit&) = 0;

        /**
         * @brief Called after returning movement generator to top position (after removing above movement generator).
         * @param owner Reference to the unit.
         */
        virtual void Reset(Unit&) = 0;

        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param time_diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        virtual bool Update(Unit&, const uint32& time_diff) = 0;

        /**
         * @brief Gets the type of the movement generator.
         * @return The type of the movement generator.
         */
        virtual MovementGeneratorType GetMovementGeneratorType() const = 0;

        /**
         * @brief Called when the unit's speed changes.
         */
        virtual void unitSpeedChanged() { }

        /**
         * @brief Used by Evade code to select point to evade with expected restart default movement.
         * @param owner Reference to the unit.
         * @param x Reference to the X-coordinate.
         * @param y Reference to the Y-coordinate.
         * @param z Reference to the Z-coordinate.
         * @param o Reference to the orientation.
         * @return True if the reset position was successfully obtained, false otherwise.
         */
        virtual bool GetResetPosition(Unit&, float& /*x*/, float& /*y*/, float& /*z*/, float& /*o*/) const { return false; }

        /**
         * @brief Checks if the given destination is unreachable due to pathfinding or other reasons.
         * @return True if the destination is reachable, false otherwise.
         */
        virtual bool IsReachable() const { return true; }

        /**
         * @brief Checks if the movement generator is still active (top movement generator) after some not safe for this calls.
         * @param owner Reference to the unit.
         * @return True if the movement generator is still active, false otherwise.
         */
        bool IsActive(Unit& u);
};

/**
 * @brief Template class for medium movement generators.
 * @tparam T Type of the unit (Player or Creature).
 * @tparam D Derived class type.
 */
template<class T, class D>
class MovementGeneratorMedium : public MovementGenerator
{
    public:
        /**
         * @brief Initializes the movement generator.
         * @param owner Reference to the unit.
         */
        void Initialize(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Initialize(*((T*)&u));
        }

        /**
         * @brief Finalizes the movement generator.
         * @param owner Reference to the unit.
         */
        void Finalize(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Finalize(*((T*)&u));
        }

        /**
         * @brief Interrupts the movement generator.
         * @param owner Reference to the unit.
         */
        void Interrupt(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Interrupt(*((T*)&u));
        }

        /**
         * @brief Resets the movement generator.
         * @param owner Reference to the unit.
         */
        void Reset(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Reset(*((T*)&u));
        }

        /**
         * @brief Updates the movement generator.
         * @param owner Reference to the unit.
         * @param time_diff Time difference.
         * @return True if the update was successful, false otherwise.
         */
        bool Update(Unit& u, const uint32& time_diff) override
        {
            // u->AssertIsType<T>();
            return (static_cast<D*>(this))->Update(*((T*)&u), time_diff);
        }

        /**
         * @brief Gets the reset position for the unit.
         * @param owner Reference to the unit.
         * @param x Reference to the X-coordinate.
         * @param y Reference to the Y-coordinate.
         * @param z Reference to the Z-coordinate.
         * @param o Reference to the orientation.
         * @return True if the reset position was successfully obtained, false otherwise.
         */
        bool GetResetPosition(Unit& u, float& x, float& y, float& z, float& o) const override
        {
            // u->AssertIsType<T>();
            return (static_cast<D const*>(this))->GetResetPosition(*((T*)&u), x, y, z, o);
        }

    public:
        // Will not link if not overridden in the generators
        void Initialize(T& u);
        void Finalize(T& u);
        void Interrupt(T& u);
        void Reset(T& u);
        bool Update(T& u, const uint32& time_diff);

        // Not always need to be overridden
        bool GetResetPosition(T& /*u*/, float& /*x*/, float& /*y*/, float& /*z*/, float& /*o*/) const { return false; }
};

/**
 * @brief SelectableMovement is a factory holder for movement generators.
 */
struct SelectableMovement : public FactoryHolder<MovementGenerator, MovementGeneratorType>
{
    /**
     * @brief Constructor for SelectableMovement.
     * @param mgt Type of the movement generator.
     */
    SelectableMovement(MovementGeneratorType mgt) : FactoryHolder<MovementGenerator, MovementGeneratorType>(mgt) {}
};

/**
 * @brief Template class for movement generator factories.
 * @tparam REAL_MOVEMENT Type of the real movement generator.
 */
template<class REAL_MOVEMENT>
struct MovementGeneratorFactory : public SelectableMovement
{
    /**
     * @brief Constructor for MovementGeneratorFactory.
     * @param mgt Type of the movement generator.
     */
    MovementGeneratorFactory(MovementGeneratorType mgt) : SelectableMovement(mgt) {}

    /**
     * @brief Creates a new movement generator.
     * @param data Pointer to the data.
     * @return Pointer to the created movement generator.
     */
    MovementGenerator* Create(void*) const override;
};

typedef FactoryHolder<MovementGenerator, MovementGeneratorType> MovementGeneratorCreator;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRegistry MovementGeneratorRegistry;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRepository MovementGeneratorRepository;

#endif // MANGOS_MOVEMENTGENERATOR_H
