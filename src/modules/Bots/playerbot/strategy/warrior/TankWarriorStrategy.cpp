#include "botpch.h"
#include "../../playerbot.h"
#include "WarriorMultipliers.h"
#include "TankWarriorStrategy.h"

using namespace ai;

class TankWarriorStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    TankWarriorStrategyActionNodeFactory()
    {
        creators["melee"] = &melee;
        creators["shield wall"] = &shield_wall;
        creators["rend"] = &rend;
        creators["revenge"] = &revenge;
        creators["devastate"] = &devastate;
        creators["shockwave"] = &shockwave;
        creators["taunt"] = &taunt;
    }
private:
    static ActionNode* melee(PlayerbotAI* ai)
    {
        return new ActionNode ("melee",
            /*P*/ NextAction::array(0, new NextAction("defensive stance"), new NextAction("reach melee"), nullptr),
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* shield_wall(PlayerbotAI* ai)
    {
        return new ActionNode ("shield wall",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("shield block"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* rend(PlayerbotAI* ai)
    {
        return new ActionNode ("rend",
            /*P*/ NextAction::array(0, new NextAction("defensive stance"), nullptr),
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* revenge(PlayerbotAI* ai)
    {
        return new ActionNode ("revenge",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* devastate(PlayerbotAI* ai)
    {
        return new ActionNode ("devastate",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("sunder armor"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* shockwave(PlayerbotAI* ai)
    {
        return new ActionNode ("shockwave",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("cleave"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* taunt(PlayerbotAI* ai)
    {
        return new ActionNode ("taunt",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("mocking blow"), nullptr),
            /*C*/ nullptr);
    }
};

TankWarriorStrategy::TankWarriorStrategy(PlayerbotAI* ai) : GenericWarriorStrategy(ai)
{
    actionNodeFactories.Add(new TankWarriorStrategyActionNodeFactory());
}

NextAction** TankWarriorStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("devastate", ACTION_NORMAL + 1), new NextAction("revenge", ACTION_NORMAL + 1), nullptr);
}

void TankWarriorStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericWarriorStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "medium rage available",
        NextAction::array(0, new NextAction("shield slam", ACTION_NORMAL + 2), new NextAction("heroic strike", ACTION_NORMAL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "disarm",
        NextAction::array(0, new NextAction("disarm", ACTION_NORMAL), nullptr)));

    triggers.push_back(new TriggerNode(
        "lose aggro",
        NextAction::array(0, new NextAction("taunt", ACTION_HIGH + 9), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium health",
        NextAction::array(0, new NextAction("shield wall", ACTION_MEDIUM_HEAL), nullptr)));

    triggers.push_back(new TriggerNode(
        "critical health",
        NextAction::array(0, new NextAction("last stand", ACTION_EMERGENCY + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium aoe",
        NextAction::array(0, new NextAction("shockwave", ACTION_HIGH + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "light aoe",
        NextAction::array(0, new NextAction("thunder clap", ACTION_HIGH + 2), new NextAction("demoralizing shout", ACTION_HIGH + 2),  new NextAction("cleave", ACTION_HIGH + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "high aoe",
        NextAction::array(0, new NextAction("challenging shout", ACTION_HIGH + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "concussion blow",
        NextAction::array(0, new NextAction("concussion blow", ACTION_INTERRUPT), nullptr)));

    triggers.push_back(new TriggerNode(
        "sword and board",
        NextAction::array(0, new NextAction("shield slam", ACTION_HIGH + 3), nullptr)));
}
