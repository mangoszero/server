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

#ifndef MANGOS_HOMEMOVEMENTGENERATOR_H
#define MANGOS_HOMEMOVEMENTGENERATOR_H

#include "IntentMovementGenerator.h"

/**
 * @brief Evade: run back to where the creature belongs, then hand back to its default
 *        movement.
 *
 * The home position is captured in Initialize and never re-read, and that is
 * load-bearing: MotionMaster::Mutate calls Initialize BEFORE pushing this generator, so
 * at that moment the stack top is still the generator being evacuated — the only one
 * that knows where "home" is (a patroller resumes at the point combat pulled it off its
 * path, not at its spawn). By the time the first leg is laid we are on top and that
 * answer is gone.
 *
 * A NOTE ON UNIT_STAT_ROAMING_MOVE. Every other generator that lays a leg holds a
 * ...._MOVE state for as long as it is moving; this one held none, and that single
 * omission was worth more than it looks. Unit::IsStopped is exactly
 * !hasUnitState(UNIT_STAT_MOVING), so a creature running home reported itself stopped
 * for the whole return. Three things followed. StopMoving() — including the root and
 * stun paths — took its early return and left the home spline running. The update
 * builder's "not moving but spline enabled" guard fired on healthy returns, 151 times in
 * one evening, and stripped MOVEFLAG_SPLINE_ENABLED off a live spline as it went, so
 * observers arriving mid-return were told nothing was happening. And nothing else in the
 * server could tell an evading creature from a standing one.
 *
 * ARRIVAL IS A PLACE, NOT AN EVENT. The driver reports `arrived` for any leg that stopped
 * running and cannot tell why it stopped, so this generator asks where the creature is
 * rather than trusting that report. Nothing else is sound: a forced stop finalizes the
 * spline mid-journey, and SetFacingTo and MonsterMoveWithSpeed launch replacement splines
 * straight past the driver, so a scripted turn-to during an evade ends a leg that was
 * never the home leg at all. Every one of those would otherwise fire JustReachedHome
 * wherever the creature was standing, and hand a patroller back to its route from the
 * wrong point.
 */
class HomeMovementGenerator final : public IntentMovementGenerator
{
    public:
        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return HOME_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        /// Whether the creature is actually standing at its home position.
        bool AtHome(Unit const& owner) const;

        /// Whether a leg that ended away from home is still worth resuming. Accumulates
        /// time spent getting no nearer, so that evade always terminates.
        bool Resumable(Unit const& owner, uint32 diff);

        Motion::Vector3 m_home;  ///< Where home is, captured before we were pushed.
        float m_facing = 0.0f;   ///< The orientation to hold once there.
        bool m_haveHome = false; ///< False when the creature could not be sent home.
        bool m_arrived = false;  ///< Whether Finalize should fire JustReachedHome.
        float m_closest = 0.0f;  ///< Nearest we have got to home, for progress detection.
        uint32 m_stalled = 0;    ///< Time spent resuming without getting any nearer.
};

#endif // MANGOS_HOMEMOVEMENTGENERATOR_H
