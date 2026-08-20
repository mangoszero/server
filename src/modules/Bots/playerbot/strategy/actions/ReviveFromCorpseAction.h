#pragma once

#include "MovementActions.h"

namespace ai
{
    // A MovementAction rather than a plain Action so a ghost can walk back to its body.
    // Without that the whole death path depended on the bot happening to die within
    // reclaim range of its own corpse or within sight of a spirit healer, and a bot that
    // died out in the world satisfied neither.
    class ReviveFromCorpseAction : public MovementAction {
        public:
            ReviveFromCorpseAction(PlayerbotAI* ai) : MovementAction(ai, "revive"),
                m_corpseRunStarted(0), m_corpseRunAllowance(0), m_corpseRunGaveUp(false),
                m_corpseLegLength(0.0f), m_corpseLegAsked(false), m_corpseLegRefusals(0),
                m_corpseLegAskedX(0.0f), m_corpseLegAskedY(0.0f), m_corpseLegAskedZ(0.0f) {}

        public:
            virtual bool Execute(Event event);

        private:
            // A deadline per corpse, not a progress counter.
            //
            // The run is no longer bounded by ReactDistance, so a route the pathfinder cannot
            // lay must not be retried forever -- MoveTo returns true as soon as it installs the
            // point generator, and routing only fails later, so nothing else catches it.
            //
            // Sampling straight-line distance was tried and rejected: an oscillation of
            // 100 -> 98 -> 100 -> 98 resets a progress counter every other sample and runs
            // forever, while a legitimate detour around a ravine or switchback moves AWAY from
            // the corpse and is abandoned. Neither is a real signal. Elapsed time is, and it is
            // keyed on the corpse guid so every fresh death gets a fresh deadline rather than
            // inheriting the last one -- the earlier counter never reset after giving up, which
            // permanently disabled corpse running for that bot.
            //
            // The allowance is computed per corpse from the actual route length at the bot's
            // live ghost speed, NOT a constant. A flat 90 seconds was tried and rejected on
            // measured data: Land's End Beach to the Gadgetzan graveyard is 3188 yards, which is
            // 364 seconds of running at ghost speed before any detour, so the constant would
            // have abandoned a perfectly ordinary Tanaris corpse run four times over. Run speed
            // is also configurable, so no universal constant is safe.
            ObjectGuid m_corpseRunGuid;
            time_t m_corpseRunStarted;
            uint32 m_corpseRunAllowance;

            // Giving up must MOVE the bot, not just decline. Returning false where the bot stands
            // leaves a ghost wedged in open country: the spirit healer fallback only fires when a
            // healer is in the bot's locally computed "nearest npcs", and out in the world it is
            // not -- so the corpse whose run just failed could never be retried either. One repop
            // per corpse puts the ghost at a graveyard, where a healer actually stands. The flag
            // is what keeps that to exactly one: without it an expired deadline would repop on
            // every tick, which is the teleport churn this branch exists to stop.
            bool m_corpseRunGaveUp;

            // How long the next leg may be, and where the bot stood when the last one was
            // asked for.
            //
            // MOVE_REQUIRE_ROUTE refusing a leg is silent from here. MoveTo returns true as
            // soon as the generator is installed, routing fails afterwards, and the player
            // path of MovementInform returns immediately for a TYPEID_PLAYER owner -- so a bot
            // is never told that its leg was refused, or that one arrived. The only signal
            // available is circumstantial and it is decisive: a refused leg lays no spline at
            // all, so the bot is still standing exactly where it asked from. A leg that
            // completed moved it most of the leg's length. Comparing the two tells refusal
            // from progress without any core change.
            //
            // Measured live, before this: 15 of 168 classified corpse runs never moved a yard.
            // Each re-issued the identical routed point every four seconds until the allowance
            // expired -- two minutes minimum -- and then repopped at a graveyard, which is the
            // teleport-revive churn this branch exists to stop, reached the slow way.
            float m_corpseLegLength;
            bool m_corpseLegAsked;
            // Consecutive refusals for this corpse. The give-up is terminal, and a near corpse
            // reaches the ladder's floor in a single step, so one is not enough evidence.
            uint32 m_corpseLegRefusals;
            float m_corpseLegAskedX;
            float m_corpseLegAskedY;
            float m_corpseLegAskedZ;
    };

    // Deliberately a plain Action, NOT a MovementAction. Walking to the healer was tried and
    // reverted: MovementAction::IsMovingAllowed rejects `IsDead() && !GetCorpse()`, so a ghost
    // with no corpse -- exactly the case this action exists to serve, seen after a restart
    // leaves the corpse table empty -- could never move, never reach the healer, and never get
    // up again. See the note on Execute for what still needs solving here.
    class SpiritHealerAction : public Action {
        public:
            SpiritHealerAction(PlayerbotAI* ai) : Action(ai, "spirit healer") {}

        public:
            virtual bool Execute(Event event);
    };
}
