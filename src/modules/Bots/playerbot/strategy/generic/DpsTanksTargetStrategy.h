#pragma once
#include "../generic/NonCombatStrategy.h"

namespace ai
{
    class DpsTanksTargetStrategy : public NonCombatStrategy
    {
        public:
            DpsTanksTargetStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai) {}
            virtual string getName() { return "dps tanks"; }
            virtual int GetType() { return STRATEGY_TYPE_DPS; }
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
    };
}
