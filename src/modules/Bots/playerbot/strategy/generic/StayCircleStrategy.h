#include "NonCombatStrategy.h"
#pragma once

namespace ai
{

    class StayCircleStrategy : public NonCombatStrategy
    {
    public:
        StayCircleStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual string getName() { return "stay circle"; }
        virtual NextAction** getDefaultActions();
    };
}