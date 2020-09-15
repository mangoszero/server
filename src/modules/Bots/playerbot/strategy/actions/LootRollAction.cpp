#include "botpch.h"
#include "../../playerbot.h"
#include "LootRollAction.h"


using namespace ai;

bool LootRollAction::Execute(Event event)
{
    Player *bot = QueryItemUsageAction::ai->GetBot();

    WorldPacket p(event.getPacket()); //WorldPacket packet for CMSG_LOOT_ROLL, (8+4+1)
    ObjectGuid guid;
    uint32 slot;
    uint8 rollType;
    p.rpos(0); //reset packet pointer
    p >> guid; //guid of the item rolled
    p >> slot; //number of players invited to roll
    p >> rollType; //need,greed or pass on roll

    Group* group = bot->GetGroup();
    if(!group)
    {
        return false;
    }

    RollVote vote = ROLL_PASS;

    ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(guid.GetEntry());
    if(proto)
    {
        switch (proto->Class)
        {
        case ITEM_CLASS_WEAPON:
        case ITEM_CLASS_ARMOR:
            if (QueryItemUsage(proto))
            {
                vote = ROLL_NEED;
            }
            break;
        default:
            if (IsLootAllowed(guid.GetEntry()))
            {
                vote = ROLL_NEED;
            }
            break;
        }
    }

    switch (group->GetLootMethod())
    {
    case MASTER_LOOT:
    case FREE_FOR_ALL:
        group->CountRollVote(bot, guid, slot, ROLL_PASS);
        break;
    default:
        group->CountRollVote(bot, guid, slot, vote);
        break;
    }

    return true;
}
