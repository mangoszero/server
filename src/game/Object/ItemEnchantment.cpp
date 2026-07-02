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



#include "Item.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "WorldPacket.h"
#include "Database/DatabaseEnv.h"
#include "ItemEnchantmentMgr.h"
#include "SQLStorages.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Sets enchantment data for a slot.
 *
 * @param slot The enchantment slot.
 * @param id The enchantment id.
 * @param duration The enchantment duration.
 * @param charges The remaining enchantment charges.
 */
void Item::SetEnchantment(EnchantmentSlot slot, uint32 id, uint32 duration, uint32 charges)
{
    // Better lost small time at check in comparison lost time at item save to DB.
    if ((GetEnchantmentId(slot) == id) && (GetEnchantmentDuration(slot) == duration) && (GetEnchantmentCharges(slot) == charges))
    {
        return;
    }

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_ID_OFFSET, id);
    SetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET, duration);
    SetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET, charges);
    SetState(ITEM_CHANGED);
}

/**
 * @brief Updates the duration for an enchantment slot.
 *
 * @param slot The enchantment slot.
 * @param duration The new duration.
 */
void Item::SetEnchantmentDuration(EnchantmentSlot slot, uint32 duration)
{
    if (GetEnchantmentDuration(slot) == duration)
    {
        return;
    }

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_DURATION_OFFSET, duration);
    SetState(ITEM_CHANGED);
}

/**
 * @brief Updates the charges for an enchantment slot.
 *
 * @param slot The enchantment slot.
 * @param charges The new charge count.
 */
void Item::SetEnchantmentCharges(EnchantmentSlot slot, uint32 charges)
{
    if (GetEnchantmentCharges(slot) == charges)
    {
        return;
    }

    SetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + ENCHANTMENT_CHARGES_OFFSET, charges);
    SetState(ITEM_CHANGED);
}

/**
 * @brief Clears all enchantment data from a slot.
 *
 * @param slot The enchantment slot.
 */
void Item::ClearEnchantment(EnchantmentSlot slot)
{
    if (!GetEnchantmentId(slot))
    {
        return;
    }

    for (uint8 x = 0; x < 3; ++x)
    {
        SetUInt32Value(ITEM_FIELD_ENCHANTMENT + slot * MAX_ENCHANTMENT_OFFSET + x, 0);
    }
    SetState(ITEM_CHANGED);
}
