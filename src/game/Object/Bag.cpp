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

#include "Bag.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "UpdateData.h"

/**
 * @brief Creates an empty bag item instance.
 */
Bag::Bag(): Item()
{
    m_objectType |= (TYPEMASK_ITEM | TYPEMASK_CONTAINER);
    m_objectTypeId = TYPEID_CONTAINER;

    m_valuesCount = CONTAINER_END;

    memset(m_bagslot, 0, sizeof(Item*) * MAX_BAG_SIZE);
}

/**
 * @brief Destroys the bag and deletes any contained item pointers.
 */
Bag::~Bag()
{
    for (int i = 0; i < MAX_BAG_SIZE; ++i)
    {
        delete m_bagslot[i];
    }
}

/**
 * @brief Adds the bag and all contained items to the world.
 */
void Bag::AddToWorld()
{
    Item::AddToWorld();

    for (uint32 i = 0;  i < GetBagSize(); ++i)
    {
        if (m_bagslot[i])
        {
            m_bagslot[i]->AddToWorld();
        }
    }
}

/**
 * @brief Removes the bag and all contained items from the world.
 */
void Bag::RemoveFromWorld()
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
    {
        if (m_bagslot[i])
        {
            m_bagslot[i]->RemoveFromWorld();
        }
    }
    Item::RemoveFromWorld();
}

/**
 * @brief Creates bag data from an item template.
 *
 * Initializes ownership, durability, stack count, and all bag slots.
 *
 * @param guidlow The low part of the bag GUID.
 * @param itemid The item entry used as the bag template.
 * @param owner The owning player, if any.
 * @return true if the bag was created successfully; otherwise, false.
 */
bool Bag::Create(uint32 guidlow, uint32 itemid, Player const* owner)
{
    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);

    if (!itemProto || itemProto->ContainerSlots > MAX_BAG_SIZE)
    {
        return false;
    }

    Object::_Create(guidlow, 0, HIGHGUID_CONTAINER);

    SetEntry(itemid);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(ITEM_FIELD_OWNER, owner ? owner->GetObjectGuid() : ObjectGuid());
    SetGuidValue(ITEM_FIELD_CONTAINED, owner ? owner->GetObjectGuid() : ObjectGuid());

    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);

    // Setting the number of Slots the Container has
    SetUInt32Value(CONTAINER_FIELD_NUM_SLOTS, itemProto->ContainerSlots);

    // Cleaning 20 slots
    for (uint8 i = 0; i < MAX_BAG_SIZE; ++i)
    {
        SetGuidValue(CONTAINER_FIELD_SLOT_1 + (i * 2), ObjectGuid());
        m_bagslot[i] = NULL;
    }

    return true;
}

/**
 * @brief Saves the bag state to the database.
 */
void Bag::SaveToDB()
{
    Item::SaveToDB();
}

/**
 * @brief Loads the bag state from the database.
 *
 * Clears slot state so contained items can be rebuilt from character inventory data.
 *
 * @param guidLow The low part of the bag GUID.
 * @param fields The database field array containing the bag data.
 * @param ownerGuid The owner GUID for the bag.
 * @return true if the bag loaded successfully; otherwise, false.
 */
bool Bag::LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid)
{
    if (!Item::LoadFromDB(guidLow, fields, ownerGuid))
    {
        return false;
    }

    // cleanup bag content related item value fields (its will be filled correctly from `character_inventory`)
    for (int i = 0; i < MAX_BAG_SIZE; ++i)
    {
        SetGuidValue(CONTAINER_FIELD_SLOT_1 + (i * 2), ObjectGuid());

        delete m_bagslot[i];
        m_bagslot[i] = NULL;
    }

    return true;
}

/**
 * @brief Deletes the bag and its contained items from the database.
 */
void Bag::DeleteFromDB()
{
    for (int i = 0; i < MAX_BAG_SIZE; ++i)
    {
        if (m_bagslot[i])
        {
            m_bagslot[i]->DeleteFromDB();
        }
    }
    Item::DeleteFromDB();
}

/**
 * @brief Counts the number of free slots in the bag.
 *
 * @return The number of currently empty bag slots.
 */
uint32 Bag::GetFreeSlots() const
{
    uint32 slots = 0;
    for (uint32 i = 0; i < GetBagSize(); ++i)
    {
        if (!m_bagslot[i])
        {
            ++slots;
        }
    }
    return slots;
}

/**
 * @brief Removes an item from a bag slot.
 *
 * @param slot The slot index to clear.
 */
void Bag::RemoveItem(uint8 slot)
{
    MANGOS_ASSERT(slot < MAX_BAG_SIZE);

    if (m_bagslot[slot])
    {
        m_bagslot[slot]->SetContainer(NULL);
    }

    m_bagslot[slot] = NULL;
    SetGuidValue(CONTAINER_FIELD_SLOT_1 + (slot * 2), ObjectGuid());
}

/**
 * @brief Stores an item in a bag slot.
 *
 * @param slot The destination slot index.
 * @param pItem The item to store.
 */
void Bag::StoreItem(uint8 slot, Item* pItem)
{
    MANGOS_ASSERT(slot < MAX_BAG_SIZE);

    if (pItem)
    {
        m_bagslot[slot] = pItem;
        SetGuidValue(CONTAINER_FIELD_SLOT_1 + (slot * 2), pItem->GetObjectGuid());
        pItem->SetGuidValue(ITEM_FIELD_CONTAINED, GetObjectGuid());
        pItem->SetGuidValue(ITEM_FIELD_OWNER, GetOwnerGuid());
        pItem->SetContainer(this);
        pItem->SetSlot(slot);
    }
}

/**
 * @brief Builds creation update data for the bag and its contents.
 *
 * @param data The update buffer to populate.
 * @param target The player receiving the update.
 */
void Bag::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    Item::BuildCreateUpdateBlockForPlayer(data, target);

    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
        {
            m_bagslot[i]->BuildCreateUpdateBlockForPlayer(data, target);
        }
}

/**
 * @brief Checks whether the bag contains any items.
 *
 * @return true if every slot is empty; otherwise, false.
 */
bool Bag::IsEmpty() const
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
        {
            return false;
        }

    return true;
}

/**
 * @brief Finds the first item in the bag matching an entry ID.
 *
 * @param item The item entry to search for.
 * @return Pointer to the matching item, or NULL if not found.
 */
Item* Bag::GetItemByEntry(uint32 item) const
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i] && m_bagslot[i]->GetEntry() == item)
        {
            return m_bagslot[i];
        }

    return NULL;
}

/**
 * @brief Counts items in the bag matching an entry ID.
 *
 * @param item The item entry to count.
 * @param eItem An optional item to exclude from the count.
 * @return The total stack count for matching items.
 */
uint32 Bag::GetItemCount(uint32 item, Item* eItem) const
{
    uint32 count = 0;

    for (uint32 i = 0; i < GetBagSize(); ++i)
        if (m_bagslot[i])
            if (m_bagslot[i] != eItem && m_bagslot[i]->GetEntry() == item)
            {
                count += m_bagslot[i]->GetCount();
            }

    return count;
}

/**
 * @brief Finds the slot containing a specific item GUID.
 *
 * @param guid The GUID of the item to locate.
 * @return The slot index, or NULL_SLOT if the item is not present.
 */
uint8 Bag::GetSlotByItemGUID(ObjectGuid guid) const
{
    for (uint32 i = 0; i < GetBagSize(); ++i)
    {
        if (m_bagslot[i])
        {
            if (m_bagslot[i]->GetObjectGuid() == guid)
            {
                return i;
            }
        }
    }
    return NULL_SLOT;
}

/**
 * @brief Retrieves the item stored in a specific slot.
 *
 * @param slot The slot index to inspect.
 * @return Pointer to the item in the slot, or NULL if the slot is invalid or empty.
 */
Item* Bag::GetItemByPos(uint8 slot) const
{
    if (slot < GetBagSize())
    {
        return m_bagslot[slot];
    }

    return NULL;
}
