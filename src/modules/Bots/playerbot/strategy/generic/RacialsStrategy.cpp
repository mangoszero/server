#include "botpch.h"
#include "../../playerbot.h"
#include "RacialsStrategy.h"

using namespace ai;


class RacialsStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    RacialsStrategyActionNodeFactory()
    {
        creators["lifeblood"] = &lifeblood;
    }
private:
    static ActionNode* lifeblood(PlayerbotAI* ai)
    {
        return new ActionNode ("lifeblood",  
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("gift of the naaru"), nullptr), 
            /*C*/ nullptr);
    }
};

void RacialsStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "low health", 
        NextAction::array(0, new NextAction("lifeblood", 71.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "low mana", 
        NextAction::array(0, new NextAction("arcane torrent", ACTION_EMERGENCY + 6), nullptr)));
}

RacialsStrategy::RacialsStrategy(PlayerbotAI* ai) : Strategy(ai)
{
    actionNodeFactories.Add(new RacialsStrategyActionNodeFactory());
}
