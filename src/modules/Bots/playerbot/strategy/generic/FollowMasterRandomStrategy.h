#include "NonCombatStrategy.h"
#pragma once

namespace ai
{
    class FollowMasterRandomStrategy : public NonCombatStrategy
    {
    public:
        FollowMasterRandomStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual string getName() { return "be near"; }
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);

    };
}