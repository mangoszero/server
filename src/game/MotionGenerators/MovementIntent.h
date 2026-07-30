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

#ifndef MANGOS_MOVEMENTINTENT_H
#define MANGOS_MOVEMENTINTENT_H

#include "Platform/Define.h"
#include "ObjectGuid.h"
#include "movement/MoveSplineInitArgs.h"

/**
 * @brief The vocabulary of the INTENT model: what a movement generator wants, and
 *        the little it is told about the leg it is riding.
 *
 * A generator answers one question per tick — "given the world right now, where
 * should this unit be heading, and how should it be facing?" — as a pure MoveIntent
 * value. It never touches the spline, the path finder or a packet. The MotionDriver
 * reconciles that intent against the live leg and owns everything mechanical:
 * routing, the re-lay test, the leg's final facing, walk/run, and Launch.
 */
namespace Motion
{
    using Movement::PointsArray;
    using Movement::Vector3;

    /**
     * @brief The orientation a leg ends in.
     *
     * The client derives a unit's facing from the spline it interpolates: it faces
     * the direction of travel while moving, and applies whichever of these forms the
     * SMSG_MONSTER_MOVE carried once the spline ends. So "face the player" is not a
     * separate action — it is the final facing of a leg. The four forms map 1:1 onto
     * Movement::MonsterMoveType.
     */
    struct Facing
    {
        enum class Mode : uint8
        {
            None,   ///< Keep facing the travel direction (the default).
            Angle,  ///< Hold a fixed heading (e.g. a waypoint's DB orientation).
            Target, ///< Point at a unit.
            Spot    ///< Face a fixed point.
        };

        Mode       mode = Mode::None;
        float      angle = 0.0f; ///< Mode::Angle (radians).
        ObjectGuid target;       ///< Mode::Target.
        Vector3    spot;         ///< Mode::Spot.

        static Facing ToAngle(float o)
        {
            Facing f;
            f.mode = Mode::Angle;
            f.angle = o;
            return f;
        }

        static Facing ToTarget(ObjectGuid guid)
        {
            Facing f;
            f.mode = Mode::Target;
            f.target = guid;
            return f;
        }

        static Facing ToSpot(Vector3 const& p)
        {
            Facing f;
            f.mode = Mode::Spot;
            f.spot = p;
            return f;
        }
    };

    /**
     * @brief Per-leg modifiers, orthogonal to where the leg is going.
     */
    enum MoveFlags : uint32
    {
        MOVE_NONE         = 0x00,
        MOVE_WALK         = 0x01, ///< Walk pace, else run.
        MOVE_FLY          = 0x02, ///< Catmull-Rom spline + flying animation.
        MOVE_STRAIGHT     = 0x04, ///< Do not route: go straight there.
        MOVE_FORCE_DEST   = 0x08, ///< Arrive at the exact goal even if unroutable.

        /// Refuse the leg when the router could not find a real path, instead of
        /// taking its straight-line fallback. Chase, follow, flee and patrol legs all
        /// want this (a mob must not walk through a wall to reach you); a point,
        /// wander or home move deliberately does not.
        MOVE_REQUIRE_PATH = 0x10
    };

    /**
     * @brief What a generator wants this tick.
     *
     * Hold: stay put (optionally turning to `facing`).
     * Move: travel to `goal` — the driver routes it and lays the leg.
     * Done: this generator has finished and asks to be popped.
     */
    struct MoveIntent
    {
        enum class Act : uint8
        {
            Hold,
            Move,
            Done
        };

        Act     act = Act::Hold;
        Vector3 goal;                  ///< Move: destination.
        Facing  facing;                ///< Desired end (Move) / hold (Hold) facing.
        uint32  flags = MOVE_NONE;     ///< MoveFlags.

        /// Move: cap the routed path length in yards (0 = the router's default).
        float pathLengthLimit = 0.0f;

        /// Move: EXACT geometry for the leg, when the generator must dictate it
        /// rather than name a point and let the driver route there. Only the smoothed
        /// waypoint patrol needs it — it welds several nodes into one spline so the
        /// creature does not visibly stop and relaunch at every one.
        ///
        /// NON-OWNING. It must point at storage that outlives the tick (a generator
        /// member), and it must stay stable for as long as the leg is being walked:
        /// the driver may re-lay from it, and rebuilding it mid-leg from a different
        /// start would walk the unit back to the beginning of its own path.
        PointsArray const* path = nullptr;

        bool Has(MoveFlags f) const { return (flags & f) != 0; }

        static MoveIntent Hold(Facing f = {})
        {
            MoveIntent i;
            i.act = Act::Hold;
            i.facing = f;
            return i;
        }

        static MoveIntent Move(Vector3 const& to, uint32 moveFlags = MOVE_NONE, Facing f = {})
        {
            MoveIntent i;
            i.act = Act::Move;
            i.goal = to;
            i.flags = moveFlags;
            i.facing = f;
            return i;
        }

        static MoveIntent Done()
        {
            MoveIntent i;
            i.act = Act::Done;
            return i;
        }

        /// Chainable setters for the fields only one or two generators ever touch,
        /// so the common `Move(goal, flags, facing)` call stays short.
        MoveIntent& Along(PointsArray const& points)
        {
            path = &points;
            return *this;
        }

        MoveIntent& WithinLength(float yards)
        {
            pathLengthLimit = yards;
            return *this;
        }
    };

    /**
     * @brief The read-only view of leg progress the driver hands the generator, so a
     *        generator can react to its leg without ever seeing the spline.
     *
     * `traveling` is true while a leg is advancing; `arrived` fires on the single tick
     * it ends — a generator's cue to advance to the next waypoint, rest a beat, pick a
     * new hop, or pop a one-shot. `blocked` says the last Move could not be laid, so a
     * one-shot pops and a patrol skips the dead node instead of butting into a wall
     * forever.
     *
     * `arrived` and `blocked` are EDGE-triggered: reported on the tick after the event,
     * then cleared. A sticky `blocked` would starve every generator whose reaction is
     * "reset my retry timer and try again shortly" — it would reset the timer on every
     * tick and never fire it.
     */
    struct MoveStatus
    {
        bool    traveling = false; ///< A leg is running right now.
        bool    arrived = false;   ///< The leg finished (once).
        bool    blocked = false;   ///< The last Move could not be laid (once).
        int32   pathIndex = 0;     ///< How far along its points the spline is.
        Vector3 legGoal;           ///< The goal of the leg the driver actually LAID.
    };
}

#endif // MANGOS_MOVEMENTINTENT_H
