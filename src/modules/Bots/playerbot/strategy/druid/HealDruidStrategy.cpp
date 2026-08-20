#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "DruidMultipliers.h"
#include "HealDruidStrategy.h"

using namespace ai;

class HealDruidStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        HealDruidStrategyActionNodeFactory()
        {}

    private:
};

NextAction** HealDruidStrategy::getDefaultActions()
{
    // There was no override here at all, so this inherited Strategy::getDefaultActions()'s
    // NULL and a healing druid had no unconditional action whatsoever -- healing, dispel and
    // utility triggers are every one of them conditional. It stood in combat doing nothing,
    // across the whole level range, and a grouped Restoration druid is selected automatically.
    // Wrath is trainable at level 1 by every druid and needs no equipment or form; melee is the
    // always-available last resort.
    return NextAction::array(0,
        new NextAction("wrath", 10.0f),
        new NextAction("melee in range", 9.0f),
        NULL);
}

HealDruidStrategy::HealDruidStrategy(PlayerbotAI* ai) : GenericDruidStrategy(ai)
{
    actionNodeFactories.Add(new HealDruidStrategyActionNodeFactory());
}

void HealDruidStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericDruidStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "enemy out of spell",
        NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    triggers.push_back(new TriggerNode(
            "tree form",
        NextAction::array(0, new NextAction("tree form", ACTION_HIGH + 1), NULL)));

    triggers.push_back(new TriggerNode(
            "medium health",
        NextAction::array(0, new NextAction("regrowth", ACTION_MEDIUM_HEAL + 2), NULL)));

    triggers.push_back(new TriggerNode(
            "party member medium health",
        NextAction::array(0, new NextAction("regrowth on party", ACTION_MEDIUM_HEAL + 1), NULL)));

    triggers.push_back(new TriggerNode(
            "almost full health",
        NextAction::array(0, new NextAction("rejuvenation", ACTION_LIGHT_HEAL + 2), NULL)));

    triggers.push_back(new TriggerNode(
            "party member almost full health",
        NextAction::array(0, new NextAction("rejuvenation on party", ACTION_LIGHT_HEAL + 1), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe heal",
        NextAction::array(0, new NextAction("tranquility", ACTION_MEDIUM_HEAL + 3), NULL)));

    triggers.push_back(new TriggerNode(
            "entangling roots",
        NextAction::array(0, new NextAction("entangling roots on cc", ACTION_HIGH + 1), NULL)));
}
