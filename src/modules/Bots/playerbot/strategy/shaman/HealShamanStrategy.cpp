#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "ShamanMultipliers.h"
#include "HealShamanStrategy.h"

using namespace ai;

class HealShamanStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        HealShamanStrategyActionNodeFactory()
        {
            creators["earthliving weapon"] = &earthliving_weapon;
            creators["mana tide totem"] = &mana_tide_totem;
        }
    private:
        static ActionNode* earthliving_weapon(PlayerbotAI* ai)
        {
            return new ActionNode ("earthliving weapon",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("flametongue weapon"), NULL),
                /*C*/ NULL);
        }
        static ActionNode* mana_tide_totem(PlayerbotAI* ai)
        {
            return new ActionNode ("mana tide totem",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("mana potion"), NULL),
                /*C*/ NULL);
        }

};

NextAction** HealShamanStrategy::getDefaultActions()
{
    // Same defect as the healing druid: no override at all, so this inherited
    // Strategy::getDefaultActions()'s NULL and a healing shaman had no unconditional action.
    // Its healing, totem and utility triggers are all conditional, so it stood in combat doing
    // nothing across the whole level range, and a grouped Restoration shaman is selected
    // automatically. Lightning Bolt is a level 1 shaman spell needing no equipment; melee is
    // the always-available last resort.
    return NextAction::array(0,
        new NextAction("lightning bolt", 10.0f),
        new NextAction("melee in range", 9.0f),
        NULL);
}

HealShamanStrategy::HealShamanStrategy(PlayerbotAI* ai) : GenericShamanStrategy(ai)
{
    actionNodeFactories.Add(new HealShamanStrategyActionNodeFactory());
}

void HealShamanStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "enemy out of spell",
        NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    // The no-aggro variant, deliberately, following the priest healer rather than the mage
    // and hunter DPS: a healer that runs while it holds aggro drags the mob across the
    // party and can pull adds, so with aggro the right play is to stand and heal until the
    // tank takes it back -- the ACTION_EMERGENCY flee routes already cover the desperate
    // cases. With no attackers on us this is a free step out of cleave range. ACTION_MOVE
    // rather than a rotation-band value so it outranks "set facing" (ACTION_NORMAL + 7 in
    // CombatStrategy), which would otherwise spend the tick turning the shaman towards the
    // thing it is stepping away from.
    triggers.push_back(new TriggerNode(
            "enemy too close for spell no aggro",
        NextAction::array(0, new NextAction("flee", ACTION_MOVE), NULL)));

    triggers.push_back(new TriggerNode(
            "shaman weapon",
        NextAction::array(0, new NextAction("earthliving weapon", 22.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "low mana",
        NextAction::array(0, new NextAction("mana tide totem", ACTION_EMERGENCY + 5), NULL)));

    triggers.push_back(new TriggerNode(
            "cleanse spirit poison",
        NextAction::array(0, new NextAction("cleanse spirit", 24.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "cleanse spirit curse",
        NextAction::array(0, new NextAction("cleanse spirit", 24.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "cleanse spirit disease",
        NextAction::array(0, new NextAction("cleanse spirit", 24.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member cleanse spirit poison",
        NextAction::array(0, new NextAction("cleanse spirit poison on party", 35.0f), new
                NextAction("cure poison on party", 33.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member cleanse spirit curse",
        NextAction::array(0, new NextAction("cleanse spirit curse on party", 35.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member cleanse spirit disease",
        NextAction::array(0, new NextAction("cleanse spirit disease on party", 35.0f), new
                NextAction("cure disease on party", 33.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe",
        NextAction::array(0, new NextAction("healing stream totem", ACTION_LIGHT_HEAL), NULL)));
}
