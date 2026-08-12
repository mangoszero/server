#include "../../botpch.h"
#include "../playerbot.h"
#include "PassiveMultiplier.h"

using namespace ai;

list<string> PassiveMultiplier::allowedActions;
list<string> PassiveMultiplier::allowedParts;

PassiveMultiplier::PassiveMultiplier(PlayerbotAI* ai) : Multiplier(ai, "passive")
{
    if (allowedActions.empty())
    {
        allowedActions.push_back("co");
        allowedActions.push_back("nc");
        allowedActions.push_back("reset ai");
        allowedActions.push_back("check mount state");
        // "security check" is what APPLIES the passive lock when a master's group loot
        // settings are wrong, and it is also the only thing that lifts it again. Without it
        // here the lock suppressed its own release: relevance went to zero the moment passive
        // took effect, Execute was never reached again, and the bot stayed passive and
        // stationary after the master had already corrected the settings.
        allowedActions.push_back("security check");
    }

    if (allowedParts.empty())
    {
        allowedParts.push_back("follow");
        allowedParts.push_back("stay");
        allowedParts.push_back("chat shortcut");
    }
}

float PassiveMultiplier::GetValue(Action* action) {
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

    for (list<string>::iterator i = allowedParts.begin(); i != allowedParts.end(); i++)
    {
        if (name.find(*i) != string::npos)
        {
            return 1.0f;
        }
    }

    return 0;
}
