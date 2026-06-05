#pragma once

#include "RangedCombatStrategy.h"

namespace ai
{
    class PullStrategy : public RangedCombatStrategy
    {
        public:
            PullStrategy(PlayerbotAI* ai, string action);

        public:
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
            virtual void InitMultipliers(std::list<Multiplier*> &multipliers);
            virtual string getName()
            {
                return "pull";
            }
            virtual NextAction** getDefaultActions();

        protected:
            string action;
    };

    class WaitForPullStrategy : public CombatStrategy
    {
        public:
            WaitForPullStrategy(PlayerbotAI* ai);
            virtual void InitMultipliers(std::list<Multiplier*> &multipliers);
            virtual string getName()
            {
                return "wait for pull";
            }
            virtual NextAction** getDefaultActions();
    };
}
