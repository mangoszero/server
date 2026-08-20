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
#include "MotionFrame.h"

namespace
{
    /// How near counts as home. A completed leg ends on the goal, so this only has to
    /// absorb the difference between the spline's end and the terrain the creature is
    /// finally placed on.
    constexpr float HOME_ARRIVAL_TOLERANCE = 2.0f;

    /// Movement within one path segment must cover this much to count as progress. The
    /// direction is deliberately irrelevant: a sound route may initially lead away from
    /// home to get around an obstacle.
    constexpr float HOME_PROGRESS_EPSILON = 0.5f;

    /// How much HomeMovementGenerator update time may pass without progress before evade
    /// gives up. This is not wall-clock time: root, stun and a covering generator prevent
    /// MotionMaster from ticking us and therefore do not spend it. Evade nevertheless MUST
    /// always terminate while it is active: a player can stop a creature from the outside
    /// as often as they like -- opening a gossip window does it, once per packet -- so
    /// resuming has to be bounded by something those stops cannot keep resetting.
    constexpr uint32 HOME_STALL_BUDGET = 10000;
}

void HomeMovementGenerator::Initialize(Unit& owner)
{
    m_arrived = false;
    m_haveHome = false;
    m_pathIndex = 0;
    m_homeIntentIssued = false;
    m_expectHomeLeg = false;
    m_onHomeLeg = false;
    m_stalled = 0;
    RefreshSpeedRates(owner);
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
    m_pathIndex = 0;
    m_homeIntentIssued = false;
    m_expectHomeLeg = false;
    m_onHomeLeg = false;
    RefreshSpeedRates(owner);
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

    // ResetLeg means the next Intent must establish a new home-leg sequence. Keep the
    // accumulated stall charge: covering this generator must not buy another budget.
    m_pathIndex = 0;
    m_homeIntentIssued = false;
    m_expectHomeLeg = false;
    m_onHomeLeg = false;
    RefreshSpeedRates(owner);
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

    // IntentMovementGenerator has already told the driver about a speed-rate change. It
    // will re-lay the home route after this Intent, which legitimately resets pathIndex;
    // remember that now so the next tick does not classify the driver's own replacement as
    // a foreign spline. SetSpeedRate propagates only real value changes, so comparing the
    // same six rates observes exactly the event that matters here.
    const bool speedChanged = RefreshSpeedRates(owner);

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
    // Accounted every tick, before anything else. Charging the budget only on an arrived
    // edge undercounted it badly: a foreign leg that runs for thirty seconds contributes a
    // single tick's worth, and something that keeps replacing the live spline before it ever
    // finalizes contributes nothing at all. Resumable therefore charges every update which
    // cannot prove progress on the home leg, whether or not a leg ended.
    const bool spent = !Resumable(owner, status, diff);

    if (status.arrived && m_haveHome && !AtHome(owner))
    {
        if (spent)
        {
            m_arrived = true;
            return Motion::MoveIntent::Done();
        }

        owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

        // The finalized spline is gone, so the Move below necessarily asks the driver for
        // a fresh home leg. Its first status sample seeds the new path sequence; merely
        // launching it is not progress and does not erase charge accumulated across stops.
        m_expectHomeLeg = true;
        m_onHomeLeg = false;
        m_homeIntentIssued = true;

        return Motion::MoveIntent::Move(m_home, Motion::MOVE_NONE,
                                        Motion::Facing::ToAngle(m_facing));
    }

    // A creature that could not be sent home — it cannot move, or there was no way back
    // at all — still counts as home. Evade MUST always terminate, or the creature stays
    // stuck in a fight it has already left.
    if (!m_haveHome || status.arrived || status.blocked || spent)
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

    // On the first update the driver has no cached leg and will lay this Move even when an
    // evacuated generator left a foreign spline running. Do not mistake that old spline's
    // path index for ours; start observing on the following update instead. The same rule
    // covers a finalized spline which reached us without producing an arrival edge.
    if (!m_homeIntentIssued || !status.traveling || speedChanged)
    {
        m_expectHomeLeg = true;
        m_onHomeLeg = false;
        m_homeIntentIssued = true;
    }

    return Motion::MoveIntent::Move(m_home, Motion::MOVE_NONE,
                                    Motion::Facing::ToAngle(m_facing));
}

bool HomeMovementGenerator::AtHome(Unit const& owner) const
{
    const Motion::Vector3 gap = Motion::FrameFor(owner).MoverPosition(owner) - m_home;

    return gap.squaredLength() <= HOME_ARRIVAL_TOLERANCE * HOME_ARRIVAL_TOLERANCE;
}

bool HomeMovementGenerator::Resumable(Unit const& owner,
                                      Motion::MoveStatus const& status,
                                      uint32 diff)
{
    if (!m_haveHome)
    {
        return false;
    }

    const Motion::Vector3 position = Motion::FrameFor(owner).MoverPosition(owner);

    // Intent runs before the driver applies its answer. m_expectHomeLeg bridges that one
    // update: it is set only when the preceding answer made the driver attempt a fresh home
    // leg, and the status below says whether one actually ran. Seed both observations
    // without awarding progress; otherwise a player who stops every new leg before it
    // advances could replenish the budget merely by making us relaunch.
    if (m_expectHomeLeg)
    {
        m_expectHomeLeg = false;
        m_onHomeLeg = status.traveling || status.arrived;
        m_pathIndex = 0;
        m_progressPosition = position;
    }

    bool progressed = false;

    if (m_onHomeLeg)
    {
        // currentPathIdx is monotonic within one non-cyclic home spline. A lower index
        // therefore means something launched a replacement straight past MotionDriver;
        // progress on that foreign spline must not buy more evade time. A higher index
        // while it is still travelling is route progress regardless of whether that route
        // runs toward, sideways to, or temporarily away from home.
        //
        // One case slips through and is worth knowing the shape of: a foreign spline that
        // replaces the home leg while that leg is still on its first point regresses no
        // index, so the displacement it causes can be credited as ours. MoveStatus carries
        // no ownership or generation marker that would settle it. What it costs is bounded
        // though -- such a creature keeps being told to go home, so the failure is that
        // evade takes longer to give up, not that it gives up in the wrong place. Arrival
        // is still decided by AtHome, which asks the ground.
        if (status.pathIndex < m_pathIndex)
        {
            m_onHomeLeg = false;
        }
        else
        {
            const Motion::Vector3 travelled = position - m_progressPosition;
            progressed = (status.traveling && status.pathIndex > m_pathIndex) ||
                travelled.squaredLength() >= HOME_PROGRESS_EPSILON * HOME_PROGRESS_EPSILON;
            m_pathIndex = status.pathIndex;

            if (progressed)
            {
                m_progressPosition = position;
            }
        }
    }

    if (progressed)
    {
        m_stalled = 0;
    }
    else
    {
        // Saturate rather than wrap if an update itself is exceptionally large.
        m_stalled = diff >= HOME_STALL_BUDGET - m_stalled
            ? HOME_STALL_BUDGET
            : m_stalled + diff;
    }

    return m_stalled < HOME_STALL_BUDGET;
}

bool HomeMovementGenerator::RefreshSpeedRates(Unit const& owner)
{
    static_assert(MAX_MOVE_TYPE == 6,
                  "HomeMovementGenerator must track every speed that can re-lay its route");

    bool changed = false;

    for (uint32 i = 0; i < m_speedRates.size(); ++i)
    {
        const float rate = owner.GetSpeedRate(UnitMoveType(i));
        changed = changed || m_speedRates[i] != rate;
        m_speedRates[i] = rate;
    }

    return changed;
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

    // The captured facing, applied here rather than left to the leg. A home leg carries it
    // and would have set it on arrival, but the leg that ends an evade is not always the
    // home leg -- a foreign SetFacingTo finishing inside the arrival tolerance ends it too,
    // pointing the creature wherever that turn wanted. LoadCreatureAddon does not restore
    // orientation, so nothing else would put it back.
    if (m_haveHome)
    {
        creature.SetFacingTo(m_facing);
    }

    creature.SetWalk(!creature.hasUnitState(UNIT_STAT_RUNNING_STATE) && !creature.IsLevitating(), false);
    creature.LoadCreatureAddon(true);
    creature.AI()->JustReachedHome();
}
