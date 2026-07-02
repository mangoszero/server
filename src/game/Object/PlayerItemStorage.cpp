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



#include "Player.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "Language.h"
#include "SpellMgr.h"
#include "World.h"
#include "UpdateMask.h"
#include "CinematicFlyover.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectAccessor.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

// Return stored item (if stored to stack, it can diff. from pItem). And pItem ca be deleted in this case.
Item* Player::StoreNewItem(ItemPosCountVec const& dest, uint32 item, bool update, int32 randomPropertyId)
{
    uint32 count = 0;
    for (ItemPosCountVec::const_iterator itr = dest.begin(); itr != dest.end(); ++itr)
    {
        count += itr->count;
    }

    Item* pItem = Item::CreateItem(item, count, this, randomPropertyId);
    if (pItem)
    {
        ItemAddedQuestCheck(item, count);
        pItem = StoreItem(dest, pItem, update);
    }
    return pItem;
}

/**
 * @brief Stores an item into one or more resolved destination positions.
 *
 * @param dest The destination positions and quantities.
 * @param pItem The item to store.
 * @param update True to send world updates for the storage change.
 * @return The final stored item instance.
 */
Item* Player::StoreItem(ItemPosCountVec const& dest, Item* pItem, bool update)
{
    if (!pItem)
    {
        return NULL;
    }

    Item* lastItem = pItem;

    for (ItemPosCountVec::const_iterator itr = dest.begin(); itr != dest.end();)
    {
        uint16 pos = itr->pos;
        uint32 count = itr->count;

        ++itr;

        if (itr == dest.end())
        {
            lastItem = _StoreItem(pos, pItem, count, false, update);
            break;
        }

        lastItem = _StoreItem(pos, pItem, count, true, update);
    }

    return lastItem;
}

// Return stored item (if stored to stack, it can diff. from pItem). And pItem ca be deleted in this case.
Item* Player::_StoreItem(uint16 pos, Item* pItem, uint32 count, bool clone, bool update)
{
    if (!pItem)
    {
        return NULL;
    }

    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;

    DEBUG_LOG("STORAGE: StoreItem bag = %u, slot = %u, item = %u, count = %u", bag, slot, pItem->GetEntry(), count);

    Item* pItem2 = GetItemByPos(bag, slot);

    if (!pItem2)
    {
        if (clone)
        {
            pItem = pItem->CloneItem(count, this);
        }
        else
        {
            pItem->SetCount(count);
        }

        if (!pItem)
        {
            return NULL;
        }

        ItemPrototype const* itemProto = pItem->GetProto();
        if (itemProto->Bonding == BIND_WHEN_PICKED_UP ||
            itemProto->Bonding == BIND_QUEST_ITEM ||
            (itemProto->Bonding == BIND_WHEN_EQUIPPED && IsBagPos(pos)))
        {
            pItem->SetBinding(true);
        }

        if (bag == INVENTORY_SLOT_BAG_0)
        {
            m_items[slot] = pItem;
            SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), pItem->GetObjectGuid());
            pItem->SetGuidValue(ITEM_FIELD_CONTAINED, GetObjectGuid());
            pItem->SetGuidValue(ITEM_FIELD_OWNER, GetObjectGuid());

            pItem->SetSlot(slot);
            pItem->SetContainer(NULL);

            if (IsInWorld() && update)
            {
                pItem->AddToWorld();
                pItem->SendCreateUpdateToPlayer(this);
            }

            pItem->SetState(ITEM_CHANGED, this);
        }
        else if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag))
        {
            pBag->StoreItem(slot, pItem);
            if (IsInWorld() && update)
            {
                pItem->AddToWorld();
                pItem->SendCreateUpdateToPlayer(this);
            }
            pItem->SetState(ITEM_CHANGED, this);
            pBag->SetState(ITEM_CHANGED, this);
        }

        AddEnchantmentDurations(pItem);
        AddItemDurations(pItem);

        return pItem;
    }
    else
    {
        ItemPrototype const* itemProto = pItem2->GetProto();
        if (itemProto->Bonding == BIND_WHEN_PICKED_UP ||
            itemProto->Bonding == BIND_QUEST_ITEM ||
            (itemProto->Bonding == BIND_WHEN_EQUIPPED && IsBagPos(pos)))
        {
            pItem2->SetBinding(true);
        }

        pItem2->SetCount(pItem2->GetCount() + count);
        if (IsInWorld() && update)
        {
            pItem2->SendCreateUpdateToPlayer(this);
        }

        if (!clone)
        {
            // delete item (it not in any slot currently)
            if (IsInWorld() && update)
            {
                pItem->RemoveFromWorld();
                pItem->DestroyForPlayer(this);
            }

            RemoveEnchantmentDurations(pItem);
            RemoveItemDurations(pItem);

            pItem->SetOwnerGuid(GetObjectGuid());           // prevent error at next SetState in case trade/mail/buy from vendor
            pItem->SetState(ITEM_REMOVED, this);
        }

        // AddItemDurations(pItem2); - pItem2 already have duration listed for player
        AddEnchantmentDurations(pItem2);

        pItem2->SetState(ITEM_CHANGED, this);

        return pItem2;
    }
}

/**
 * @brief Creates and equips a new item into a destination slot.
 *
 * @param pos The packed destination position.
 * @param item The item entry to create.
 * @param update True to send world updates for the equip action.
 * @return The equipped item, or null on failure.
 */
Item* Player::EquipNewItem(uint16 pos, uint32 item, bool update)
{
    if (Item* pItem = Item::CreateItem(item, 1, this))
    {
        ItemAddedQuestCheck(item, 1);
        return EquipItem(pos, pItem, update);
    }

    return NULL;
}

/**
 * @brief Equips an item into the specified destination position.
 *
 * @param pos The packed destination position.
 * @param pItem The item to equip.
 * @param update True to send world updates for the equip action.
 * @return The equipped or merged item instance.
 */
Item* Player::EquipItem(uint16 pos, Item* pItem, bool update)
{
    AddEnchantmentDurations(pItem);
    AddItemDurations(pItem);

    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;

    Item* pItem2 = GetItemByPos(bag, slot);

    if (!pItem2)
    {
        VisualizeItem(slot, pItem);

        if (IsAlive())
        {
            ItemPrototype const* pProto = pItem->GetProto();

            // item set bonuses applied only at equip and removed at unequip, and still active for broken items
            if (pProto && pProto->ItemSet)
            {
                AddItemsSetItem(this, pItem);
            }

            _ApplyItemMods(pItem, slot, true);

            // Weapons and also Totem/Relic/Sigil/etc
            if (pProto && IsInCombat() && (pProto->Class == ITEM_CLASS_WEAPON || pProto->InventoryType == INVTYPE_RELIC) && m_weaponChangeTimer == 0)
            {
                uint32 cooldownSpell = SPELL_ID_WEAPON_SWITCH_COOLDOWN_1_5s;

                if (getClass() == CLASS_ROGUE)
                {
                    cooldownSpell = SPELL_ID_WEAPON_SWITCH_COOLDOWN_1_0s;
                }

                SpellEntry const* spellProto = sSpellStore.LookupEntry(cooldownSpell);

                if (!spellProto)
                {
                    sLog.outError("Weapon switch cooldown spell %u couldn't be found in Spell.dbc", cooldownSpell);
                }
                else
                {
                    m_weaponChangeTimer = spellProto->StartRecoveryTime;

                    WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 4 + 4);
                    data << GetObjectGuid();
                    data << uint32(cooldownSpell);
                    data << uint32(0);
                    GetSession()->SendPacket(&data);
                }
            }
        }

        if (IsInWorld() && update)
        {
            pItem->AddToWorld();
            pItem->SendCreateUpdateToPlayer(this);
        }

        ApplyEquipCooldown(pItem);
    }
    else
    {
        pItem2->SetCount(pItem2->GetCount() + pItem->GetCount());
        if (IsInWorld() && update)
        {
            pItem2->SendCreateUpdateToPlayer(this);
        }

        // delete item (it not in any slot currently)
        // pItem->DeleteFromDB();
        if (IsInWorld() && update)
        {
            pItem->RemoveFromWorld();
            pItem->DestroyForPlayer(this);
        }

        RemoveEnchantmentDurations(pItem);
        RemoveItemDurations(pItem);

        pItem->SetOwnerGuid(GetObjectGuid());               // prevent error at next SetState in case trade/mail/buy from vendor
        pItem->SetState(ITEM_REMOVED, this);
        pItem2->SetState(ITEM_CHANGED, this);

        ApplyEquipCooldown(pItem2);

        // Used by Eluna
#ifdef ENABLE_ELUNA
        if (Eluna* e = GetEluna())
        {
            e->OnEquip(this, pItem2, bag, slot); // This is depricated and will be removed in the future
            e->OnItemEquip(this, pItem2, slot);
        }
#endif /* ENABLE_ELUNA */

        return pItem2;
    }
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnEquip(this, pItem, bag, slot); // This is depricated and will be removed in the future
        e->OnItemEquip(this, pItem, slot);
    }
#endif /* ENABLE_ELUNA */

    return pItem;
}

/**
 * @brief Equips an item without full validation, typically during loading.
 *
 * @param pos The packed destination position.
 * @param pItem The item to equip.
 */
void Player::QuickEquipItem(uint16 pos, Item* pItem)
{
    if (pItem)
    {
        AddEnchantmentDurations(pItem);
        AddItemDurations(pItem);

        uint8 slot = pos & 255;
        VisualizeItem(slot, pItem);

        if (IsInWorld())
        {
            pItem->AddToWorld();
            pItem->SendCreateUpdateToPlayer(this);
        }
    }
}

/**
 * @brief Updates the visible equipment fields for an equipment slot.
 *
 * @param slot The equipment slot to update.
 * @param pItem The item to display, or null to clear the slot.
 */
void Player::SetVisibleItemSlot(uint8 slot, Item* pItem)
{
    if (pItem)
    {
        SetGuidValue(PLAYER_VISIBLE_ITEM_1_CREATOR + (slot * MAX_VISIBLE_ITEM_OFFSET), pItem->GetGuidValue(ITEM_FIELD_CREATOR));

        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * MAX_VISIBLE_ITEM_OFFSET);
        SetUInt32Value(VisibleBase + 0, pItem->GetEntry());

        for (int i = 0; i < MAX_INSPECTED_ENCHANTMENT_SLOT; ++i)
        {
            SetUInt32Value(VisibleBase + 1 + i, pItem->GetEnchantmentId(EnchantmentSlot(i)));
        }

        // Use SetInt16Value to prevent set high part to FFFF for negative value
        SetInt16Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + (slot * MAX_VISIBLE_ITEM_OFFSET), 0, pItem->GetItemRandomPropertyId());
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + 1 + (slot * MAX_VISIBLE_ITEM_OFFSET), pItem->GetItemSuffixFactor());
    }
    else
    {
        SetGuidValue(PLAYER_VISIBLE_ITEM_1_CREATOR + (slot * MAX_VISIBLE_ITEM_OFFSET), ObjectGuid());

        int VisibleBase = PLAYER_VISIBLE_ITEM_1_0 + (slot * MAX_VISIBLE_ITEM_OFFSET);
        SetUInt32Value(VisibleBase + 0, 0);

        for (int i = 0; i < MAX_INSPECTED_ENCHANTMENT_SLOT; ++i)
        {
            SetUInt32Value(VisibleBase + 1 + i, 0);
        }

        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + 0 + (slot * MAX_VISIBLE_ITEM_OFFSET), 0);
        SetUInt32Value(PLAYER_VISIBLE_ITEM_1_PROPERTIES + 1 + (slot * MAX_VISIBLE_ITEM_OFFSET), 0);
    }
}

/**
 * @brief Places an item into an equipment slot and updates visible state.
 *
 * @param slot The destination equipment slot.
 * @param pItem The item to visualize.
 */
void Player::VisualizeItem(uint8 slot, Item* pItem)
{
    if (!pItem)
    {
        return;
    }

    // check also  BIND_WHEN_PICKED_UP and BIND_QUEST_ITEM for .additem or .additemset case by GM (not binded at adding to inventory)
    ItemPrototype const* itemProto = pItem->GetProto();
    if (itemProto->Bonding == BIND_WHEN_EQUIPPED || itemProto->Bonding == BIND_WHEN_PICKED_UP || itemProto->Bonding == BIND_QUEST_ITEM)
    {
        pItem->SetBinding(true);
    }

    DEBUG_LOG("STORAGE: EquipItem slot = %u, item = %u", slot, pItem->GetEntry());

    m_items[slot] = pItem;
    SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), pItem->GetObjectGuid());
    pItem->SetGuidValue(ITEM_FIELD_CONTAINED, GetObjectGuid());
    pItem->SetGuidValue(ITEM_FIELD_OWNER, GetObjectGuid());
    pItem->SetSlot(slot);
    pItem->SetContainer(NULL);

    if (slot < EQUIPMENT_SLOT_END)
    {
        SetVisibleItemSlot(slot, pItem);
    }

    pItem->SetState(ITEM_CHANGED, this);
}

/**
 * @brief Temporarily removes an item from player storage without deleting it.
 *
 * @param bag The bag containing the item.
 * @param slot The slot containing the item.
 * @param update True to send inventory updates to the client.
 */
void Player::RemoveItem(uint8 bag, uint8 slot, bool update)
{
    // note: removeitem does not actually change the item
    // it only takes the item out of storage temporarily
    // note2: if removeitem is to be used for delinking
    // the item must be removed from the player's updatequeue

    Item* pItem = GetItemByPos(bag, slot);
    if (pItem)
    {
        DEBUG_LOG("STORAGE: RemoveItem bag = %u, slot = %u, item = %u", bag, slot, pItem->GetEntry());

        RemoveEnchantmentDurations(pItem);
        RemoveItemDurations(pItem);

        if (bag == INVENTORY_SLOT_BAG_0)
        {
            if (slot < INVENTORY_SLOT_BAG_END)
            {
                ItemPrototype const* pProto = pItem->GetProto();
                // item set bonuses applied only at equip and removed at unequip, and still active for broken items

                if (pProto && pProto->ItemSet)
                {
                    RemoveItemsSetItem(this, pProto);
                }

                _ApplyItemMods(pItem, slot, false);

                // remove item dependent auras and casts (only weapon and armor slots)
                if (slot < EQUIPMENT_SLOT_END)
                {
                    RemoveItemDependentAurasAndCasts(pItem);

                    // remove held enchantments
                    if (slot == EQUIPMENT_SLOT_MAINHAND)
                    {
                        pItem->ClearEnchantment(PROP_ENCHANTMENT_SLOT_3);
                    }
                }
            }

            m_items[slot] = NULL;
            SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), ObjectGuid());

            if (slot < EQUIPMENT_SLOT_END)
            {
                SetVisibleItemSlot(slot, NULL);
            }
        }
        else
        {
            Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
            if (pBag)
            {
                pBag->RemoveItem(slot);
            }
        }
        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
        // pItem->SetGuidValue(ITEM_FIELD_OWNER, ObjectGuid()); not clear owner at remove (it will be set at store). This used in mail and auction code
        pItem->SetSlot(NULL_SLOT);
        if (IsInWorld() && update)
        {
            pItem->SendCreateUpdateToPlayer(this);
        }
    }
}

// Common operation need to remove item from inventory without delete in trade, auction, guild bank, mail....
void Player::MoveItemFromInventory(uint8 bag, uint8 slot, bool update)
{
    if (Item* it = GetItemByPos(bag, slot))
    {
        ItemRemovedQuestCheck(it->GetEntry(), it->GetCount());
        RemoveItem(bag, slot, update);
        it->RemoveFromUpdateQueueOf(this);
        if (it->IsInWorld())
        {
            it->RemoveFromWorld();
            it->DestroyForPlayer(this);
        }
    }
}

// Common operation need to add item from inventory without delete in trade, guild bank, mail....
void Player::MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB)
{
    // update quest counters
    ItemAddedQuestCheck(pItem->GetEntry(), pItem->GetCount());

    // store item
    Item* pLastItem = StoreItem(dest, pItem, update);

    // only set if not merged to existing stack (pItem can be deleted already but we can compare pointers any way)
    if (pLastItem == pItem)
    {
        // update owner for last item (this can be original item with wrong owner
        if (pLastItem->GetOwnerGuid() != GetObjectGuid())
        {
            pLastItem->SetOwnerGuid(GetObjectGuid());
        }

        // if this original item then it need create record in inventory
        // in case trade we already have item in other player inventory
        pLastItem->SetState(in_characterInventoryDB ? ITEM_CHANGED : ITEM_NEW, this);
    }
}

/**
 * @brief Permanently destroys an item from player storage.
 *
 * @param bag The bag containing the item.
 * @param slot The slot containing the item.
 * @param update True to send inventory updates to the client.
 */
void Player::DestroyItem(uint8 bag, uint8 slot, bool update)
{
    Item* pItem = GetItemByPos(bag, slot);
    if (pItem)
    {
        DEBUG_LOG("STORAGE: DestroyItem bag = %u, slot = %u, item = %u", bag, slot, pItem->GetEntry());

        // start from destroy contained items (only equipped bag can have its)
        if (pItem->IsBag() && pItem->IsEquipped())          // this also prevent infinity loop if empty bag stored in bag==slot
        {
            for (int i = 0; i < MAX_BAG_SIZE; ++i)
            {
                DestroyItem(slot, i, update);
            }
        }

        if (pItem->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        {
            static SqlStatementID delGifts ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
            stmt.PExecute(pItem->GetGUIDLow());
        }

        RemoveEnchantmentDurations(pItem);
        RemoveItemDurations(pItem);

        ItemRemovedQuestCheck(pItem->GetEntry(), pItem->GetCount());
#ifdef ENABLE_ELUNA
        if (Eluna* e = GetEluna())
        {
            e->OnRemove(this, pItem);
        }
#endif /* ENABLE_ELUNA */

        if (bag == INVENTORY_SLOT_BAG_0)
        {
            SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), ObjectGuid());

            // equipment and equipped bags can have applied bonuses
            if (slot < INVENTORY_SLOT_BAG_END)
            {
                ItemPrototype const* pProto = pItem->GetProto();

                // item set bonuses applied only at equip and removed at unequip, and still active for broken items
                if (pProto && pProto->ItemSet)
                {
                    RemoveItemsSetItem(this, pProto);
                }

                _ApplyItemMods(pItem, slot, false);
            }

            if (slot < EQUIPMENT_SLOT_END)
            {
                // remove item dependent auras and casts (only weapon and armor slots)
                RemoveItemDependentAurasAndCasts(pItem);

                // equipment visual show
                SetVisibleItemSlot(slot, NULL);
            }

            m_items[slot] = NULL;
        }
        else if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag))
        {
            pBag->RemoveItem(slot);
        }

        if (IsInWorld() && update)
        {
            pItem->RemoveFromWorld();
            pItem->DestroyForPlayer(this);
        }

        // pItem->SetOwnerGUID(0);
        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());
        pItem->SetSlot(NULL_SLOT);
        pItem->SetState(ITEM_REMOVED, this);
    }
}

/**
 * @brief Destroys up to a requested count of an item across player storage.
 *
 * @param item The item entry to destroy.
 * @param count The requested quantity to destroy.
 * @param update True to send inventory updates to the client.
 * @param unequip_check True to validate equipped items before destroying them.
 * @param delete_from_bank True to include bank storage in the search.
 * @param delete_from_buyback True to include vendor buyback slots in the search.
 * @return The number of items removed.
 */
uint32 Player::DestroyItemCount(uint32 item, uint32 count, bool update, bool unequip_check, bool delete_from_bank,bool delete_from_buyback)
{
    DEBUG_LOG("STORAGE: DestroyItemCount item = %u, count = %u", item, count);
    uint32 remcount = 0;

    // Search in default bagpack
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {
                    // all items in inventory can unequipped
                    remcount += pItem->GetCount();
                    DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                    if (remcount >= count)
                    {
                        return remcount;
                    }
                }
                else
                {
                    ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    if (IsInWorld() && update)
                    {
                        pItem->SendCreateUpdateToPlayer(this);
                    }
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    // Search in keyring slots
    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {
                    // all keys can be unequipped
                    remcount += pItem->GetCount();
                    DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                    if (remcount >= count)
                    {
                        return remcount;
                    }
                }
                else
                {
                    ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    if (IsInWorld() && update)
                    {
                        pItem->SendCreateUpdateToPlayer(this);
                    }
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    // Search in inventory bags
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->GetEntry() == item && !pItem->IsInTrade())
                    {
                        // all items in bags can be unequipped
                        if (pItem->GetCount() + remcount <= count)
                        {
                            remcount += pItem->GetCount();
                            DestroyItem(i, j, update);

                            if (remcount >= count)
                            {
                                return remcount;
                            }
                        }
                        else
                        {
                            ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                            pItem->SetCount(pItem->GetCount() - count + remcount);
                            if (IsInWorld() && update)
                            {
                                pItem->SendCreateUpdateToPlayer(this);
                            }
                            pItem->SetState(ITEM_CHANGED, this);
                            return remcount;
                        }
                    }
                }
            }
        }
    }

    // Search in Equiped items
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem && pItem->GetEntry() == item && !pItem->IsInTrade())
            {
                if (pItem->GetCount() + remcount <= count)
                {
                    if (!unequip_check || CanUnequipItem(INVENTORY_SLOT_BAG_0 << 8 | i, false) == EQUIP_ERR_OK)
                    {
                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                }
                else
                {
                    ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                    pItem->SetCount(pItem->GetCount() - count + remcount);
                    if (IsInWorld() && update)
                    {
                        pItem->SendCreateUpdateToPlayer(this);
                    }
                    pItem->SetState(ITEM_CHANGED, this);
                    return remcount;
                }
            }
        }
    }

    // Search in bank items
    if (delete_from_bank)
    {
        // Normal bank slots
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == item && !pItem->IsInTrade())
                {
                    if (pItem->GetCount() + remcount <= count)
                    {
                        // all items in inventory can unequipped
                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                    else
                    {
                        ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                        pItem->SetCount(pItem->GetCount() - count + remcount);
                        if (IsInWorld() && update)
                        {
                            pItem->SendCreateUpdateToPlayer(this);
                        }
                        pItem->SetState(ITEM_CHANGED, this);
                        return remcount;
                    }
                }
            }
        }

        // Bank bagslots
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    if (Item* pItem = pBag->GetItemByPos(j))
                    {
                        if (pItem->GetEntry() == item && !pItem->IsInTrade())
                        {
                            // all items in bags can be unequipped
                            if (pItem->GetCount() + remcount <= count)
                            {
                                remcount += pItem->GetCount();
                                DestroyItem(i, j, update);

                                if (remcount >= count)
                                {
                                    return remcount;
                                }
                            }
                            else
                            {
                                ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                                pItem->SetCount(pItem->GetCount() - count + remcount);
                                if (IsInWorld() && update)
                                {
                                    pItem->SendCreateUpdateToPlayer(this);
                                }
                                pItem->SetState(ITEM_CHANGED, this);
                                return remcount;
                            }
                        }
                    }
                }
            }
        }
    }

    // Search in buyback npcs vendor tab
    if (delete_from_buyback)
    {
        for (int i = BUYBACK_SLOT_START; i < BUYBACK_SLOT_END; ++i)
        {
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                if (pItem->GetEntry() == item && !pItem->IsInTrade())
                {
                    if (pItem->GetCount() + remcount <= count)
                    {
                        // all keys can be unequipped
                        remcount += pItem->GetCount();
                        DestroyItem(INVENTORY_SLOT_BAG_0, i, update);

                        if (remcount >= count)
                        {
                            return remcount;
                        }
                    }
                    else
                    {
                        ItemRemovedQuestCheck(pItem->GetEntry(), count - remcount);
                        pItem->SetCount(pItem->GetCount() - count + remcount);
                        if (IsInWorld() && update)
                        {
                            pItem->SendCreateUpdateToPlayer(this);
                        }
                        pItem->SetState(ITEM_CHANGED, this);
                        return remcount;
                    }
                }
            }
        }
    }

    return remcount;
}

/**
 * @brief Destroys items that are no longer valid in the player's current zone or map.
 *
 * @param update True to send inventory updates to the client.
 * @param new_zone The zone identifier to validate against.
 */
void Player::DestroyZoneLimitedItem(bool update, uint32 new_zone)
{
    DEBUG_LOG("STORAGE: DestroyZoneLimitedItem in map %u and area %u", GetMapId(), new_zone);

    // in inventory
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }

    // in inventory bags
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
                    {
                        DestroyItem(i, j, update);
                    }
                }
            }
        }
    }

    // in equipment and bag list
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsLimitedToAnotherMapOrZone(GetMapId(), new_zone))
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }
}

/**
 * @brief Destroys conjured consumables from player storage.
 *
 * @param update True to send inventory updates to the client.
 */
void Player::DestroyConjuredItems(bool update)
{
    // destroys all conjured items
    DEBUG_LOG("STORAGE: DestroyConjuredItems");

    // in inventory
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsConjuredConsumable())
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }

    // in inventory bags
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->IsConjuredConsumable())
                    {
                        DestroyItem(i, j, update);
                    }
                }
            }
        }
    }

    // in equipment and bag list
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->IsConjuredConsumable())
            {
                DestroyItem(INVENTORY_SLOT_BAG_0, i, update);
            }
        }
    }
}

/**
 * @brief Destroys or decrements a specific item instance by a requested count.
 *
 * @param pItem The item instance to modify.
 * @param count The remaining quantity to destroy; updated by the call.
 * @param update True to send inventory updates to the client.
 */
void Player::DestroyItemCount(Item* pItem, uint32& count, bool update)
{
    if (!pItem)
    {
        return;
    }

    DEBUG_LOG("STORAGE: DestroyItemCount item (GUID: %u, Entry: %u) count = %u", pItem->GetGUIDLow(), pItem->GetEntry(), count);

    if (pItem->GetCount() <= count)
    {
        count -= pItem->GetCount();

        DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), update);
    }
    else
    {
        ItemRemovedQuestCheck(pItem->GetEntry(), count);
        pItem->SetCount(pItem->GetCount() - count);
        count = 0;
        if (IsInWorld() && update)
        {
            pItem->SendCreateUpdateToPlayer(this);
        }
        pItem->SetState(ITEM_CHANGED, this);
    }
}

/**
 * @brief Splits a stack into a new destination position.
 *
 * @param src The packed source position.
 * @param dst The packed destination position.
 * @param count The quantity to split from the source stack.
 */
void Player::SplitItem(uint16 src, uint16 dst, uint32 count)
{
    uint8 srcbag = src >> 8;
    uint8 srcslot = src & 255;

    uint8 dstbag = dst >> 8;
    uint8 dstslot = dst & 255;

    Item* pSrcItem = GetItemByPos(srcbag, srcslot);
    if (!pSrcItem)
    {
        SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pSrcItem, NULL);
        return;
    }

    if (pSrcItem->HasGeneratedLoot())                       // prevent split looting item (stackable items can has only temporary loot and this meaning that loot window open)
    {
        // best error message found for attempting to split while looting
        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, NULL);
        return;
    }

    // not let split all items (can be only at cheating)
    if (pSrcItem->GetCount() == count)
    {
        SendEquipError(EQUIP_ERR_COULDNT_SPLIT_ITEMS, pSrcItem, NULL);
        return;
    }

    // not let split more existing items (can be only at cheating)
    if (pSrcItem->GetCount() < count)
    {
        SendEquipError(EQUIP_ERR_TRIED_TO_SPLIT_MORE_THAN_COUNT, pSrcItem, NULL);
        return;
    }

    DEBUG_LOG("STORAGE: SplitItem bag = %u, slot = %u, item = %u, count = %u", dstbag, dstslot, pSrcItem->GetEntry(), count);
    Item* pNewItem = pSrcItem->CloneItem(count, this);
    if (!pNewItem)
    {
        SendEquipError(EQUIP_ERR_ITEM_NOT_FOUND, pSrcItem, NULL);
        return;
    }

    if (IsInventoryPos(dst))
    {
        // change item amount before check (for unique max count check)
        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        ItemPosCountVec dest;
        InventoryResult msg = CanStoreItem(dstbag, dstslot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, NULL);
            return;
        }

        if (IsInWorld())
        {
            pSrcItem->SendCreateUpdateToPlayer(this);
        }
        pSrcItem->SetState(ITEM_CHANGED, this);
        StoreItem(dest, pNewItem, true);
    }
    else if (IsBankPos(dst))
    {
        // change item amount before check (for unique max count check)
        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        ItemPosCountVec dest;
        InventoryResult msg = CanBankItem(dstbag, dstslot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, NULL);
            return;
        }

        if (IsInWorld())
        {
            pSrcItem->SendCreateUpdateToPlayer(this);
        }
        pSrcItem->SetState(ITEM_CHANGED, this);
        BankItem(dest, pNewItem, true);
    }
    else if (IsEquipmentPos(dst))
    {
        // change item amount before check (for unique max count check), provide space for splitted items
        pSrcItem->SetCount(pSrcItem->GetCount() - count);

        uint16 dest;
        InventoryResult msg = CanEquipItem(dstslot, dest, pNewItem, false);
        if (msg != EQUIP_ERR_OK)
        {
            delete pNewItem;
            pSrcItem->SetCount(pSrcItem->GetCount() + count);
            SendEquipError(msg, pSrcItem, NULL);
            return;
        }

        if (IsInWorld())
        {
            pSrcItem->SendCreateUpdateToPlayer(this);
        }
        pSrcItem->SetState(ITEM_CHANGED, this);
        EquipItem(dest, pNewItem, true);
        AutoUnequipOffhandIfNeed();
    }
}

/**
 * @brief Moves, merges, or swaps items between two storage positions.
 *
 * @param src The packed source position.
 * @param dst The packed destination position.
 */
void Player::SwapItem(uint16 src, uint16 dst)
{
    uint8 srcbag = src >> 8;
    uint8 srcslot = src & 255;

    uint8 dstbag = dst >> 8;
    uint8 dstslot = dst & 255;

    Item* pSrcItem = GetItemByPos(srcbag, srcslot);
    Item* pDstItem = GetItemByPos(dstbag, dstslot);

    if (!pSrcItem)
    {
        return;
    }

    DEBUG_LOG("STORAGE: SwapItem bag = %u, slot = %u, item = %u", dstbag, dstslot, pSrcItem->GetEntry());

    if (!IsAlive())
    {
        SendEquipError(EQUIP_ERR_YOU_ARE_DEAD, pSrcItem, pDstItem);
        return;
    }

    // SRC checks

    // check unequip potability for equipped items and bank bags
    if (IsEquipmentPos(src) || IsBagPos(src))
    {
        // bags can be swapped with empty bag slots, or with empty bag (items move possibility checked later)
        InventoryResult msg = CanUnequipItem(src, !IsBagPos(src) || IsBagPos(dst) || (pDstItem && pDstItem->IsBag() && ((Bag*)pDstItem)->IsEmpty()));
        if (msg != EQUIP_ERR_OK)
        {
            SendEquipError(msg, pSrcItem, pDstItem);
            return;
        }
    }

    // prevent put equipped/bank bag in self
    if (IsBagPos(src) && srcslot == dstbag)
    {
        SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pSrcItem, pDstItem);
        return;
    }

    // prevent put equipped/bank bag in self
    if (IsBagPos(dst) && dstslot == srcbag)
    {
        SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pDstItem, pSrcItem);
        return;
    }

    // DST checks

    if (pDstItem)
    {
        // check unequip potability for equipped items and bank bags
        if (IsEquipmentPos(dst) || IsBagPos(dst))
        {
            // bags can be swapped with empty bag slots, or with empty bag (items move possibility checked later)
            InventoryResult msg = CanUnequipItem(dst, !IsBagPos(dst) || IsBagPos(src) || (pSrcItem->IsBag() && ((Bag*)pSrcItem)->IsEmpty()));
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, pDstItem);
                return;
            }
        }
    }

    // NOW this is or item move (swap with empty), or swap with another item (including bags in bag possitions)
    // or swap empty bag with another empty or not empty bag (with items exchange)

    // Move case
    if (!pDstItem)
    {
        if (IsInventoryPos(dst))
        {
            ItemPosCountVec dest;
            InventoryResult msg = CanStoreItem(dstbag, dstslot, dest, pSrcItem, false);
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, NULL);
                return;
            }

            RemoveItem(srcbag, srcslot, true);
            StoreItem(dest, pSrcItem, true);
        }
        else if (IsBankPos(dst))
        {
            ItemPosCountVec dest;
            InventoryResult msg = CanBankItem(dstbag, dstslot, dest, pSrcItem, false);
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, NULL);
                return;
            }

            RemoveItem(srcbag, srcslot, true);
            BankItem(dest, pSrcItem, true);
        }
        else if (IsEquipmentPos(dst))
        {
            uint16 dest;
            InventoryResult msg = CanEquipItem(dstslot, dest, pSrcItem, false);
            if (msg != EQUIP_ERR_OK)
            {
                SendEquipError(msg, pSrcItem, NULL);
                return;
            }

            RemoveItem(srcbag, srcslot, true);
            EquipItem(dest, pSrcItem, true);
            AutoUnequipOffhandIfNeed();
        }

        return;
    }

    // attempt merge to / fill target item
    if (!pSrcItem->IsBag() && !pDstItem->IsBag())
    {
        InventoryResult msg;
        ItemPosCountVec sDest;
        uint16 eDest;
        if (IsInventoryPos(dst))
        {
            msg = CanStoreItem(dstbag, dstslot, sDest, pSrcItem, false);
        }
        else if (IsBankPos(dst))
        {
            msg = CanBankItem(dstbag, dstslot, sDest, pSrcItem, false);
        }
        else if (IsEquipmentPos(dst))
        {
            msg = CanEquipItem(dstslot, eDest, pSrcItem, false);
        }
        else
        {
            return;
        }

        // can be merge/fill
        if (msg == EQUIP_ERR_OK)
        {
            ItemPrototype const* itemProto = pSrcItem->GetProto();
            if (pSrcItem->GetCount() + pDstItem->GetCount() <= itemProto->GetMaxStackSize())
            {
                RemoveItem(srcbag, srcslot, true);

                if (IsInventoryPos(dst))
                {
                    StoreItem(sDest, pSrcItem, true);
                }
                else if (IsBankPos(dst))
                {
                    BankItem(sDest, pSrcItem, true);
                }
                else if (IsEquipmentPos(dst))
                {
                    EquipItem(eDest, pSrcItem, true);
                    AutoUnequipOffhandIfNeed();
                }
            }
            else
            {
                pSrcItem->SetCount(pSrcItem->GetCount() + pDstItem->GetCount() - itemProto->GetMaxStackSize());
                pDstItem->SetCount(itemProto->GetMaxStackSize());
                pSrcItem->SetState(ITEM_CHANGED, this);
                pDstItem->SetState(ITEM_CHANGED, this);
                if (IsInWorld())
                {
                    pSrcItem->SendCreateUpdateToPlayer(this);
                    pDstItem->SendCreateUpdateToPlayer(this);
                }
            }
            return;
        }
    }

    // impossible merge/fill, do real swap
    InventoryResult msg = EQUIP_ERR_YOU_CAN_NEVER_USE_THAT_ITEM; // Initialize msg with a default value which will never be used

    // check src->dest move possibility
    ItemPosCountVec sDest;
    uint16 eDest = 0;
    if (IsInventoryPos(dst))
    {
        msg = CanStoreItem(dstbag, dstslot, sDest, pSrcItem, true);
    }
    else if (IsBankPos(dst))
    {
        msg = CanBankItem(dstbag, dstslot, sDest, pSrcItem, true);
    }
    else if (IsEquipmentPos(dst))
    {
        msg = CanEquipItem(dstslot, eDest, pSrcItem, true);
        if (msg == EQUIP_ERR_OK)
        {
            msg = CanUnequipItem(eDest, true);
        }
    }

    if (msg != EQUIP_ERR_OK)
    {
        SendEquipError(msg, pSrcItem, pDstItem);
        return;
    }

    // check dest->src move possibility
    ItemPosCountVec sDest2;
    uint16 eDest2 = 0;
    if (IsInventoryPos(src))
    {
        msg = CanStoreItem(srcbag, srcslot, sDest2, pDstItem, true);
    }
    else if (IsBankPos(src))
    {
        msg = CanBankItem(srcbag, srcslot, sDest2, pDstItem, true);
    }
    else if (IsEquipmentPos(src))
    {
        msg = CanEquipItem(srcslot, eDest2, pDstItem, true);
        if (msg == EQUIP_ERR_OK)
        {
            msg = CanUnequipItem(eDest2, true);
        }
    }

    if (msg != EQUIP_ERR_OK)
    {
        SendEquipError(msg, pDstItem, pSrcItem);
        return;
    }

    // Check bag swap with item exchange (one from empty in not bag possition (equipped (not possible in fact) or store)
    if (pSrcItem->IsBag() && pDstItem->IsBag())
    {
        Bag* emptyBag = NULL;
        Bag* fullBag = NULL;
        if (((Bag*)pSrcItem)->IsEmpty() && !IsBagPos(src))
        {
            emptyBag = (Bag*)pSrcItem;
            fullBag  = (Bag*)pDstItem;
        }
        else if (((Bag*)pDstItem)->IsEmpty() && !IsBagPos(dst))
        {
            emptyBag = (Bag*)pDstItem;
            fullBag  = (Bag*)pSrcItem;
        }

        // bag swap (with items exchange) case
        if (emptyBag && fullBag)
        {
            ItemPrototype const* emotyProto = emptyBag->GetProto();

            uint32 count = 0;

            for (uint32 i = 0; i < fullBag->GetBagSize(); ++i)
            {
                Item* bagItem = fullBag->GetItemByPos(i);
                if (!bagItem)
                {
                    continue;
                }

                ItemPrototype const* bagItemProto = bagItem->GetProto();
                if (!bagItemProto || !ItemCanGoIntoBag(bagItemProto, emotyProto))
                {
                    // one from items not go to empty target bag
                    SendEquipError(EQUIP_ERR_NONEMPTY_BAG_OVER_OTHER_BAG, pSrcItem, pDstItem);
                    return;
                }

                ++count;
            }

            if (count > emptyBag->GetBagSize())
            {
                // too small targeted bag
                SendEquipError(EQUIP_ERR_ITEMS_CANT_BE_SWAPPED, pSrcItem, pDstItem);
                return;
            }

            // Items swap
            count = 0;                                      // will pos in new bag
            for (uint32 i = 0; i < fullBag->GetBagSize(); ++i)
            {
                Item* bagItem = fullBag->GetItemByPos(i);
                if (!bagItem)
                {
                    continue;
                }

                fullBag->RemoveItem(i);
                emptyBag->StoreItem(count, bagItem);
                bagItem->SetState(ITEM_CHANGED, this);

                ++count;
            }
        }
    }

    // now do moves, remove...
    RemoveItem(dstbag, dstslot, false);
    RemoveItem(srcbag, srcslot, false);

    // add to dest
    if (IsInventoryPos(dst))
    {
        StoreItem(sDest, pSrcItem, true);
    }
    else if (IsBankPos(dst))
    {
        BankItem(sDest, pSrcItem, true);
    }
    else if (IsEquipmentPos(dst))
    {
        EquipItem(eDest, pSrcItem, true);
    }

    // add to src
    if (IsInventoryPos(src))
    {
        StoreItem(sDest2, pDstItem, true);
    }
    else if (IsBankPos(src))
    {
        BankItem(sDest2, pDstItem, true);
    }
    else if (IsEquipmentPos(src))
    {
        EquipItem(eDest2, pDstItem, true);
    }

    AutoUnequipOffhandIfNeed();
}

/**
 * @brief Adds an item to the vendor buyback list.
 *
 * @param pItem The item to place into a buyback slot.
 */
void Player::AddItemToBuyBackSlot(Item* pItem)
{
    if (pItem)
    {
        uint32 slot = m_currentBuybackSlot;
        // if current back slot non-empty search oldest or free
        if (m_items[slot])
        {
            uint32 oldest_time = GetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1);
            uint32 oldest_slot = BUYBACK_SLOT_START;

            for (uint32 i = BUYBACK_SLOT_START + 1; i < BUYBACK_SLOT_END; ++i)
            {
                // found empty
                if (!m_items[i])
                {
                    slot = i;
                    break;
                }

                uint32 i_time = GetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + i - BUYBACK_SLOT_START);

                if (oldest_time > i_time)
                {
                    oldest_time = i_time;
                    oldest_slot = i;
                }
            }

            // find oldest
            slot = oldest_slot;
        }

        RemoveItemFromBuyBackSlot(slot, true);
        DEBUG_LOG("STORAGE: AddItemToBuyBackSlot item = %u, slot = %u", pItem->GetEntry(), slot);

        m_items[slot] = pItem;
        time_t base = time(NULL);
        uint32 etime = uint32(base - m_logintime + (30 * 3600));
        uint32 eslot = slot - BUYBACK_SLOT_START;

        SetGuidValue(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + (eslot * 2), pItem->GetObjectGuid());
        if (ItemPrototype const* pProto = pItem->GetProto())
        {
            SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + eslot, pProto->SellPrice * pItem->GetCount());
        }
        else
        {
            SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + eslot, 0);
        }
        SetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + eslot, (uint32)etime);

        // move to next (for non filled list is move most optimized choice)
        if (m_currentBuybackSlot < BUYBACK_SLOT_END - 1)
        {
            ++m_currentBuybackSlot;
        }
    }
}

/**
 * @brief Gets an item from a vendor buyback slot.
 *
 * @param slot The buyback slot index.
 * @return The item in the slot, or null if none exists.
 */
Item* Player::GetItemFromBuyBackSlot(uint32 slot)
{
    DEBUG_LOG("STORAGE: GetItemFromBuyBackSlot slot = %u", slot);
    if (slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END)
    {
        return m_items[slot];
    }
    return NULL;
}

/**
 * @brief Removes an item from a vendor buyback slot.
 *
 * @param slot The buyback slot index.
 * @param del True to mark the removed item for deletion.
 */
void Player::RemoveItemFromBuyBackSlot(uint32 slot, bool del)
{
    DEBUG_LOG("STORAGE: RemoveItemFromBuyBackSlot slot = %u", slot);
    if (slot >= BUYBACK_SLOT_START && slot < BUYBACK_SLOT_END)
    {
        Item* pItem = m_items[slot];
        if (pItem)
        {
            pItem->RemoveFromWorld();
            if (del)
            {
                pItem->SetState(ITEM_REMOVED, this);
            }
        }

        m_items[slot] = NULL;

        uint32 eslot = slot - BUYBACK_SLOT_START;
        SetGuidValue(PLAYER_FIELD_VENDORBUYBACK_SLOT_1 + (eslot * 2), ObjectGuid());
        SetUInt32Value(PLAYER_FIELD_BUYBACK_PRICE_1 + eslot, 0);
        SetUInt32Value(PLAYER_FIELD_BUYBACK_TIMESTAMP_1 + eslot, 0);

        // if current backslot is filled set to now free slot
        if (m_items[m_currentBuybackSlot])
        {
            m_currentBuybackSlot = slot;
        }
    }
}

/**
 * @brief Sends an inventory error packet to the client.
 *
 * @param msg The inventory error code.
 * @param pItem The primary item involved in the error.
 * @param pItem2 The secondary item involved in the error.
 * @param itemid An optional item entry used for some error variants.
 */
void Player::SendEquipError(InventoryResult msg, Item* pItem, Item* pItem2, uint32 itemid /*= 0*/) const
{
    DEBUG_LOG("WORLD: Sent SMSG_INVENTORY_CHANGE_FAILURE (%u)", msg);
    WorldPacket data(SMSG_INVENTORY_CHANGE_FAILURE, (msg == EQUIP_ERR_CANT_EQUIP_LEVEL_I ? 22 : (msg == EQUIP_ERR_OK ? 1 : 18)));
    data << uint8(msg);

    if (msg != EQUIP_ERR_OK)
    {
        if (msg == EQUIP_ERR_CANT_EQUIP_LEVEL_I)
        {
            ItemPrototype const* proto = pItem ? pItem->GetProto() : ObjectMgr::GetItemPrototype(itemid);
            data << uint32(proto ? proto->RequiredLevel : 0);
        }
        data << (pItem ? pItem->GetObjectGuid() : ObjectGuid());
        data << (pItem2 ? pItem2->GetObjectGuid() : ObjectGuid());
        data << uint8(0);                                   // bag type subclass, used with EQUIP_ERR_EVENT_AUTOEQUIP_BIND_CONFIRM and EQUIP_ERR_ITEM_DOESNT_GO_INTO_BAG2
    }
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a buy failure result to the client.
 *
 * @param msg The buy failure code.
 * @param pCreature The vendor involved in the transaction.
 * @param item The item entry that failed to purchase.
 * @param param Unused extra parameter.
 */
void Player::SendBuyError(BuyResult msg, Creature* pCreature, uint32 item, uint32 /*param*/)
{
    DEBUG_LOG("WORLD: Sent SMSG_BUY_FAILED");
    WorldPacket data(SMSG_BUY_FAILED, (8 + 4 + 1));
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << uint32(item);
    data << uint8(msg);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends a sell failure result to the client.
 *
 * @param msg The sell failure code.
 * @param pCreature The vendor involved in the transaction.
 * @param itemGuid The item GUID that failed to sell.
 * @param param Unused extra parameter.
 */
void Player::SendSellError(SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 /*param*/)
{
    DEBUG_LOG("WORLD: Sent SMSG_SELL_ITEM");
    WorldPacket data(SMSG_SELL_ITEM, (8 + 8 + /*(param ? 4 : 0) +*/ 1)); // last check [ZERO]
    data << (pCreature ? pCreature->GetObjectGuid() : ObjectGuid());
    data << ObjectGuid(itemGuid);
    data << uint8(msg);
    GetSession()->SendPacket(&data);
}
