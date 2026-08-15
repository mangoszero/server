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

void HomeMovementGenerator::Initialize(Unit& owner)
{
    m_arrived = false;
    m_haveHome = false;
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
                                                 uint32 /*diff*/)
{
    // There is deliberately no UNIT_STAT_CAN_NOT_MOVE guard here, because one would be dead
    // code: MotionMaster::UpdateMotion returns before touching the top generator while that
    // state is set, so a rooted or stunned creature never reaches this function at all.
    // (PointMovementGenerator carries such a guard; it is unreachable for the same reason.)
    // What a root actually does to us is stop the spline, and that is what is caught below.

    // A STOP IS NOT AN ARRIVAL.
    //
    // The driver reports `arrived` for any leg that stopped running, and it cannot tell why
    // it stopped. The unit state can: a leg that ran out leaves UNIT_STAT_ROAMING_MOVE
    // standing, while every forced stop goes through Unit::StopMoving, which clears
    // UNIT_STAT_MOVING -- and ROAMING_MOVE lives inside that mask.
    //
    // This matters only because making the move state honest is what let StopMoving work on
    // an evading creature at all. Now that it does, a root or a stun taken mid-return would
    // otherwise end evade and fire JustReachedHome wherever the creature happened to be
    // standing, handing a patroller back to its waypoints from the wrong point on its route.
    // Nor is it only combat: opening a gossip, quest or vendor window calls StopMoving on the
    // creature directly, and none of those refuse an NPC merely for being on its way home.
    //
    // So a stop that was not an arrival resumes the journey instead of ending it.
    if (status.arrived && m_haveHome && !owner.hasUnitState(UNIT_STAT_ROAMING_MOVE))
    {
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
