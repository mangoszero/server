#include "botpch.h"
#include "../../playerbot.h"
#include "WarriorMultipliers.h"
#include "DpsWarriorStrategy.h"

using namespace ai;

class DpsWarriorStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    DpsWarriorStrategyActionNodeFactory()
    {
        creators["overpower"] = &overpower;
        creators["melee"] = &melee;
        creators["charge"] = &charge;
        creators["bloodthirst"] = &bloodthirst;
        creators["rend"] = &rend;
        creators["mocking blow"] = &mocking_blow;
        creators["death wish"] = &death_wish;
        creators["execute"] = &execute;
    }
private:
    static ActionNode* overpower(PlayerbotAI* ai)
    {
        return new ActionNode ("overpower",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* melee(PlayerbotAI* ai)
    {
        return new ActionNode ("melee",
            /*P*/ NextAction::array(0, new NextAction("charge"), nullptr),
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* charge(PlayerbotAI* ai)
    {
        return new ActionNode ("charge",
            /*P*/ NextAction::array(0, new NextAction("battle stance"), nullptr),
            /*A*/ NextAction::array(0, new NextAction("reach melee"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* bloodthirst(PlayerbotAI* ai)
    {
        return new ActionNode ("bloodthirst",
            /*P*/ NextAction::array(0, new NextAction("battle stance"), nullptr),
            /*A*/ NextAction::array(0, new NextAction("heroic strike"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* rend(PlayerbotAI* ai)
    {
        return new ActionNode ("rend",
            /*P*/ NextAction::array(0, new NextAction("battle stance"), nullptr),
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* mocking_blow(PlayerbotAI* ai)
    {
        return new ActionNode ("mocking blow",
            /*P*/ NextAction::array(0, new NextAction("battle stance"), nullptr),
            /*A*/ NextAction::array(0, nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* death_wish(PlayerbotAI* ai)
    {
        return new ActionNode ("death wish",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("berserker rage"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* execute(PlayerbotAI* ai)
    {
        return new ActionNode ("execute",
            /*P*/ NextAction::array(0, new NextAction("battle stance"), nullptr),
            /*A*/ NextAction::array(0, new NextAction("heroic strike"), nullptr),
            /*C*/ nullptr);
    }
};

DpsWarriorStrategy::DpsWarriorStrategy(PlayerbotAI* ai) : GenericWarriorStrategy(ai)
{
    actionNodeFactories.Add(new DpsWarriorStrategyActionNodeFactory());
}

NextAction** DpsWarriorStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("bloodthirst", ACTION_NORMAL + 1), nullptr);
}

void DpsWarriorStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericWarriorStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "enemy out of melee",
        NextAction::array(0, new NextAction("charge", ACTION_NORMAL + 9), nullptr)));

    triggers.push_back(new TriggerNode(
        "target critical health",
        NextAction::array(0, new NextAction("execute", ACTION_HIGH + 4), nullptr)));

    triggers.push_back(new TriggerNode(
        "hamstring",
        NextAction::array(0, new NextAction("hamstring", ACTION_INTERRUPT), nullptr)));

    triggers.push_back(new TriggerNode(
        "victory rush",
        NextAction::array(0, new NextAction("victory rush", ACTION_HIGH + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "death wish",
        NextAction::array(0, new NextAction("death wish", ACTION_HIGH + 2), nullptr)));
}


void DpsWarrirorAoeStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "rend on attacker",
        NextAction::array(0, new NextAction("rend on attacker", ACTION_HIGH + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "light aoe",
        NextAction::array(0, new NextAction("thunder clap", ACTION_HIGH + 2), new NextAction("demoralizing shout", ACTION_HIGH + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium aoe",
        NextAction::array(0, new NextAction("cleave", ACTION_HIGH + 3), nullptr)));
}
