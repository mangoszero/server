#pragma once

#include "ObjectAccessor.h"
#include "MovementActions.h"

namespace ai
{
    class CcReachSpellAction : public MovementAction
    {
        public:
            CcReachSpellAction(PlayerbotAI* ai)
                : MovementAction(ai, "cc reach spell") {}

            virtual bool Execute(Event event);
            virtual bool isUseful();
    };

    class CastCcOnMyTargetAction : public MovementAction
    {
        public:
            CastCcOnMyTargetAction(PlayerbotAI* ai)
                : MovementAction(ai, "cc on my target") {}

            virtual bool isPersistent();
            virtual bool isUseful();
            virtual NextAction** getPrerequisites();
            virtual bool Execute(Event event);

        private:
            time_t m_lastCastAttempt = 0;
            uint32 m_castAttempts = 0;
            ObjectGuid m_ccTargetGuid;
            bool     m_ccWithdrawn = false;

            string GetCcSpell(Unit* target);
            void WithdrawToGroupCenter(Unit* ccTarget);
    };
}
