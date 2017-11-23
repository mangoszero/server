#include "botpch.h"
#include "../../playerbot.h"
#include "RogueMultipliers.h"
#include "DpsRogueStrategy.h"

using namespace ai;

class DpsRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    DpsRogueStrategyActionNodeFactory()
    {
        creators["riposte"] = &riposte;
        creators["mutilate"] = &mutilate;
        creators["sinister strike"] = &sinister_strike;
        creators["kick"] = &kick;
        creators["kidney shot"] = &kidney_shot;
        creators["rupture"] = &rupture;
        creators["backstab"] = &backstab;
    }
private:
    static ActionNode* riposte(PlayerbotAI* ai)
    {
        return new ActionNode ("riposte",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("mutilate"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* mutilate(PlayerbotAI* ai)
    {
        return new ActionNode ("mutilate",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("sinister strike"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* sinister_strike(PlayerbotAI* ai)
    {
        return new ActionNode ("sinister strike",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* kick(PlayerbotAI* ai)
    {
        return new ActionNode ("kick",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("kidney shot"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* kidney_shot(PlayerbotAI* ai)
    {
        return new ActionNode ("kidney shot",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* rupture(PlayerbotAI* ai)
    {
        return new ActionNode ("rupture",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("eviscerate"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* backstab(PlayerbotAI* ai)
    {
        return new ActionNode ("backstab",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("mutilate"), nullptr),
            /*C*/ nullptr);
    }
};

DpsRogueStrategy::DpsRogueStrategy(PlayerbotAI* ai) : MeleeCombatStrategy(ai)
{
    actionNodeFactories.Add(new DpsRogueStrategyActionNodeFactory());
}

NextAction** DpsRogueStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("riposte", ACTION_NORMAL), nullptr);
}

void DpsRogueStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    MeleeCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "combo points available",
        NextAction::array(0, new NextAction("rupture", ACTION_HIGH + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium threat",
        NextAction::array(0, new NextAction("vanish", ACTION_HIGH), nullptr)));

    triggers.push_back(new TriggerNode(
        "low health",
        NextAction::array(0, new NextAction("evasion", ACTION_EMERGENCY), new NextAction("feint", ACTION_EMERGENCY), nullptr)));

    triggers.push_back(new TriggerNode(
        "kick",
        NextAction::array(0, new NextAction("kick", ACTION_INTERRUPT + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "kick on enemy healer",
        NextAction::array(0, new NextAction("kick on enemy healer", ACTION_INTERRUPT + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "behind target",
        NextAction::array(0, new NextAction("backstab", ACTION_NORMAL), nullptr)));
}
