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
 * @brief The strategy behind a unit's movement. One is active at a time — the top
 *        of the MotionMaster's stack.
 *
 * Everything here speaks Unit. There is no Player/Creature template and no
 * curiously-recurring base: the handful of places where the two owner types really
 * do behave differently are a `GetTypeId()` check inside the one function that
 * cares, which is both shorter and easier to follow than a type parameter threaded
 * through the whole hierarchy.
 */
class MovementGenerator
{
    public:
        virtual ~MovementGenerator() = default;

        /// Called before the generator is pushed onto the motion stack.
        ///
        /// NOTE: it runs BEFORE the push, so `owner.GetMotionMaster()->top()` here
        /// is still the generator being replaced. HomeMovementGenerator depends on
        /// that to ask it where "home" is; anything else that needs the outgoing
        /// generator must likewise capture it now, not on the first tick.
        virtual void Initialize(Unit& owner) = 0;

        /// Called after the generator is removed from the motion stack.
        virtual void Finalize(Unit& owner) = 0;

        /// Called before losing top position (a new generator is being pushed above).
        virtual void Interrupt(Unit& owner) = 0;

        /// Called after regaining top position (the generator above was removed).
        virtual void Reset(Unit& owner) = 0;

        /// One tick. Returning false asks the MotionMaster to pop this generator.
        virtual bool Update(Unit& owner, uint32 diff) = 0;

        virtual MovementGeneratorType GetMovementGeneratorType() const = 0;

        /// The owner's speed changed, so any leg in flight is paced wrong.
        virtual void unitSpeedChanged() {}

        /// Where the owner should be sent when it evades, if this generator knows
        /// (a patroller resumes where combat pulled it off its path). False when it
        /// has no opinion and the caller should fall back to the spawn point.
        virtual bool GetResetPosition(Unit& /*owner*/, float& /*x*/, float& /*y*/,
                                      float& /*z*/, float& /*o*/) const
        {
            return false;
        }

        /// False once a route to the goal only got partway there.
        virtual bool IsReachable() const { return true; }

        /// Still the top generator? Call after anything that may have re-entered the
        /// motion stack (an AI hook, a script).
        bool IsActive(Unit& owner);
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
     * @param data Pointer to the creature the generator will drive.
     * @return Pointer to the created movement generator.
     */
    MovementGenerator* Create(void* data) const override;
};

typedef FactoryHolder<MovementGenerator, MovementGeneratorType> MovementGeneratorCreator;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRegistry MovementGeneratorRegistry;
typedef FactoryHolder<MovementGenerator, MovementGeneratorType>::FactoryHolderRepository MovementGeneratorRepository;

#endif // MANGOS_MOVEMENTGENERATOR_H
