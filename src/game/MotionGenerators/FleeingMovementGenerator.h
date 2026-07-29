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

#ifndef MANGOS_FLEEINGMOVEMENTGENERATOR_H
#define MANGOS_FLEEINGMOVEMENTGENERATOR_H

#include "IntentMovementGenerator.h"
#include "ObjectGuid.h"

#include <optional>

/**
 * @brief Panic: bolt away from a fear source in short bursts, pausing a beat between them.
 *
 * Unlike wander, a flee leg REFUSES the router's straight-line fallback
 * (MOVE_REQUIRE_PATH): a panicking unit that cannot actually get somewhere must pick a
 * different somewhere, not bolt through a wall.
 */
class FleeingMovementGenerator : public IntentMovementGenerator
{
    public:
        explicit FleeingMovementGenerator(ObjectGuid fright) : m_frightGuid(fright) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return FLEEING_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

        /// Somewhere to bolt to, away from the fear source. Nothing when the bearing it
        /// picked has no ground under it.
        std::optional<Motion::Vector3> PickFleePoint(Unit& owner) const;

    private:
        ObjectGuid m_frightGuid;      ///< What the unit is running from.

        TimeTracker m_restTime{0};    ///< Time left standing before the next bolt.
        Motion::Vector3 m_fleePoint;  ///< Where the current bolt is heading.
        bool m_haveFleePoint = false; ///< False before the first point has been picked.
};

/**
 * @brief Fleeing that gives up after a fixed time and re-engages, rather than running
 *        until the aura that caused it is removed.
 */
class TimedFleeingMovementGenerator final : public FleeingMovementGenerator
{
    public:
        TimedFleeingMovementGenerator(ObjectGuid fright, uint32 time)
            : FleeingMovementGenerator(fright), m_totalFleeTime(time) {}

        MovementGeneratorType GetMovementGeneratorType() const override { return TIMED_FLEEING_MOTION_TYPE; }

        void Finalize(Unit& owner) override;

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        TimeTracker m_totalFleeTime; ///< How long the panic lasts.
};

#endif // MANGOS_FLEEINGMOVEMENTGENERATOR_H
