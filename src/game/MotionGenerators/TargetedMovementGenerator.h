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

#ifndef MANGOS_TARGETEDMOVEMENTGENERATOR_H
#define MANGOS_TARGETEDMOVEMENTGENERATOR_H

#include "FollowerReference.h"
#include "IntentMovementGenerator.h"
#include "Unit.h"

/**
 * @brief Holds the link to the tracked target.
 *
 * A separate class only because FollowerReference is declared as
 * Reference<Unit, TargetedMovementGeneratorBase> — the reference machinery names this
 * type, so it has to keep existing under this name.
 */
class TargetedMovementGeneratorBase
{
    public:
        explicit TargetedMovementGeneratorBase(Unit& target) { i_target.link(&target, this); }

        void stopFollowing() {}

    protected:
        FollowerReference i_target;
};

/**
 * @brief The two movement kinds that track ANOTHER object rather than a fixed point.
 *
 * Both answer the same question each tick — "where do I want to stand relative to that
 * thing, and am I there yet?" — and differ only in how the standing spot is derived and
 * what happens on arrival. Everything that used to make these the most duplicated code
 * in the movement tree (owning a PathFinder, deciding when a moving goal is stale enough
 * to re-route, building a spline, sending the packet) now lives once, in the MotionDriver.
 *
 * What is left here is the genuinely per-kind policy, expressed as the handful of hooks
 * below: how far "close enough" is, how often to look, and what to do on arrival.
 */
class TargetedMovementGenerator : public IntentMovementGenerator,
                                  public TargetedMovementGeneratorBase
{
    public:
        Unit* GetTarget() const { return i_target.getTarget(); }

    protected:
        TargetedMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGeneratorBase(target), m_offset(offset), m_angle(angle) {}

        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) final;

        /// Set/clear the unit state that says "I am moving because of THIS generator".
        virtual void AddMoveState(Unit& owner) const = 0;
        virtual void ClearMoveState(Unit& owner) const = 0;

        /// How far from the target the unit wants to stand (forRangeCheck = false), and
        /// how far the target may drift before that spot is stale (true). The gap
        /// between the two is the hysteresis that stops a victim shuffling a yard inside
        /// melee from provoking a string of sub-yard catch-up legs.
        virtual float TargetDistance(Unit& owner, bool forRangeCheck) const = 0;

        /// The target is gone in a way only this kind can detect (a creature that killed
        /// its own pet is no longer chasing anything).
        virtual bool LostTarget(Unit& /*owner*/) const { return false; }

        /// The unit is now as close as it asked to be. Fires once per approach.
        virtual void ReachTarget(Unit& /*owner*/) {}

        virtual bool EnableWalking(Unit& /*owner*/) const { return false; }

        /// How often the standing spot is re-derived. Deriving it is the expensive half
        /// (it snaps to the ground), so it is throttled — and a follower looks twice as
        /// often as a chaser, because a pet lagging behind its master reads far worse
        /// than a mob lagging a step behind its victim.
        virtual uint32 RecheckIntervalMs() const { return 100; }

        /// Reset the tracking state. Call from Initialize/Interrupt.
        void ResetTracking();

        float m_offset; ///< Distance to keep from the target.
        float m_angle;  ///< Bearing to keep, relative to the target's facing.

    private:
        /// Where the unit wants to stand, relative to the target.
        Motion::Vector3 ComputeDestination(Unit& owner) const;

        /// Has the target moved far enough from `spot` that it is stale?
        bool RequiresNewPosition(Unit& owner, Motion::Vector3 const& spot) const;

        TimeTracker m_recheckTime{0};
        Motion::Vector3 m_dest;      ///< The standing spot we are heading for.
        bool m_haveDest = false;     ///< False before the first spot has been derived.
        bool m_targetReached = false;///< ReachTarget already fired for this approach.
};

/**
 * @brief Combat pursuit: run the victim down and stay in its face.
 */
class ChaseMovementGenerator final : public TargetedMovementGenerator
{
    public:
        ChaseMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGenerator(target, offset, angle) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return CHASE_MOTION_TYPE; }

    protected:
        void AddMoveState(Unit& owner) const override { owner.addUnitState(UNIT_STAT_CHASE_MOVE); }
        void ClearMoveState(Unit& owner) const override { owner.clearUnitState(UNIT_STAT_CHASE_MOVE); }

        float TargetDistance(Unit& owner, bool forRangeCheck) const override;
        bool LostTarget(Unit& owner) const override;
        void ReachTarget(Unit& owner) override;
};

/**
 * @brief Keep station on a target: a pet at its master's heel, an escorted NPC.
 */
class FollowMovementGenerator final : public TargetedMovementGenerator
{
    public:
        FollowMovementGenerator(Unit& target, float offset, float angle)
            : TargetedMovementGenerator(target, offset, angle) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return FOLLOW_MOTION_TYPE; }

    protected:
        void AddMoveState(Unit& owner) const override { owner.addUnitState(UNIT_STAT_FOLLOW_MOVE); }
        void ClearMoveState(Unit& owner) const override { owner.clearUnitState(UNIT_STAT_FOLLOW_MOVE); }

        float TargetDistance(Unit& owner, bool forRangeCheck) const override;
        bool EnableWalking(Unit& owner) const override;
        uint32 RecheckIntervalMs() const override { return 50; }

    private:
        /// A pet mirrors its master's speed, so it can actually keep up.
        void SyncSpeedWithMaster(Unit& owner) const;
};

#endif // MANGOS_TARGETEDMOVEMENTGENERATOR_H
