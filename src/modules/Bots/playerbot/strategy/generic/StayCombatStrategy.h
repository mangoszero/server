#include "NonCombatStrategy.h"
#pragma once

namespace ai
{
    class StayCombatStrategy : public NonCombatStrategy
    {
    public:
        StayCombatStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual string getName() { return "stay combat"; }
        virtual NextAction** getDefaultActions();
    };
}