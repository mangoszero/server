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

#include "TargetedMovementGenerator.h"
#include "PathFinder.h"
#include "Unit.h"
#include "Creature.h"
#include "Player.h"
#include "World.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"

/**
 * @brief Set the target location for the owner.
 *
 * @tparam T The type of the owner.
 * @tparam D The type of the derived class.
 * @param owner The owner.
 * @param updateDestination Whether to update the destination.
 */
template<class T, typename D>
void TargetedMovementGeneratorMedium<T, D>::_setTargetLocation(T& owner, bool updateDestination)
{
    if (!i_target.isValid() || !i_target->IsInWorld())
    {
        return;
    }

    if (owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    float x, y, z;

    // i_path can be NULL in case this is the first call for this MMGen (via Update)
    // Can happen for example if no path was created on MMGen-Initialize because of the owner being stunned
    if (updateDestination || !i_path)
    {
        owner.GetPosition(x, y, z);

        // prevent redundant micro-movement for pets, other followers.
        if (!RequiresNewPosition(owner, x, y, z))
        {
            if (!owner.movespline->Finalized())
            {
                return;
            }
        }
        // Chase Movement and angle == 0 case: Chase to current angle
        else if (this->GetMovementGeneratorType() == CHASE_MOTION_TYPE && i_angle == 0.0f)
        {
            i_target->GetNearPoint(&owner, x, y, z, owner.GetObjectBoundingRadius(), this->GetDynamicTargetDistance(owner, false), i_target->GetAngle(&owner));
        }
        // Targeted movement to at i_offset distance from target and i_angle from target facing
        else
        {
            i_target->GetNearPoint(&owner, x, y, z, owner.GetObjectBoundingRadius(), this->GetDynamicTargetDistance(owner, false), i_target->GetOrientation() + i_angle);
        }
    }
    else
    {
        // the destination has not changed, we just need to refresh the path (usually speed change)
        G3D::Vector3 end = i_path->getEndPosition();
        x = end.x;
        y = end.y;
        z = end.z;
    }

    if (!i_path)
    {
        i_path = new PathFinder(&owner);
    }

    // allow pets following their master to cheat while generating paths
    bool forceDest = (owner.GetTypeId() == TYPEID_UNIT && ((Creature*)&owner)->IsPet()
                      && owner.hasUnitState(UNIT_STAT_FOLLOW));
    i_path->calculate(x, y, z, forceDest);
    if (i_path->getPathType() & PATHFIND_NOPATH)
    {
        return;
    }

    D::_addUnitStateMove(owner);
    i_targetReached = false;
    m_speedChanged = false;

    Movement::MoveSplineInit init(owner);
    init.MovebyPath(i_path->getPath());
    init.SetWalk(((D*)this)->EnableWalking());
    init.Launch();
}

/**
 * @brief Update the movement generator.
 *
 * @tparam T The type of the owner.
 * @tparam D The type of the derived class.
 * @param owner The owner.
 * @param time_diff The time difference.
 * @return true If the update was successful.
 * @return false Otherwise.
 */
template<class T, typename D>
bool TargetedMovementGeneratorMedium<T, D>::Update(T& owner, const uint32& time_diff)
{
    if (!i_target.isValid() || !i_target->IsInWorld())
    {
        return false;
    }

    if (!owner.IsAlive())
    {
        return true;
    }

    if (owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        D::_clearUnitStateMove(owner);
        return true;
    }

    if (this->GetMovementGeneratorType() == CHASE_MOTION_TYPE && owner.hasUnitState(UNIT_STAT_NO_COMBAT_MOVEMENT))
    {
        D::_clearUnitStateMove(owner);
        return true;
    }

    // prevent movement while casting spells with cast time or channel time
    if (owner.IsNonMeleeSpellCasted(false, false, true, true))
    {
        if (!owner.movespline->Finalized())
        {
            if (owner.IsClientControlled())
            {
                owner.StopMoving(true);
            }
            else
            {
                owner.InterruptMoving();
            }
        }
        return true;
    }

    // prevent crash after creature killed pet
    if (static_cast<D*>(this)->_lostTarget(owner))
    {
        D::_clearUnitStateMove(owner);
        return true;
    }

    bool targetMoved = false;
    i_recheckDistance.Update(time_diff);
    if (i_recheckDistance.Passed())
    {
        i_recheckDistance.Reset(this->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE ? 50 : 100);
        G3D::Vector3 dest = owner.movespline->FinalDestination();
        targetMoved = RequiresNewPosition(owner, dest.x, dest.y, dest.z);
        if (!targetMoved)
        {
            // This unit is in hitbox of target
            // howewer we have to check if the target not moved a bit to update the orientation
            // client do it automatically 'visually' but it need this new orientation send or it will retrieve old orientation in some case (like stun)
            G3D::Vector3 currTargetPos;
            i_target->GetPosition(currTargetPos.x, currTargetPos.y, currTargetPos.z);
            if (owner.movespline->Finalized() && currTargetPos != m_prevTargetPos)
            {
                // position changed we need to adjust owner orientation to continue facing it
                m_prevTargetPos = currTargetPos;
                owner.SetInFront(i_target.getTarget());         // set movementinfo orientation, needed for next movement if any

                float angle = owner.GetAngle(i_target.getTarget());
                owner.SetFacingTo(angle);                       // inform client that orientation changed
            }
        }
    }

    if (m_speedChanged || targetMoved)
    {
        _setTargetLocation(owner, targetMoved);
    }

    if (owner.movespline->Finalized())
    {
        if (!i_targetReached)
        {
            i_targetReached = true;
            static_cast<D*>(this)->_reachTarget(owner);
        }
    }
    return true;
}

/**
 * @brief Check if the target is reachable.
 *
 * @tparam T The type of the owner.
 * @tparam D The type of the derived class.
 * @return true If the target is reachable.
 * @return false Otherwise.
 */
template<class T, typename D>
bool TargetedMovementGeneratorMedium<T, D>::IsReachable() const
{
    return (i_path) ? (i_path->getPathType() & PATHFIND_NORMAL) : true;
}

/**
 * @brief Check if a new position is required for the owner.
 *
 * @tparam T The type of the owner.
 * @tparam D The type of the derived class.
 * @param owner The owner.
 * @param x The x-coordinate.
 * @param y The y-coordinate.
 * @param z The z-coordinate.
 * @return true If a new position is required.
 * @return false Otherwise.
 */
template<class T, typename D>
bool TargetedMovementGeneratorMedium<T, D>::RequiresNewPosition(T& owner, float x, float y, float z) const
{
    // More distance let have better performance, less distance let have more sensitive reaction at target move.
    if (owner.GetTypeId() == TYPEID_UNIT && ((Creature*)&owner)->CanFly())
    {
        return !i_target->IsWithinDist3d(x, y, z, this->GetDynamicTargetDistance(owner, true));
    }
    else
    {
        return !i_target->IsWithinDist2d(x, y, this->GetDynamicTargetDistance(owner, true));
    }
}

/**
 * @brief Clear the chase movement state for the owner.
 *
 * @tparam T The type of the owner.
 * @param u The owner.
 */
template<class T>
void ChaseMovementGenerator<T>::_clearUnitStateMove(T& u) { u.clearUnitState(UNIT_STAT_CHASE_MOVE); }

/**
 * @brief Add the chase movement state for the owner.
 *
 * @tparam T The type of the owner.
 * @param u The owner.
 */
template<class T>
void ChaseMovementGenerator<T>::_addUnitStateMove(T& u) { u.addUnitState(UNIT_STAT_CHASE_MOVE); }

/**
 * @brief Check if the owner has lost the target.
 *
 * @tparam T The type of the owner.
 * @param u The owner.
 * @return true If the owner has lost the target.
 * @return false Otherwise.
 */
template<class T>
bool ChaseMovementGenerator<T>::_lostTarget(T& u) const
{
    return u.getVictim() != this->GetTarget();
}

/**
 * @brief Handle reaching the target for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 */
template<class T>
void ChaseMovementGenerator<T>::_reachTarget(T& owner)
{
    if (owner.CanReachWithMeleeAttack(this->i_target.getTarget()))
    {
        owner.Attack(this->i_target.getTarget(), true);
    }
}

/**
 * @brief Initialize the chase movement generator for the player.
 *
 * @param owner The player.
 */
template<>
void ChaseMovementGenerator<Player>::Initialize(Player& owner)
{
    owner.addUnitState(UNIT_STAT_CHASE);                    // _MOVE set in _SetTargetLocation after required checks
    _setTargetLocation(owner, true);
    i_target->GetPosition(m_prevTargetPos.x, m_prevTargetPos.y, m_prevTargetPos.z);
}

/**
 * @brief Initialize the chase movement generator for the creature.
 *
 * @param owner The creature.
 */
template<>
void ChaseMovementGenerator<Creature>::Initialize(Creature& owner)
{
    owner.SetWalk(false, false);                            // Chase movement is running
    owner.addUnitState(UNIT_STAT_CHASE);                    // _MOVE set in _SetTargetLocation after required checks
    _setTargetLocation(owner, true);
    i_target->GetPosition(m_prevTargetPos.x, m_prevTargetPos.y, m_prevTargetPos.z);
}

/**
 * @brief Finalize the chase movement generator for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 */
template<class T>
void ChaseMovementGenerator<T>::Finalize(T& owner)
{
    owner.clearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
}

/**
 * @brief Interrupt the chase movement generator for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 */
template<class T>
void ChaseMovementGenerator<T>::Interrupt(T& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
}

/**
 * @brief Reset the chase movement generator for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 */
template<class T>
void ChaseMovementGenerator<T>::Reset(T& owner)
{
    Initialize(owner);
}

// Chase-Movement: These factors depend on combat-reach distance
#define CHASE_DEFAULT_RANGE_FACTOR                        0.5f
#define CHASE_RECHASE_RANGE_FACTOR                        0.75f

/**
 * @brief Get the dynamic target distance for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 * @param forRangeCheck Whether the distance is for range check.
 * @return float The dynamic target distance.
 */
template<class T>
float ChaseMovementGenerator<T>::GetDynamicTargetDistance(T& owner, bool forRangeCheck) const
{
    if (!forRangeCheck)
    {
        return this->i_offset + CHASE_DEFAULT_RANGE_FACTOR * this->i_target->GetCombatReach(&owner);
    }

    return CHASE_RECHASE_RANGE_FACTOR * this->i_target->GetCombatReach(&owner) - this->i_target->GetObjectBoundingRadius();
}

/**
 * @brief Clear the follow movement state for the owner.
 *
 * @tparam T The type of the owner.
 * @param u The owner.
 */
template<class T>
void FollowMovementGenerator<T>::_clearUnitStateMove(T& u) { u.clearUnitState(UNIT_STAT_FOLLOW_MOVE); }

/**
 * @brief Add the follow movement state for the owner.
 *
 * @tparam T The type of the owner.
 * @param u The owner.
 */
template<class T>
void FollowMovementGenerator<T>::_addUnitStateMove(T& u) { u.addUnitState(UNIT_STAT_FOLLOW_MOVE); }

/**
 * @brief Enable walking for the creature.
 *
 * @return true If walking is enabled.
 * @return false Otherwise.
 */
template<>
bool FollowMovementGenerator<Creature>::EnableWalking() const
{
    return i_target.isValid() && i_target->IsWalking();
}

/**
 * @brief Enable walking for the player.
 *
 * @return true If walking is enabled.
 * @return false Otherwise.
 */
template<>
bool FollowMovementGenerator<Player>::EnableWalking() const
{
    return false;
}

/**
 * @brief Update the speed for the player.
 *
 * @param u The player.
 */
template<>
void FollowMovementGenerator<Player>::_updateSpeed(Player& /*u*/)
{
    // nothing to do for Player
}

/**
 * @brief Update the speed for the creature.
 *
 * @param u The creature.
 */
template<>
void FollowMovementGenerator<Creature>::_updateSpeed(Creature& u)
{
    // pet only sync speed with owner
    if (!((Creature&)u).IsPet() || !i_target.isValid() || i_target->GetObjectGuid() != u.GetOwnerGuid())
    {
        return;
    }

    u.UpdateSpeed(MOVE_RUN, true);
    u.UpdateSpeed(MOVE_WALK, true);
    u.UpdateSpeed(MOVE_SWIM, true);
}

/**
 * @brief Initialize the follow movement generator for the player.
 *
 * @param owner The player.
 */
template<>
void FollowMovementGenerator<Player>::Initialize(Player& owner)
{
    owner.addUnitState(UNIT_STAT_FOLLOW);                   // _MOVE set in _SetTargetLocation after required checks
    _updateSpeed(owner);
    _setTargetLocation(owner, true);
}

/**
 * @brief Initialize the follow movement generator for the creature.
 *
 * @param owner The creature.
 */
template<>
void FollowMovementGenerator<Creature>::Initialize(Creature& owner)
{
    owner.addUnitState(UNIT_STAT_FOLLOW);                   // _MOVE set in _SetTargetLocation after required checks
    _updateSpeed(owner);
    _setTargetLocation(owner, true);
}

/**
 * @brief Finalize the follow movement generator for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 */
template<class T>
void FollowMovementGenerator<T>::Finalize(T& owner)
{
    owner.clearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    _updateSpeed(owner);
}

/**
 * @brief Interrupt the follow movement generator for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 */
template<class T>
void FollowMovementGenerator<T>::Interrupt(T& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    _updateSpeed(owner);
}

/**
 * @brief Reset the follow movement generator for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 */
template<class T>
void FollowMovementGenerator<T>::Reset(T& owner)
{
    Initialize(owner);
}

// This factor defines how much of the bounding-radius (as measurement of size) will be used for recalculating a new following position
//   The smaller, the more micro movement, the bigger, possibly no proper movement updates
#define FOLLOW_RECALCULATE_FACTOR                         1.0f
// This factor defines when the distance of a follower will have impact onto following-position updates
#define FOLLOW_DIST_GAP_FOR_DIST_FACTOR                   3.0f
// This factor defines how much of the follow-distance will be used as sloppyness value (if the above distance is exceeded)
#define FOLLOW_DIST_RECALCULATE_FACTOR                    1.0f

/**
 * @brief Get the dynamic target distance for the owner.
 *
 * @tparam T The type of the owner.
 * @param owner The owner.
 * @param forRangeCheck Whether the distance is for range check.
 * @return float The dynamic target distance.
 */
template<class T>
float FollowMovementGenerator<T>::GetDynamicTargetDistance(T& owner, bool forRangeCheck) const
{
    if (!forRangeCheck)
    {
        return this->i_offset + owner.GetObjectBoundingRadius() + this->i_target->GetObjectBoundingRadius();
    }

    float allowed_dist = sWorld.getConfig(CONFIG_FLOAT_RATE_TARGET_POS_RECALCULATION_RANGE) - this->i_target->GetObjectBoundingRadius();
    allowed_dist += FOLLOW_RECALCULATE_FACTOR * (owner.GetObjectBoundingRadius() + this->i_target->GetObjectBoundingRadius());
    if (this->i_offset > FOLLOW_DIST_GAP_FOR_DIST_FACTOR)
    {
        allowed_dist += FOLLOW_DIST_RECALCULATE_FACTOR * this->i_offset;
    }

    return allowed_dist;
}

/**
 * @brief Template specialization for setting the target location for a player using ChaseMovementGenerator.
 * @param owner The player.
 * @param updateDestination Whether to update the destination.
 */
template void TargetedMovementGeneratorMedium<Player, ChaseMovementGenerator<Player> >::_setTargetLocation(Player&, bool);

/**
 * @brief Template specialization for setting the target location for a player using FollowMovementGenerator.
 * @param owner The player.
 * @param updateDestination Whether to update the destination.
 */
template void TargetedMovementGeneratorMedium<Player, FollowMovementGenerator<Player> >::_setTargetLocation(Player&, bool);

/**
 * @brief Template specialization for setting the target location for a creature using ChaseMovementGenerator.
 * @param owner The creature.
 * @param updateDestination Whether to update the destination.
 */
template void TargetedMovementGeneratorMedium<Creature, ChaseMovementGenerator<Creature> >::_setTargetLocation(Creature&, bool);

/**
 * @brief Template specialization for setting the target location for a creature using FollowMovementGenerator.
 * @param owner The creature.
 * @param updateDestination Whether to update the destination.
 */
template void TargetedMovementGeneratorMedium<Creature, FollowMovementGenerator<Creature> >::_setTargetLocation(Creature&, bool);

/**
 * @brief Updates the movement generator for the player using ChaseMovementGenerator.
 * @param owner The player.
 * @param time_diff The time difference.
 * @return true If the update was successful.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Player, ChaseMovementGenerator<Player> >::Update(Player&, const uint32&);

/**
 * @brief Updates the movement generator for the player using FollowMovementGenerator.
 * @param owner The player.
 * @param time_diff The time difference.
 * @return true If the update was successful.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Player, FollowMovementGenerator<Player> >::Update(Player&, const uint32&);

/**
 * @brief Updates the movement generator for the creature using ChaseMovementGenerator.
 * @param owner The creature.
 * @param time_diff The time difference.
 * @return true If the update was successful.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Creature, ChaseMovementGenerator<Creature> >::Update(Creature&, const uint32&);

/**
 * @brief Updates the movement generator for the creature using FollowMovementGenerator.
 * @param owner The creature.
 * @param time_diff The time difference.
 * @return true If the update was successful.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Creature, FollowMovementGenerator<Creature> >::Update(Creature&, const uint32&);

/**
 * @brief Checks if the target is reachable for the player using ChaseMovementGenerator.
 * @return true If the target is reachable.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Player, ChaseMovementGenerator<Player> >::IsReachable() const;

/**
 * @brief Checks if the target is reachable for the player using FollowMovementGenerator.
 * @return true If the target is reachable.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Player, FollowMovementGenerator<Player> >::IsReachable() const;

/**
 * @brief Checks if the target is reachable for the creature using ChaseMovementGenerator.
 * @return true If the target is reachable.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Creature, ChaseMovementGenerator<Creature> >::IsReachable() const;

/**
 * @brief Checks if the target is reachable for the creature using FollowMovementGenerator.
 * @return true If the target is reachable.
 * @return false Otherwise.
 */
template bool TargetedMovementGeneratorMedium<Creature, FollowMovementGenerator<Creature> >::IsReachable() const;

/**
 * @brief Clears the chase movement state for the player.
 * @param u The player.
 */
template void ChaseMovementGenerator<Player>::_clearUnitStateMove(Player& u);

/**
 * @brief Adds the chase movement state for the creature.
 * @param u The creature.
 */
template void ChaseMovementGenerator<Creature>::_addUnitStateMove(Creature& u);

/**
 * @brief Checks if the player has lost the target.
 * @param u The player.
 * @return true If the player has lost the target.
 * @return false Otherwise.
 */
template bool ChaseMovementGenerator<Player>::_lostTarget(Player& u) const;

/**
 * @brief Checks if the creature has lost the target.
 * @param u The creature.
 * @return true If the creature has lost the target.
 * @return false Otherwise.
 */
template bool ChaseMovementGenerator<Creature>::_lostTarget(Creature& u) const;

/**
 * @brief Handles reaching the target for the player.
 * @param owner The player.
 */
template void ChaseMovementGenerator<Player>::_reachTarget(Player&);

/**
 * @brief Handles reaching the target for the creature.
 * @param owner The creature.
 */
template void ChaseMovementGenerator<Creature>::_reachTarget(Creature&);

/**
 * @brief Finalizes the chase movement generator for the player.
 * @param owner The player.
 */
template void ChaseMovementGenerator<Player>::Finalize(Player&);

/**
 * @brief Finalizes the chase movement generator for the creature.
 * @param owner The creature.
 */
template void ChaseMovementGenerator<Creature>::Finalize(Creature&);

/**
 * @brief Initializes the chase movement generator for the player.
 * @param owner The player.
 */
template void ChaseMovementGenerator<Player>::Initialize(Player&);

/**
 * @brief Initializes the chase movement generator for the creature.
 * @param owner The creature.
 */
template void ChaseMovementGenerator<Creature>::Initialize(Creature&);

/**
 * @brief Interrupts the chase movement generator for the player.
 * @param owner The player.
 */
template void ChaseMovementGenerator<Player>::Interrupt(Player&);

/**
 * @brief Interrupts the chase movement generator for the creature.
 * @param owner The creature.
 */
template void ChaseMovementGenerator<Creature>::Interrupt(Creature&);

/**
 * @brief Resets the chase movement generator for the player.
 * @param owner The player.
 */
template void ChaseMovementGenerator<Player>::Reset(Player&);

/**
 * @brief Resets the chase movement generator for the creature.
 * @param owner The creature.
 */
template void ChaseMovementGenerator<Creature>::Reset(Creature&);

/**
 * @brief Gets the dynamic target distance for the creature.
 * @param owner The creature.
 * @param forRangeCheck Whether the distance is for range check.
 * @return float The dynamic target distance.
 */
template float ChaseMovementGenerator<Creature>::GetDynamicTargetDistance(Creature&, bool) const;

/**
 * @brief Gets the dynamic target distance for the player.
 * @param owner The player.
 * @param forRangeCheck Whether the distance is for range check.
 * @return float The dynamic target distance.
 */
template float ChaseMovementGenerator<Player>::GetDynamicTargetDistance(Player&, bool) const;

/**
 * @brief Clears the follow movement state for the player.
 * @param u The player.
 */
template void FollowMovementGenerator<Player>::_clearUnitStateMove(Player& u);

/**
 * @brief Adds the follow movement state for the creature.
 * @param u The creature.
 */
template void FollowMovementGenerator<Creature>::_addUnitStateMove(Creature& u);

/**
 * @brief Finalizes the follow movement generator for the player.
 * @param owner The player.
 */
template void FollowMovementGenerator<Player>::Finalize(Player&);

/**
 * @brief Finalizes the follow movement generator for the creature.
 * @param owner The creature.
 */
template void FollowMovementGenerator<Creature>::Finalize(Creature&);

/**
 * @brief Initializes the follow movement generator for the player.
 * @param owner The player.
 */
template void FollowMovementGenerator<Player>::Initialize(Player&);

/**
 * @brief Initializes the follow movement generator for the creature.
 * @param owner The creature.
 */
template void FollowMovementGenerator<Creature>::Initialize(Creature&);

/**
 * @brief Interrupts the follow movement generator for the player.
 * @param owner The player.
 */
template void FollowMovementGenerator<Player>::Interrupt(Player&);

/**
 * @brief Interrupts the follow movement generator for the creature.
 * @param owner The creature.
 */
template void FollowMovementGenerator<Creature>::Interrupt(Creature&);

/**
 * @brief Resets the follow movement generator for the player.
 * @param owner The player.
 */
template void FollowMovementGenerator<Player>::Reset(Player&);

/**
 * @brief Resets the follow movement generator for the creature.
 * @param owner The creature.
 */
template void FollowMovementGenerator<Creature>::Reset(Creature&);

/**
 * @brief Gets the dynamic target distance for the creature.
 * @param owner The creature.
 * @param forRangeCheck Whether the distance is for range check.
 * @return float The dynamic target distance.
 */
template float FollowMovementGenerator<Creature>::GetDynamicTargetDistance(Creature&, bool) const;

/**
 * @brief Gets the dynamic target distance for the player.
 * @param owner The player.
 * @param forRangeCheck Whether the distance is for range check.
 * @return float The dynamic target distance.
 */
template float FollowMovementGenerator<Player>::GetDynamicTargetDistance(Player&, bool) const;
