#pragma once
#include "../Value.h"

namespace ai
{
    class CurrentTargetValue : public UnitManualSetValue
    {
        public:
            CurrentTargetValue(PlayerbotAI* ai) : UnitManualSetValue(ai, NULL),
                m_losChecked(0), m_inLos(false) {}

            virtual Unit* Get();
            virtual void Set(Unit* unit);

        private:
            ObjectGuid selection;

            // Line of sight to the current target, remembered for a short window. Get() is
            // read several times per tick by triggers and actions alike and each read was
            // a fresh vmap raycast.
            ObjectGuid m_losTarget;
            uint32 m_losChecked;
            bool m_inLos;
    };
}
