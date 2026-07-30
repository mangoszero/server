/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "TargetedMovementGenerator.h"
#include "Creature.h"
#include "MotionFrame.h"
#include "World.h"

namespace
{
    // Chase distances, as fractions of the target's combat reach. The gap between the
    // two is the hysteresis band: we close to CHASE_RANGE, but only start closing again
    // once the target has drifted past CHASE_RECHASE_RANGE.
    constexpr float CHASE_RANGE = 0.5f;
    constexpr float CHASE_RECHASE_RANGE = 0.75f;

    // How much of the bounding radius counts toward "the follow spot is stale". Smaller
    // means more micro-movement; bigger means the follower may not update at all.
    constexpr float FOLLOW_RECALCULATE_FACTOR = 1.0f;
    /// A follow distance beyond this gets extra slop, so a far-following unit is not
    /// re-routed for every step its target takes.
    constexpr float FOLLOW_DIST_GAP_FOR_DIST_FACTOR = 3.0f;
    constexpr float FOLLOW_DIST_RECALCULATE_FACTOR = 1.0f;
}

void TargetedMovementGenerator::ResetTracking()
{
    m_haveDest = false;
    m_targetReached = false;
    m_recheckTime.Reset(0);
    ResetLeg();
}

Motion::Vector3 TargetedMovementGenerator::ComputeDestination(Unit& owner) const
{
    Motion::IMotionFrame const& frame = Motion::FrameFor(owner);
    Unit const& target = *i_target.getTarget();

    // Chase with no angle: close on the bearing we are already approaching from, so a
    // pursuer runs straight at its victim instead of arcing around to one side of it.
    //
    // The bearing is taken between FRAME positions, not with target.Where().BearingTo(owner.Where()). On a
    // deck the latter would read two world positions that are only caches of an estimated
    // vessel pose, and hand back a bearing rotated by the ship's yaw — aiming the chase at
    // open water.
    const bool chaseHeadOn =
        GetMovementGeneratorType() == CHASE_MOTION_TYPE && m_angle == 0.0f;

    const float absAngle = chaseHeadOn
        ? Motion::AngleBetween(frame.ObjectPosition(owner, target), frame.MoverPosition(owner))
        : frame.ObjectOrientation(owner, target) + m_angle;

    return frame.NearPoint(owner, target, owner.Where().Extent(),
                           TargetDistance(owner, false), absAngle);
}

bool TargetedMovementGenerator::RequiresNewPosition(Unit& owner,
                                                    Motion::Vector3 const& spot) const
{
    const float allowed = TargetDistance(owner, true);

    // `spot` is a FRAME point, so the target has to be read in the same frame — asking
    // i_target->IsWithinDist2d(spot...) would measure a deck offset against a map
    // coordinate and answer nonsense. (The distance itself needs no correction once both
    // ends are in one frame: a rigid transform preserves lengths.)
    Motion::IMotionFrame const& frame = Motion::FrameFor(owner);
    const Motion::Vector3 target = frame.ObjectPosition(owner, *i_target.getTarget());

    const float dx = spot.x - target.x;
    const float dy = spot.y - target.y;
    float distSq = dx * dx + dy * dy;

    // A flier cares about height too; anything on the ground does not.
    if (owner.GetTypeId() == TYPEID_UNIT && static_cast<Creature&>(owner).CanFly())
    {
        const float dz = spot.z - target.z;
        distSq += dz * dz;
    }

    // The bounding radius is folded into the tolerance exactly as WorldObject's own
    // IsWithinDist2d/3d fold it in, so this stays the same test it always was for the
    // (overwhelmingly common) world-frame case.
    const float maxdist = allowed + i_target->Where().Extent();
    return !(distSq < maxdist * maxdist);
}

Motion::MoveIntent TargetedMovementGenerator::Intent(Unit& owner,
                                                     Motion::MoveStatus const& status,
                                                     uint32 diff)
{
    // The target despawned or logged out: hand control back to whatever default movement
    // sits beneath us on the stack.
    if (!i_target.isValid() || !i_target->IsInWorld())
    {
        return Motion::MoveIntent::Done();
    }

    if (!owner.IsAlive())
    {
        return Motion::MoveIntent::Hold();
    }

    const bool blockedByState =
        owner.hasUnitState(UNIT_STAT_NOT_MOVE) ||
        (GetMovementGeneratorType() == CHASE_MOTION_TYPE &&
         owner.hasUnitState(UNIT_STAT_NO_COMBAT_MOVEMENT)) ||
        LostTarget(owner);

    if (blockedByState)
    {
        ClearMoveState(owner);
        return Motion::MoveIntent::Hold();
    }

    // No shuffling out from under a cast with a cast time or a channel.
    if (owner.IsNonMeleeSpellCasted(false, false, true))
    {
        if (!owner.IsStopped())
        {
            owner.StopMoving();
        }
        return Motion::MoveIntent::Hold();
    }

    // Re-derive the standing spot only every recheck interval.
    //
    // The spot TESTED is the goal the driver actually LAID a leg to, not the one we last
    // wanted. That distinction is load-bearing: when the route failed, no leg was laid,
    // the stale goal fails this test, and we derive a fresh spot and try again on the
    // next recheck — instead of standing there believing we are already on our way.
    bool needDest = !m_haveDest;
    m_recheckTime.Update(diff);
    if (m_recheckTime.Passed())
    {
        m_recheckTime.Reset(RecheckIntervalMs());
        needDest = RequiresNewPosition(owner, status.legGoal);
    }

    if (needDest)
    {
        m_dest = ComputeDestination(owner);
        m_haveDest = true;
        m_targetReached = false;
        AddMoveState(owner);
    }

    // The leg ended (or none was ever needed): we are as close as we asked to be.
    if (!status.traveling && !m_targetReached)
    {
        m_targetReached = true;
        ReachTarget(owner);
    }

    // A chase with no angle ends every leg — and every idle tick in melee — turned toward
    // its victim. The driver owns the facing; we only say we want it.
    const Motion::Facing facing = (m_angle == 0.0f)
        ? Motion::Facing::ToTarget(i_target->GetObjectGuid())
        : Motion::Facing{};

    // Standing on a spot that is still good: hold. Asking to Move here instead would make
    // the driver re-lay the same finished leg on every single tick.
    if (!needDest && !status.traveling)
    {
        return Motion::MoveIntent::Hold(facing);
    }

    uint32 flags = Motion::MOVE_REQUIRE_PATH;

    if (EnableWalking(owner))
    {
        flags |= Motion::MOVE_WALK;
    }

    // A pet heeling its master is allowed to cheat its way to the exact spot; otherwise
    // it strands itself on the wrong side of scenery its master walked straight through.
    if (owner.GetTypeId() == TYPEID_UNIT && static_cast<Creature&>(owner).IsPet() &&
        owner.hasUnitState(UNIT_STAT_FOLLOW))
    {
        flags |= Motion::MOVE_FORCE_DEST;
    }

    return Motion::MoveIntent::Move(m_dest, flags, facing);
}

//----- Chase

void ChaseMovementGenerator::Initialize(Unit& owner)
{
    if (owner.GetTypeId() == TYPEID_UNIT)
    {
        static_cast<Creature&>(owner).SetWalk(false, false); // a chase runs
    }

    owner.addUnitState(UNIT_STAT_CHASE); // _MOVE is set once a leg is laid
    ResetTracking();
}

void ChaseMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void ChaseMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
    ResetTracking();
}

void ChaseMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_CHASE | UNIT_STAT_CHASE_MOVE);
}

bool ChaseMovementGenerator::LostTarget(Unit& owner) const
{
    return owner.getVictim() != GetTarget();
}

void ChaseMovementGenerator::ReachTarget(Unit& owner)
{
    if (InMeleeReach(owner, *(GetTarget())))
    {
        owner.Attack(GetTarget(), true);
    }
}

float ChaseMovementGenerator::TargetDistance(Unit& owner, bool forRangeCheck) const
{
    if (!forRangeCheck)
    {
        return m_offset + CHASE_RANGE * CombatReachBetween(*i_target.getTarget(), owner);
    }

    return CHASE_RECHASE_RANGE * CombatReachBetween(*i_target.getTarget(), owner) -
           i_target->Where().Extent();
}

//----- Follow

void FollowMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_FOLLOW); // _MOVE is set once a leg is laid
    SyncSpeedWithMaster(owner);
    ResetTracking();
}

void FollowMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void FollowMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    SyncSpeedWithMaster(owner);
    ResetTracking();
}

void FollowMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_FOLLOW | UNIT_STAT_FOLLOW_MOVE);
    SyncSpeedWithMaster(owner);
}

bool FollowMovementGenerator::EnableWalking(Unit& owner) const
{
    // A creature follower matches its target's gait; a player follower never walks.
    return owner.GetTypeId() == TYPEID_UNIT && i_target.isValid() && i_target->IsWalking();
}

void FollowMovementGenerator::SyncSpeedWithMaster(Unit& owner) const
{
    if (owner.GetTypeId() != TYPEID_UNIT)
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    // Only a pet mirrors its OWNER's speed — an escorted NPC keeps its own.
    if (!creature.IsPet() || !i_target.isValid() ||
        i_target->GetObjectGuid() != creature.GetOwnerGuid())
    {
        return;
    }

    creature.UpdateSpeed(MOVE_RUN, true);
    creature.UpdateSpeed(MOVE_WALK, true);
    creature.UpdateSpeed(MOVE_SWIM, true);
}

float FollowMovementGenerator::TargetDistance(Unit& owner, bool forRangeCheck) const
{
    if (!forRangeCheck)
    {
        return m_offset + owner.Where().Extent() +
               i_target->Where().Extent();
    }

    float allowed = sWorld.getConfig(CONFIG_FLOAT_RATE_TARGET_POS_RECALCULATION_RANGE) -
                    i_target->Where().Extent();

    allowed += FOLLOW_RECALCULATE_FACTOR *
               (owner.Where().Extent() + i_target->Where().Extent());

    if (m_offset > FOLLOW_DIST_GAP_FOR_DIST_FACTOR)
    {
        allowed += FOLLOW_DIST_RECALCULATE_FACTOR * m_offset;
    }

    return allowed;
}
