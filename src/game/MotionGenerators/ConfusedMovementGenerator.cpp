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

#include "ConfusedMovementGenerator.h"
#include "MotionFrame.h"
#include "Unit.h"
#include "Util.h"

namespace
{
    /// How far from the spot it was confused at a unit may stagger.
    constexpr float STAGGER_RADIUS = 10.0f;

    constexpr uint32 STAGGER_INTERVAL_MIN = 800;
    constexpr uint32 STAGGER_INTERVAL_MAX = 1500;

    /// Retry delay after a lurch that could not be routed or placed.
    constexpr uint32 RETRY_DELAY = 50;
}

void ConfusedMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_CONFUSED);

    // The stagger is anchored to wherever the unit stood when it lost its wits — read
    // through the frame, because for a boarded unit GetPosition() returns the world cache
    // (an estimate of a pose the server does not know), while RandomPoint below will be
    // handed this anchor as a DECK offset. Mixing the two would anchor the stagger to a
    // point somewhere out at sea.
    m_anchor = Motion::FrameFor(owner).MoverPosition(owner);

    m_staggerTime.Reset(0);
    m_haveLurch = false;
    ResetLeg();

    if (!owner.IsAlive() || owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    owner.StopMoving();
    owner.addUnitState(UNIT_STAT_CONFUSED_MOVE);
}

void ConfusedMovementGenerator::Reset(Unit& owner)
{
    m_staggerTime.Reset(0);
    m_haveLurch = false;
    ResetLeg();

    if (!owner.IsAlive() || owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    owner.StopMoving();
    owner.addUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_CONFUSED_MOVE);
}

void ConfusedMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    // The confused state itself outlives the generator being suspended.
    owner.clearUnitState(UNIT_STAT_CONFUSED_MOVE);
    m_haveLurch = false;
    ResetLeg();
}

void ConfusedMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_CONFUSED_MOVE);

    // A player is left where it stands with its client told to stop; a creature's
    // spline is simply abandoned to whatever generator takes over.
    if (owner.GetTypeId() == TYPEID_PLAYER)
    {
        owner.StopMoving(true);
    }
}

Motion::MoveIntent ConfusedMovementGenerator::Intent(Unit& owner,
                                                     Motion::MoveStatus const& status,
                                                     uint32 diff)
{
    // Ignore while any OTHER no-reaction state applies (stunned, rooted, ...).
    if (owner.hasUnitState(UNIT_STAT_CAN_NOT_REACT & ~UNIT_STAT_CONFUSED))
    {
        return Motion::MoveIntent::Hold();
    }

    owner.addUnitState(UNIT_STAT_CONFUSED_MOVE);

    if (status.blocked)
    {
        m_haveLurch = false;
        m_staggerTime.Reset(RETRY_DELAY);
    }

    // The timer runs even mid-leg, which is the point: when it fires early the new
    // destination supersedes the one being walked to and the leg is cut off part-way.
    m_staggerTime.Update(diff);
    if (!m_staggerTime.Passed())
    {
        return (status.traveling && m_haveLurch)
            ? Motion::MoveIntent::Move(m_lurch, Motion::MOVE_WALK)
            : Motion::MoveIntent::Hold();
    }

    const auto lurch = Motion::FrameFor(owner).RandomPoint(owner, m_anchor, STAGGER_RADIUS);
    if (!lurch)
    {
        m_staggerTime.Reset(RETRY_DELAY);
        return Motion::MoveIntent::Hold();
    }

    m_lurch = *lurch;
    m_haveLurch = true;
    m_staggerTime.Reset(urand(STAGGER_INTERVAL_MIN, STAGGER_INTERVAL_MAX));

    return Motion::MoveIntent::Move(m_lurch, Motion::MOVE_WALK);
}
