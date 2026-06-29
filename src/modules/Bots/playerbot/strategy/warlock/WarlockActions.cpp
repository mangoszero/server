#include "botpch.h"
#include "../../playerbot.h"
#include "WarlockActions.h"
#include "../actions/InventoryAction.h"

using namespace ai;

bool CastCreateSpellstoneAction::isUseful()
{
    FindNamedItemVisitor visitor("spellstone");
    return CastBuffSpellAction::isUseful() &&
           InventoryAction::FindPlayerItem(bot, &visitor) == nullptr &&
           AI_VALUE2(uint8, "item count", "soul shard") > 1 &&
           !bot->IsTwoHandUsed() &&
           bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) == nullptr;
}

bool CastCreateFirestoneAction::isUseful()
{
    FindNamedItemVisitor visitor("firestone");
    return CastBuffSpellAction::isUseful() &&
           InventoryAction::FindPlayerItem(bot, &visitor) == nullptr &&
           AI_VALUE2(uint8, "item count", "soul shard") > 1 &&
           !bot->IsTwoHandUsed() &&
           bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) == nullptr;
}

bool EquipSpellstoneAction::isUseful()
{
    FindNamedItemVisitor visitor("spellstone");
    return InventoryAction::FindPlayerItem(bot, &visitor) != nullptr &&
           !bot->IsTwoHandUsed() &&
           bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) == nullptr;
}

bool EquipSpellstoneAction::Execute(Event event)
{
    FindNamedItemVisitor visitor("spellstone");
    Item* item = InventoryAction::FindPlayerItem(bot, &visitor);
    if (!item ||
        bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) != nullptr ||
        bot->IsTwoHandUsed())
    {
        return false;
    }

    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();

    WorldPacket* const packet = new WorldPacket(CMSG_AUTOEQUIP_ITEM, 2);
    *packet << bagIndex << slot;
    bot->GetSession()->QueuePacket(packet);
    return true;
}

bool CastRainOfFireAction::isUseful()
{
    if (!CastSpellAction::isUseful())
    {
        return false;
    }
    if (!ai->HasStrategy("cautious"))
    {
        return true;
    }
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || ai->HasNonCombatantInRange(8.0f,
        target->GetPositionX(), target->GetPositionY(), target->GetPositionZ()))
    {
        return false;
    }
    return true;
}
