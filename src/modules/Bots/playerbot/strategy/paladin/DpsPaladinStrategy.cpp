#include "botpch.h"
#include "../../playerbot.h"
#include "PaladinMultipliers.h"
#include "DpsPaladinStrategy.h"

using namespace ai;

class DpsPaladinStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    DpsPaladinStrategyActionNodeFactory()
    {
        creators["seal of vengeance"] = &seal_of_vengeance;
        creators["seal of command"] = &seal_of_command;
        creators["blessing of might"] = &blessing_of_might;
        creators["crusader strike"] = &crusader_strike;
    }

private:
    static ActionNode* seal_of_vengeance(PlayerbotAI* ai)
    {
        return new ActionNode ("seal of vengeance",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("seal of command"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* seal_of_command(PlayerbotAI* ai)
    {
        return new ActionNode ("seal of command",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("seal of wisdom"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* blessing_of_might(PlayerbotAI* ai)
    {
        return new ActionNode ("blessing of might",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("blessing of kings"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* crusader_strike(PlayerbotAI* ai)
    {
        return new ActionNode ("crusader strike",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
            /*C*/ nullptr);
    }
};

DpsPaladinStrategy::DpsPaladinStrategy(PlayerbotAI* ai) : GenericPaladinStrategy(ai)
{
    actionNodeFactories.Add(new DpsPaladinStrategyActionNodeFactory());
}

NextAction** DpsPaladinStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("crusader strike", ACTION_NORMAL + 1), nullptr);
}

void DpsPaladinStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericPaladinStrategy::InitTriggers(triggers);
    
    triggers.push_back(new TriggerNode(
        "low health",
        NextAction::array(0, new NextAction("divine shield", ACTION_CRITICAL_HEAL + 2), new NextAction("holy light", ACTION_CRITICAL_HEAL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "judgement of wisdom",
        NextAction::array(0, new NextAction("judgement of wisdom", ACTION_NORMAL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "blessing",
        NextAction::array(0, new NextAction("blessing of might", ACTION_HIGH + 8), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium aoe",
        NextAction::array(0, new NextAction("divine storm", ACTION_HIGH + 1), new NextAction("consecration", ACTION_HIGH + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "art of war",
        NextAction::array(0, new NextAction("exorcism", ACTION_HIGH + 2), nullptr)));
}
