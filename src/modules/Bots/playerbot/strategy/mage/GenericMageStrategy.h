#pragma once

#include <list>
#include "../Strategy.h"
#include "../generic/RangedCombatStrategy.h"

namespace ai
{
    class GenericMageStrategy : public RangedCombatStrategy
    {
        public:
            GenericMageStrategy(PlayerbotAI* ai);
            virtual string getName()
            {
                return "mage";
            }

        public:
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
    };
}
