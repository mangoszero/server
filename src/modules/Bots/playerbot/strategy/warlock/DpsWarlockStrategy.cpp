#include "botpch.h"
#include "../../playerbot.h"
#include "WarlockTriggers.h"
#include "WarlockMultipliers.h"
#include "DpsWarlockStrategy.h"
#include "WarlockActions.h"

using namespace ai;

class DpsWarlockStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    DpsWarlockStrategyActionNodeFactory()
    {
        creators["shadow bolt"] = &shadow_bolt;
        creators["incinerate"] = &incinerate;
    }
private:
    static ActionNode* shadow_bolt(PlayerbotAI* ai)
    {
        return new ActionNode ("shadow bolt",
            /*P*/ NULL,
            /*A*/ NextAction::array(0, new NextAction("shoot"), NULL),
            /*C*/ NULL);
    }
    static ActionNode* incinerate(PlayerbotAI* ai)
    {
        return new ActionNode ("incinerate",
            /*P*/ NULL,
            /*A*/ NextAction::array(0, new NextAction("shadow bolt"), NULL),
            /*C*/ NULL);
    }
};

class DpsAoeWarlockStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    DpsAoeWarlockStrategyActionNodeFactory()
    {
        creators["seed of corruption"] = &seed_of_corruption;
        creators["shadowfury"] = &shadowfury;
    }
private:
    static ActionNode* seed_of_corruption(PlayerbotAI* ai)
    {
        return new ActionNode ("seed of corruption",
            /*P*/ NULL,
            /*A*/ NextAction::array(0, new NextAction("rain of fire"), NULL),
            /*C*/ NULL);
    }
    static ActionNode* shadowfury(PlayerbotAI* ai)
    {
        return new ActionNode ("shadowfury",
            /*P*/ NULL,
            /*A*/ NextAction::array(0, new NextAction("rain of fire"), NULL),
            /*C*/ NULL);
    }
};

DpsWarlockStrategy::DpsWarlockStrategy(PlayerbotAI* ai) : GenericWarlockStrategy(ai)
{
    actionNodeFactories.Add(new DpsWarlockStrategyActionNodeFactory());
}

DpsAoeWarlockStrategy::DpsAoeWarlockStrategy(PlayerbotAI* ai) : CombatStrategy(ai)
{
    actionNodeFactories.Add(new DpsAoeWarlockStrategyActionNodeFactory());
}


NextAction** DpsWarlockStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("incinerate", 10.0f), new NextAction("shadow bolt", 10.0f), NULL);
}

void DpsWarlockStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericWarlockStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "shadow trance",
        NextAction::array(0, new NextAction("shadow bolt", 20.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "backlash",
        NextAction::array(0, new NextAction("shadow bolt", 20.0f), NULL)));
}

void DpsAoeWarlockStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "high aoe",
        NextAction::array(0, new NextAction("rain of fire", 30.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "medium aoe",
        NextAction::array(0, new NextAction("seed of corruption", 31.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "light aoe",
        NextAction::array(0, new NextAction("shadowfury", 29.0f), NULL)));

    triggers.push_back(new TriggerNode(
        "corruption on attacker",
        NextAction::array(0, new NextAction("corruption on attacker", 28.0f), NULL)));

}

void DpsWarlockDebuffStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "corruption",
        NextAction::array(0, new NextAction("corruption", 12.0f), NULL)));
}
