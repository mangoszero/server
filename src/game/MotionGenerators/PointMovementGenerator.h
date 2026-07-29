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
