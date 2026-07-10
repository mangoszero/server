#pragma once
#include "../Value.h"

namespace ai
{
    class CcReachTargetValue : public UnitManualSetValue
    {
        public:
            CcReachTargetValue(PlayerbotAI* ai) : UnitManualSetValue(ai, NULL) {}

            virtual Unit* Get();
            virtual void Set(Unit* unit);

        private:
            ObjectGuid selection;
    };
}
