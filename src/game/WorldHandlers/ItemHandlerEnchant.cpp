/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file ItemHandler.cpp
 * @brief Item inventory and interaction opcode handlers
 *
 * This file handles item-related opcodes including:
 * - CMSG_SPLIT_ITEM: Split item stack
 * - CMSG_SWAP_ITEM: Swap items between inventory slots
 * - CMSG_SWAP_INV_ITEM: Swap inventory items
 * - CMSG_DESTROYITEM: Destroy item
 * - CMSG_AUTOEQUIP_ITEM: Auto-equip item
 * - CMSG_ITEM_NAME_QUERY: Query item name
 * - CMSG_READ_ITEM: Read item (books, scrolls)
 * - CMSG_WRAP_ITEM: Wrap item with gift wrap
 * - CMSG_USE_ITEM: Use item (consume, equip, etc.)
 * - CMSG_OPEN_ITEM: Open item (containers)
 * - CMSG_BUY_ITEM: Buy item from vendor
 * - CMSG_SELL_ITEM: Sell item to vendor
 * - CMSG_REPAIR_ITEM: Repair item
 */



#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Item.h"
#include "UpdateData.h"
#include "Chat.h"
#include "World.h"

/**
 * @brief Sends an enchantment log packet to the client.
 *
 * @param targetGuid The enchanted target guid.
 * @param casterGuid The caster guid.
 * @param itemId The item entry id.
 * @param spellId The enchantment spell id.
 */
void WorldSession::SendEnchantmentLog(ObjectGuid targetGuid, ObjectGuid casterGuid, uint32 itemId, uint32 spellId)
{
    WorldPacket data(SMSG_ENCHANTMENTLOG, (8 + 8 + 4 + 4 + 1)); // last check 2.0.10
    data << ObjectGuid(targetGuid);
    data << ObjectGuid(casterGuid);
    data << uint32(itemId);
    data << uint32(spellId);
    data << uint8(0);
    SendPacket(&data);
}

/**
 * @brief Sends a temporary enchantment timer update.
 *
 * @param playerGuid The owning player guid.
 * @param itemGuid The enchanted item guid.
 * @param slot The equipment slot index.
 * @param duration The remaining duration in milliseconds.
 */
void WorldSession::SendItemEnchantTimeUpdate(ObjectGuid playerGuid, ObjectGuid itemGuid, uint32 slot, uint32 duration)
{
    // last check 2.0.10
    WorldPacket data(SMSG_ITEM_ENCHANT_TIME_UPDATE, (8 + 4 + 4 + 8));
    data << ObjectGuid(itemGuid);
    data << uint32(slot);
    data << uint32(duration);
    data << ObjectGuid(playerGuid);
    SendPacket(&data);
}

/**
 * @brief Wraps an item using wrapping paper.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleWrapItemOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_WRAP_ITEM");

    uint8 gift_bag, gift_slot, item_bag, item_slot;
    // recv_data.hexlike();

    recv_data >> gift_bag >> gift_slot;                     // paper
    recv_data >> item_bag >> item_slot;                     // item

    DEBUG_LOG("WRAP: receive gift_bag = %u, gift_slot = %u, item_bag = %u, item_slot = %u", gift_bag, gift_slot, item_bag, item_slot);

    Item* gift = _player->GetItemByPos(gift_bag, gift_slot);
    if (!gift)
    {
        _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL);
        return;
    }

    // cheating: non-wrapper wrapper (all empty wrappers is stackable)
    if (!(gift->GetProto()->Flags & ITEM_FLAG_WRAPPER) || gift->GetMaxStackCount() == 1)
    {
        _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, gift, NULL);
        return;
    }

    Item* item = _player->GetItemByPos(item_bag, item_slot);

    if (!item)
    {
        _player->SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, item, NULL);
        return;
    }

    if (item == gift)                                       // not possible with packet from real client
    {
        _player->SendEquipError(EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->IsEquipped())
    {
        _player->SendEquipError(EQUIP_ERR_EQUIPPED_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    // HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED)
    if (item->GetGuidValue(ITEM_FIELD_GIFTCREATOR))
    {
        _player->SendEquipError(EQUIP_ERR_WRAPPED_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->IsBag())
    {
        _player->SendEquipError(EQUIP_ERR_BAGS_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->IsSoulBound())
    {
        _player->SendEquipError(EQUIP_ERR_BOUND_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    if (item->GetMaxStackCount() != 1)
    {
        _player->SendEquipError(EQUIP_ERR_STACKABLE_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    // maybe not correct check  (it is better than nothing)
    if (item->GetProto()->MaxCount > 0)
    {
        _player->SendEquipError(EQUIP_ERR_UNIQUE_CANT_BE_WRAPPED, item, NULL);
        return;
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("INSERT INTO `character_gifts` VALUES ('%u', '%u', '%u', '%u')", item->GetOwnerGuid().GetCounter(), item->GetGUIDLow(), item->GetEntry(), item->GetUInt32Value(ITEM_FIELD_FLAGS));
    item->SetEntry(gift->GetEntry());

    switch (item->GetEntry())
    {
        case 5042:  item->SetEntry(5043); break;
        case 5048:  item->SetEntry(5044); break;
        case 17303: item->SetEntry(17302); break;
        case 17304: item->SetEntry(17305); break;
        case 17307: item->SetEntry(17308); break;
        case 21830: item->SetEntry(21831); break;
    }
    item->SetGuidValue(ITEM_FIELD_GIFTCREATOR, _player->GetObjectGuid());
    item->SetUInt32Value(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
    item->SetState(ITEM_CHANGED, _player);

    if (item->GetState() == ITEM_NEW)                       // save new item, to have alway for `character_gifts` record in `item_instance`
    {
        // after save it will be impossible to remove the item from the queue
        item->RemoveFromUpdateQueueOf(_player);
        item->SaveToDB();                                   // item gave inventory record unchanged and can be save standalone
    }
    CharacterDatabase.CommitTransaction();

    uint32 count = 1;
    _player->DestroyItemCount(gift, count, true);
}

/**
 * @brief Cancels a temporary weapon enchantment.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleCancelTempEnchantmentOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: CMSG_CANCEL_TEMP_ENCHANTMENT");

    uint32 eslot;

    recv_data >> eslot;

    // apply only to equipped item
    if (!Player::IsEquipmentPos(INVENTORY_SLOT_BAG_0, eslot))
    {
        return;
    }

    Item* item = GetPlayer()->GetItemByPos(INVENTORY_SLOT_BAG_0, eslot);

    if (!item)
    {
        return;
    }

    if (!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
    {
        return;
    }

    GetPlayer()->ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, false);
    item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
}
