#pragma once
#include "../Value.h"

namespace ai
{
    class LastTargetPositionValue : public ManualSetValue<float>
    {
        public:
            LastTargetPositionValue(PlayerbotAI* ai) : ManualSetValue<float>(ai, 0.0f, "last target position") {}
    };
}
