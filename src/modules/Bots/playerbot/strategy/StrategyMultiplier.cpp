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
    // "security check" belongs with the always-allowed set for the same reason those do: it
    // is an escape hatch, not gameplay. It applies the passive lock when a master's loot
    // settings are wrong and is the only thing that lifts it again, so a multiplier that
    // zeroes it strands the bot passive and stationary after the master has already fixed
    // the settings. PassiveMultiplier allows it; this one must too, or a bot that also holds
    // "wait for pull" -- which another group bot can add without checking passive state --
    // has its recovery zeroed here instead.
    allowedActions.push_back("security check");

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

