#include "../generic/NonCombatStrategy.h"
#pragma once

namespace ai
{
    class FollowMasterStrategy : public NonCombatStrategy
    {
    public:
        FollowMasterStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
        virtual string getName() { return "follow master"; }
        virtual NextAction** getDefaultActions();
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);

    };

}
