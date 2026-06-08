#pragma once
#include "../Value.h"
#include "TargetValue.h"

namespace ai
{

    class SpellRangeValue : public CalculatedValue<float>, public Qualified
    {
        public:
            SpellRangeValue(PlayerbotAI* ai);

        public:
            virtual float Calculate();
    };
}
