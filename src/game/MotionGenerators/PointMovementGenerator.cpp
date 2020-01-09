/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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
#include "TemporarySummon.h"
#include "World.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"

//----- Point Movement Generator
template<class T>
void PointMovementGenerator<T>::Initialize(T& unit)
{
    if (unit.hasUnitState(UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE))
        { return; }

    unit.StopMoving();

    unit.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    Movement::MoveSplineInit init(unit);
    init.MoveTo(i_x, i_y, i_z, m_generatePath);
    init.Launch();
}

template<class T>
void PointMovementGenerator<T>::Finalize(T& unit)
{
    unit.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    if (unit.movespline->Finalized())
        { MovementInform(unit); }
}

template<class T>
void PointMovementGenerator<T>::Interrupt(T& unit)
{
    unit.InterruptMoving();
    unit.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

template<class T>
void PointMovementGenerator<T>::Reset(T& unit)
{
    unit.StopMoving();
    unit.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
}

template<class T>
bool PointMovementGenerator<T>::Update(T& unit, const uint32& /*diff*/)
{
    if (unit.hasUnitState(UNIT_STAT_CAN_NOT_MOVE))
    {
        unit.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return true;
    }

    if (!unit.hasUnitState(UNIT_STAT_ROAMING_MOVE) && unit.movespline->Finalized())
        { Initialize(unit); }

    return !unit.movespline->Finalized();
}

template<>
void PointMovementGenerator<Player>::MovementInform(Player&)
{
}

template <>
void PointMovementGenerator<Creature>::MovementInform(Creature& unit)
{
    if (unit.AI())
        { unit.AI()->MovementInform(POINT_MOTION_TYPE, id); }

    if (unit.IsTemporarySummon())
    {
        TemporarySummon* pSummon = (TemporarySummon*)(&unit);
        if (pSummon->GetSummonerGuid().IsCreature())
            if (Creature* pSummoner = unit.GetMap()->GetCreature(pSummon->GetSummonerGuid()))
                if (pSummoner->AI())
                    { pSummoner->AI()->SummonedMovementInform(&unit, POINT_MOTION_TYPE, id); }
    }
}

template void PointMovementGenerator<Player>::Initialize(Player&);
template void PointMovementGenerator<Creature>::Initialize(Creature&);
template void PointMovementGenerator<Player>::Finalize(Player&);
template void PointMovementGenerator<Creature>::Finalize(Creature&);
template void PointMovementGenerator<Player>::Interrupt(Player&);
template void PointMovementGenerator<Creature>::Interrupt(Creature&);
template void PointMovementGenerator<Player>::Reset(Player&);
template void PointMovementGenerator<Creature>::Reset(Creature&);
template bool PointMovementGenerator<Player>::Update(Player&, const uint32& diff);
template bool PointMovementGenerator<Creature>::Update(Creature&, const uint32& diff);


void AssistanceMovementGenerator::Initialize(Unit& unit)
{
    if (unit.hasUnitState(UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE))
        { return; }

    if (!unit.IsStopped())
        { unit.StopMoving(); }

    unit.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    Movement::MoveSplineInit init(unit);
    init.MoveTo(i_x, i_y, i_z, m_generatePath);
    //Slow down the mob that is running for assistance
    //TODO: There are different speeds for the different mobs, isn't there?
    //That should probably be taken into account here
    init.SetWalk(true);
    init.Launch();
}

void AssistanceMovementGenerator::Finalize(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);

    ((Creature*)&unit)->SetNoCallAssistance(false);
    ((Creature*)&unit)->CallAssistance();
    if (unit.IsAlive())
        { unit.GetMotionMaster()->MoveSeekAssistanceDistract(sWorld.getConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY)); }
}

bool EffectMovementGenerator::Update(Unit& unit, const uint32&)
{
    return !unit.movespline->Finalized();
}

void EffectMovementGenerator::Finalize(Unit& unit)
{
    if (unit.GetTypeId() != TYPEID_UNIT)
        { return; }

    if (((Creature&)unit).AI() && unit.movespline->Finalized())
        { ((Creature&)unit).AI()->MovementInform(EFFECT_MOTION_TYPE, m_Id); }
    // Need restore previous movement since we have no proper states system
    if (unit.IsAlive() && !unit.hasUnitState(UNIT_STAT_CONFUSED | UNIT_STAT_FLEEING | UNIT_STAT_NO_COMBAT_MOVEMENT))
    {
        if (Unit* victim = unit.getVictim())
            { unit.GetMotionMaster()->MoveChase(victim); }
        else
            { unit.GetMotionMaster()->Initialize(); }
    }
}

void FlyOrLandMovementGenerator::Initialize(Unit& unit)
{
    if (unit.hasUnitState(UNIT_STAT_CAN_NOT_REACT | UNIT_STAT_NOT_MOVE))
        { return; }

    unit.StopMoving();

    float x, y, z;
    GetDestination(x, y, z);
    unit.addUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    Movement::MoveSplineInit init(unit);
    init.SetFly();
    init.MoveTo(x, y, z, false);
    init.Launch();
}
