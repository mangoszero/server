#include "botpch.h"
#include "../../playerbot.h"
#include "GenericDruidStrategy.h"
#include "DruidAiObjectContext.h"

using namespace ai;

class GenericDruidStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericDruidStrategyActionNodeFactory()
    {
        creators["melee"] = &melee;
        creators["caster form"] = &caster_form;
        creators["cure poison"] = &cure_poison;
        creators["cure poison on party"] = &cure_poison_on_party;
        creators["abolish poison"] = &abolish_poison;
        creators["abolish poison on party"] = &abolish_poison_on_party;
        creators["rebirth"] = &rebirth;
        creators["entangling roots on cc"] = &entangling_roots_on_cc;
        creators["innervate"] = &innervate;
    }

private:
    static ActionNode* melee(PlayerbotAI* ai)
    {
        return new ActionNode ("melee",
            /*P*/ NextAction::array(0, new NextAction("reach melee"), nullptr),
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* caster_form(PlayerbotAI* ai)
    {
        return new ActionNode ("caster form",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* cure_poison(PlayerbotAI* ai)
    {
        return new ActionNode ("cure poison",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* cure_poison_on_party(PlayerbotAI* ai)
    {
        return new ActionNode ("cure poison on party",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* abolish_poison(PlayerbotAI* ai)
    {
        return new ActionNode ("abolish poison",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* abolish_poison_on_party(PlayerbotAI* ai)
    {
        return new ActionNode ("abolish poison on party",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* rebirth(PlayerbotAI* ai)
    {
        return new ActionNode ("rebirth",
            /*P*/ nullptr,
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* entangling_roots_on_cc(PlayerbotAI* ai)
    {
        return new ActionNode ("entangling roots on cc",
            /*P*/ NextAction::array(0, new NextAction("caster form"), nullptr),
            /*A*/ nullptr,
            /*C*/ nullptr);
    }
    static ActionNode* innervate(PlayerbotAI* ai)
    {
        return new ActionNode ("innervate",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("mana potion"), nullptr),
            /*C*/ nullptr);
    }
};

GenericDruidStrategy::GenericDruidStrategy(PlayerbotAI* ai) : CombatStrategy(ai)
{
    actionNodeFactories.Add(new GenericDruidStrategyActionNodeFactory());
}

void GenericDruidStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "low health",
        NextAction::array(0, new NextAction("regrowth", ACTION_MEDIUM_HEAL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member low health",
        NextAction::array(0, new NextAction("regrowth on party", ACTION_MEDIUM_HEAL + 1), nullptr)));


    triggers.push_back(new TriggerNode(
        "critical health",
        NextAction::array(0, new NextAction("regrowth", ACTION_CRITICAL_HEAL + 2), new NextAction("healing touch", ACTION_CRITICAL_HEAL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member critical health",
        NextAction::array(0,  new NextAction("regrowth on party", ACTION_CRITICAL_HEAL + 1), new NextAction("healing touch on party", ACTION_CRITICAL_HEAL + 1), nullptr)));


    triggers.push_back(new TriggerNode(
        "cure poison",
        NextAction::array(0, new NextAction("abolish poison", ACTION_DISPEL + 2), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member cure poison",
        NextAction::array(0, new NextAction("abolish poison on party", ACTION_DISPEL + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member dead",
        NextAction::array(0, new NextAction("rebirth", ACTION_HIGH + 1), nullptr)));

    triggers.push_back(new TriggerNode(
        "low mana",
        NextAction::array(0, new NextAction("innervate", ACTION_EMERGENCY + 5), nullptr)));
}
