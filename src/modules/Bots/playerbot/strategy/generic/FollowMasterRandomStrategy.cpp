#include "botpch.h"
#include "playerbot.h"
#include "FollowMasterRandomStrategy.h"

using namespace ai;

void FollowMasterRandomStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "far from master",
        NextAction::array(0, new NextAction("be near", 1.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "random",
        NextAction::array(0, new NextAction("move random", 2.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "target in sight",
        NextAction::array(0, new NextAction("stay combat",3.0f), NULL)));
}