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

#include "HomeMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "MotionFrame.h"
#include "movement/MoveSpline.h"

namespace
{
    /// How near counts as home. A completed leg ends on the goal, so this only has to
    /// absorb the difference between the spline's end and the terrain the creature is
    /// finally placed on.
    constexpr float HOME_ARRIVAL_TOLERANCE = 2.0f;

    /// A resume must close at least this much distance to count as progress.
    constexpr float HOME_PROGRESS_EPSILON = 0.5f;

    /// How long a creature may keep failing to get nearer before evade gives up. Evade
    /// MUST always terminate: a player can stop a creature from the outside as often as
    /// they like -- opening a gossip window does it, once per packet -- so resuming has
    /// to be bounded by something the player cannot keep resetting.
    constexpr uint32 HOME_STALL_BUDGET = 10000;

    /// Put the creature where its spline actually left it.
    ///
    /// MoveSplineInit::Stop computes the true stop position for the packet it sends but
    /// never relocates the unit, and Unit::UpdateSplineMovement skips its 400 ms placement
    /// once the spline is finalized. So the server position can be most of a step behind,
    /// and a leg routed from there walks the creature back over ground it has left.
    void SyncToSpline(Unit& owner)
    {
        if (owner.GetTypeId() != TYPEID_UNIT || !owner.movespline->Initialized())
        {
            return;
        }

        const Movement::Location at = owner.movespline->ComputePosition();
        owner.GetMap()->CreatureRelocation(static_cast<Creature*>(&owner),
                                           at.x, at.y, at.z, at.orientation);
    }
}

void HomeMovementGenerator::Initialize(Unit& owner)
{
    m_arrived = false;
    m_haveHome = false;
    m_closest = 0.0f;
    m_stalled = 0;
    ResetLeg();

    if (owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    // MotionMaster::Mutate initializes us BEFORE pushing us, so the stack top here is
    // still the generator we are evacuating — and it is the only one that knows where
    // this creature belongs. Ask it now; once we are on top the answer is unreachable.
    float x, y, z, o;
    MotionMaster* motion = owner.GetMotionMaster();

    if (motion->empty() || !motion->top()->GetResetPosition(owner, x, y, z, o))
    {
        const Creature& home = static_cast<Creature&>(owner);
        x = home.Spawn().X();
        y = home.Spawn().Y();
        z = home.Spawn().Z();
        o = home.Spawn().Facing();
    }

    m_home = Motion::Vector3(x, y, z);
    m_facing = o;
    m_haveHome = true;

    owner.clearUnitState(UNIT_STAT_ALL_DYN_STATES);

    // AFTER the wipe above, not before. Evade drops every dynamic state the fight left
    // behind — that is the point of the clear — but the creature is about to run, and a
    // runner has to say so. See the header for what a missing move state cost.
    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

void HomeMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    ResetLeg();
}

void HomeMovementGenerator::Reset(Unit& owner)
{
    // Not Initialize. Reset runs when this generator is uncovered and becomes the top of
    // the stack again, and by then it IS the top — so re-running Initialize would ask
    // itself where home is, get no answer, and fall back to the spawn point, throwing
    // away the patrol position captured before we were pushed. Restore the state an
    // Interrupt cleared and let the next tick lay a fresh leg.
    if (m_haveHome)
    {
        owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    }

    ResetLeg();
}

Motion::MoveIntent HomeMovementGenerator::Intent(Unit& owner,
                                                 Motion::MoveStatus const& status,
                                                 uint32 diff)
{
    // There is deliberately no UNIT_STAT_CAN_NOT_MOVE guard here, because one would be dead
    // code: MotionMaster::UpdateMotion returns before touching the top generator while that
    // state is set, so a rooted or stunned creature never reaches this function at all.
    // (PointMovementGenerator carries such a guard; it is unreachable for the same reason.)
    // What a root actually does to us is stop the spline, and that is caught below.

    // ARRIVAL IS A PLACE, NOT AN EVENT.
    //
    // `status.arrived` means only that a leg stopped running, and the driver cannot tell
    // why. Three different things produce it and only one of them is arriving home:
    //
    //   * the home leg ran out -- the creature is home;
    //   * something stopped it -- a root, a stun, or a player opening a gossip, quest or
    //     vendor window, all of which call Unit::StopMoving on the creature directly;
    //   * something replaced the spline entirely -- SetFacingTo and MonsterMoveWithSpeed
    //     launch their own without telling the driver, so a scripted TURN_TO during an
    //     evade ends a leg that was never the home leg.
    //
    // Only the first is an arrival, and no flag distinguishes them: the replacement-spline
    // case leaves every movement state exactly as a real arrival does. So ask the ground
    // instead. If the creature is not at home, it has not reached home, whatever ended the
    // leg -- and it should carry on rather than announce it got there.
    if (status.arrived && m_haveHome && !AtHome(owner))
    {
        if (!Resumable(owner, diff))
        {
            m_arrived = true;
            return Motion::MoveIntent::Done();
        }

        SyncToSpline(owner);
        owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

        return Motion::MoveIntent::Move(m_home, Motion::MOVE_NONE,
                                        Motion::Facing::ToAngle(m_facing));
    }

    // A creature that could not be sent home — it cannot move, or there was no way back
    // at all — still counts as home. Evade MUST always terminate, or the creature stays
    // stuck in a fight it has already left.
    if (!m_haveHome || status.arrived || status.blocked)
    {
        // Only `arrived` means a leg ran out. The other two end evade with a spline
        // possibly still in flight: `blocked` is the router refusing the NEXT leg while
        // the previous one runs, and `!m_haveHome` never laid one at all, so whatever the
        // evacuated generator left running is still going. Ending here without stopping it
        // hands the next generator a creature already travelling somewhere else.
        if (!status.arrived)
        {
            owner.InterruptMoving();
        }

        m_arrived = true;
        return Motion::MoveIntent::Done();
    }

    // Re-asserted every travelling tick, not just in Initialize, so it survives anything
    // that strips it while the journey is still unfinished.
    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    return Motion::MoveIntent::Move(m_home, Motion::MOVE_NONE,
                                    Motion::Facing::ToAngle(m_facing));
}

bool HomeMovementGenerator::AtHome(Unit const& owner) const
{
    const Motion::Vector3 gap = Motion::FrameFor(owner).MoverPosition(owner) - m_home;

    return gap.squaredLength() <= HOME_ARRIVAL_TOLERANCE * HOME_ARRIVAL_TOLERANCE;
}

bool HomeMovementGenerator::Resumable(Unit const& owner, uint32 diff)
{
    const Motion::Vector3 gap = Motion::FrameFor(owner).MoverPosition(owner) - m_home;
    const float distance = gap.length();

    // Progress resets the budget, so an evade interrupted many times over a long return is
    // not punished for it -- only one that stops getting nearer is. m_closest starts at
    // zero and is seeded on the first resume, since until then there is nothing to beat.
    if (m_stalled == 0 && m_closest == 0.0f)
    {
        m_closest = distance;
    }
    else if (distance < m_closest - HOME_PROGRESS_EPSILON)
    {
        m_closest = distance;
        m_stalled = 0;
    }
    else
    {
        m_stalled += diff;
    }

    return m_stalled < HOME_STALL_BUDGET;
}

void HomeMovementGenerator::Finalize(Unit& owner)
{
    // Unconditionally, and before the early return below. The state says "this generator
    // is moving the creature", so it must not outlive the generator — and Finalize runs on
    // every removal, including the ones that arrive here with m_arrived false because
    // something else evicted us mid-return.
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    if (!m_arrived)
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (creature.GetTemporaryFactionFlags() & TEMPFACTION_RESTORE_REACH_HOME)
    {
        creature.ClearTemporaryFaction();
    }

    creature.SetWalk(!creature.hasUnitState(UNIT_STAT_RUNNING_STATE) && !creature.IsLevitating(), false);
    creature.LoadCreatureAddon(true);
    creature.AI()->JustReachedHome();
}
