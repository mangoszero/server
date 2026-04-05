#pragma once

#include "GenericWarlockStrategy.h"

namespace ai
{
    class TankWarlockStrategy : public GenericWarlockStrategy
    {
    public:
        TankWarlockStrategy(PlayerbotAI* ai);
        virtual string getName() { return "tank"; }
        virtual int GetType() { return STRATEGY_TYPE_TANK | STRATEGY_TYPE_RANGED; }

    public:
        virtual void InitTriggers(std::list<TriggerNode*> &triggers);
        virtual NextAction** getDefaultActions();
    };
}
