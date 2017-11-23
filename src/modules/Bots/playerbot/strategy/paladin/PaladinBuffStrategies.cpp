#include "botpch.h"
#include "../../playerbot.h"
#include "PaladinMultipliers.h"
#include "PaladinBuffStrategies.h"

using namespace ai;

void PaladinBuffManaStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "seal",
        NextAction::array(0, new NextAction("seal of wisdom", 90.0f), nullptr)));
}

void PaladinBuffHealthStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "seal",
        NextAction::array(0, new NextAction("seal of light", 90.0f), nullptr)));
}

void PaladinBuffSpeedStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "crusader aura",
        NextAction::array(0, new NextAction("crusader aura", 40.0f), nullptr)));
}

void PaladinBuffDpsStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "seal",
        NextAction::array(0, new NextAction("seal of vengeance", 89.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "retribution aura",
        NextAction::array(0, new NextAction("retribution aura", 90.0f), nullptr)));
}

void PaladinShadowResistanceStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "shadow resistance aura",
        NextAction::array(0, new NextAction("shadow resistance aura", 90.0f), nullptr)));
}

void PaladinFrostResistanceStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "frost resistance aura",
        NextAction::array(0, new NextAction("frost resistance aura", 90.0f), nullptr)));
}

void PaladinFireResistanceStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "fire resistance aura",
        NextAction::array(0, new NextAction("fire resistance aura", 90.0f), nullptr)));
}


void PaladinBuffArmorStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "seal",
        NextAction::array(0, new NextAction("seal of light", 89.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "devotion aura",
        NextAction::array(0, new NextAction("devotion aura", 90.0f), nullptr)));
}

