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

#ifndef MANGOS_MOTIONDRIVER_H
#define MANGOS_MOTIONDRIVER_H

#include "Platform/Define.h"
#include "MotionFrame.h"
#include "MovementIntent.h"

#include <memory>

class Unit;

/**
 * @brief The ENGINE of the intent model: everything mechanical about a movement leg,
 *        in one place.
 *
 * A generator says WHAT it wants (a MoveIntent). This says HOW that happens:
 *
 *   * reads the live spline and reports leg progress as a MoveStatus, so a generator
 *     can react to arriving without ever seeing the spline;
 *   * decides whether a fresh leg is actually needed (none running, the last one
 *     ended, the goal drifted past the intent's tolerance, the speed changed) — the
 *     test every generator used to carry its own subtly different copy of;
 *   * routes the leg through the mover's IMotionFrame, so the same code walks world
 *     terrain or (later) a transport deck;
 *   * resolves the leg's final facing, picks walk/run, and calls Launch exactly once;
 *   * latches "that Move could not be laid" and hands it back as MoveStatus::blocked,
 *     so a one-shot pops and a patrol skips a dead node rather than retrying into a
 *     wall forever.
 *
 * One driver belongs to one generator (it replaced the PathFinder the targeted
 * generators used to own), and its per-leg state is cleared whenever that generator is
 * interrupted or reset.
 */
class MotionDriver
{
    public:
        /**
         * @brief Read the live leg and hand back what the generator is allowed to know
         *        about it. Call once per tick, before Apply.
         */
        Motion::MoveStatus BeginTick(Unit& owner);

        /**
         * @brief Reconcile the generator's intent against the live leg.
         * @return False when the generator asked to be popped (Act::Done).
         */
        bool Apply(Unit& owner, Motion::MoveIntent const& intent);

        /// The unit's speed changed: any leg in flight is paced wrong, so re-lay it.
        void OnSpeedChanged() { m_speedChanged = true; }

        /// Forget the leg we laid. Called when the generator is interrupted or reset,
        /// so the next tick lays a fresh one instead of believing a stale destination
        /// is still being walked to.
        void ResetLeg();

        /// False once a route only got partway to its goal (the IsReachable contract).
        bool Reachable() const { return !m_query || m_query->Reachable(); }

    private:
        /// Lay a fresh leg if one is needed; false when nothing was launched.
        bool ReconcileMove(Unit& owner, Motion::MoveIntent const& intent);

        /// Hold: never cuts a running leg short (letting it finish is what stops an
        /// arriving chase from stuttering) — it only maintains the requested facing
        /// once there is nothing left to travel.
        void ReconcileHold(Unit& owner, Motion::MoveIntent const& intent);

        /// Route and launch. False when the mover is blocked in place.
        bool LayLeg(Unit& owner, Motion::MoveIntent const& intent);

        /// The router for the mover's CURRENT frame, rebuilt when the frame under it
        /// changes (boarding a transport) — a leg never spans two frames.
        Motion::IPathQuery* Query(Unit const& owner);

        std::unique_ptr<Motion::IPathQuery> m_query;
        Motion::FrameKind m_queryFrame = Motion::FrameKind::World;

        /// The goal of the leg we last laid, so the drift test can tell when a tracked
        /// destination has moved far enough to be worth re-routing.
        Motion::Vector3 m_legGoal;
        bool m_haveLeg = false;

        bool m_blocked = false;      ///< The last Move could not be laid.
        bool m_speedChanged = false; ///< A speed change invalidated the running leg.
        bool m_wasTraveling = false; ///< Previous tick had a live leg (arrival edge).
};

#endif // MANGOS_MOTIONDRIVER_H
