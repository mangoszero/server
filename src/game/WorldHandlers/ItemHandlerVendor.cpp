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
 * @brief Sells an item stack to a vendor.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSellItemOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SELL_ITEM");

    ObjectGuid vendorGuid;
    ObjectGuid itemGuid;
    uint8 _count;

    recv_data >> vendorGuid;
    recv_data >> itemGuid;
    recv_data >> _count;

    // prevent possible overflow, as mangos uses uint32 for item count
    uint32 count = _count;

    if (!itemGuid)
    {
        return;
    }

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleSellItemOpcode - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        _player->SendSellError(SELL_ERR_CANT_FIND_VENDOR, NULL, itemGuid, 0);
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    Item* pItem = _player->GetItemByGuid(itemGuid);
    if (pItem)
    {
        // prevent sell not owner item
        if (_player->GetObjectGuid() != pItem->GetOwnerGuid())
        {
            _player->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        // prevent sell non empty bag by drag-and-drop at vendor's item list
        if (pItem->IsBag() && !((Bag*)pItem)->IsEmpty())
        {
            _player->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        // prevent sell currently looted item
        if (_player->GetLootGuid() == pItem->GetObjectGuid())
        {
            _player->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            return;
        }

        // special case at auto sell (sell all)
        if (count == 0)
        {
            count = pItem->GetCount();
        }
        else
        {
            // prevent sell more items that exist in stack (possible only not from client)
            if (count > pItem->GetCount())
            {
                _player->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
                return;
            }
        }

        ItemPrototype const* pProto = pItem->GetProto();
        if (pProto)
        {
            if (pProto->SellPrice > 0)
            {
                if (count < pItem->GetCount())              // need split items
                {
                    Item* pNewItem = pItem->CloneItem(count, _player);
                    if (!pNewItem)
                    {
                        sLog.outError("WORLD: HandleSellItemOpcode - could not create clone of item %u; count = %u", pItem->GetEntry(), count);
                        _player->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
                        return;
                    }

                    pItem->SetCount(pItem->GetCount() - count);
                    _player->ItemRemovedQuestCheck(pItem->GetEntry(), count);
                    if (_player->IsInWorld())
                    {
                        pItem->SendCreateUpdateToPlayer(_player);
                    }
                    pItem->SetState(ITEM_CHANGED, _player);

                    _player->AddItemToBuyBackSlot(pNewItem);
                    if (_player->IsInWorld())
                    {
                        pNewItem->SendCreateUpdateToPlayer(_player);
                    }
                }
                else
                {
                    _player->ItemRemovedQuestCheck(pItem->GetEntry(), pItem->GetCount());
                    _player->RemoveItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
                    pItem->RemoveFromUpdateQueueOf(_player);
                    _player->AddItemToBuyBackSlot(pItem);
                }

                uint32 money = pProto->SellPrice * count;

                _player->ModifyMoney(money);
            }
            else
            {
                _player->SendSellError(SELL_ERR_CANT_SELL_ITEM, pCreature, itemGuid, 0);
            }
            return;
        }
    }
    _player->SendSellError(SELL_ERR_CANT_FIND_ITEM, pCreature, itemGuid, 0);
    return;
}

/**
 * @brief Buys back a previously sold item.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleBuybackItem(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BUYBACK_ITEM");
    ObjectGuid vendorGuid;
    uint32 slot;

    recv_data >> vendorGuid >> slot;

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_VENDOR);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandleBuybackItem - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        _player->SendSellError(SELL_ERR_CANT_FIND_VENDOR, NULL, ObjectGuid(), 0);
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    Item* pItem = _player->GetItemFromBuyBackSlot(slot);
    if (pItem)
    {
        uint32 price = _player->GetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + slot - BUYBACK_SLOT_START);
        if (_player->GetMoney() < price)
        {
            _player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, pItem->GetEntry(), 0);
            return;
        }

        ItemPosCountVec dest;
        InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg == EQUIP_ERR_OK)
        {
            _player->ModifyMoney(-(int32)price);
            _player->RemoveItemFromBuyBackSlot(slot, false);
            _player->ItemAddedQuestCheck(pItem->GetEntry(), pItem->GetCount());
            _player->StoreItem(dest, pItem, true);
        }
        else
        {
            _player->SendEquipError(msg, pItem, NULL);
        }
        return;
    }
    else
    {
        _player->SendBuyError(BUY_ERR_CANT_FIND_ITEM, pCreature, 0, 0);
    }
}

/**
 * @brief Buys an item from a vendor into automatic storage.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleBuyItemOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_BUY_ITEM");
    ObjectGuid vendorGuid;
    uint32 item;
    uint8 count, unk1;

    recv_data >> vendorGuid >> item >> count >> unk1;

    GetPlayer()->BuyItemFromVendor(vendorGuid, item, count, NULL_BAG, NULL_SLOT);
}

/**
 * @brief Requests the inventory list of a vendor.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleListInventoryOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    if (!GetPlayer()->IsAlive())
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_LIST_INVENTORY");

    SendListInventory(guid);
}

/**
 * @brief Sends the available inventory of a vendor to the client.
 *
 * @param vendorguid The vendor guid.
 */
void WorldSession::SendListInventory(ObjectGuid vendorguid)
{
    DEBUG_LOG("WORLD: Sent SMSG_LIST_INVENTORY");

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorguid, UNIT_NPC_FLAG_VENDOR);

    if (!pCreature)
    {
        DEBUG_LOG("WORLD: SendListInventory - %s not found or you can't interact with him.", vendorguid.GetString().c_str());
        _player->SendSellError(SELL_ERR_CANT_FIND_VENDOR, NULL, ObjectGuid(), 0);
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // Stop the npc if moving
    pCreature->StopMoving();

    VendorItemData const* vItems = pCreature->GetVendorItems();
    VendorItemData const* tItems = pCreature->GetVendorTemplateItems();

    if (!vItems && !tItems)
    {
        WorldPacket data(SMSG_LIST_INVENTORY, (8 + 1 + 1));
        data << ObjectGuid(vendorguid);
        data << uint8(0);                                   // count==0, next will be error code
        data << uint8(0);                                   // "Vendor has no inventory"
        SendPacket(&data);
        return;
    }

    uint8 customitems = vItems ? vItems->GetItemCount() : 0;
    uint8 numitems = customitems + (tItems ? tItems->GetItemCount() : 0);

    uint8 count = 0;

    WorldPacket data(SMSG_LIST_INVENTORY, (8 + 1 + numitems * 7 * 4));
    data << ObjectGuid(vendorguid);

    size_t count_pos = data.wpos();
    data << uint8(count);

    float discountMod = _player->GetReputationPriceDiscount(pCreature);

    for (int i = 0; i < numitems; ++i)
    {
        VendorItem const* crItem = i < customitems ? vItems->GetItem(i) : tItems->GetItem(i - customitems);

        if (crItem)
        {
            uint32 itemId = crItem->item;
            ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(itemId);
            if (pProto)
            {
                if (!_player->isGameMaster())
                {
                    // class wrong item skip only for bindable case
                    if ((pProto->AllowableClass & _player->getClassMask()) == 0 && pProto->Bonding == BIND_WHEN_PICKED_UP)
                    {
                        continue;
                    }

                    // race wrong item skip always
                    if ((pProto->AllowableRace & _player->getRaceMask()) == 0)
                    {
                        continue;
                    }

                    // when no faction required but rank > 0 will be used faction id from the vendor faction template to compare the rank
                    if (!pProto->RequiredReputationFaction && pProto->RequiredReputationRank > 0 &&
                        ReputationRank(pProto->RequiredReputationRank) > _player->GetReputationRank(pCreature->getFactionTemplateEntry()->Faction))
                    {
                        continue;
                    }

                    if (crItem->conditionId && !sObjectMgr.IsPlayerMeetToCondition(crItem->conditionId, _player, pCreature->GetMap(), pCreature, CONDITION_FROM_VENDOR))
                    {
                        continue;
                    }
                }

                ++count;

                uint32 price = 0;
                // check if the item to sell is a mount
                switch (itemId)
                {
                    case 1132: // all regular mounts
                    case 2411:
                    case 2414:
                    case 5655:
                    case 5656:
                    case 5665:
                    case 5668:
                    case 5864:
                    case 5872:
                    case 5873:
                    case 8563:
                    case 8588:
                    case 8591:
                    case 8592:
                    case 8595:
                    case 8629:
                    case 8631:
                    case 8632:
                    case 12325:
                    case 12326:
                    case 12327:
                    case 13321:
                    case 13322:
                    case 13331:
                    case 13332:
                    case 13333:
                    case 15277:
                    case 15290:
                    case 18241:
                    case 18242:
                    case 18243:
                    case 18244:
                    case 18245:
                    case 18246:
                    case 18247:
                    case 18248:
                        // apply discount for regular mount and set price
                        price = uint32(floor(AccountTypes(sWorld.getConfig(CONFIG_UINT32_MOUNT_COST)) * discountMod));
                        break;
                    case 12302: // all epic mounts
                    case 12303:
                    case 12330:
                    case 12351:
                    case 12353:
                    case 12354:
                    case 13086:
                    case 13326:
                    case 13327:
                    case 13328:
                    case 13329:
                    case 13334:
                    case 13335:
                    case 18766:
                    case 18767:
                    case 18768:
                    case 18772:
                    case 18773:
                    case 18774:
                    case 18776:
                    case 18777:
                    case 18778:
                    case 18785:
                    case 18786:
                    case 18787:
                    case 18788:
                    case 18789:
                    case 18790:
                    case 18791:
                    case 18793:
                    case 18794:
                    case 18795:
                    case 18796:
                    case 18797:
                    case 18798:
                    case 18902:
                        // apply discount for epic mount and set price
                        price = uint32(floor(AccountTypes(sWorld.getConfig(CONFIG_UINT32_EPIC_MOUNT_COST)) * discountMod));
                        break;
                    default:
                        // any other items
                        price = uint32(floor(pProto->BuyPrice * discountMod));
                        break;
                }

                data << uint32(count);
                data << uint32(itemId);
                data << uint32(pProto->DisplayInfoID);
                data << uint32(crItem->maxcount <= 0 ? 0xFFFFFFFF : pCreature->GetVendorItemCurrentCount(crItem));
                data << uint32(price);
                data << uint32(pProto->MaxDurability);
                data << uint32(pProto->BuyCount);
            }
        }
    }

    if (count == 0)
    {
        data << uint8(0);                                   // "Vendor has no inventory"
        SendPacket(&data);
        return;
    }

    data.put<uint8>(count_pos, count);
    SendPacket(&data);
}

/**
 * @brief Auto-stores an item into a destination bag.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleAutoStoreBagItemOpcode(WorldPacket& recv_data)
{
    // DEBUG_LOG("WORLD: CMSG_AUTOSTORE_BAG_ITEM");
    uint8 srcbag, srcslot, dstbag;

    recv_data >> srcbag >> srcslot >> dstbag;
    // DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u, dstbag = %u", srcbag, srcslot, dstbag);

    Item* pItem = _player->GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    if (!_player->IsValidPos(dstbag, NULL_SLOT, false))     // can be autostore pos
    {
        _player->SendEquipError(EQUIP_ERR_ITEM_DOESNT_GO_TO_SLOT, NULL, NULL);
        return;
    }

    uint16 src = pItem->GetPos();

    // check unequip potability for equipped items and bank bags
    if (_player->IsEquipmentPos(src) || _player->IsBagPos(src))
    {
        InventoryResult msg = _player->CanUnequipItem(src, !_player->IsBagPos(src));
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, pItem, NULL);
            return;
        }
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreItem(dstbag, NULL_SLOT, dest, pItem, false);
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, pItem, NULL);
        return;
    }

    // no-op: placed in same slot
    if (dest.size() == 1 && dest[0].pos == src)
    {
        // just remove gray item state
        _player->SendEquipError(EQUIP_ERR_NONE, pItem, NULL);
        return;
    }

    _player->RemoveItem(srcbag, srcslot, true);
    _player->StoreItem(dest, pItem, true);
}

/**
 * @brief Verifies that a guid can be used as a banker interaction target.
 *
 * @param guid The banker or player guid.
 * @return true if banking is allowed; otherwise false.
 */
bool WorldSession::CheckBanker(ObjectGuid guid)
{
    // GM case
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!ChatHandler(GetPlayer()).FindCommand("bank"))
        {
            DEBUG_LOG("%s attempt open bank in cheating way.", guid.GetString().c_str());
            return false;
        }
    }
    // banker case
    else
    {
        if (!GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_BANKER))
        {
            DEBUG_LOG("Banker %s not found or you can't interact with him.", guid.GetString().c_str());
            return false;
        }
    }

    return true;
}

/**
 * @brief Purchases the next available bank bag slot.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleBuyBankSlotOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_BUY_BANK_SLOT");

    ObjectGuid guid;
    recvPacket >> guid;

    WorldPacket data(SMSG_BUY_BANK_SLOT_RESULT, 4);

    if (!CheckBanker(guid))
    {
        data << uint32(ERR_BANKSLOT_NOTBANKER);
        SendPacket(&data);
        return;
    }

    uint32 slot = _player->GetBankBagSlotCount();

    // next slot
    ++slot;

    DETAIL_LOG("PLAYER: Buy bank bag slot, slot number = %u", slot);

    BankBagSlotPricesEntry const* slotEntry = sBankBagSlotPricesStore.LookupEntry(slot);

    if (!slotEntry)
    {
        data << uint32(ERR_BANKSLOT_FAILED_TOO_MANY);
        SendPacket(&data);
        return;
    }

    uint32 price = slotEntry->Price;

    if (_player->GetMoney() < price)
    {
        data << uint32(ERR_BANKSLOT_INSUFFICIENT_FUNDS);
        SendPacket(&data);
        return;
    }

    _player->SetBankBagSlotCount(slot);
    _player->ModifyMoney(-int32(price));
}

/**
 * @brief Moves an item from inventory into the bank automatically.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleAutoBankItemOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOBANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item* pItem = _player->GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, pItem, NULL);
        return;
    }

    // no-op: placed in same slot
    if (dest.size() == 1 && dest[0].pos == pItem->GetPos())
    {
        // just remove gray item state
        _player->SendEquipError(EQUIP_ERR_NONE, pItem, NULL);
        return;
    }

    _player->RemoveItem(srcbag, srcslot, true);
    _player->BankItem(dest, pItem, true);
}

/**
 * @brief Moves an item between bank and inventory automatically.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleAutoStoreBankItemOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: CMSG_AUTOSTORE_BANK_ITEM");
    uint8 srcbag, srcslot;

    recvPacket >> srcbag >> srcslot;
    DEBUG_LOG("STORAGE: receive srcbag = %u, srcslot = %u", srcbag, srcslot);

    Item* pItem = _player->GetItemByPos(srcbag, srcslot);
    if (!pItem)
    {
        return;
    }

    if (_player->IsBankPos(srcbag, srcslot))                // moving from bank to inventory
    {
        ItemPosCountVec dest;
        InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, pItem, NULL);
            return;
        }

        _player->RemoveItem(srcbag, srcslot, true);
        _player->StoreItem(dest, pItem, true);
    }
    else                                                    // moving from inventory to bank
    {
        ItemPosCountVec dest;
        InventoryResult msg = _player->CanBankItem(NULL_BAG, NULL_SLOT, dest, pItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            _player->SendEquipError(msg, pItem, NULL);
            return;
        }

        _player->RemoveItem(srcbag, srcslot, true);
        _player->BankItem(dest, pItem, true);
    }
}

/**
 * @brief Sets or clears the player's equipped ammunition.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleSetAmmoOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->IsAlive())
    {
        GetPlayer()->SendEquipError(EQUIP_ERR_YOU_ARE_DEAD, NULL, NULL);
        return;
    }

    DEBUG_LOG("WORLD: CMSG_SET_AMMO");
    uint32 item;

    recv_data >> item;

    if (!item)
    {
        GetPlayer()->RemoveAmmo();
    }
    else
    {
        GetPlayer()->SetAmmo(item);
    }
}
