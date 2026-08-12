#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "PriestMultipliers.h"
#include "HealPriestStrategy.h"

using namespace ai;

NextAction** HealPriestStrategy::getDefaultActions()
{
    // "shoot" is not a floor: it needs an equipped wand, and NONE of the ten stock priest
    // race/sex starting outfits contains one. A healing priest was therefore left with no
    // unconditional action at all -- its healing and utility triggers are all conditional --
    // so a wandless one simply stood in combat doing nothing, at any level from 1 to 60.
    // Smite is trainable at level 1 by every priest and needs no equipment; melee is the last
    // resort that always works. Shoot stays on top so a priest that DOES have a wand still
    // prefers it.
    return NextAction::array(0,
        new NextAction("shoot", 10.0f),
        new NextAction("smite", 9.0f),
        new NextAction("melee in range", 8.0f),
        NULL);
}

void HealPriestStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "enemy out of spell",
        NextAction::array(0, new NextAction("reach spell", ACTION_NORMAL + 9), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe heal",
        NextAction::array(0, new NextAction("circle of healing", 27.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "almost full health",
        NextAction::array(0, new NextAction("renew", 15.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member almost full health",
        NextAction::array(0, new NextAction("renew on party", 10.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "has attackers",
        NextAction::array(0, new NextAction("fade", 50.0f), new NextAction("flee", 49.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "shackle undead",
        NextAction::array(0, new NextAction("shackle undead", 18.0f), NULL)));
}
