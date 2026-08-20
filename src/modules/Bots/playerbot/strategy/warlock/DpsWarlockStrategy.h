#pragma once

#include <list>
#include "GenericWarlockStrategy.h"
#include "../generic/CombatStrategy.h"

namespace ai
{
    class DpsWarlockStrategy : public GenericWarlockStrategy
    {
        public:
            DpsWarlockStrategy(PlayerbotAI* ai);
            virtual string getName()
            {
                return "dps";
            }

        public:
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
            virtual NextAction** getDefaultActions();

            // Inherited RangedCombatStrategy's COMBAT|RANGED and never declared DPS, unlike
            // its opposite numbers CasterDruidStrategy and CasterShamanStrategy. Nothing
            // depended on it yet, but ContainsStrategy(STRATEGY_TYPE_DPS) is exactly the
            // sort of question the strategy selector asks, and a damage strategy that does
            // not answer to it is a trap left lying around.
            virtual int GetType()
            {
                return STRATEGY_TYPE_COMBAT | STRATEGY_TYPE_DPS | STRATEGY_TYPE_RANGED;
            }
    };

    class DpsAoeWarlockStrategy : public CombatStrategy
    {
        public:
            DpsAoeWarlockStrategy(PlayerbotAI* ai);

        public:
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
            virtual string getName()
            {
                return "aoe";
            }
    };

    class DpsWarlockDebuffStrategy : public CombatStrategy
    {
        public:
            DpsWarlockDebuffStrategy(PlayerbotAI* ai) : CombatStrategy(ai) {}

        public:
            virtual void InitTriggers(std::list<TriggerNode*> &triggers);
            virtual string getName()
            {
                return "dps debuff";
            }
    };
}
