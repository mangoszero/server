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

#include "MotionMaster.h"
#include "ConfusedMovementGenerator.h"
#include "FleeingMovementGenerator.h"
#include "HomeMovementGenerator.h"
#include "IdleMovementGenerator.h"
#include "PointMovementGenerator.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "RandomMovementGenerator.h"
#include "movement/MoveSpline.h"
#include "movement/MoveSplineInit.h"
#include "Map.h"
#include "CreatureAISelector.h"
#include "Creature.h"
#include "CreatureLinkingMgr.h"
#include "Pet.h"
#include "DBCStores.h"
#include "Log.h"

#include <cassert>

/**
 * @brief Checks if the movement generator is static (idle movement).
 * @param mv Pointer to the movement generator.
 * @return True if the movement generator is static, false otherwise.
 */
inline static bool isStatic(MovementGenerator* mv)
{
    return (mv == &si_idleMovement);
}

/**
 * @brief Initializes the MotionMaster.
 */
void MotionMaster::Initialize()
{
    // Stop current move
    m_owner->StopMoving();

    // Clear ALL movement generators (including default)
    Clear(false, true);

    // Set new default movement generator
    if (m_owner->GetTypeId() == TYPEID_UNIT && !m_owner->hasUnitState(UNIT_STAT_CONTROLLED))
    {
        MovementGenerator* movement = FactorySelector::selectMovementGenerator((Creature*)m_owner);
        push(movement == NULL ? &si_idleMovement : movement);
        top()->Initialize(*m_owner);
        if (top()->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            (static_cast<WaypointMovementGenerator<Creature>*>(top()))->InitializeWaypointPath(*((Creature*)(m_owner)), 0, PATH_NO_PATH, 0, 0);
        }
    }
    else
    {
        push(&si_idleMovement);
    }
}

/**
 * @brief Destructor for MotionMaster.
 */
MotionMaster::~MotionMaster()
{
    // Just deallocate movement generator, but do not Finalize since it may access to already deallocated owner's memory
    while (!empty())
    {
        MovementGenerator* m = top();
        pop();
        if (!isStatic(m))
        {
            delete m;
        }
    }
}

/**
 * @brief Updates the motion of the unit.
 * @param diff Time difference.
 */
void MotionMaster::UpdateMotion(uint32 diff)
{
    if (m_owner->hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        return;
    }

    MANGOS_ASSERT(!empty());
    m_cleanFlag |= MMCF_UPDATE;

    if (!top()->Update(*m_owner, diff))
    {
        m_cleanFlag &= ~MMCF_UPDATE;
        MovementExpired();
    }
    else
    {
        m_cleanFlag &= ~MMCF_UPDATE;
    }

    if (m_expList)
    {
        for (size_t i = 0; i < m_expList->size(); ++i)
        {
            MovementGenerator* mg = (*m_expList)[i];
            if (!isStatic(mg))
            {
                delete mg;
            }
        }

        delete m_expList;
        m_expList = NULL;

        if (empty())
        {
            Initialize();
        }

        if (m_cleanFlag & MMCF_RESET)
        {
            top()->Reset(*m_owner);
            m_cleanFlag &= ~MMCF_RESET;
        }
    }
}

/**
 * @brief Directly cleans the movement generators.
 * @param reset Whether to reset the movement generators.
 * @param all Whether to clear all movement generators.
 */
void MotionMaster::DirectClean(bool reset, bool all)
{
    while (all ? !empty() : size() > 1)
    {
        MovementGenerator* curr = top();
        pop();
        curr->Finalize(*m_owner);

        if (!isStatic(curr))
        {
            delete curr;
        }
    }

    if (!all && reset)
    {
        MANGOS_ASSERT(!empty());
        top()->Reset(*m_owner);
    }
}

/**
 * @brief Delays the cleaning of the movement generators.
 * @param reset Whether to reset the movement generators.
 * @param all Whether to clear all movement generators.
 */
void MotionMaster::DelayedClean(bool reset, bool all)
{
    if (reset)
    {
        m_cleanFlag |= MMCF_RESET;
    }
    else
    {
        m_cleanFlag &= ~MMCF_RESET;
    }

    if (empty() || (!all && size() == 1))
    {
        return;
    }

    if (!m_expList)
    {
        m_expList = new ExpireList();
    }

    while (all ? !empty() : size() > 1)
    {
        MovementGenerator* curr = top();
        pop();
        curr->Finalize(*m_owner);

        if (!isStatic(curr))
        {
            m_expList->push_back(curr);
        }
    }
}

/**
 * @brief Directly expires the current movement generator.
 * @param reset Whether to reset the movement generator.
 */
void MotionMaster::DirectExpire(bool reset)
{
    if (empty() || size() == 1)
    {
        return;
    }

    MovementGenerator* curr = top();
    pop();

    // Also drop stored under top() targeted motions
    while (!empty() && (top()->GetMovementGeneratorType() == CHASE_MOTION_TYPE || top()->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE))
    {
        MovementGenerator* temp = top();
        pop();
        temp->Finalize(*m_owner);
        delete temp;
    }

    // Store current top MMGen, as Finalize might push a new MMGen
    MovementGenerator* nowTop = empty() ? NULL : top();
    // It can add another motions instead
    curr->Finalize(*m_owner);

    if (!isStatic(curr))
    {
        delete curr;
    }

    if (empty())
    {
        Initialize();
    }

    // Prevent reseting possible new pushed MMGen
    if (reset && top() == nowTop)
    {
        top()->Reset(*m_owner);
    }
}

/**
 * @brief Delays the expiration of the current movement generator.
 * @param reset Whether to reset the movement generator.
 */
void MotionMaster::DelayedExpire(bool reset)
{
    if (reset)
    {
        m_cleanFlag |= MMCF_RESET;
    }
    else
    {
        m_cleanFlag &= ~MMCF_RESET;
    }

    if (empty() || size() == 1)
    {
        return;
    }

    MovementGenerator* curr = top();
    pop();

    if (!m_expList)
    {
        m_expList = new ExpireList();
    }

    // Also drop stored under top() targeted motions
    while (!empty() && (top()->GetMovementGeneratorType() == CHASE_MOTION_TYPE || top()->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE))
    {
        MovementGenerator* temp = top();
        pop();
        temp ->Finalize(*m_owner);
        m_expList->push_back(temp);
    }

    curr->Finalize(*m_owner);

    if (!isStatic(curr))
    {
        m_expList->push_back(curr);
    }
}

/**
 * @brief Moves the unit to idle state.
 */
void MotionMaster::MoveIdle()
{
    if (empty() || !isStatic(top()))
    {
        push(&si_idleMovement);
    }
}

/**
 * @brief Moves the unit randomly around a point.
 * @param x X-coordinate of the center point.
 * @param y Y-coordinate of the center point.
 * @param z Z-coordinate of the center point.
 * @param radius Radius of the random movement.
 * @param verticalZ Vertical offset for the movement.
 */
void MotionMaster::MoveRandomAroundPoint(float x, float y, float z, float radius, float verticalZ)
{
    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        sLog.outError("%s attempt to move random.", m_owner->GetGuidStr().c_str());
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s move random.", m_owner->GetGuidStr().c_str());
        Mutate(new RandomMovementGenerator<Creature>(x, y, z, radius, verticalZ));
    }
}

/**
 * @brief Moves the unit to its home position.
 */
void MotionMaster::MoveTargetedHome()
{
    if (m_owner->hasUnitState(UNIT_STAT_LOST_CONTROL))
    {
        return;
    }

    Clear(false);

    if (m_owner->GetTypeId() == TYPEID_UNIT && !((Creature*)m_owner)->GetCharmerOrOwnerGuid())
    {
        // Manual exception for linked mobs
        if (m_owner->IsLinkingEventTrigger() && m_owner->GetMap()->GetCreatureLinkingHolder()->TryFollowMaster((Creature*)m_owner))
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s refollowed linked master", m_owner->GetGuidStr().c_str());
        }
        else
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s targeted home", m_owner->GetGuidStr().c_str());
            Mutate(new HomeMovementGenerator<Creature>());
        }
    }
    else if (m_owner->GetTypeId() == TYPEID_UNIT && ((Creature*)m_owner)->GetCharmerOrOwnerGuid())
    {
        if (Unit* target = ((Creature*)m_owner)->GetCharmerOrOwner())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s follow to %s", m_owner->GetGuidStr().c_str(), target->GetGuidStr().c_str());
            Mutate(new FollowMovementGenerator<Creature>(*target, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE));
        }
        else
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s attempt but fail to follow owner", m_owner->GetGuidStr().c_str());
        }
    }
    else
    {
        sLog.outError("%s attempt targeted home", m_owner->GetGuidStr().c_str());
    }
}

/**
 * @brief Makes the unit move in a confused manner.
 */
void MotionMaster::MoveConfused()
{
    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s move confused", m_owner->GetGuidStr().c_str());

    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        Mutate(new ConfusedMovementGenerator<Player>());
    }
    else
    {
        Mutate(new ConfusedMovementGenerator<Creature>());
    }
}

/**
 * @brief Makes the unit chase a target.
 * @param target Pointer to the target unit.
 * @param dist Distance to maintain from the target.
 * @param angle Angle to maintain from the target.
 */
void MotionMaster::MoveChase(Unit* target, float dist, float angle)
{
    // Ignore movement request if target not exist
    if (!target)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s chase to %s", m_owner->GetGuidStr().c_str(), target->GetGuidStr().c_str());

    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        Mutate(new ChaseMovementGenerator<Player>(*target, dist, angle));
    }
    else
    {
        Mutate(new ChaseMovementGenerator<Creature>(*target, dist, angle));
    }
}

/**
 * @brief Makes the unit follow a target.
 * @param target Pointer to the target unit.
 * @param dist Distance to maintain from the target.
 * @param angle Angle to maintain from the target.
 */
void MotionMaster::MoveFollow(Unit* target, float dist, float angle)
{
    if (m_owner->hasUnitState(UNIT_STAT_LOST_CONTROL))
    {
        return;
    }

    Clear();

    // Ignore movement request if target not exist
    if (!target)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s follow to %s", m_owner->GetGuidStr().c_str(), target->GetGuidStr().c_str());

    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        Mutate(new FollowMovementGenerator<Player>(*target, dist, angle));
    }
    else
    {
        Mutate(new FollowMovementGenerator<Creature>(*target, dist, angle));
    }
}

/**
 * @brief Moves the unit to a specific point.
 * @param id ID of the movement.
 * @param x X-coordinate of the destination.
 * @param y Y-coordinate of the destination.
 * @param z Z-coordinate of the destination.
 * @param generatePath Whether to generate a path to the destination.
 */
void MotionMaster::MovePoint(uint32 id, float x, float y, float z, bool generatePath)
{
    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s targeted point (Id: %u X: %f Y: %f Z: %f)", m_owner->GetGuidStr().c_str(), id, x, y, z);

    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        Mutate(new PointMovementGenerator<Player>(id, x, y, z, generatePath));
    }
    else
    {
        Mutate(new PointMovementGenerator<Creature>(id, x, y, z, generatePath));
    }
}

/**
 * @brief Makes the unit seek assistance at a specific point.
 * @param x X-coordinate of the assistance point.
 * @param y Y-coordinate of the assistance point.
 * @param z Z-coordinate of the assistance point.
 */
void MotionMaster::MoveSeekAssistance(float x, float y, float z)
{
    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        sLog.outError("%s attempt to seek assistance", m_owner->GetGuidStr().c_str());
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s seek assistance (X: %f Y: %f Z: %f)",
                         m_owner->GetGuidStr().c_str(), x, y, z);
        Mutate(new AssistanceMovementGenerator(x, y, z));
    }
}

/**
 * @brief Makes the unit seek assistance and then distract.
 * @param timer Time for the distraction.
 */
void MotionMaster::MoveSeekAssistanceDistract(uint32 time)
{
    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        sLog.outError("%s attempt to call distract after assistance", m_owner->GetGuidStr().c_str());
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s is distracted after assistance call (Time: %u)",
                         m_owner->GetGuidStr().c_str(), time);
        Mutate(new AssistanceDistractMovementGenerator(time));
    }
}

/**
 * @brief Makes the unit flee from an enemy.
 * @param enemy Pointer to the enemy unit.
 * @param time Time limit for the fleeing movement.
 */
void MotionMaster::MoveFleeing(Unit* enemy, uint32 time)
{
    if (!enemy)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s flee from %s", m_owner->GetGuidStr().c_str(), enemy->GetGuidStr().c_str());

    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        Mutate(new FleeingMovementGenerator<Player>(enemy->GetObjectGuid()));
    }
    else
    {
        if (time)
        {
            Mutate(new TimedFleeingMovementGenerator(enemy->GetObjectGuid(), time));
        }
        else
        {
            Mutate(new FleeingMovementGenerator<Creature>(enemy->GetObjectGuid()));
        }
    }
}

/**
 * @brief Moves the unit along a waypoint path.
 * @param id ID of the waypoint path.
 * @param source Source of the waypoint path.
 * @param initialDelay Initial delay before starting the movement.
 * @param overwriteEntry Entry to overwrite.
 */
void MotionMaster::MoveWaypoint(int32 id /*=0*/, uint32 source /*=0==PATH_NO_PATH*/, uint32 initialDelay /*=0*/, uint32 overwriteEntry /*=0*/)
{
    if (m_owner->GetTypeId() == TYPEID_UNIT)
    {
        if (GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            sLog.outError("%s attempt to MoveWaypoint() but is already using waypoint", m_owner->GetGuidStr().c_str());
            return;
        }

        Creature* creature = (Creature*)m_owner;

        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s start MoveWaypoint()", m_owner->GetGuidStr().c_str());
        WaypointMovementGenerator<Creature>* newWPMMgen = new WaypointMovementGenerator<Creature>(*creature);
        Mutate(newWPMMgen);
        newWPMMgen->InitializeWaypointPath(*creature, id, (WaypointPathOrigin)source, initialDelay, overwriteEntry);
    }
    else
    {
        sLog.outError("Non-creature %s attempt to MoveWaypoint()", m_owner->GetGuidStr().c_str());
    }
}

/**
 * @brief Moves the unit along a taxi flight path.
 * @param path ID of the flight path.
 * @param pathnode Node of the flight path.
 */
void MotionMaster::MoveTaxiFlight(uint32 path, uint32 pathnode)
{
    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        if (path < sTaxiPathNodesByPath.size())
        {
            DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s taxi to (Path %u node %u)", m_owner->GetGuidStr().c_str(), path, pathnode);
            FlightPathMovementGenerator* mgen = new FlightPathMovementGenerator(sTaxiPathNodesByPath[path], pathnode);
            Mutate(mgen);
        }
        else
        {
            sLog.outError("%s attempt taxi to (nonexistent Path %u node %u)",
                          m_owner->GetGuidStr().c_str(), path, pathnode);
        }
    }
    else
    {
        sLog.outError("%s attempt taxi to (Path %u node %u)",
                      m_owner->GetGuidStr().c_str(), path, pathnode);
    }
}

/**
 * @brief Makes the unit distracted for a specified time.
 * @param timer Time limit for the distraction.
 */
void MotionMaster::MoveDistract(uint32 timer)
{
    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s distracted (timer: %u)", m_owner->GetGuidStr().c_str(), timer);
    DistractMovementGenerator* mgen = new DistractMovementGenerator(timer);
    Mutate(mgen);
}

/**
 * @brief Makes the unit fly or land.
 * @param id ID of the movement.
 * @param x X-coordinate of the destination.
 * @param y Y-coordinate of the destination.
 * @param z Z-coordinate of the destination.
 * @param liftOff Whether the unit should lift off or land.
 */
void MotionMaster::MoveFlyOrLand(uint32 id, float x, float y, float z, bool liftOff)
{
    if (m_owner->GetTypeId() != TYPEID_UNIT)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "%s targeted point for %s (Id: %u X: %f Y: %f Z: %f)", m_owner->GetGuidStr().c_str(), liftOff ? "liftoff" : "landing", id, x, y, z);
    Mutate(new FlyOrLandMovementGenerator(id, x, y, z, liftOff));
}

/**
 * @brief Changes the current movement generator to a new one.
 * @param m Pointer to the new movement generator.
 */
void MotionMaster::Mutate(MovementGenerator* m)
{
    if (!empty())
    {
        switch (top()->GetMovementGeneratorType())
        {
                // HomeMovement is not that important, delete it if meanwhile a new comes
            case HOME_MOTION_TYPE:
                // DistractMovement interrupted by any other movement
            case DISTRACT_MOTION_TYPE:
                MovementExpired(false);
            default:
                break;
        }

        if (!empty())
        {
            top()->Interrupt(*m_owner);
        }
    }

    m->Initialize(*m_owner);
    push(m);
}

/**
 * @brief Propagates the speed change to the movement generators.
 */
void MotionMaster::PropagateSpeedChange()
{
    Impl::container_type::iterator it = Impl::c.begin();
    for (; it != end(); ++it)
    {
        (*it)->unitSpeedChanged();
    }
}

/**
 * @brief Sets the next waypoint for the unit.
 * @param pointId ID of the next waypoint.
 * @return True if the next waypoint was successfully set, false otherwise.
 */
bool MotionMaster::SetNextWaypoint(uint32 pointId)
{
    for (Impl::container_type::reverse_iterator rItr = Impl::c.rbegin(); rItr != Impl::c.rend(); ++rItr)
    {
        if ((*rItr)->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            return (static_cast<WaypointMovementGenerator<Creature>*>(*rItr))->SetNextWaypoint(pointId);
        }
    }
    return false;
}

/**
 * @brief Gets the last reached waypoint.
 * @return The ID of the last reached waypoint.
 */
uint32 MotionMaster::getLastReachedWaypoint() const
{
    for (Impl::container_type::const_reverse_iterator rItr = Impl::c.rbegin(); rItr != Impl::c.rend(); ++rItr)
    {
        if ((*rItr)->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            return (static_cast<WaypointMovementGenerator<Creature>*>(*rItr))->getLastReachedWaypoint();
        }
    }
    return 0;
}

/**
 * @brief Gets the type of the current movement generator.
 * @return The type of the current movement generator.
 */
MovementGeneratorType MotionMaster::GetCurrentMovementGeneratorType() const
{
    if (empty())
    {
        return IDLE_MOTION_TYPE;
    }

    return top()->GetMovementGeneratorType();
}

/**
 * @brief Gets the waypoint path information.
 * @param oss Output stream to store the waypoint path information.
 */
void MotionMaster::GetWaypointPathInformation(std::ostringstream& oss) const
{
    for (Impl::container_type::const_reverse_iterator rItr = Impl::c.rbegin(); rItr != Impl::c.rend(); ++rItr)
    {
        if ((*rItr)->GetMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
        {
            static_cast<WaypointMovementGenerator<Creature>*>(*rItr)->GetPathInformation(oss);
            return;
        }
    }
}

/**
 * @brief Gets the destination coordinates.
 * @param x Reference to the X-coordinate.
 * @param y Reference to the Y-coordinate.
 * @param z Reference to the Z-coordinate.
 * @return True if the destination coordinates were successfully obtained, false otherwise.
 */
bool MotionMaster::GetDestination(float& x, float& y, float& z)
{
    if (m_owner->movespline->Finalized())
    {
        return false;
    }

    const G3D::Vector3& dest = m_owner->movespline->FinalDestination();
    x = dest.x;
    y = dest.y;
    z = dest.z;
    return true;
}

/**
 * @brief Makes the unit fall to the ground.
 */
void MotionMaster::MoveFall()
{
    // Use larger distance for vmap height search than in most other cases
    float tz = m_owner->GetMap()->GetHeight(m_owner->GetPositionX(), m_owner->GetPositionY(), m_owner->GetPositionZ());
    if (tz <= INVALID_HEIGHT)
    {
        DEBUG_LOG("MotionMaster::MoveFall: unable retrive a proper height at map %u (x: %f, y: %f, z: %f).",
                  m_owner->GetMap()->GetId(), m_owner->GetPositionX(), m_owner->GetPositionY(), m_owner->GetPositionZ());
        return;
    }

    // Abort too if the ground is very near
    if (fabs(m_owner->GetPositionZ() - tz) < 0.1f)
    {
        return;
    }

    Movement::MoveSplineInit init(*m_owner);
    init.MoveTo(m_owner->GetPositionX(), m_owner->GetPositionY(), tz);
    init.SetFall();
    init.Launch();
}
