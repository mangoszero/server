#include "../../botpch.h"
#include "../playerbot.h"
#include "StrategyMultiplier.h"
#include "Strategy.h"

using namespace ai;

StrategyMultiplier::StrategyMultiplier(PlayerbotAI* ai, Strategy* strategy) : Multiplier(ai, "strategy")
{
    if (!strategy)
    {
        return;
    }

    NextAction** defaultActions = strategy->getDefaultActions();
    if (!defaultActions)
    {
        return;
    }

    for (int i = 0; defaultActions[i]; ++i)
    {
        allowedActions.push_back(defaultActions[i]->getName());
    }
    NextAction::destroy(defaultActions);

    allowedActions.push_back("co");
    allowedActions.push_back("nc");
    allowedActions.push_back("reset ai");

}

float StrategyMultiplier::GetValue(Action* action)
{
    if (!action)
    {
        return 1.0f;
    }

    string name = action->getName();
    for (list<string>::iterator i = allowedActions.begin(); i != allowedActions.end(); i++)
    {
        if (name == *i)
        {
            return 1.0f;
        }
    }
    return 0;
}

