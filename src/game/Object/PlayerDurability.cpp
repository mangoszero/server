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
#include "CreatureAI.h"
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
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Applies percentage-based durability loss to equipped items and optionally inventory items.
 *
 * @param percent The fraction of maximum durability to remove.
 * @param inventory True to include inventory and bag contents.
 */
void Player::DurabilityLossAll(double percent, bool inventory)
{
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            DurabilityLoss(pItem, percent);
        }
    }

    if (inventory)
    {
        // bags not have durability
        // for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)

        for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                DurabilityLoss(pItem, percent);
            }
        }

        // keys not have durability
        // for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    if (Item* pItem = GetItemByPos(i, j))
                    {
                        DurabilityLoss(pItem, percent);
                    }
                }
            }
        }
    }
}

/**
 * @brief Applies percentage-based durability loss to a single item.
 *
 * @param item The item to damage.
 * @param percent The fraction of maximum durability to remove.
 */
void Player::DurabilityLoss(Item* item, double percent)
{
    if (!item)
    {
        return;
    }

    uint32 pMaxDurability =  item ->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);

    if (!pMaxDurability)
    {
        return;
    }

    uint32 pDurabilityLoss = uint32(pMaxDurability * percent);

    if (pDurabilityLoss < 1)
    {
        pDurabilityLoss = 1;
    }

    DurabilityPointsLoss(item, pDurabilityLoss);
}

/**
 * @brief Applies flat durability loss to equipped items and optionally inventory items.
 *
 * @param points The number of durability points to remove.
 * @param inventory True to include inventory and bag contents.
 */
void Player::DurabilityPointsLossAll(int32 points, bool inventory)
{
    for (int i = EQUIPMENT_SLOT_START; i < EQUIPMENT_SLOT_END; ++i)
    {
        if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
        {
            DurabilityPointsLoss(pItem, points);
        }
    }
    if (inventory)
    {
        // bags not have durability
        // for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)

        for (int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
        {
            if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                DurabilityPointsLoss(pItem, points);
            }
        }

        // keys not have durability
        // for (int i = KEYRING_SLOT_START; i < KEYRING_SLOT_END; ++i)

        for (int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
        {
            if (Bag* pBag = (Bag*)GetItemByPos(INVENTORY_SLOT_BAG_0, i))
            {
                for (uint32 j = 0; j < pBag->GetBagSize(); ++j)
                {
                    if (Item* pItem = GetItemByPos(i, j))
                    {
                        DurabilityPointsLoss(pItem, points);
                    }
                }
            }
        }
    }
}

/**
 * @brief Applies flat durability loss to a single item.
 *
 * @param item The item to damage.
 * @param points The number of durability points to remove.
 */
void Player::DurabilityPointsLoss(Item* item, int32 points)
{
    int32 pMaxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    int32 pOldDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);
    int32 pNewDurability = pOldDurability - points;

    if (pNewDurability < 0)
    {
        pNewDurability = 0;
    }
    else if (pNewDurability > pMaxDurability)
    {
        pNewDurability = pMaxDurability;
    }

    if (pOldDurability != pNewDurability)
    {
        // modify item stats _before_ Durability set to 0 to pass _ApplyItemMods internal check
        if (pNewDurability == 0 && pOldDurability > 0 && item->IsEquipped())
        {
            _ApplyItemMods(item, item->GetSlot(), false);
        }

        item->SetUInt32Value(ITEM_FIELD_DURABILITY, pNewDurability);

        // modify item stats _after_ restore durability to pass _ApplyItemMods internal check
        if (pNewDurability > 0 && pOldDurability == 0 && item->IsEquipped())
        {
            _ApplyItemMods(item, item->GetSlot(), true);
        }

        item->SetState(ITEM_CHANGED, this);
    }
}

/**
 * @brief Removes one durability point from an equipped item slot.
 *
 * @param slot The equipment slot to damage.
 */
void Player::DurabilityPointLossForEquipSlot(EquipmentSlots slot)
{
    if (Item* pItem = GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
    {
        DurabilityPointsLoss(pItem, 1);
    }
}

/**
 * @brief Repairs all eligible equipped and carried items.
 *
 * @param cost True to charge the player for repairs.
 * @param discountMod The vendor discount multiplier to apply.
 * @return The total repair cost.
 */
uint32 Player::DurabilityRepairAll(bool cost, float discountMod)
{
    uint32 TotalCost = 0;
    // equipped, backpack, bags itself
    for (int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_ITEM_END; ++i)
    {
        TotalCost += DurabilityRepair(((INVENTORY_SLOT_BAG_0 << 8) | i), cost, discountMod);
    }

    // bank, buyback and keys not repaired

    // items in inventory bags
    for (int j = INVENTORY_SLOT_BAG_START; j < INVENTORY_SLOT_BAG_END; ++j)
    {
        for (int i = 0; i < MAX_BAG_SIZE; ++i)
        {
            TotalCost += DurabilityRepair(((j << 8) | i), cost, discountMod);
        }
    }
    return TotalCost;
}

/**
 * @brief Repairs a single item to full durability.
 *
 * @param pos The packed inventory position of the item.
 * @param cost True to charge the player for the repair.
 * @param discountMod The vendor discount multiplier to apply.
 * @return The repair cost for the item.
 */
uint32 Player::DurabilityRepair(uint16 pos, bool cost, float discountMod)
{
    Item* item = GetItemByPos(pos);

    uint32 TotalCost = 0;
    if (!item)
    {
        return TotalCost;
    }

    uint32 maxDurability = item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
    if (!maxDurability)
    {
        return TotalCost;
    }

    uint32 curDurability = item->GetUInt32Value(ITEM_FIELD_DURABILITY);

    if (cost)
    {
        uint32 LostDurability = maxDurability - curDurability;
        if (LostDurability > 0)
        {
            ItemPrototype const* ditemProto = item->GetProto();

            DurabilityCostsEntry const* dcost = sDurabilityCostsStore.LookupEntry(ditemProto->ItemLevel);
            if (!dcost)
            {
                sLog.outError("RepairDurability: Wrong item lvl %u", ditemProto->ItemLevel);
                return TotalCost;
            }

            uint32 dQualitymodEntryId = (ditemProto->Quality + 1) * 2;
            DurabilityQualityEntry const* dQualitymodEntry = sDurabilityQualityStore.LookupEntry(dQualitymodEntryId);
            if (!dQualitymodEntry)
            {
                sLog.outError("RepairDurability: Wrong dQualityModEntry %u", dQualitymodEntryId);
                return TotalCost;
            }

            uint32 dmultiplier = dcost->WeaponSubClassCost[ItemSubClassToDurabilityMultiplierId(ditemProto->Class, ditemProto->SubClass)];
            uint32 costs = uint32(LostDurability * dmultiplier * double(dQualitymodEntry->quality_mod));

            costs = uint32(costs * discountMod);

            if (costs == 0)                                 // fix for ITEM_QUALITY_ARTIFACT
            {
                costs = 1;
            }

            if (GetMoney() < costs)
            {
                DEBUG_LOG("You do not have enough money");
                return TotalCost;
            }
            else
            {
                ModifyMoney(-int32(costs));
            }
        }
    }

    item->SetUInt32Value(ITEM_FIELD_DURABILITY, maxDurability);
    item->SetState(ITEM_CHANGED, this);

    // reapply mods for total broken and repaired item if equipped
    if (IsEquipmentPos(pos) && !curDurability)
    {
        _ApplyItemMods(item, pos & 255, true);
    }
    return TotalCost;
}
