#include "botpch.h"
#include "../../playerbot.h"
#include "../Strategy.h"
#include "DeadStrategy.h"

using namespace ai;

void DeadStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    PassTroughStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "dead",
        NextAction::array(0, new NextAction("revive from corpse", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "resurrect request",
        NextAction::array(0, new NextAction("accept resurrect", relevance), nullptr)));
}

DeadStrategy::DeadStrategy(PlayerbotAI* ai) : PassTroughStrategy(ai)
{
}
