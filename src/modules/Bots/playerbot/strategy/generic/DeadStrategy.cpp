#include "botpch.h"
#include "../../playerbot.h"
#include "../Strategy.h"
#include "DeadStrategy.h"

using namespace ai;

void DeadStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    PassTroughStrategy::InitTriggers(triggers);

    // Trigger name changed from "dead" to "bot dead" because of collision with AI_VALUE2(bool, "dead", ...))
    triggers.push_back(new TriggerNode(
        "bot dead",
        NextAction::array(0, new NextAction("revive from corpse", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "resurrect request",
        NextAction::array(0, new NextAction("accept resurrect", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "master released spirit",
        NextAction::array(0, new NextAction("release spirit with master", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "master spirit healer",
        NextAction::array(0, new NextAction("spirit healer with master", relevance), NULL)));
}

DeadStrategy::DeadStrategy(PlayerbotAI* ai) : PassTroughStrategy(ai)
{}
