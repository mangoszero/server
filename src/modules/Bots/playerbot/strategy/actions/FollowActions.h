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
        FollowMasterAction(PlayerbotAI* ai) : MovementAction(ai, "follow master") {}
        virtual bool Execute(Event event);

        virtual bool isUseful()
        {
            return AI_VALUE2(float, "distance", "master target") > sPlayerbotAIConfig.followDistance &&
            !AI_VALUE(bool, "can loot") || transportBoardingDelayTime > 0;
        }

    };

    class FollowMasterRandomAction : public MovementAction {
    public:
        FollowMasterRandomAction(PlayerbotAI* ai) : MovementAction(ai, "be near") {}
        virtual bool Execute(Event event);
    };
}
