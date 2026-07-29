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

#include "MotionDriver.h"
#include "ObjectLookup.h"
#include "Unit.h"
#include "movement/MoveSpline.h"
#include "movement/MoveSplineInit.h"

#include <cmath>

namespace
{
    /// A goal that moved less than this is treated as the same goal. Re-laying a leg
    /// for a sub-yard nudge floods the client with SMSG_MONSTER_MOVE and reads on
    /// screen as a foot-slide — exactly what the per-generator "RequiresNewPosition"
    /// guards existed to prevent.
    constexpr float MIN_RELAY_DISTANCE = 0.5f;

    /// Below this the unit already faces where it was asked to; re-orienting would
    /// only cost a packet.
    constexpr float FACING_EPSILON = 0.01f;
}

void MotionDriver::ResetLeg()
{
    m_legGoal = Motion::Vector3();
    m_haveLeg = false;
    m_blocked = false;
    m_speedChanged = false;
    m_wasTraveling = false;
}

Motion::IPathQuery* MotionDriver::Query(Unit const& owner)
{
    Motion::IMotionFrame const& frame = Motion::FrameFor(owner);

    // A leg never spans two frames: if the mover boarded (or left) a transport since
    // the last leg, the old router speaks the wrong coordinate system.
    if (!m_query || m_queryFrame != frame.Kind())
    {
        m_query = frame.CreatePathQuery(owner);
        m_queryFrame = frame.Kind();
    }

    return m_query.get();
}

Motion::MoveStatus MotionDriver::BeginTick(Unit& owner)
{
    const bool traveling = !owner.movespline->Finalized();

    Motion::MoveStatus status;
    status.traveling = traveling;
    status.arrived = m_wasTraveling && !traveling;
    status.blocked = m_blocked;
    status.pathIndex = owner.movespline->Initialized() ? owner.movespline->currentPathIdx() : 0;

    if (m_haveLeg)
    {
        status.legGoal = m_legGoal;
    }

    // Both edges are reported exactly once. A sticky `blocked` would starve every
    // generator whose answer to it is "reset my retry timer and try again in a moment":
    // it would reset the timer on every tick and never fire it.
    m_blocked = false;
    m_wasTraveling = traveling;

    return status;
}

bool MotionDriver::Apply(Unit& owner, Motion::MoveIntent const& intent)
{
    switch (intent.act)
    {
        case Motion::MoveIntent::Act::Done:
            return false;

        case Motion::MoveIntent::Act::Move:
            ReconcileMove(owner, intent);
            return true;

        case Motion::MoveIntent::Act::Hold:
            ReconcileHold(owner, intent);
            return true;
    }

    return true;
}

bool MotionDriver::ReconcileMove(Unit& owner, Motion::MoveIntent const& intent)
{
    // Lay a fresh leg when there is nothing to ride, or — for a goal that tracks
    // something that moves — when it has drifted past the intent's tolerance. A live
    // leg whose goal is still fresh is left alone: re-routing every tick would spam the
    // client and read as a foot-slide.
    bool relay = !m_haveLeg || owner.movespline->Finalized();

    // A speed change re-paces a routed leg (the route from HERE to the goal is still
    // the right one, it is just being walked at the wrong pace). It must NOT re-lay an
    // explicit leg: that geometry was built once, from the leg's START, and rebuilding
    // it from a point halfway along would walk the unit back to the beginning of its
    // own path. Such a leg keeps its old pacing until it ends, which is what waypoint
    // movement has always done.
    if (!relay && m_speedChanged && !intent.path)
    {
        relay = true;
    }

    if (!relay)
    {
        const Motion::Vector3 drift = intent.goal - m_legGoal;
        relay = drift.squaredLength() > MIN_RELAY_DISTANCE * MIN_RELAY_DISTANCE;
    }

    return relay ? LayLeg(owner, intent) : false;
}

bool MotionDriver::LayLeg(Unit& owner, Motion::MoveIntent const& intent)
{
    Movement::MoveSplineInit init(owner);

    if (intent.path && intent.path->size() >= 2)
    {
        // The generator dictated the exact geometry (the smoothed patrol).
        init.MovebyPath(*intent.path);
    }
    else if (intent.Has(Motion::MOVE_STRAIGHT))
    {
        // No routing at all: jumps, effects, forced moves.
        init.MoveTo(intent.goal.x, intent.goal.y, intent.goal.z, false);
    }
    else
    {
        // Route toward the goal through the mover's frame — the one call behind which
        // all of collision, obstacle avoidance and (later) the transport deck live.
        Motion::IPathQuery* query = Query(owner);
        const Motion::Vector3 start = Motion::FrameFor(owner).MoverPosition(owner);

        const bool routed = query && query->Calculate(start, intent.goal,
                                                      intent.Has(Motion::MOVE_FORCE_DEST),
                                                      intent.pathLengthLimit);

        // Nothing usable at all, or the router failed and this movement kind is one
        // that refuses the straight-line fallback through whatever is in the way.
        // Either way no leg is laid, and the generator is told so on its next tick so
        // it can give up or pick somewhere else.
        if (!routed || (intent.Has(Motion::MOVE_REQUIRE_PATH) && query->Failed()))
        {
            m_blocked = true;
            return false;
        }

        init.MovebyPath(query->Points());
    }

    switch (intent.facing.mode)
    {
        case Motion::Facing::Mode::Angle:
            init.SetFacing(intent.facing.angle);
            break;

        case Motion::Facing::Mode::Spot:
            init.SetFacing(intent.facing.spot);
            break;

        case Motion::Facing::Mode::Target:
            if (Unit* target = ObjectLookup::GetUnit(owner, intent.facing.target))
            {
                init.SetFacing(target);
            }
            break;

        case Motion::Facing::Mode::None:
            break;
    }

    init.SetWalk(intent.Has(Motion::MOVE_WALK));

    if (intent.Has(Motion::MOVE_FLY))
    {
        init.SetFly();
    }

    // The velocity is left to MoveSplineInit, which resolves the unit's live
    // walk/run/swim/flight speed from its movement flags at Launch — so a speed change
    // re-paces the next leg instead of a stale value being baked in here.
    init.Launch();

    m_legGoal = intent.goal;
    m_haveLeg = true;
    m_blocked = false;
    m_speedChanged = false;
    m_wasTraveling = true;

    return true;
}

void MotionDriver::ReconcileHold(Unit& owner, Motion::MoveIntent const& intent)
{
    // A running leg is deliberately NOT cut short: letting it finish is what stops an
    // arriving chase from stuttering a yard short of its victim. A generator that
    // really must halt calls Unit::StopMoving itself — that is a unit-level action, not
    // a decision about the next leg.
    if (!owner.movespline->Finalized())
    {
        return;
    }

    switch (intent.facing.mode)
    {
        case Motion::Facing::Mode::Target:
        {
            Unit* target = ObjectLookup::GetUnit(owner, intent.facing.target);
            if (target && !owner.Where().HasInArc(target->Where(), FACING_EPSILON))
            {
                owner.SetInFront(target);
            }
            break;
        }
        case Motion::Facing::Mode::Angle:
        {
            if (std::fabs(owner.Where().Facing() - intent.facing.angle) > FACING_EPSILON)
            {
                owner.SetFacingTo(intent.facing.angle);
            }
            break;
        }
        case Motion::Facing::Mode::Spot:
        {
            const float angle = owner.Where().BearingTo(Geometry::Vector2(intent.facing.spot.x, intent.facing.spot.y));
            if (std::fabs(owner.Where().Facing() - angle) > FACING_EPSILON)
            {
                owner.SetFacingTo(angle);
            }
            break;
        }
        case Motion::Facing::Mode::None:
            break;
    }
}
