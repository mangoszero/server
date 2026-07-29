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

#include "PointMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "MotionFrame.h"
#include "TemporarySummon.h"
#include "World.h"
#include "movement/MoveSpline.h"

void PointMovementGenerator::Initialize(Unit& owner)
{
    if (owner.hasUnitState(UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    owner.StopMoving();
    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    // The leg itself is laid on the first tick, by the driver.
    ResetLeg();
}

void PointMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

void PointMovementGenerator::Interrupt(Unit& owner)
{
    owner.InterruptMoving();
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    ResetLeg();
}

void PointMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    // Only a leg that ran to completion counts as reaching the point; one cut short by
    // an interrupt does not.
    if (owner.movespline->Finalized())
    {
        MovementInform(owner);
    }
}

void PointMovementGenerator::MovementInform(Unit& owner) const
{
    if (owner.GetTypeId() != TYPEID_UNIT)
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (creature.AI())
    {
        creature.AI()->MovementInform(POINT_MOTION_TYPE, m_id);
    }

    if (!creature.IsTemporarySummon())
    {
        return;
    }

    const ObjectGuid summonerGuid = static_cast<TemporarySummon&>(creature).GetSummonerGuid();
    if (!summonerGuid.IsCreature())
    {
        return;
    }

    if (Creature* summoner = creature.GetMap()->GetCreature(summonerGuid))
    {
        if (summoner->AI())
        {
            summoner->AI()->SummonedMovementInform(&creature, POINT_MOTION_TYPE, m_id);
        }
    }
}

Motion::MoveIntent PointMovementGenerator::Intent(Unit& owner,
                                                  Motion::MoveStatus const& status,
                                                  uint32 /*diff*/)
{
    if (owner.hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        owner.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return Motion::MoveIntent::Hold();
    }

    // Arrived, or there was no way to get there at all: either way this one-shot is
    // over and the generator beneath it takes back over. Finalize fires the AI inform.
    if (status.arrived || status.blocked)
    {
        return Motion::MoveIntent::Done();
    }

    owner.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    // m_dest arrived from a script, an AI or a spell effect, and those speak WORLD
    // coordinates — they always will, since that is the only kind the database and the
    // script API have. Read straight as a goal it would be taken for a deck offset by a
    // boarded unit. Converted here, not in the constructor, because the conversion must
    // survive a Reset() and must be re-done if the frame beneath us ever changes.
    //
    // FromWorld is the identity in the world frame, so this costs nothing for everyone
    // who is not standing on a boat.
    const Motion::Vector3 goal = Motion::FrameFor(owner).FromWorld(owner, m_dest);

    return Motion::MoveIntent::Move(goal, LegFlags());
}

void AssistanceMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    Creature& creature = static_cast<Creature&>(owner);
    creature.SetNoCallAssistance(false);
    creature.CallAssistance();

    if (creature.IsAlive())
    {
        creature.GetMotionMaster()->MoveSeekAssistanceDistract(
            sWorld.getConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY));
    }
}

Motion::MoveIntent EffectMovementGenerator::Intent(Unit& /*owner*/,
                                                   Motion::MoveStatus const& status,
                                                   uint32 /*diff*/)
{
    // Note this is `traveling`, not `arrived`: the spline was launched by the effect,
    // not by us, so if it was never running at all we must pop immediately rather than
    // wait for an arrival edge that will never come.
    return status.traveling ? Motion::MoveIntent::Hold() : Motion::MoveIntent::Done();
}

void EffectMovementGenerator::Finalize(Unit& owner)
{
    if (owner.GetTypeId() != TYPEID_UNIT)
    {
        return;
    }

    Creature& creature = static_cast<Creature&>(owner);

    if (creature.AI() && owner.movespline->Finalized())
    {
        creature.AI()->MovementInform(EFFECT_MOTION_TYPE, m_id);
    }

    // Restore the previous movement, since we have no proper state system for it.
    if (!owner.IsAlive() ||
        owner.hasUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING | UNIT_STAT_NO_COMBAT_MOVEMENT))
    {
        return;
    }

    if (Unit* victim = owner.getVictim())
    {
        owner.GetMotionMaster()->MoveChase(victim);
    }
    else
    {
        owner.GetMotionMaster()->Initialize();
    }
}
