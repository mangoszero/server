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

#include <algorithm>
#include "RandomMovementGenerator.h"
#include "Creature.h"
#include "MotionFrame.h"
#include "Util.h"

namespace
{
    /// Percent chance the creature does not pause at all between hops, so a wandering
    /// mob occasionally strings two legs together instead of always resting.
    constexpr int CHANCE_NO_BREAK = 30;

    /// A leash radius below this is meaningless and would make every hop degenerate.
    constexpr float MIN_WANDER_RADIUS = 0.1f;

    constexpr uint32 REST_AFTER_HOP_MIN = 3000;
    constexpr uint32 REST_AFTER_HOP_MAX = 10000;

    /// Retry delay after a hop that could not be routed, or a point that could not be
    /// found. Short enough to look alive, long enough not to hammer the router.
    constexpr uint32 RETRY_DELAY = 50;
}

RandomMovementGenerator::RandomMovementGenerator(float x, float y, float z, float radius)
    : m_centre(x, y, z), m_radius(radius)
{
    if (m_radius < MIN_WANDER_RADIUS)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "RandomMovementGenerator: wrong value for spawn distance. Set to %f", MIN_WANDER_RADIUS);
        m_radius = MIN_WANDER_RADIUS;
    }
}

RandomMovementGenerator::RandomMovementGenerator(Creature const& creature)
{
    float x, y, z, o, wanderDistance;
    x = creature.Spawn().X();
    y = creature.Spawn().Y();
    z = creature.Spawn().Z();
    o = creature.Spawn().Facing();
    wanderDistance = creature.GetRespawnRadius();

    m_centre = Motion::Vector3(x, y, z);
    m_radius = std::max(wanderDistance, MIN_WANDER_RADIUS);
}

void RandomMovementGenerator::Initialize(Unit& owner)
{
    // _MOVE is set once a hop is actually picked.
    owner.addUnitState(UNIT_STAT_ROAMING);

    m_restTime.Reset(0);
    m_haveHop = false;
    ResetLeg();
}

void RandomMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void RandomMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    Finalize(owner);
    m_haveHop = false;
    ResetLeg();
}

void RandomMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    static_cast<Creature&>(owner).SetWalk(!owner.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
}

Motion::MoveIntent RandomMovementGenerator::Intent(Unit& owner,
                                                   Motion::MoveStatus const& status,
                                                   uint32 diff)
{
    if (!owner.IsAlive() || owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        m_restTime.Reset(0);
        owner.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return Motion::MoveIntent::Hold();
    }

    // The point we picked turned out to be unreachable: try somewhere else shortly,
    // rather than hammering the router every tick.
    if (status.blocked)
    {
        m_haveHop = false;
        m_restTime.Reset(RETRY_DELAY);
    }

    // Mid-hop: re-state the same goal, which the driver recognises as the leg it is
    // already walking and leaves alone.
    if (status.traveling && m_haveHop)
    {
        return Motion::MoveIntent::Move(m_hop, Motion::MOVE_WALK);
    }

    // Standing: run down the rest timer.
    m_restTime.Update(diff);
    if (!m_restTime.Passed())
    {
        return Motion::MoveIntent::Hold();
    }

    const auto hop = Motion::FrameFor(owner).RandomPoint(owner, m_centre, m_radius);
    if (!hop)
    {
        m_restTime.Reset(RETRY_DELAY);
        return Motion::MoveIntent::Hold();
    }

    owner.addUnitState(UNIT_STAT_ROAMING_MOVE);
    m_hop = *hop;
    m_haveHop = true;

    // The rest that follows THIS hop is decided now: the timer only runs while the
    // creature is standing, so it starts counting the moment the leg ends.
    m_restTime.Reset(roll_chance_i(CHANCE_NO_BREAK)
        ? RETRY_DELAY
        : urand(REST_AFTER_HOP_MIN, REST_AFTER_HOP_MAX));

    return Motion::MoveIntent::Move(m_hop, Motion::MOVE_WALK);
}
