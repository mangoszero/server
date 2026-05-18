#pragma once
#include "../Value.h"

namespace ai
{
    class PetTargetValue : public UnitCalculatedValue
    {
    public:
        PetTargetValue(PlayerbotAI* ai) : UnitCalculatedValue(ai) {}

        virtual Unit* Calculate() { return ai->GetBot()->GetPet(); }
    };
}
