#pragma once

#include <list>
#include "GenericPaladinStrategy.h"

namespace ai
{
    /**
     * @brief Holy paladin combat behaviour.
     *
     * Paladin was the one healing class with no heal strategy registered, so
     * AiFactory had nowhere to send a Holy talent build and sent it to "dps"
     * instead -- roughly a fifth of paladin bots by the shipped spec
     * probabilities, playing a damage rotation on a healing build.
     *
     * GenericPaladinStrategy already carries the healing triggers this needs, at
     * the module's standard priorities. What this adds is the part that makes a
     * dedicated healer: topping party members up before they are hurt, closing
     * to range on someone who needs a heal, and reaching for the paladin's own
     * survival cooldowns rather than trading blows when something is chewing on
     * it.
     */
    class HealPaladinStrategy : public GenericPaladinStrategy
    {
        public:
            HealPaladinStrategy(PlayerbotAI* ai) : GenericPaladinStrategy(ai) {}

        public:
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
            virtual NextAction** getDefaultActions();
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
