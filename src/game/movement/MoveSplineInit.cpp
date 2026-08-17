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

#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include "packet_builder.h"
#include "Unit.h"
#include "Transports.h"
#include "TransportMap.h"
#include "Map.h"
#include "Player.h"
#include "Creature.h"

namespace
{
    /// The vessel whose deck this unit is standing on, or an empty guid. Derived from the
    /// map, so a spline goes out as SMSG_MONSTER_MOVE_TRANSPORT for anything on a deck --
    /// crew, pet or totem alike -- without anyone having registered it as anything.
    ObjectGuid DeckVesselGuidOf(Unit const& unit)
    {
        if (Map* on = unit.GetMap())
        {
            if (TransportMap* hull = on->AsTransport())
            {
                if (Transport* vessel = hull->Vessel())
                {
                    return vessel->GetObjectGuid();
                }
            }
        }
        return ObjectGuid();
    }
}

namespace Movement
{

    /**
     * @brief Selects the appropriate speed type based on movement flags.
     * @param moveFlags The movement flags.
     * @return The selected UnitMoveType.
     */
    UnitMoveType SelectSpeedType(uint32 moveFlags)
    {
        if (moveFlags & MOVEFLAG_SWIMMING)
        {
            if (moveFlags & MOVEFLAG_BACKWARD /*&& speed_obj.swim >= speed_obj.swim_back*/)
            {
                return MOVE_SWIM_BACK;
            }
            else
            {
                return MOVE_SWIM;
            }
        }
        else if (moveFlags & MOVEFLAG_WALK_MODE)
        {
            // if ( speed_obj.run > speed_obj.walk )
            return MOVE_WALK;
        }
        else if (moveFlags & MOVEFLAG_BACKWARD /*&& speed_obj.run >= speed_obj.run_back*/)
        {
            return MOVE_RUN_BACK;
        }

        return MOVE_RUN;
    }

    /**
     * @brief Final pass of initialization that launches spline movement.
     * @return int32 duration - estimated travel time
     */
    int32 MoveSplineInit::Launch()
    {
        MoveSpline& move_spline = *unit.movespline;

        // A DECK IS NOT A SEAT. The unit's map is the vessel and its position is already
        // deck-local, so Where() is the answer and nothing is composed or looked up.
        const ObjectGuid vesselGuid = DeckVesselGuidOf(unit);

        Vector3 real_position(unit.Where().X(), unit.Where().Y(), unit.Where().Z());
        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows calculate spline position and update map position in much greater intervals
        if (!move_spline.Finalized())
        {
            real_position = move_spline.ComputePosition();
        }

        if (args.path.empty())
        {
            // should i do the things that user should do?
            MoveTo(real_position);
        }

        // correct first vertex
        args.path[0] = real_position;
        uint32 moveFlags = unit.m_movementInfo.GetMovementFlags();
        if (args.flags.runmode)
        {
            moveFlags &= ~MOVEFLAG_WALK_MODE;
        }
        else
        {
            moveFlags |= MOVEFLAG_WALK_MODE;
        }

        moveFlags |= (MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD);

        if (args.velocity == 0.f)
        {
            args.velocity = unit.GetSpeed(SelectSpeedType(moveFlags));
        }

        if (!args.Validate(&unit))
        {
            return 0;
        }

        unit.m_movementInfo.SetMovementFlags((MovementFlags)moveFlags);
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (!vesselGuid.IsEmpty())
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data << vesselGuid.WriteAsPacked();
        }

        PacketBuilder::WriteMonsterMove(move_spline, data);
        unit.SendMessageToSet(&data, true);

        return move_spline.Duration();
    }

    /**
     * @brief Stops any creature movement.
     */
    void MoveSplineInit::Stop()
    {
        MoveSpline& move_spline = *unit.movespline;

        // No need to stop if we are not moving
        if (move_spline.Finalized())
        {
            return;
        }

        const ObjectGuid vesselGuid = DeckVesselGuidOf(unit);

        Location real_position(unit.Where().X(), unit.Where().Y(), unit.Where().Z(), unit.Where().Facing());

        // there is a big chance that current position is unknown if current state is not finalized, need compute it
        // this also allows calculate spline position and update map position in much greater intervals
        if (!move_spline.Finalized() /*&& !transportInfo*/)
        {
            real_position = move_spline.ComputePosition();
        }
        if (args.path.empty())
        {
            // should i do the things that user should do?
            MoveTo(real_position);
        }

        // current first vertex
        args.path[0] = real_position;

        // COMMIT IT. This function is the only thing that knows where the unit really
        // stopped: it interpolates the live spline to find out, writes that into the packet
        // below -- and used to throw it away, because MoveSpline::Initialize clears the
        // spline the moment the Done flag is set. The server then believed the unit was
        // wherever Unit::UpdateSplineMovement last placed it, which it does only every
        // POSITION_UPDATE_DELAY, so it could disagree with the stop it had just announced by
        // most of a 400 ms step -- nearly three yards at ordinary run speed, more under a
        // speed effect. Every later question about where that unit is, from an evading
        // creature asking whether it is home to a path routed from its feet, was answered
        // from the stale one.
        //
        // Once a stop reaches MoveSplineInit::Stop, doing the relocation here makes the
        // server agree with the packet it sends. Unit::InterruptMoving still relocates
        // first and then calls through to here; that duplicate is safe because both use the
        // same spline coordinate.
        if (unit.IsInWorld())
        {
            if (unit.GetTypeId() == TYPEID_PLAYER)
            {
                static_cast<Player*>(&unit)->SetPosition(real_position.x, real_position.y,
                                                         real_position.z, real_position.orientation);
            }
            else
            {
                unit.GetMap()->CreatureRelocation(static_cast<Creature*>(&unit),
                                                  real_position.x, real_position.y,
                                                  real_position.z, real_position.orientation);
            }
        }

        args.flags = MoveSplineFlag::Done;
        unit.m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_FORWARD | MOVEFLAG_SPLINE_ENABLED));
        move_spline.Initialize(args);

        WorldPacket data(SMSG_MONSTER_MOVE, 64);
        data << unit.GetPackGUID();

        if (!vesselGuid.IsEmpty())
        {
            data.SetOpcode(SMSG_MONSTER_MOVE_TRANSPORT);
            data << vesselGuid.WriteAsPacked();
        }

        data << real_position.x << real_position.y << real_position.z;
        data << move_spline.GetId();
        data << uint8(MonsterMoveStop);
        unit.SendMessageToSet(&data, true);
    }

    /**
     * @brief Constructor that initializes the MoveSplineInit with a reference to a Unit.
     * @param m Reference to the Unit to be moved.
     */
    MoveSplineInit::MoveSplineInit(Unit& m) : unit(m)
    {
        // mix existing state into new
        args.flags.runmode = !unit.m_movementInfo.HasMovementFlag(MOVEFLAG_WALK_MODE);
        args.flags.flying = unit.m_movementInfo.HasMovementFlag((MovementFlags)(MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING | MOVEFLAG_LEVITATING));
    }

    /**
     * @brief Sets unit's facing to a specified target after all path done.
     * @param target The target to face.
     */
    void MoveSplineInit::SetFacing(const Unit* target)
    {
        args.flags.EnableFacingTarget();
        args.facing.target = target->GetObjectGuid().GetRawValue();
    }

    /**
     * @brief Adds final facing animation.
     * Sets unit's facing to specified point/angle after all path done.
     * You can have only one final facing: previous will be overridden.
     * @param angle The angle to face.
     */
    void MoveSplineInit::SetFacing(float angle)
    {
        args.facing.angle = Geometry::wrap(angle, 0.f, (float)Geometry::twoPi());
        args.flags.EnableFacingAngle();
    }
}
