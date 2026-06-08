#include "botpch.h"
#include "../../playerbot.h"
#include "ReachTargetActions.h"

using namespace ai;

void ReachShootRangeAction::Reset()
{
    context->GetValue<float>("last target position")->Set(0);
    ReachTargetAction::Reset();
}

string ReachShootRangeAction::GetShootSpell(Player *bot)
{
    Item* rangedItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!rangedItem)
    {
        return "";
    }
    uint32 weaponSubClass = rangedItem->GetProto()->SubClass;
    switch (weaponSubClass)
    {
        case ITEM_SUBCLASS_WEAPON_BOW:
            return "shoot bow";
        case ITEM_SUBCLASS_WEAPON_GUN:
            return "shoot gun";
        case ITEM_SUBCLASS_WEAPON_CROSSBOW:
            return "shoot crossbow";
        case ITEM_SUBCLASS_WEAPON_THROWN:
            return "throw";
    }
    return "shoot";
}

bool ReachShootRangeAction::isUseful()
{
    Unit* target = GetTarget();
    if (target == bot)
    {
        target = context->GetValue<Unit*>("old target")->Get();
        context->GetValue<Unit*>("current target")->Set(target);
    }
    if (!target || !target->IsAlive())
    {
        Reset();
        return false;
    }

    if (!ReachTargetAction::isUseful())
    {
        Reset();
        return false;
    }
    float lastDistance = context->GetValue<float>("last target position")->Get();
    const float targetDistance = sPlayerbotAIConfig.sightDistance + sPlayerbotAIConfig.meleeDistance;

    if (lastDistance == 0)
    {
        context->GetValue<float>("last target position")->Set(targetDistance);
    }

    float currentDistance = bot->GetDistance(target);
    if ((currentDistance <= sPlayerbotAIConfig.meleeDistance)
    || (currentDistance > targetDistance))
    {
        Reset();
        return false;
    }

    if (currentDistance < lastDistance)
    {
        context->GetValue<float>("last target position")->Set(currentDistance);
        SetPersistenceStartTime(getMSTime());
    }
    return true;
}
