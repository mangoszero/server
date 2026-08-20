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

#ifndef MANGOS_POINTMOVEMENTGENERATOR_H
#define MANGOS_POINTMOVEMENTGENERATOR_H

#include "IntentMovementGenerator.h"

/**
 * @brief One-shot: go to a fixed point, then pop.
 *
 * "Go there; when the leg ends, I am done" is all an intent-model one-shot ever is.
 * The three variants below change only the flavour of the leg and what happens at the
 * end, which is why they override nothing but LegFlags and Finalize.
 */
class PointMovementGenerator : public IntentMovementGenerator
{
    public:
        PointMovementGenerator(uint32 id, float x, float y, float z, bool generatePath)
            : m_id(id), m_dest(x, y, z), m_generatePath(generatePath) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return POINT_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

        /// The flavour of the leg this generator lays. The one hook the variants need.
        virtual uint32 LegFlags() const
        {
            return m_generatePath ? Motion::MOVE_NONE : Motion::MOVE_STRAIGHT;
        }

        /// Tell the AI (and the summoner, if any) that the point was reached.
        void MovementInform(Unit& owner) const;

        uint32 m_id;             ///< Echoed to the AI on arrival.
        Motion::Vector3 m_dest;  ///< Where we are going.
        bool m_generatePath;     ///< Route around geometry, or go straight there.
};

/**
 * @brief A creature running to fetch help. It walks, so the players it is fetching
 *        have a chance to catch it, and it calls that help when it gets there.
 */
class AssistanceMovementGenerator final : public PointMovementGenerator
{
    public:
        AssistanceMovementGenerator(float x, float y, float z)
            : PointMovementGenerator(0, x, y, z, true) {}

        MovementGeneratorType GetMovementGeneratorType() const override { return ASSISTANCE_MOTION_TYPE; }

        void Finalize(Unit& owner) override;

    protected:
        uint32 LegFlags() const override { return Motion::MOVE_WALK; }
};

/**
 * @brief A point move that refuses to cheat: if the router cannot actually route it,
 *        no leg is laid and the mover stays put.
 *
 * A plain point move takes PathFinder's straight-line fallback silently. That is fine for
 * a short hop, and wrong for anything long: an unloaded destination tile comes back as
 * PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH, and the "path" is then a terrain-clamped line
 * drawn through cliffs, walls and buildings for the whole distance. MOVE_REQUIRE_PATH does
 * not catch that -- it tests NOPATH only -- so this asks for MOVE_REQUIRE_ROUTE instead.
 *
 * The caller must have somewhere else to go when the leg is refused, because it will be.
 */
class RoutedPointMovementGenerator final : public PointMovementGenerator
{
    public:
        RoutedPointMovementGenerator(uint32 id, float x, float y, float z)
            : PointMovementGenerator(id, x, y, z, true), m_arrived(false) {}

        void Initialize(Unit& owner) override
        {
            m_arrived = false;
            PointMovementGenerator::Initialize(owner);
        }

        /// Reports arrival only when the mover actually arrived.
        ///
        /// The base tests movespline->Finalized(), which is also true when no leg was ever
        /// laid -- Initialize stops the mover, so a refused route leaves a finalized spline
        /// that never went anywhere, and the point would be reported as reached. A Player
        /// caller never notices (MovementInform ignores non-creatures), but a creature one
        /// would advance its AI or script on an arrival that did not happen.
        void Finalize(Unit& owner) override;

        /// Public, matching the base declaration: callers hold a MovementGenerator* and ask
        /// through it, which is the whole point of the hook.
        bool IsRoutedLeg() const override { return true; }

    protected:
        uint32 LegFlags() const override { return Motion::MOVE_REQUIRE_ROUTE; }

        /// Latches arrival POSITIVELY, the same way HomeMovementGenerator does, rather than
        /// latching refusal. Refusal is only delivered on the tick after the driver rejects
        /// the leg, so an external Clear() or MovementExpired() in between calls Finalize()
        /// directly and a "was it refused" flag would still read false -- reporting an
        /// arrival that never happened, which is the bug this class exists to avoid.
        ///
        /// status.arrived alone is not enough to prove it, though: the driver derives it from
        /// "was travelling, spline is now finalized", and StopMoving() finalizes the spline
        /// wherever the mover is standing -- so a root or a stun mid-route produces it too.
        /// Hence the proximity test in the definition. The tolerance is deliberately loose,
        /// because a spline ends near the goal rather than exactly on it and a missed inform
        /// would stall a creature's AI; it only has to be tight enough to reject a mover
        /// frozen partway.
        ///
        /// Defined out of line because the body reads Unit's members and this header sees
        /// only a forward declaration of Unit -- inline here, it compiles under MSVC solely
        /// because every Windows translation unit happened to include Unit.h first, and
        /// fails under clang and gcc where one does not.
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        bool m_arrived; ///< A leg actually completed; cleared on Initialize/Reset.
};

/**
 * @brief A straight line through the air, with the flying animation along it.
 */
class FlyOrLandMovementGenerator final : public PointMovementGenerator
{
    public:
        /// `liftOff` is not stored: the leg is a straight line through the air either
        /// way, and whether it is a take-off or a landing is already implied by the
        /// height of the destination.
        FlyOrLandMovementGenerator(uint32 id, float x, float y, float z, bool /*liftOff*/)
            : PointMovementGenerator(id, x, y, z, false) {}

    protected:
        uint32 LegFlags() const override
        {
            return Motion::MOVE_FLY | Motion::MOVE_STRAIGHT;
        }
};

/**
 * @brief Guards a spline that something ELSE launched — a knockback, a jump, a
 *        scripted effect.
 *
 * It has no destination of its own to want, so its intent is the minimal one: hold
 * while that spline is still playing out, and be done the moment it is not. That is
 * what stops the generator underneath from interrupting the effect mid-flight.
 */
class EffectMovementGenerator final : public IntentMovementGenerator
{
    public:
        explicit EffectMovementGenerator(uint32 id) : m_id(id) {}

        void Initialize(Unit&) override {}
        void Interrupt(Unit&) override {}
        void Reset(Unit&) override {}
        void Finalize(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return EFFECT_MOTION_TYPE; }

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        uint32 m_id; ///< Echoed to the AI when the effect's spline ends.
};

#endif // MANGOS_POINTMOVEMENTGENERATOR_H
