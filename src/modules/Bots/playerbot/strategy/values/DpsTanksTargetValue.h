#pragma once
#include "../Value.h"
#include "TargetValue.h"

namespace ai
{
    class DpsTanksTargetValue : public TargetValue
    {
        public:
            DpsTanksTargetValue(PlayerbotAI* ai) : TargetValue(ai) {}

        public:
            Unit* Calculate() override
            {
                Player* tank = ai->GetGroupTank(bot);
                if (!tank || !tank->IsAlive())
                {
                    return AI_VALUE(Unit*, "least hp target");
                }
                return tank->getVictim();
            }
    };
}
