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
}

Motion::MoveIntent HomeMovementGenerator::Intent(Unit& /*owner*/,
                                                 Motion::MoveStatus const& status,
                                                 uint32 /*diff*/)
{
    // A creature that could not be sent home — it cannot move, or there was no way back
    // at all — still counts as home. Evade MUST always terminate, or the creature stays
    // stuck in a fight it has already left.
    if (!m_haveHome || status.arrived || status.blocked)
    {
        m_arrived = true;
        return Motion::MoveIntent::Done();
    }

    return Motion::MoveIntent::Move(m_home, Motion::MOVE_NONE,
                                    Motion::Facing::ToAngle(m_facing));
}

void HomeMovementGenerator::Finalize(Unit& owner)
{
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
