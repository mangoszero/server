#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "DruidMultipliers.h"
#include "GenericDruidNonCombatStrategy.h"

using namespace ai;

class GenericDruidNonCombatStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        GenericDruidNonCombatStrategyActionNodeFactory()
        {
            creators["mark of the wild"] = &mark_of_the_wild;
            creators["mark of the wild on party"] = &mark_of_the_wild_on_party;
            creators["innervate"] = &innervate;
        }
    private:
        static ActionNode* mark_of_the_wild(PlayerbotAI* ai)
        {
            return new ActionNode ("mark of the wild",
                /*P*/ NextAction::array(0, new NextAction("caster form"), NULL),
                /*A*/ NULL,
                /*C*/ NULL);
        }
        static ActionNode* mark_of_the_wild_on_party(PlayerbotAI* ai)
        {
            return new ActionNode ("mark of the wild on party",
                /*P*/ NextAction::array(0, new NextAction("caster form"), NULL),
                /*A*/ NULL,
                /*C*/ NULL);
        }
        static ActionNode* innervate(PlayerbotAI* ai)
        {
            return new ActionNode ("innervate",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("drink"), NULL),
                /*C*/ NULL);
        }
};

GenericDruidNonCombatStrategy::GenericDruidNonCombatStrategy(PlayerbotAI* ai) : NonCombatStrategy(ai)
{
    actionNodeFactories.Add(new GenericDruidNonCombatStrategyActionNodeFactory());
}

void GenericDruidNonCombatStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "mark of the wild",
        NextAction::array(0, new NextAction("mark of the wild", 12.0f), NULL)));

    // Feral's free-spell proc buff. Permanent once cast, so maintaining it out of combat
    // is the whole of the work; an untalented druid fails CanCastSpell and moves on.
    triggers.push_back(new TriggerNode(
            "omen of clarity",
        NextAction::array(0, new NextAction("omen of clarity", 11.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "mark of the wild on party",
        NextAction::array(0, new NextAction("mark of the wild on party", 11.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "cure poison",
        NextAction::array(0, new NextAction("abolish poison", 21.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member cure poison",
        NextAction::array(0, new NextAction("abolish poison on party", 20.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "remove curse",
        NextAction::array(0, new NextAction("remove curse", ACTION_DISPEL + 2), NULL)));

    triggers.push_back(new TriggerNode(
            "remove curse on party",
        NextAction::array(0, new NextAction("remove curse on party", ACTION_DISPEL + 1), NULL)));

    // "revive" is not a registered druid action -- the DBC row of that name (24341) is
    // something else entirely -- so a dead party member produced a silent NULL and the druid
    // simply stood there. Rebirth is the druid resurrection in 1.12 and is registered.
    triggers.push_back(new TriggerNode(
            "party member dead",
        NextAction::array(0, new NextAction("rebirth", 22.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "low mana",
        NextAction::array(0, new NextAction("innervate", ACTION_EMERGENCY + 5), NULL)));
}
