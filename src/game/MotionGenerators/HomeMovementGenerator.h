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
 * Holding the state is also what lets Intent tell a stop from an arrival. The driver
 * reports both the same way; UNIT_STAT_ROAMING_MOVE is the difference, because a leg that
 * ran out leaves it standing and Unit::StopMoving does not.
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
        Motion::Vector3 m_home;  ///< Where home is, captured before we were pushed.
        float m_facing = 0.0f;   ///< The orientation to hold once there.
        bool m_haveHome = false; ///< False when the creature could not be sent home.
        bool m_arrived = false;  ///< Whether Finalize should fire JustReachedHome.
};

#endif // MANGOS_HOMEMOVEMENTGENERATOR_H
