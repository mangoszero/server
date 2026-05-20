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

#include "Creature.h"
#include "CreatureAI.h"
#include "FleeingMovementGenerator.h"
#include "ObjectAccessor.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "PathFinder.h"

#define MIN_QUIET_DISTANCE 28.0f
#define MAX_QUIET_DISTANCE 43.0f

template<class T>

/**
 * @brief Chooses and launches a new fleeing destination for the owner.
 *
 * @param owner The unit using the movement generator.
 */
void FleeingMovementGenerator<T>::_setTargetLocation(T& owner)
{
    // Ignore if the unit is in a state where it cannot react or move, except for fleeing
    if (owner.hasUnitState((UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE) & ~UNIT_STAT_FLEEING))
    {
        return;
    }

    float x, y, z;
    if (!_getPoint(owner, x, y, z))
    {
        // Random point not found, recheck later
        i_nextCheckTime.Reset(50);
        return;
    }

    owner.addUnitState(UNIT_STAT_FLEEING_MOVE);

    PathFinder path(&owner);
    path.setPathLengthLimit(30.0f);
    path.calculate(x, y, z);
    if (path.getPathType() & PATHFIND_NOPATH)
    {
        // Path not found, recheck later
        i_nextCheckTime.Reset(50);
        return;
    }

    Movement::MoveSplineInit init(owner);
    init.MovebyPath(path.getPath());
    init.SetWalk(false);
    int32 traveltime = init.Launch();
    i_nextCheckTime.Reset(traveltime + urand(800, 1500));
}

template<class T>

/**
 * @brief Computes a suitable flee point away from the frightening source.
 *
 * @param owner The unit using the movement generator.
 * @param x Receives the destination X coordinate.
 * @param y Receives the destination Y coordinate.
 * @param z Receives the destination Z coordinate.
 * @return true if a valid flee point was found; otherwise false.
 */
bool FleeingMovementGenerator<T>::_getPoint(T& owner, float& x, float& y, float& z)
{
    float dist_from_caster, angle_to_caster;
    if (Unit* fright = sObjectAccessor.GetUnit(owner, i_frightGuid))
    {
        dist_from_caster = fright->GetDistance(&owner);
        if (dist_from_caster > 0.2f)
        {
            angle_to_caster = fright->GetAngle(&owner);
        }
        else
        {
            angle_to_caster = frand(0, 2 * M_PI_F);
        }
    }
    else
    {
        dist_from_caster = 0.0f;
        angle_to_caster = frand(0, 2 * M_PI_F);
    }

    float dist, angle;
    if (dist_from_caster < MIN_QUIET_DISTANCE)
    {
        dist = frand(0.4f, 1.3f) * (MIN_QUIET_DISTANCE - dist_from_caster);
        angle = angle_to_caster + frand(-M_PI_F / 8, M_PI_F / 8);
    }
    else if (dist_from_caster > MAX_QUIET_DISTANCE)
    {
        dist = frand(0.4f, 1.0f) * (MAX_QUIET_DISTANCE - MIN_QUIET_DISTANCE);
        angle = -angle_to_caster + frand(-M_PI_F / 4, M_PI_F / 4);
    }
    else    // We are inside quiet range
    {
        dist = frand(0.6f, 1.2f) * (MAX_QUIET_DISTANCE - MIN_QUIET_DISTANCE);
        angle = frand(0, 2 * M_PI_F);
    }

    float curr_x = 0.0, curr_y = 0.0, curr_z = 0.0;
    owner.GetPosition(curr_x, curr_y, curr_z);

    x = curr_x + dist * cos(angle);
    y = curr_y + dist * sin(angle);
    z = curr_z + 0.5f;

    // Try to fix z
    if (!owner.GetMap()->GetHeightInRange(x, y, z))
    {
        return false;
    }

    if (owner.GetTypeId() == TYPEID_PLAYER)
    {
        // Check any collision
        float testZ = z + 0.5f; // Needed to avoid some false positive hit detection of terrain or passable little object
        if (owner.GetMap()->GetHitPosition(curr_x, curr_y, curr_z + 0.5f, x, y, testZ, -0.1f))
        {
            z = testZ;
            if (!owner.GetMap()->GetHeightInRange(x, y, z))
            {
                return false;
            }
        }
    }

    return true;
}

template<class T>

/**
 * @brief Initializes fleeing movement and clears creature targets when needed.
 *
 * @param owner The unit using the movement generator.
 */
void FleeingMovementGenerator<T>::Initialize(T& owner)
{
    owner.addUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
    owner.StopMoving();

    if (owner.GetTypeId() == TYPEID_UNIT)
    {
        ((Creature&) owner).SetWalk(false, false);
        owner.SetTargetGuid(ObjectGuid());
    }

    _setTargetLocation(owner);
}

template<>

/**
 * @brief Finalizes fleeing movement for a player.
 *
 * @param owner The player using the movement generator.
 */
void FleeingMovementGenerator<Player>::Finalize(Player& owner)
{
    owner.clearUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
    owner.StopMoving();
}

template<>

/**
 * @brief Finalizes fleeing movement for a creature and restores walk state.
 *
 * @param owner The creature using the movement generator.
 */
void FleeingMovementGenerator<Creature>::Finalize(Creature& owner)
{
    owner.SetWalk(!owner.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
    owner.clearUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
}

template<class T>

/**
 * @brief Interrupts fleeing movement while keeping the flee state active.
 *
 * @param owner The unit using the movement generator.
 */
void FleeingMovementGenerator<T>::Interrupt(T& owner)
{
    owner.InterruptMoving();
    // Flee state still applied while movegen disabled
    owner.clearUnitState(UNIT_STAT_FLEEING_MOVE);
}

template<class T>

/**
 * @brief Resets fleeing movement by reinitializing the generator.
 *
 * @param owner The unit using the movement generator.
 */
void FleeingMovementGenerator<T>::Reset(T& owner)
{
    Initialize(owner);
}

template<class T>

/**
 * @brief Updates fleeing movement and picks a new path when the timer expires.
 *
 * @param owner The unit using the movement generator.
 * @param time_diff The elapsed update time in milliseconds.
 * @return true while the generator remains active; otherwise false.
 */
bool FleeingMovementGenerator<T>::Update(T& owner, const uint32& time_diff)
{
    if (!owner.IsAlive())
    {
        return false;
    }

    // Ignore if the unit is in a state where it cannot react or move, except for fleeing
    if (owner.hasUnitState((UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE) & ~UNIT_STAT_FLEEING))
    {
        owner.clearUnitState(UNIT_STAT_FLEEING_MOVE);
        return true;
    }

    i_nextCheckTime.Update(time_diff);
    if (i_nextCheckTime.Passed() && owner.movespline->Finalized())
    {
        _setTargetLocation(owner);
    }

    return true;
}

// Template instantiations for Player and Creature
template void FleeingMovementGenerator<Player>::Initialize(Player&);
template void FleeingMovementGenerator<Creature>::Initialize(Creature&);
template bool FleeingMovementGenerator<Player>::_getPoint(Player&, float&, float&, float&);
template bool FleeingMovementGenerator<Creature>::_getPoint(Creature&, float&, float&, float&);
template void FleeingMovementGenerator<Player>::_setTargetLocation(Player&);
template void FleeingMovementGenerator<Creature>::_setTargetLocation(Creature&);
template void FleeingMovementGenerator<Player>::Interrupt(Player&);
template void FleeingMovementGenerator<Creature>::Interrupt(Creature&);
template void FleeingMovementGenerator<Player>::Reset(Player&);
template void FleeingMovementGenerator<Creature>::Reset(Creature&);
template bool FleeingMovementGenerator<Player>::Update(Player&, const uint32&);
template bool FleeingMovementGenerator<Creature>::Update(Creature&, const uint32&);

/**
 * @brief Finalizes the TimedFleeingMovementGenerator.
 * @param owner Reference to the unit.
 */
void TimedFleeingMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_FLEEING | UNIT_STAT_FLEEING_MOVE);
    if (Unit* victim = owner.getVictim())
    {
        if (owner.IsAlive())
        {
            owner.AttackStop(true);
            ((Creature*)&owner)->AI()->AttackStart(victim);
        }
    }
}

/**
 * @brief Updates the TimedFleeingMovementGenerator.
 * @param owner Reference to the unit.
 * @param time_diff Time difference.
 * @return True if the update was successful, false otherwise.
 */
bool TimedFleeingMovementGenerator::Update(Unit& owner, const uint32& time_diff)
{
    if (!owner.IsAlive())
    {
        return false;
    }

    // Ignore if the unit is in a state where it cannot react or move, except for fleeing
    if (owner.hasUnitState((UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE) & ~UNIT_STAT_FLEEING))
    {
        owner.clearUnitState(UNIT_STAT_FLEEING_MOVE);
        return true;
    }

    i_totalFleeTime.Update(time_diff);
    if (i_totalFleeTime.Passed())
    {
        return false;
    }

    // This calls the grandparent Update method hidden by FleeingMovementGenerator::Update(Creature &, const uint32 &) version
    // This is done instead of casting Unit& to Creature& and calling the parent method, so we can use Unit directly
    return MovementGeneratorMedium< Creature, FleeingMovementGenerator<Creature> >::Update(owner, time_diff);
}
