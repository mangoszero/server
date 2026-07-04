#pragma once

#include "../Trigger.h"

namespace ai
{
    class WeaponsAreSavedTrigger : public Trigger
    {
        public:
            WeaponsAreSavedTrigger(PlayerbotAI* ai) : Trigger(ai, "weapons are saved", 1) {}
            virtual bool IsActive()
            {
                ObjectGuid savedMainhand = AI_VALUE(ObjectGuid, "saved mainhand weapon");
                bool masterFishing = AI_VALUE(bool, "master is fishing");

                if (savedMainhand == ObjectGuid())
                {
                    return false;
                }
                if (masterFishing)
                {
                    return false;
                }

                return true;
            }
    };
}
