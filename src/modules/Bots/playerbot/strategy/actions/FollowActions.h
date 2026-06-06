#pragma once

#include "../Action.h"
#include "MovementActions.h"

namespace ai
{
    class FollowAction : public MovementAction {
        public:
            FollowAction(PlayerbotAI* ai, string name) : MovementAction(ai, name) {}
    };

    class FollowLineAction : public FollowAction {
        public:
            FollowLineAction(PlayerbotAI* ai) : FollowAction(ai, "follow line") {}
            virtual bool Execute(Event event);
    };

    class FollowMasterAction : public MovementAction {
        public:
            FollowMasterAction(PlayerbotAI* ai, string name="follow master") : MovementAction(ai, name) {}
            virtual bool Execute(Event event);
            virtual bool isUseful();
    };

    class ReachMasterAction : public FollowMasterAction {
        public:
            ReachMasterAction(PlayerbotAI* ai) : FollowMasterAction(ai, "reach master") {}
            virtual bool Execute(Event event);
            virtual bool isUseful();
    };

    class FollowMasterRandomAction : public MovementAction {
        public:
            FollowMasterRandomAction(PlayerbotAI* ai) : MovementAction(ai, "be near") {}
            virtual bool Execute(Event event);
    };
}
