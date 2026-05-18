#include "NonCombatStrategy.h"
#pragma once

namespace ai
{
    class FollowLineStrategy : public NonCombatStrategy
    {
    public:
        FollowLineStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual string getName() { return "follow line"; }
        virtual NextAction** getDefaultActions();
    };
}