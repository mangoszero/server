#include "botpch.h"
#include "../../playerbot.h"
#include "PaladinMultipliers.h"
#include "TankPaladinStrategy.h"

using namespace ai;

class TankPaladinStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    TankPaladinStrategyActionNodeFactory()
    {
        creators["blessing of sanctuary"] = &blessing_of_sanctuary;
    }
private:
    static ActionNode* blessing_of_sanctuary(PlayerbotAI* ai)
    {
        return new ActionNode ("blessing of sanctuary",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("blessing of kings"), nullptr),
            /*C*/ nullptr);
    }
};

TankPaladinStrategy::TankPaladinStrategy(PlayerbotAI* ai) : GenericPaladinStrategy(ai)
{
    actionNodeFactories.Add(new TankPaladinStrategyActionNodeFactory());
}

NextAction** TankPaladinStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("melee", ACTION_NORMAL), nullptr);
}

void TankPaladinStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericPaladinStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "judgement of light",
        NextAction::array(0, new NextAction("judgement of light", ACTION_NORMAL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium mana",
        NextAction::array(0, new NextAction("judgement of wisdom", ACTION_NORMAL + 3), nullptr)));

    triggers.push_back(new TriggerNode(
        "righteous fury",
        NextAction::array(0, new NextAction("righteous fury", ACTION_HIGH + 8), nullptr)));

    triggers.push_back(new TriggerNode(
        "light aoe",
        NextAction::array(0, new NextAction("hammer of the righteous", ACTION_HIGH + 6), new NextAction("avenger's shield", ACTION_HIGH + 6), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium aoe",
        NextAction::array(0, new NextAction("consecration", ACTION_HIGH + 6), nullptr)));

    triggers.push_back(new TriggerNode(
        "lose aggro",
        NextAction::array(0, new NextAction("hand of reckoning", ACTION_HIGH + 7), nullptr)));

    triggers.push_back(new TriggerNode(
        "holy shield",
        NextAction::array(0, new NextAction("holy shield", ACTION_HIGH + 7), nullptr)));

    triggers.push_back(new TriggerNode(
        "blessing",
        NextAction::array(0, new NextAction("blessing of sanctuary", ACTION_HIGH + 9), nullptr)));
}
