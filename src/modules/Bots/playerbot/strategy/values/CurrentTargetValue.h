#pragma once
#include "../Value.h"

namespace ai
{
    class CurrentTargetValue : public UnitManualSetValue
    {
        public:
            CurrentTargetValue(PlayerbotAI* ai) : UnitManualSetValue(ai, NULL),
                m_visibilityChecked(0), m_targetVisible(false) {}

            virtual Unit* Get();
            virtual void Set(Unit* unit);

        private:
            ObjectGuid selection;

            // Visibility of the current target, remembered for a short window. Get() is
            // read several times per tick by triggers and actions alike, while detection
            // and line of sight only need to be re-decided periodically.
            ObjectGuid m_visibilityTarget;
            uint32 m_visibilityChecked;
            bool m_targetVisible;
    };
}
