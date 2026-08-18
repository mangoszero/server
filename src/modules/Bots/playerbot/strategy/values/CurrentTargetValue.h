#pragma once
#include "../Value.h"

namespace ai
{
    class CurrentTargetValue : public UnitManualSetValue
    {
        public:
            CurrentTargetValue(PlayerbotAI* ai) : UnitManualSetValue(ai, NULL),
                m_losChecked(0), m_targetInLos(false), m_concealmentWasAccepted(false),
                m_targetContextRevision(0) {}

            virtual Unit* Get();
            virtual void Set(Unit* unit);

        private:
            bool IsBotOrPetAttacking(Unit const* target) const;
            bool IsConcealed(Unit const* target) const;
            bool MustDisengageFromConcealedTarget(Unit const* target) const;

            ObjectGuid selection;

            // Only the vmap raycast is cached. Concealment/detection is deliberately fresh.
            ObjectGuid m_losTarget;
            uint32 m_losChecked;
            bool m_targetInLos;

            // This records a positive pre-attack detection, not an unavailable-target latch.
            // Without it, concealment is rejected only while the bot or pet owns a live attack.
            ObjectGuid m_concealmentTarget;
            bool m_concealmentWasAccepted;
            uint32 m_targetContextRevision;
    };
}
