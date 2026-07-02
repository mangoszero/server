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
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
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
#include "ObjectMgr.h"
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
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Updates a visible virtual weapon slot and consumes temporary enchant charges when needed.
 *
 * @param i The virtual slot index.
 * @param item The item to reflect in the virtual slot.
 */
void Player::SetVirtualItemSlot(uint8 i, Item* item)
{
    MANGOS_ASSERT(i < 3);
    if (i < 2 && item)
    {
        if (!item->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        {
            return;
        }
        uint32 charges = item->GetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT);
        if (charges == 0)
        {
            return;
        }
        if (charges > 1)
        {
            item->SetEnchantmentCharges(TEMP_ENCHANTMENT_SLOT, charges - 1);
        }
        else if (charges <= 1)
        {
            ApplyEnchantment(item, TEMP_ENCHANTMENT_SLOT, false);
            item->ClearEnchantment(TEMP_ENCHANTMENT_SLOT);
        }
    }
}

/**
 * @brief Updates the player's sheathed weapon state and visible weapon slots.
 *
 * @param sheathed The new sheath state.
 */
void Player::SetSheath(SheathState sheathed)
{
    switch (sheathed)
    {
        case SHEATH_STATE_UNARMED:                          // no prepared weapon
            SetVirtualItemSlot(0, NULL);
            SetVirtualItemSlot(1, NULL);
            SetVirtualItemSlot(2, NULL);
            break;
        case SHEATH_STATE_MELEE:                            // prepared melee weapon
        {
            SetVirtualItemSlot(0, GetWeaponForAttack(BASE_ATTACK, true, true));
            SetVirtualItemSlot(1, GetWeaponForAttack(OFF_ATTACK, true, true));
            SetVirtualItemSlot(2, NULL);
        };  break;
        case SHEATH_STATE_RANGED:                           // prepared ranged weapon
            SetVirtualItemSlot(0, NULL);
            SetVirtualItemSlot(1, NULL);
            SetVirtualItemSlot(2, GetWeaponForAttack(RANGED_ATTACK, true, true));
            break;
        default:
            SetVirtualItemSlot(0, NULL);
            SetVirtualItemSlot(1, NULL);
            SetVirtualItemSlot(2, NULL);
            break;
    }
    Unit::SetSheath(sheathed);                              // this must visualize Sheath changing for other players...
}

/**
 * @brief Finds an appropriate equipment slot for an item prototype.
 *
 * @param proto The item prototype to equip.
 * @param slot The preferred slot, or NULL_SLOT to auto-select.
 * @param swap True to allow replacing an occupied slot.
 * @return The chosen slot, or NULL_SLOT if none fits.
 */
uint8 Player::FindEquipSlot(ItemPrototype const* proto, uint32 slot, bool swap) const
{
    uint8 pClass = getClass();

    uint8 slots[4];
    slots[0] = NULL_SLOT;
    slots[1] = NULL_SLOT;
    slots[2] = NULL_SLOT;
    slots[3] = NULL_SLOT;
    switch (proto->InventoryType)
    {
        case INVTYPE_HEAD:
            slots[0] = EQUIPMENT_SLOT_HEAD;
            break;
        case INVTYPE_NECK:
            slots[0] = EQUIPMENT_SLOT_NECK;
            break;
        case INVTYPE_SHOULDERS:
            slots[0] = EQUIPMENT_SLOT_SHOULDERS;
            break;
        case INVTYPE_BODY:
            slots[0] = EQUIPMENT_SLOT_BODY;
            break;
        case INVTYPE_CHEST:
            slots[0] = EQUIPMENT_SLOT_CHEST;
            break;
        case INVTYPE_ROBE:
            slots[0] = EQUIPMENT_SLOT_CHEST;
            break;
        case INVTYPE_WAIST:
            slots[0] = EQUIPMENT_SLOT_WAIST;
            break;
        case INVTYPE_LEGS:
            slots[0] = EQUIPMENT_SLOT_LEGS;
            break;
        case INVTYPE_FEET:
            slots[0] = EQUIPMENT_SLOT_FEET;
            break;
        case INVTYPE_WRISTS:
            slots[0] = EQUIPMENT_SLOT_WRISTS;
            break;
        case INVTYPE_HANDS:
            slots[0] = EQUIPMENT_SLOT_HANDS;
            break;
        case INVTYPE_FINGER:
            slots[0] = EQUIPMENT_SLOT_FINGER1;
            slots[1] = EQUIPMENT_SLOT_FINGER2;
            break;
        case INVTYPE_TRINKET:
            slots[0] = EQUIPMENT_SLOT_TRINKET1;
            slots[1] = EQUIPMENT_SLOT_TRINKET2;
            break;
        case INVTYPE_CLOAK:
            slots[0] =  EQUIPMENT_SLOT_BACK;
            break;
        case INVTYPE_WEAPON:
        {
            slots[0] = EQUIPMENT_SLOT_MAINHAND;

            // suggest offhand slot only if know dual wielding
            // (this will be replace mainhand weapon at auto equip instead unwonted "you don't known dual wielding" ...
            if (CanDualWield())
            {
                slots[1] = EQUIPMENT_SLOT_OFFHAND;
            }
            break;
        }
        case INVTYPE_SHIELD:
            slots[0] = EQUIPMENT_SLOT_OFFHAND;
            break;
        case INVTYPE_RANGED:
            slots[0] = EQUIPMENT_SLOT_RANGED;
            break;
        case INVTYPE_2HWEAPON:
            slots[0] = EQUIPMENT_SLOT_MAINHAND;
            break;
        case INVTYPE_TABARD:
            slots[0] = EQUIPMENT_SLOT_TABARD;
            break;
        case INVTYPE_WEAPONMAINHAND:
            slots[0] = EQUIPMENT_SLOT_MAINHAND;
            break;
        case INVTYPE_WEAPONOFFHAND:
            slots[0] = EQUIPMENT_SLOT_OFFHAND;
            break;
        case INVTYPE_HOLDABLE:
            slots[0] = EQUIPMENT_SLOT_OFFHAND;
            break;
        case INVTYPE_THROWN:
            slots[0] = EQUIPMENT_SLOT_RANGED;
            break;
        case INVTYPE_RANGEDRIGHT:
            slots[0] = EQUIPMENT_SLOT_RANGED;
            break;
        case INVTYPE_BAG:
            slots[0] = INVENTORY_SLOT_BAG_START + 0;
            slots[1] = INVENTORY_SLOT_BAG_START + 1;
            slots[2] = INVENTORY_SLOT_BAG_START + 2;
            slots[3] = INVENTORY_SLOT_BAG_START + 3;
            break;
        case INVTYPE_RELIC:
        {
            switch (proto->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_LIBRAM:
                    if (pClass == CLASS_PALADIN)
                    {
                        slots[0] = EQUIPMENT_SLOT_RANGED;
                    }
                    break;
                case ITEM_SUBCLASS_ARMOR_IDOL:
                    if (pClass == CLASS_DRUID)
                    {
                        slots[0] = EQUIPMENT_SLOT_RANGED;
                    }
                    break;
                case ITEM_SUBCLASS_ARMOR_TOTEM:
                    if (pClass == CLASS_SHAMAN)
                    {
                        slots[0] = EQUIPMENT_SLOT_RANGED;
                    }
                    break;
                case ITEM_SUBCLASS_ARMOR_MISC:
                    if (pClass == CLASS_WARLOCK)
                    {
                        slots[0] = EQUIPMENT_SLOT_RANGED;
                    }
                    break;
            }
            break;
        }
        default:
            return NULL_SLOT;
    }

    if (slot != NULL_SLOT)
    {
        if (swap || !GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            for (int i = 0; i < 4; ++i)
            {
                if (slots[i] == slot)
                {
                    return slot;
                }
            }
        }
    }
    else
    {
        // search free slot at first
        for (int i = 0; i < 4; ++i)
        {
            if (slots[i] != NULL_SLOT && !GetItemByPos(INVENTORY_SLOT_BAG_0, slots[i]))
            {
                // in case 2hand equipped weapon offhand slot empty but not free
                if (slots[i] != EQUIPMENT_SLOT_OFFHAND || !IsTwoHandUsed())
                {
                    return slots[i];
                }
            }
        }

        // if not found free and can swap return first appropriate from used
        for (int i = 0; i < 4; ++i)
        {
            if (slots[i] != NULL_SLOT && swap)
            {
                return slots[i];
            }
        }
    }

    // no free position
    return NULL_SLOT;
}

/**
 * @brief Checks whether enough matching items can be unequipped or removed.
 *
 * @param item The item entry to search for.
 * @param count The required total item count.
 * @return The inventory result describing whether the request is allowed.
 */
InventoryResult Player::CanUnequipItems(uint32 item, uint32 count) const
{
    Item* pItem;
    uint32 tempcount = 0;

    InventoryResult res = EQUIP_ERR_OK;

    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            InventoryResult ires = CanUnequipItem(INVENTORY_SLOT_BAG_0 << 8 | i, false);
            if (ires == EQUIP_ERR_OK)
            {
                tempcount += pItem->GetCount();
                if (tempcount >= count)
                {
                    return EQUIP_ERR_OK;
                }
            }
            else
            {
                res = ires;
            }
        }
    }
    for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
            {
                return EQUIP_ERR_OK;
            }
        }
    }
    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem->GetEntry() == item)
        {
            tempcount += pItem->GetCount();
            if (tempcount >= count)
            {
                return EQUIP_ERR_OK;
            }
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pBag)
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                pItem = GetItemByPos(i, j);
                if (pItem && pItem->GetEntry() == item)
                {
                    tempcount += pItem->GetCount();
                    if (tempcount >= count)
                    {
                        return EQUIP_ERR_OK;
                    }
                }
            }
        }
    }

    // not found req. item count and have unequippable items
    return res;
}

/**
 * @brief Counts matching items owned by the player.
 *
 * @param item The item entry to count.
 * @param inBankAlso True to include bank storage.
 * @param skipItem An item instance to exclude from the count.
 * @return The total matching item count.
 */
uint32 Player::GetItemCount(uint32 item, bool inBankAlso, Item* skipItem) const
{
    uint32 count = 0;
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem != skipItem &&  pItem->GetEntry() == item)
        {
            count += pItem->GetCount();
        }
    }
    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pItem && pItem != skipItem && pItem->GetEntry() == item)
        {
            count += pItem->GetCount();
        }
    }
    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
        if (pBag)
        {
            count += pBag->GetItemCount(item, skipItem);
        }
    }

    if (inBankAlso)
    {
        for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
        {
            Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pItem && pItem != skipItem && pItem->GetEntry() == item)
            {
                count += pItem->GetCount();
            }
        }
        for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
        {
            Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i);
            if (pBag)
            {
                count += pBag->GetItemCount(item, skipItem);
            }
        }
    }

    return count;
}

/**
 * @brief Finds the first owned item matching an entry identifier.
 *
 * @param item The item entry to search for.
 * @return The first matching item, or null if none is found.
 */
Item* Player::GetItemByEntry(uint32 item) const
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetEntry() == item)
            {
                return pItem;
            }
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (Item* itemPtr = pBag->GetItemByEntry(item))
            {
                return itemPtr;
            }
        }
    }

    return NULL;
}

/**
 * @brief Finds an owned item by GUID across inventory and bank storage.
 *
 * @param guid The item GUID to search for.
 * @return The matching item, or null if none is found.
 */
Item* Player::GetItemByGuid(ObjectGuid guid) const
{
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetObjectGuid() == guid)
            {
                return pItem;
            }
        }
    }

    for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetObjectGuid() == guid)
            {
                return pItem;
            }
        }
    }

    for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->GetObjectGuid() == guid)
                    {
                        return pItem;
                    }
                }
            }
        }
    }

    for (int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            if (pItem->GetObjectGuid() == guid)
            {
                return pItem;
            }
        }
    }

    for (int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
    {
        if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
            {
                if (Item* pItem = pBag->GetItemByPos(j))
                {
                    if (pItem->GetObjectGuid() == guid)
                    {
                        return pItem;
                    }
                }
            }
        }
    }

    return NULL;
}

/**
 * @brief Gets an item from a packed inventory position.
 *
 * @param pos The packed bag and slot position.
 * @return The item at that position, or null if empty.
 */
Item* Player::GetItemByPos(uint16 pos) const
{
    uint8 bag = pos >> 8;
    uint8 slot = pos & 255;
    return GetItemByPos(bag, slot);
}

/**
 * @brief Gets an item from a specific bag and slot location.
 *
 * @param bag The bag identifier.
 * @param slot The slot within the bag or inventory.
 * @return The item at that position, or null if empty.
 */
Item* Player::GetItemByPos(uint8 bag, uint8 slot) const
{
    if (bag == INVENTORY_SLOT_BAG_0 &&
        (slot < BANK_SLOT_BAG_END  || // Equiped, Default Bagpack Inventory, Default Bank SLots
        slot >= BUYBACK_SLOT_START) &&
        slot < KEYRING_SLOT_END)
    {
        return m_items[slot];
    }
    else if ((bag >= INVENTORY_SLOT_BAG_START && bag < INVENTORY_SLOT_BAG_END) ||
        (bag >= BANK_SLOT_BAG_START && bag < BANK_SLOT_BAG_END))
    {
        Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
        if (pBag)
        {
            return pBag->GetItemByPos(slot);
        }
    }
    return NULL;
}

/**
 * @brief Gets the equipped weapon used for a specific attack type.
 *
 * @param attackType The attack type to inspect.
 * @param nonbroken True to reject broken weapons.
 * @param useable True to require the weapon to be currently usable.
 * @return The weapon item, or null if none qualifies.
 */
Item* Player::GetWeaponForAttack(WeaponAttackType attackType, bool nonbroken, bool useable) const
{
    uint8 slot;
    switch (attackType)
    {
        case BASE_ATTACK:   slot = EQUIPMENT_SLOT_MAINHAND; break;
        case OFF_ATTACK:    slot = EQUIPMENT_SLOT_OFFHAND;  break;
        case RANGED_ATTACK: slot = EQUIPMENT_SLOT_RANGED;   break;
        default: return NULL;
    }

    Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
    if (!item || item->GetProto()->Class != ITEM_CLASS_WEAPON)
    {
        return NULL;
    }

    if (useable && !CanUseEquippedWeapon(attackType))
    {
        return NULL;
    }

    if (nonbroken && item->IsBroken())
    {
        return NULL;
    }

    return item;
}

/**
 * @brief Gets the equipped shield item, if any.
 *
 * @param useable True to require the shield to be usable and not broken.
 * @return The shield item, or null if none qualifies.
 */
Item* Player::GetShield(bool useable) const
{
    Item* item = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (!item || item->GetProto()->Class != ITEM_CLASS_ARMOR)
    {
        return NULL;
    }

    if (!useable)
    {
        return item;
    }

    if (item->IsBroken() || !CanUseEquippedWeapon(OFF_ATTACK))
    {
        return NULL;
    }

    return item;
}

/**
 * @brief Maps an equipment slot to its corresponding attack type.
 *
 * @param slot The equipment slot to inspect.
 * @return The mapped attack type, or MAX_ATTACK if not weapon-related.
 */
uint32 Player::GetAttackBySlot(uint8 slot)
{
    switch (slot)
    {
        case EQUIPMENT_SLOT_MAINHAND: return BASE_ATTACK;
        case EQUIPMENT_SLOT_OFFHAND:  return OFF_ATTACK;
        case EQUIPMENT_SLOT_RANGED:   return RANGED_ATTACK;
        default:                      return MAX_ATTACK;
    }
}
