#include "botpch.h"
#include "../../playerbot.h"
#include "PriestMultipliers.h"
#include "ShadowPriestStrategy.h"
#include "ShadowPriestStrategyActionNodeFactory.h"

using namespace ai;

ShadowPriestStrategy::ShadowPriestStrategy(PlayerbotAI* ai) : GenericPriestStrategy(ai)
{
    actionNodeFactories.Add(new ShadowPriestStrategyActionNodeFactory());
}

NextAction** ShadowPriestStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("mind blast", 30.0f), NULL);
}

void ShadowPriestStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "shadow word: pain",
        NextAction::array(0, new NextAction("shadow word: pain", 32.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "enemy out of spell",
        NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    triggers.push_back(new TriggerNode(
            "shadowform",
        NextAction::array(0, new NextAction("shadowform", 35.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "low mana",
        NextAction::array(0, new NextAction("dispersion", ACTION_EMERGENCY + 5), NULL)));

    triggers.push_back(new TriggerNode(
            "vampiric embrace",
        NextAction::array(0, new NextAction("vampiric embrace", 36.0f), NULL)));
}

void ShadowPriestAoeStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "shadow word: pain on attacker",
        NextAction::array(0, new NextAction("shadow word: pain on attacker", 31.0f), NULL)));
}

void ShadowPriestDebuffStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "devouring plague",
        NextAction::array(0, new NextAction("devouring plague", 33.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "vampiric touch",
        NextAction::array(0, new NextAction("vampiric touch", 31.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "shadow word: pain",
        NextAction::array(0, new NextAction("shadow word: pain", 32.0f), NULL)));
}
