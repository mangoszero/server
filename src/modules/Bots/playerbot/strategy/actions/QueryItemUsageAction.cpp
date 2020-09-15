#include "botpch.h"
#include "playerbot.h"
#include "ahbot/AhBot.h"
#include "QueryItemUsageAction.h"

#include "../values/ItemUsageValue.h"
#include "../../RandomPlayerbotMgr.h"


using namespace ai;


bool QueryItemUsageAction::Execute(Event event)
{
    WorldPacket& data = event.getPacket();
    if (!data.empty())
    {
        data.rpos(0);

        ObjectGuid guid;
        data >> guid;
        if (guid != bot->GetObjectGuid())
        {
            return false;
        }

        uint32 received, created, isShowChatMessage, notUsed, itemId,
            suffixFactor, itemRandomPropertyId, count, invCount;
        uint8 bagSlot;

        data >> received;                               // 0=looted, 1=from npc
        data >> created;                                // 0=received, 1=created
        data >> isShowChatMessage;                                      // IsShowChatMessage
        data >> bagSlot;
                                                                // item slot, but when added to stack: 0xFFFFFFFF
        data >> notUsed;
        data >> itemId;
        data >> suffixFactor;
        data >> itemRandomPropertyId;
        data >> count;

        ItemPrototype const *item = sItemStorage.LookupEntry<ItemPrototype>(itemId);
        if (!item)
        {
            return false;
        }

        ostringstream out; out << chat->formatItem(item, count);
        if (created)
        {
            out << " created";
        }
        else if (received)
        {
            out << " received";
        }
        ai->TellMaster(out);

        QueryItemUsage(item);
        QueryQuestItem(itemId);
        return true;
    }

    string text = event.getParam();

    ItemIds items = chat->parseItems(text);
    QueryItemsUsage(items);
    return true;
}

bool QueryItemUsageAction::QueryItemUsage(ItemPrototype const *item)
{
    ostringstream out; out << item->ItemId;
    ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", out.str());
    switch (usage)
    {
    case ITEM_USAGE_EQUIP:
        ai->TellMaster("Equip");
        return true;
    case ITEM_USAGE_REPLACE:
        ai->TellMaster("Equip (replace)");
        return true;
    case ITEM_USAGE_SKILL:
        ai->TellMaster("Tradeskill");
        return true;
    case ITEM_USAGE_USE:
        ai->TellMaster("Use");
        return true;
    }

    return false;
}

void QueryItemUsageAction::QueryItemPrice(ItemPrototype const *item)
{
    if (!sRandomPlayerbotMgr.IsRandomBot(bot))
    {
        return;
    }

    if (item->Bonding == BIND_WHEN_PICKED_UP)
    {
        return;
    }

    list<Item*> items = InventoryAction::parseItems(item->Name1);
    if (!items.empty())
    {
        for (list<Item*>::iterator i = items.begin(); i != items.end(); ++i)
        {
            Item* sell = *i;
            int32 sellPrice = sell->GetCount() * auctionbot.GetSellPrice(sell->GetProto()) * sRandomPlayerbotMgr.GetSellMultiplier(bot);
            ostringstream out;
            out << "Selling " << chat->formatItem(sell->GetProto(), sell->GetCount()) << " for " << chat->formatMoney(sellPrice);
            ai->TellMaster(out.str());
        }
    }

    ostringstream out; out << item->ItemId;
    ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", out.str());
    if (usage == ITEM_USAGE_NONE)
    {
        return;
    }

    int32 buyPrice = auctionbot.GetBuyPrice(item) * sRandomPlayerbotMgr.GetBuyMultiplier(bot);
    if (buyPrice)
    {
        ostringstream out;
        out << "Will buy for " << chat->formatMoney(buyPrice);
        ai->TellMaster(out.str());
    }
}

void QueryItemUsageAction::QueryItemsUsage(ItemIds items)
{
    for (ItemIds::iterator i = items.begin(); i != items.end(); i++)
    {
        ItemPrototype const *item = sItemStorage.LookupEntry<ItemPrototype>(*i);
        QueryItemUsage(item);
        QueryQuestItem(*i);
        QueryItemPrice(item);
    }
}

void QueryItemUsageAction::QueryQuestItem(uint32 itemId)
{
    Player *bot = ai->GetBot();
    QuestStatusMap& questMap = bot->getQuestStatusMap();
    for (QuestStatusMap::const_iterator i = questMap.begin(); i != questMap.end(); i++)
    {
        const Quest *questTemplate = sObjectMgr.GetQuestTemplate( i->first );
        if( !questTemplate )
        {
            continue;
        }

        uint32 questId = questTemplate->GetQuestId();
        QuestStatus status = bot->GetQuestStatus(questId);
        if (status == QUEST_STATUS_INCOMPLETE || (status == QUEST_STATE_COMPLETE && !bot->GetQuestRewardStatus(questId)))
        {
            QuestStatusData const& questStatus = i->second;
            QueryQuestItem(itemId, questTemplate, &questStatus);
        }
    }
}


void QueryItemUsageAction::QueryQuestItem(uint32 itemId, const Quest *questTemplate, const QuestStatusData *questStatus)
{
    for (int i = 0; i < QUEST_OBJECTIVES_COUNT; i++)
    {
        if (questTemplate->ReqItemId[i] != itemId)
        {
            continue;
        }

        int required = questTemplate->ReqItemCount[i];
        int available = questStatus->m_itemcount[i];

        if (!required)
        {
            continue;
        }

        ai->TellMaster(chat->formatQuestObjective(chat->formatQuest(questTemplate), available, required));
    }
}

