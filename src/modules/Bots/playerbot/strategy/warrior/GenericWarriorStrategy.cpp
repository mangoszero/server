#include "botpch.h"
#include "../../playerbot.h"
#include "GenericWarriorStrategy.h"
#include "WarriorAiObjectContext.h"

using namespace ai;

class GenericWarriorStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericWarriorStrategyActionNodeFactory()
    {
        creators["hamstring"] = &hamstring;
        creators["heroic strike"] = &heroic_strike;
        creators["battle shout"] = &battle_shout;
    }
private:
    static ActionNode* hamstring(PlayerbotAI* ai)
    {
        return new ActionNode ("hamstring",
            /*P*/ NextAction::array(0, new NextAction("battle stance"), nullptr),
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* heroic_strike(PlayerbotAI* ai)
    {
        return new ActionNode ("heroic strike",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* battle_shout(PlayerbotAI* ai)
    {
        return new ActionNode ("battle shout",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("melee"), nullptr),
            /*C*/ nullptr);
    }
};

GenericWarriorStrategy::GenericWarriorStrategy(PlayerbotAI* ai) : MeleeCombatStrategy(ai)
{
    actionNodeFactories.Add(new GenericWarriorStrategyActionNodeFactory());
}

void GenericWarriorStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    MeleeCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "battle shout",
        NextAction::array(0, new NextAction("battle shout", ACTION_HIGH + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "rend",
        NextAction::array(0, new NextAction("rend", ACTION_NORMAL + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "bloodrage",
        NextAction::array(0, new NextAction("bloodrage", ACTION_HIGH + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "shield bash",
        NextAction::array(0, new NextAction("shield bash", ACTION_INTERRUPT + 4), nullptr)));

    triggers.push_back(new TriggerNode(
        "shield bash on enemy healer",
        NextAction::array(0, new NextAction("shield bash on enemy healer", ACTION_INTERRUPT + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "critical health",
        NextAction::array(0, new NextAction("intimidating shout", ACTION_EMERGENCY), nullptr)));
}
