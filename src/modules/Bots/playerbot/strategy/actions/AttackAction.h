#pragma once

#include "../Action.h"
#include "MovementActions.h"

namespace ai
{
    class AttackAction : public MovementAction
    {
        public:
            AttackAction(PlayerbotAI* ai, string name) : MovementAction(ai, name) {}

        public:
            virtual bool Execute(Event event);

        protected:
            bool Attack(Unit* target);
    };

    class AttackMyTargetAction : public AttackAction
    {
        public:
            AttackMyTargetAction(PlayerbotAI* ai, string name = "attack my target") : AttackAction(ai, name) {}

        public:
            virtual bool Execute(Event event);
    };

    class PullMyTargetAction : public AttackMyTargetAction
    {
        public:
            PullMyTargetAction(PlayerbotAI* ai, string name = "pull my target") : AttackMyTargetAction(ai, name) {}

        public:
            virtual bool Execute(Event event)
            {
                MakeVerbose();
                ai->ChangeStrategy("+pull", BOT_STATE_COMBAT);
                if (!AttackMyTargetAction::Execute(event))
                {
                    ai->ChangeStrategy("-pull", BOT_STATE_COMBAT);
                    return false;
                }
                return true;
            }
    };

    class AttackDuelOpponentAction : public AttackAction
    {
        public:
            AttackDuelOpponentAction(PlayerbotAI* ai, string name = "attack duel opponent") : AttackAction(ai, name) {}

        public:
            virtual bool Execute(Event event);
            virtual bool isUseful();
    };
}
