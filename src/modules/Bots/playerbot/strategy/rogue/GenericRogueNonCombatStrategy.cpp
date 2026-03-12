#include "botpch.h"
#include "../../playerbot.h"
#include "RogueTriggers.h"
#include "RogueMultipliers.h"
#include "GenericRogueNonCombatStrategy.h"
#include "RogueActions.h"

using namespace ai;

void GenericRogueNonCombatStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "attack",
        NextAction::array(0, new NextAction("begin ambush", 101.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "sap",
        NextAction::array(0, new NextAction("begin sap", ACTION_HIGH + 1), NULL)));

    triggers.push_back(new TriggerNode(
        "stealth",
        NextAction::array(0, new NextAction("stealth", ACTION_NORMAL), NULL)));
}
