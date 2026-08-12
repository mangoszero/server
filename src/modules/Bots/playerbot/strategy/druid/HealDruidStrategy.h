#pragma once

#include <list>
#include "GenericDruidStrategy.h"

namespace ai
{
    class HealDruidStrategy : public GenericDruidStrategy
    {
        public:
            HealDruidStrategy(PlayerbotAI* ai);

        public:
            // Without this the strategy inherited Strategy::getDefaultActions()'s NULL, so a
            // healing druid had NO unconditional action -- every trigger it owns is conditional.
            virtual NextAction** getDefaultActions();
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
            virtual string getName()
            {
                return "heal";
            }

            virtual int GetType()
            {
                return STRATEGY_TYPE_HEAL;
            }
    };
}
