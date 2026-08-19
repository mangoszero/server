#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "ShamanMultipliers.h"
#include "CasterShamanStrategy.h"

using namespace ai;

class CasterShamanStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        CasterShamanStrategyActionNodeFactory()
        {
            creators["magma totem"] = &magma_totem;
            creators["thunderstorm"] = &thunderstorm;
        }
    private:
        static ActionNode* magma_totem(PlayerbotAI* ai)
        {
            return new ActionNode ("magma totem",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NextAction::array(0,
                    new NextAction("fire nova", ACTION_NORMAL), NULL));
        }
        static ActionNode* thunderstorm(PlayerbotAI* ai)
        {
            return new ActionNode ("thunderstorm",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("chain lightning"), NULL),
                /*C*/ NULL);
        }
};

CasterShamanStrategy::CasterShamanStrategy(PlayerbotAI* ai) : GenericShamanStrategy(ai)
{
    actionNodeFactories.Add(new CasterShamanStrategyActionNodeFactory());
}

NextAction** CasterShamanStrategy::getDefaultActions()
{
    // Lightning Bolt was the ONLY default action, and the cast-time multiplier suppresses
    // it against a nearly-dead target -- which left Elemental with nothing at all in that
    // state. Melee at 9.0f is the same always-available floor the heal spec and the healing
    // druid use: it never outranks the nuke, and only runs when the nuke cannot.
    return NextAction::array(0,
        new NextAction("lightning bolt", 10.0f),
        new NextAction("melee in range", 9.0f),
        NULL);
}

void CasterShamanStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "enemy out of spell",
        NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    // The aggressive too-close variant (fires at <= spellDistance / 2, aggro allowed), as
    // the mage and hunter DPS use -- Elemental had a route that closes distance but nothing
    // outside the sub-20%-health emergency flees that ever reopened it, so a mob that
    // walked in kept the shaman in melee for the rest of the fight. Shaman has no root, so
    // snare first: Frost Shock is what makes the flee open a gap against a mob that has
    // aggro on us. If it is untrained, unaffordable or the snare is already on the target,
    // the ladder falls through to plain flee, which FleeAction::isUseful confines to
    // tooCloseDistance -- in the 7-15 yard band this ladder therefore snares and goes back
    // to casting rather than running. The existing "frost shock snare" route at 21.0f aims
    // the same spell at fleeing targets; the debuff check stops the two double-casting.
    //
    // Known, accepted cost: all three shocks share cooldown Category 19 (Spell.dbc), so
    // each snare cast here spends the cooldown the Earth Shock interrupt at 23.0f
    // (GenericShamanStrategy) also needs. The snare keeps 50.0f anyway. When this trigger
    // fires the bot is being meleed inside 15 yards, and opening a gap outranks an
    // interrupt -- the same priority the relevance gap already encodes, so a cooldown
    // gate would not change which action wins, only add cross-trigger state the framework
    // does not model. And the starvation is partial, not total: the debuff check means
    // the shared 6s cooldown is spent at most once per 8s snare window, and against the
    // common caster-vs-caster fight -- enemy casting from range -- this trigger does not
    // fire at all, leaving the interrupt untouched. Accept the degraded interrupt inside
    // melee range rather than complicate the ladder.
    triggers.push_back(new TriggerNode(
            "enemy too close for spell",
        NextAction::array(0,
            new NextAction("frost shock", 50.0f),
            new NextAction("flee", 49.0f),
            NULL)));

    triggers.push_back(new TriggerNode(
            "shaman weapon",
        NextAction::array(0, new NextAction("flametongue weapon", 23.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "searing totem",
        NextAction::array(0, new NextAction("searing totem", 19.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "shock",
        NextAction::array(0, new NextAction("earth shock", 20.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "frost shock snare",
        NextAction::array(0, new NextAction("frost shock", 21.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe",
        NextAction::array(0, new NextAction("flametongue totem", ACTION_LIGHT_HEAL), NULL)));
}

void CasterAoeShamanStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    MeleeAoeShamanStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "light aoe",
        NextAction::array(0, new NextAction("chain lightning", 25.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe",
        NextAction::array(0, new NextAction("thunderstorm", 26.0f), NULL)));
}
