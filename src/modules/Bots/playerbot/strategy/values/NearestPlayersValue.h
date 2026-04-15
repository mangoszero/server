#pragma once
#include "../Value.h"
#include "NearestUnitsValue.h"
#include "../../PlayerbotAIConfig.h"

namespace ai
{
    class NearestPlayersValue : public NearestUnitsValue
    {
        public:
            NearestPlayersValue(PlayerbotAI* ai, float range = sPlayerbotAIConfig.sightDistance) :
            NearestUnitsValue(ai) {}

        protected:
            void FindUnits(list<Unit*> &targets);
            bool AcceptUnit(Unit* unit);
    };
}
