#pragma once
#include "Multiplier.h"
#include "Action.h"

namespace ai
{
    class Strategy;

    class StrategyMultiplier : public Multiplier
    {
        public:
            StrategyMultiplier(PlayerbotAI* ai, Strategy* strategy);
            void AddAction(string name)
            {
                allowedActions.push_back(name);
            }
            virtual float GetValue(Action* action);
        protected:
            list<string> allowedActions;
    };
}
