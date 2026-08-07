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
            ReviveFromCorpseAction(PlayerbotAI* ai) : MovementAction(ai, "revive") {}

        public:
            virtual bool Execute(Event event);
    };

    class SpiritHealerAction : public Action {
        public:
            SpiritHealerAction(PlayerbotAI* ai) : Action(ai, "spirit healer") {}

        public:
            virtual bool Execute(Event event);
    };
}
