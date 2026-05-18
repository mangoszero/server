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
 * @brief Base class for all movement generators
 *
 * Provides the interface for all movement generator implementations.
 * Movement generators control how units (players, creatures, NPCs) move
 * through the game world.
 */
class MovementGenerator
{
    public:
        /**
         * @brief Virtual destructor
         */
        virtual ~MovementGenerator();

        /**
         * @brief Initialize the movement generator
         *
         * Called before adding movement generator to motion stack.
         *
         * @param owner Reference to the unit
         */
        virtual void Initialize(Unit&) = 0;

        /**
         * @brief Finalize the movement generator
         *
         * Called after removing movement generator from motion stack.
         *
         * @param owner Reference to the unit
         */
        virtual void Finalize(Unit&) = 0;

        /**
         * @brief Interrupt the movement generator
         *
         * Called before losing top position (before pushing new
         * movement generator above).
         *
         * @param owner Reference to the unit
         */
        virtual void Interrupt(Unit&) = 0;

        /**
         * @brief Reset the movement generator
         *
         * Called after returning movement generator to top position
         * (after removing above movement generator).
         *
         * @param owner Reference to the unit
         */
        virtual void Reset(Unit&) = 0;

        /**
         * @brief Update the movement generator
         * @param owner Reference to the unit
         * @param time_diff Time difference in milliseconds
         * @return True if update successful, false otherwise
         */
        virtual bool Update(Unit&, const uint32& time_diff) = 0;

        /**
         * @brief Get the type of the movement generator
         * @return Movement generator type
         */
        virtual MovementGeneratorType GetMovementGeneratorType() const = 0;

        /**
         * @brief Called when the unit's speed changes
         */
        virtual void unitSpeedChanged() { }

        /**
         * @brief Get reset position for evade behavior
         *
         * Used by evade code to select point to evade with expected
         * restart of default movement.
         *
         * @param owner Reference to the unit
         * @param x X-coordinate output
         * @param y Y-coordinate output
         * @param z Z-coordinate output
         * @param o Orientation output
         * @return True if reset position obtained, false otherwise
         */
        virtual bool GetResetPosition(Unit&, float& /*x*/, float& /*y*/, float& /*z*/, float& /*o*/) const { return false; }

        /**
         * @brief Check if destination is reachable
         * @return True if reachable, false otherwise
         */
        virtual bool IsReachable() const { return true; }

        /**
         * @brief Check if movement generator is still active
         *
         * Checks if this is the top movement generator after calls
         * that may not be safe for this generator.
         *
         * @param u Reference to the unit
         * @return True if still active, false otherwise
         */
        bool IsActive(Unit& u);
};

/**
 * @brief Template class for medium movement generators
 *
 * Provides type-safe casting for unit-specific movement generators.
 *
 * @tparam T Type of the unit (Player or Creature)
 * @tparam D Derived class type
 */
template<class T, class D>
class MovementGeneratorMedium : public MovementGenerator
{
    public:
        /**
         * @brief Initialize the movement generator
         * @param u Reference to the unit
         */
        void Initialize(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Initialize(*((T*)&u));
        }

        /**
         * @brief Finalize the movement generator
         * @param u Reference to the unit
         */
        void Finalize(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Finalize(*((T*)&u));
        }

        /**
         * @brief Interrupt the movement generator
         * @param u Reference to the unit
         */
        void Interrupt(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Interrupt(*((T*)&u));
        }

        /**
         * @brief Reset the movement generator
         * @param u Reference to the unit
         */
        void Reset(Unit& u) override
        {
            // u->AssertIsType<T>();
            (static_cast<D*>(this))->Reset(*((T*)&u));
        }

        /**
         * @brief Update the movement generator
         * @param u Reference to the unit
         * @param time_diff Time difference in milliseconds
         * @return True if update successful, false otherwise
         */
        bool Update(Unit& u, const uint32& time_diff) override
        {
            // u->AssertIsType<T>();
            return (static_cast<D*>(this))->Update(*((T*)&u), time_diff);
        }

        /**
         * @brief Get reset position for the unit
         * @param u Reference to the unit
         * @param x X-coordinate output
         * @param y Y-coordinate output
         * @param z Z-coordinate output
         * @param o Orientation output
         * @return True if reset position obtained, false otherwise
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
