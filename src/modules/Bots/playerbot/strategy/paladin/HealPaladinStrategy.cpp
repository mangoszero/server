#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "PaladinMultipliers.h"
#include "HealPaladinStrategy.h"

using namespace ai;

/**
 * @brief What a Holy paladin does when nothing needs healing.
 *
 * A priest falls back to its wand here; a 1.12 paladin has no ranged attack at
 * all, so melee is the only honest answer. It is also the correct one -- a vanilla
 * Holy paladin stands in melee between casts rather than idling at range.
 */
NextAction** HealPaladinStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("melee", 10.0f), NULL);
}

void HealPaladinStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    // The base already binds medium/low/critical health for self and party to flash of
    // light, holy light and lay on hands, plus the three cleanses. Inheriting it keeps
    // one definition of those priorities instead of a second copy that drifts.
    GenericPaladinStrategy::InitTriggers(triggers);

    // Same binding the druid, priest and shaman healers use. Range to the heal target is
    // handled inside HealPartyMemberAction, which resolves "party member to heal" itself,
    // so this only needs to cover closing on the enemy like every other caster strategy.
    triggers.push_back(new TriggerNode(
            "enemy out of spell",
        NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    // Topping up before the target is actually in trouble is the difference between a
    // healer and a class that happens to own heal spells. Flash of Light is the efficient
    // choice here; Holy Light stays reserved for the low-health bindings in the base.
    triggers.push_back(new TriggerNode(
            "almost full health",
        NextAction::array(0, new NextAction("flash of light", 12.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member almost full health",
        NextAction::array(0, new NextAction("flash of light on party", 11.0f), NULL)));

    // Being attacked is a healer's real emergency: casts get pushed back and the group
    // stops receiving heals. Divine Protection buys the cast; Divine Shield is the last
    // resort. Both exist in 1.12 and both are already registered in the paladin context.
    triggers.push_back(new TriggerNode(
            "has attackers",
        NextAction::array(0, new NextAction("divine protection", 51.0f), new NextAction("divine shield", 50.0f), NULL)));

    // Mana is the healer's ammunition, so drinking matters more here than it does for a
    // damage build that can just swing.
    triggers.push_back(new TriggerNode(
            "medium mana",
        NextAction::array(0, new NextAction("mana potion", ACTION_EMERGENCY + 4), NULL)));

    // Bringing someone back is worth more than any single heal once the fight allows it.
    triggers.push_back(new TriggerNode(
            "party member dead",
        NextAction::array(0, new NextAction("redemption", ACTION_CRITICAL_HEAL + 3), NULL)));
}
