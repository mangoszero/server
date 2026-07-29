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

#ifndef MANGOS_INTENTMOVEMENTGENERATOR_H
#define MANGOS_INTENTMOVEMENTGENERATOR_H

#include "MotionDriver.h"
#include "MovementGenerator.h"
#include "MovementIntent.h"

/**
 * @brief The seam between the two halves of the movement system.
 *
 * It implements the MovementGenerator::Update contract in terms of the single new
 * question a derived generator must answer:
 *
 *     MoveIntent Intent(owner, status, diff)
 *
 * Each tick it reads the live leg, asks the generator what it wants, and hands that to
 * the MotionDriver — which does all the routing, re-laying, facing and launching. A
 * generator returning Act::Done makes Update return false, which is how the
 * MotionMaster has always been told to pop a finished generator.
 *
 * A derived generator still owns POLICY: Initialize / Finalize / Interrupt / Reset —
 * unit states, AI informs, walk flags, its own timers. Those are decisions about the
 * creature, not about the leg.
 *
 * It no longer touches MECHANISM: PathFinder, MoveSplineInit, movespline, and every
 * hand-rolled copy of "has my goal drifted far enough to re-path".
 */
class IntentMovementGenerator : public MovementGenerator
{
    public:
        bool Update(Unit& owner, uint32 diff) final
        {
            const Motion::MoveStatus status = m_driver.BeginTick(owner);
            return m_driver.Apply(owner, Intent(owner, status, diff));
        }

        void unitSpeedChanged() final { m_driver.OnSpeedChanged(); }

        bool IsReachable() const final { return m_driver.Reachable(); }

    protected:
        /**
         * @brief The per-tick decision — the ONE function a movement kind is.
         * @param owner The moving unit.
         * @param status What the driver knows about the live leg.
         * @param diff The elapsed update time in milliseconds.
         */
        virtual Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                          uint32 diff) = 0;

        /// Forget the leg we laid — on Interrupt/Reset, so the next tick lays a fresh
        /// one instead of believing a stale destination is still valid.
        void ResetLeg() { m_driver.ResetLeg(); }

    private:
        MotionDriver m_driver;
};

#endif // MANGOS_INTENTMOVEMENTGENERATOR_H
