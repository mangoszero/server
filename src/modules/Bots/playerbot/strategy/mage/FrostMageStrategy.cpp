#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "MageMultipliers.h"
#include "FrostMageStrategy.h"

using namespace ai;

FrostMageStrategy::FrostMageStrategy(PlayerbotAI* ai) : GenericMageStrategy(ai)
{}

NextAction** FrostMageStrategy::getDefaultActions()
{
    // Frostbolt is level 4, and its fallback chain ends in wand-dependent "shoot", so a frost
    // mage at levels 1-3 had nothing it could cast. Fireball is trainable by every mage at
    // level 1 and covers that gap; melee is the last resort for the wandless, out-of-mana or
    // already-in-melee case. Frostbolt stays on top so the spec plays as frost the moment it
    // has it.
    //
    // "shoot" sits between fireball and melee, as it does in the arcane array. Moving the
    // frostbolt node's alternative off shoot and onto fireball left this array as the ONLY
    // route to shoot, and it had none -- so an out-of-mana frost mage with an equipped wand
    // and a ranged target failed frostbolt, failed fireball, declined melee-in-range on
    // distance, and executed nothing at all.
    return NextAction::array(0,
        new NextAction("frostbolt", 8.0f),
        new NextAction("fireball", 7.0f),
        new NextAction("shoot", 6.0f),
        new NextAction("melee in range", 5.0f),
        NULL);
}

void FrostMageStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericMageStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "icy veins",
        NextAction::array(0, new NextAction("icy veins", 50.0f), NULL)));
}

void FrostMageAoeStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "high aoe",
        NextAction::array(0, new NextAction("blizzard", 40.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe",
        NextAction::array(0, new NextAction("cone of cold", 30.0f), NULL)));
}
