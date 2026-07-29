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

#ifndef MANGOS_CONFUSEDMOVEMENTGENERATOR_H
#define MANGOS_CONFUSEDMOVEMENTGENERATOR_H

#include "IntentMovementGenerator.h"

/**
 * @brief Disoriented staggering: lurch toward a random point near where the unit lost
 *        its wits, roughly once a second, with no goal at all.
 *
 * The lurch is deliberate. The stagger timer keeps running WHILE a leg is being walked,
 * so a fresh point is picked before the last one is reached and the leg is cut short.
 * That interrupted, never-quite-arriving motion is what reads on screen as confusion —
 * it is not a rest-then-hop rhythm, which is what wander is.
 */
class ConfusedMovementGenerator final : public IntentMovementGenerator
{
    public:
        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return CONFUSED_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        Motion::Vector3 m_anchor;    ///< Where the unit stood when it was confused.

        TimeTracker m_staggerTime{0}; ///< Time left before the next lurch.
        Motion::Vector3 m_lurch;      ///< Where the current lurch is heading.
        bool m_haveLurch = false;     ///< False before the first point has been picked.
};

#endif // MANGOS_CONFUSEDMOVEMENTGENERATOR_H
