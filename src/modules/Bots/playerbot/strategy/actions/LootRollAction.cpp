#include "botpch.h"
#include "../../playerbot.h"
#include "LootRollAction.h"
#include "../values/ItemUsageValue.h"

using namespace ai;

bool LootRollAction::Execute(Event event)
{
    Player *bot = QueryItemUsageAction::ai->GetBot();

    WorldPacket p(event.getPacket());
    ObjectGuid lootTargetGuid;
    uint32 slot;
    uint32 itemid;
    uint32 randomSuffix;
    uint32 itemRandomPropId;
    p.rpos(0);
    p >> lootTargetGuid;
    p >> slot;
    p >> itemid;
    p >> randomSuffix;
    p >> itemRandomPropId;

    Group* group = bot->GetGroup();
    if (!group)
    {
        return false;
    }

    RollVote vote = ROLL_PASS;

    ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(itemid);
    if (proto)
    {
        AiObjectContext* context = QueryItemUsageAction::context;
        ostringstream out; out << itemid;
        ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", out.str());

        switch (proto->Class)
        {
        case ITEM_CLASS_WEAPON:
        case ITEM_CLASS_ARMOR:
            if (usage == ITEM_USAGE_EQUIP || usage == ITEM_USAGE_REPLACE)
                vote = ROLL_NEED;
            else if (bot->CanUseItem(proto) == EQUIP_ERR_OK && proto->Bonding != BIND_WHEN_PICKED_UP)
                vote = ROLL_GREED;
            break;
        default:
            if (usage == ITEM_USAGE_SKILL || usage == ITEM_USAGE_USE)
                vote = ROLL_NEED;
            else if (proto->StartQuest || proto->Bonding == BIND_QUEST_ITEM ||
                     proto->Bonding == BIND_QUEST_ITEM1 || proto->Class == ITEM_CLASS_QUEST)
                vote = ROLL_NEED;
            else if (proto->SellPrice > 0 && proto->Bonding != BIND_WHEN_PICKED_UP)
                vote = ROLL_GREED;
            break;
        }
    }

    switch (group->GetLootMethod())
    {
    case MASTER_LOOT:
    case FREE_FOR_ALL:
        group->CountRollVote(bot, lootTargetGuid, slot, ROLL_PASS);
        break;
    default:
        group->CountRollVote(bot, lootTargetGuid, slot, vote);
        break;
    }

    return true;
}
