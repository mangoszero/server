#include "botpch.h"
#include "../../playerbot.h"
#include "MageMultipliers.h"
#include "GenericMageStrategy.h"

using namespace ai;

class GenericMageStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericMageStrategyActionNodeFactory()
    {
        creators["frostbolt"] = &frostbolt;
        creators["fire blast"] = &fire_blast;
        creators["scorch"] = &scorch;
        creators["frost nova"] = &frost_nova;
        creators["icy veins"] = &icy_veins;
        creators["combustion"] = &combustion;
        creators["evocation"] = &evocation;
        creators["dragon's breath"] = &dragons_breath;
        creators["blast wave"] = &blast_wave;
    }
private:
    static ActionNode* frostbolt(PlayerbotAI* ai)
    {
        return new ActionNode ("frostbolt",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("shoot"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* fire_blast(PlayerbotAI* ai)
    {
        return new ActionNode ("fire blast",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("scorch"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* scorch(PlayerbotAI* ai)
    {
        return new ActionNode ("scorch",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("shoot"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* frost_nova(PlayerbotAI* ai)
    {
        return new ActionNode ("frost nova",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("flee"), nullptr),
            /*C*/ NextAction::array(0, new NextAction("flee"), nullptr));
    }
    static ActionNode* icy_veins(PlayerbotAI* ai)
    {
        return new ActionNode ("icy veins",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* combustion(PlayerbotAI* ai)
    {
        return new ActionNode ("combustion",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* evocation(PlayerbotAI* ai)
    {
        return new ActionNode ("evocation",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("mana potion"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* dragons_breath(PlayerbotAI* ai)
    {
        return new ActionNode ("dragon's breath",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("blast wave"), nullptr),
            /*C*/ NextAction::array(0, new NextAction("flamestrike", 71.0f), nullptr));
    }
    static ActionNode* blast_wave(PlayerbotAI* ai)
    {
        return new ActionNode ("blast wave",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("frost nova"), nullptr),
            /*C*/ NextAction::array(0, new NextAction("flamestrike", 71.0f), nullptr));
    }
};

GenericMageStrategy::GenericMageStrategy(PlayerbotAI* ai) : RangedCombatStrategy(ai)
{
    actionNodeFactories.Add(new GenericMageStrategyActionNodeFactory());
}

void GenericMageStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    RangedCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "remove curse",
        NextAction::array(0, new NextAction("remove curse", 41.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "remove curse on party",
        NextAction::array(0, new NextAction("remove curse on party", 40.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "enemy too close for spell",
        NextAction::array(0, new NextAction("frost nova", 50.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "counterspell",
        NextAction::array(0, new NextAction("counterspell", 40.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "counterspell on enemy healer",
        NextAction::array(0, new NextAction("counterspell on enemy healer", 40.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "critical health",
        NextAction::array(0, new NextAction("ice block", 80.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "polymorph",
        NextAction::array(0, new NextAction("polymorph", 30.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "spellsteal",
        NextAction::array(0, new NextAction("spellsteal", 40.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium threat",
        NextAction::array(0, new NextAction("invisibility", 60.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "low mana",
        NextAction::array(0, new NextAction("evocation", ACTION_EMERGENCY + 5), nullptr)));
}
