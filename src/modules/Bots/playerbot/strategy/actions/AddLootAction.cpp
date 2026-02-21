#include "botpch.h"
#include "../../playerbot.h"
#include "AddLootAction.h"

#include "../../LootObjectStack.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

bool AddLootAction::Execute(Event event)
{
    ObjectGuid guid = event.getObject();
    if (!guid)
    {
        return false;
    }

    return AI_VALUE(LootObjectStack*, "available loot")->Add(guid);
}

bool AddAllLootAction::Execute(Event event)
{
    bool added = false;

    list<ObjectGuid> gos = context->GetValue<list<ObjectGuid> >("nearest game objects")->Get();
    for (list<ObjectGuid>::iterator i = gos.begin(); i != gos.end(); i++)
    {
        added |= AddLoot(*i);
    }

    list<ObjectGuid> corpses = context->GetValue<list<ObjectGuid> >("nearest corpses")->Get();
    for (list<ObjectGuid>::iterator i = corpses.begin(); i != corpses.end(); i++)
    {
        added |= AddLoot(*i);
    }

    return added;
}

bool AddLootAction::isUseful()
{
    return AI_VALUE(uint8, "bag space") < 80;
}

bool AddAllLootAction::isUseful()
{
    return AI_VALUE(uint8, "bag space") < 80;
}

bool AddAllLootAction::AddLoot(ObjectGuid guid)
{
    return AI_VALUE(LootObjectStack*, "available loot")->Add(guid);
}

bool AddGatheringLootAction::AddLoot(ObjectGuid guid)
{
    LootObject loot(bot, guid);

    if (loot.IsEmpty() || !loot.GetWorldObject(bot))
    {
        return false;
    }

    if (loot.skillId == SKILL_NONE)
    {
        return false;
    }

    if (!loot.IsLootPossible(bot))
    {
        return false;
    }

    return AddAllLootAction::AddLoot(guid);
}

bool AddGatheringLootAction::isUseful()
{
    // Don't gather in dungeons or raids
    if (bot->GetMap()->IsDungeon())
    {
        return false;
    }

    // NC gathering is a problem if you are supposed to be following
    Player* master = ai->GetMaster();
    if (master && bot->GetGroup())
    {
        float masterDist = bot->GetDistance(master);
        if (masterDist > sPlayerbotAIConfig.reactDistance)
        {
            return false;
        }
    }

    return AddAllLootAction::isUseful();
}
