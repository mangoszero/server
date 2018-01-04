#pragma once
#include "../Value.h"

namespace ai
{
    class CurrentTargetValue : public UnitManualSetValue
    {
    public:
        CurrentTargetValue(PlayerbotAI* ai) : UnitManualSetValue(ai, nullptr) {}

        virtual Unit* Get();
        virtual void Set(Unit* unit);

    private:
        ObjectGuid selection;
    };
}
