#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "MageMultipliers.h"
#include "FireMageStrategy.h"

using namespace ai;

NextAction** FireMageStrategy::getDefaultActions()
{
    // Scorch is level 22; Fireball is level 1. The array below is walked in relevance
    // order, but Scorch's ActionNode alternative was "shoot", pushed at relevance + 0.03 --
    // so a fire mage that could not yet cast Scorch preferred a WAND over its own starter
    // spell, and a wandless one only reached Fireball after shoot had failed. The node's
    // alternative is now Fireball (see GenericMageStrategy), and shoot sits here below it
    // for a mage that has a wand and no mana, with melee as the last resort.
    return NextAction::array(0,
        new NextAction("scorch", 8.0f),
        new NextAction("fireball", 7.0f),
        new NextAction("fire blast", 6.0f),
        new NextAction("shoot", 5.0f),
        new NextAction("melee in range", 4.0f),
        NULL);
}

void FireMageStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericMageStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "pyroblast",
        NextAction::array(0, new NextAction("pyroblast", 10.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "hot streak",
        NextAction::array(0, new NextAction("pyroblast", 25.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "combustion",
        NextAction::array(0, new NextAction("combustion", 50.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "enemy too close for spell",
        NextAction::array(0, new NextAction("dragon's breath", 70.0f), NULL)));
}

void FireMageAoeStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "medium aoe",
        NextAction::array(0, new NextAction("flamestrike", 20.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "living bomb",
        NextAction::array(0, new NextAction("living bomb", 25.0f), NULL)));
}

