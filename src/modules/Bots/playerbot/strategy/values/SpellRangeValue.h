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

            /// The one effective-range formula, static so callers that already hold
            /// a spell id (the qualified "reach spell" mover, UpdateRange) compute
            /// the same destination this value reports, without an AI context.
            static float EffectiveRange(uint32 spellId);
    };
}
