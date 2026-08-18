#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "ShamanMultipliers.h"
#include "ShamanNonCombatStrategy.h"

using namespace ai;

void ShamanNonCombatStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    NonCombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "party member dead",
        NextAction::array(0, new NextAction("ancestral spirit", 33.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "water breathing",
        NextAction::array(0, new NextAction("water breathing", 12.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "water walking",
        NextAction::array(0, new NextAction("water walking", 12.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "water breathing on party",
        NextAction::array(0, new NextAction("water breathing on party", 11.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "water walking on party",
        NextAction::array(0, new NextAction("water walking on party", 11.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "critical health",
        NextAction::array(0, new NextAction("healing wave", 70.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member critical health",
        NextAction::array(0, new NextAction("healing wave on party", 60.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe heal",
        NextAction::array(0, new NextAction("chain heal", 27.0f), NULL)));

    // Automatic travel casting is deliberately disabled until Ghost Wolf has a complete
    // travel lifecycle. This also clears persisted auras when an already-wolfed bot logs in.
    triggers.push_back(new TriggerNode(
            "ghost wolf active",
        NextAction::array(0, new NextAction("cancel ghost wolf", ACTION_EMERGENCY + 10), NULL)));

    // The cleanse spirit rungs are WotLK and never resolve; the single-target cures below
    // them are the real 1.12 spells and are what actually fires. The totems are the genuine
    // 1.12 area cleanses and sit last, so a shaman only pays a totem's global cooldown when
    // the direct cure is unavailable.
    triggers.push_back(new TriggerNode(
            "party member cleanse spirit poison",
        NextAction::array(0, new NextAction("cleanse spirit poison on party", 35.0f), new
                NextAction("cure poison on party", 33.0f), new
                NextAction("poison cleansing totem", 31.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member cleanse spirit curse",
        NextAction::array(0, new NextAction("cleanse spirit curse on party", 35.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "party member cleanse spirit disease",
        NextAction::array(0, new NextAction("cleanse spirit disease on party", 35.0f), new
                NextAction("cure disease on party", 33.0f), new
                NextAction("disease cleansing totem", 31.0f), NULL)));
}

void ShamanNonCombatStrategy::InitMultipliers(std::list<Multiplier*> &multipliers)
{
    NonCombatStrategy::InitMultipliers(multipliers);
}
