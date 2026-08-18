#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "HunterMultipliers.h"
#include "HunterBuffStrategies.h"

using namespace ai;

void HunterBuffDpsStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "aspect of the hawk",
        NextAction::array(0, new NextAction("aspect of the hawk", 90.0f), NULL)));
}

void HunterNatureResistanceStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "aspect of the wild",
        NextAction::array(0, new NextAction("aspect of the wild", 90.0f), NULL)));
}

void HunterBuffSpeedStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "aspect of the pack",
        NextAction::array(0, new NextAction("aspect of the pack", 10.0f), NULL)));
}

void HunterBuffManaStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    // Aspect of the Viper is TBC. 1.12 gives the hunter no mana aspect at all, so this
    // strategy has nothing to wire. Drinking is already covered by UseFoodStrategy, which
    // schedules "drink" in its own right and does not need routing through a dead aspect.
}
