#pragma once

#include "../Trigger.h"
#include "../../PlayerbotAIConfig.h"

namespace ai
{
    class MasterIsFishingTrigger : public Trigger
    {
        public:
            MasterIsFishingTrigger(PlayerbotAI* ai) : Trigger(ai, "master is fishing", 1)
            {
                m_lastFished = 0;
            }
            virtual bool IsActive()
            {
                GameObject* bobber = AI_VALUE(GameObject*, "master bobber");
                float distance = AI_VALUE2(float, "distance", "master target");

                bool active = false;
                if (bobber && distance <= sPlayerbotAIConfig.reactDistance)
                {
                    m_lastFished = time(nullptr);
                    active = true;
                }
                else if (m_lastFished && time(nullptr) - m_lastFished <= 5)
                {
                    active = true;
                }

                context->GetValue<bool>("master is fishing")->Set(active);
                return active;
            }

        private:
            time_t m_lastFished;
    };
}
