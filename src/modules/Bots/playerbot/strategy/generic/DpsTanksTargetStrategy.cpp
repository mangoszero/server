#include "botpch.h"
#include "../../playerbot.h"
#include "DpsTanksTargetStrategy.h"

using namespace ai;

void DpsTanksTargetStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "no tanks target active",
        NextAction::array(0, new NextAction("attack tanks target", 50.0f), NULL)));
}

