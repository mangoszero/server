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
 * @brief Initializes a new item with prototype and owner data.
 *
 * @param guidlow The low GUID for the item.
 * @param itemid The item entry identifier.
 * @param owner The initial owner.
 * @return true if creation succeeded; otherwise, false.
 */
bool Item::Create(uint32 guidlow, uint32 itemid, Player const* owner)
{
    Object::_Create(guidlow, 0, HIGHGUID_ITEM);

    SetEntry(itemid);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(ITEM_FIELD_OWNER, owner ? owner->GetObjectGuid() : ObjectGuid());
    SetGuidValue(ITEM_FIELD_CONTAINED, ObjectGuid());

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(itemid);
    if (!itemProto)
    {
        return false;
    }

    SetUInt32Value(ITEM_FIELD_STACK_COUNT, 1);
    SetUInt32Value(ITEM_FIELD_MAXDURABILITY, itemProto->MaxDurability);
    SetUInt32Value(ITEM_FIELD_DURABILITY, itemProto->MaxDurability);

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        SetSpellCharges(i, itemProto->Spells[i].SpellCharges);
    }

    SetUInt32Value(ITEM_FIELD_DURATION, itemProto->Duration);

    return true;
}

/**
 * @brief Checks whether this item is a bag that still contains items.
 *
 * @return true if the item is a non-empty bag; otherwise, false.
 */
bool Item::IsNotEmptyBag() const
{
    if (Bag const* bag = ToBag())
    {
        return !bag->IsEmpty();
    }
    return false;
}

/**
 * @brief Updates remaining duration for a temporary item.
 *
 * @param owner The owning player.
 * @param diff Elapsed time in milliseconds.
 */
void Item::UpdateDuration(Player* owner, uint32 diff)
{
    if (!GetUInt32Value(ITEM_FIELD_DURATION))
    {
        return;
    }

    // DEBUG_LOG("Item::UpdateDuration Item (Entry: %u Duration %u Diff %u)", GetEntry(), GetUInt32Value(ITEM_FIELD_DURATION), diff);

    if (GetUInt32Value(ITEM_FIELD_DURATION) <= diff)
    {
        // Used by Eluna
#ifdef ENABLE_ELUNA
        if (Eluna* e = owner->GetEluna())
        {
            e->OnExpire(owner, GetProto());
        }
#endif /* ENABLE_ELUNA */
        owner->DestroyItem(GetBagSlot(), GetSlot(), true);
        return;
    }

    SetUInt32Value(ITEM_FIELD_DURATION, GetUInt32Value(ITEM_FIELD_DURATION) - diff);
    SetState(ITEM_CHANGED, owner);                          // save new time in database
}

/**
 * @brief Persists the item instance and saved loot state to the database.
 */
void Item::SaveToDB()
{
    uint32 guid = GetGUIDLow();
    switch (uState)
    {
        case ITEM_NEW:
        {
            static SqlStatementID delItem ;
            static SqlStatementID insItem ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(delItem, "DELETE FROM `item_instance` WHERE `guid` = ?");
            stmt.PExecute(guid);

            std::ostringstream ss;
            for (uint16 i = 0; i < m_valuesCount; ++i)
            {
                ss << GetUInt32Value(i) << " ";
            }

            stmt = CharacterDatabase.CreateStatement(insItem, "INSERT INTO `item_instance` (`guid`,`owner_guid`,`data`,`text`) VALUES (?, ?, ?, ?)");
            stmt.PExecute(guid, GetOwnerGuid().GetCounter(), ss.str().c_str(), m_text.c_str());
        } break;
        case ITEM_CHANGED:
        {
            std::string text = m_text;
            CharacterDatabase.escape_string(text);
            static SqlStatementID updInstance ;
            static SqlStatementID updGifts ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(updInstance, "UPDATE `item_instance` SET `data` = ?, `owner_guid` = ?, `text` = ? WHERE `guid` = ?");

            std::ostringstream ss;
            for (uint16 i = 0; i < m_valuesCount; ++i)
            {
                ss << GetUInt32Value(i) << " ";
            }

            stmt.PExecute(ss.str().c_str(), GetOwnerGuid().GetCounter(), m_text.c_str(), guid);

            if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                stmt = CharacterDatabase.CreateStatement(updGifts, "UPDATE `character_gifts` SET `guid` = ? WHERE `item_guid` = ?");
                stmt.PExecute(GetOwnerGuid().GetCounter(), GetGUIDLow());
            }
        } break;
        case ITEM_REMOVED:
        {
            static SqlStatementID delInst ;
            static SqlStatementID delGifts ;
            static SqlStatementID delLoot ;

            SqlStatement stmt = CharacterDatabase.CreateStatement(delInst, "DELETE FROM `item_instance` WHERE `guid` = ?");
            stmt.PExecute(guid);

            if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
            {
                stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
                stmt.PExecute(GetGUIDLow());
            }

            if (HasSavedLoot())
            {
                stmt = CharacterDatabase.CreateStatement(delLoot, "DELETE FROM `item_loot` WHERE `guid` = ?");
                stmt.PExecute(GetGUIDLow());
            }

            delete this;
            return;
        }
        case ITEM_UNCHANGED:
            return;
    }

    if (m_lootState == ITEM_LOOT_CHANGED || m_lootState == ITEM_LOOT_REMOVED)
    {
        static SqlStatementID delLoot ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(delLoot, "DELETE FROM `item_loot` WHERE `guid` = ?");
        stmt.PExecute(GetGUIDLow());
    }

    if (m_lootState == ITEM_LOOT_NEW || m_lootState == ITEM_LOOT_CHANGED)
    {
        if (Player* owner = GetOwner())
        {
            static SqlStatementID saveGold ;
            static SqlStatementID saveLoot ;

            // save money as 0 itemid data
            if (loot.gold)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(saveGold, "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`property`) VALUES (?, ?, 0, ?, 0)");
                stmt.PExecute(GetGUIDLow(), owner->GetGUIDLow(), loot.gold);
            }

            SqlStatement stmt = CharacterDatabase.CreateStatement(saveLoot, "INSERT INTO `item_loot` (`guid`,`owner_guid`,`itemid`,`amount`,`property`) VALUES (?, ?, ?, ?, ?)");

            // save items and quest items (at load its all will added as normal, but this not important for item loot case)
            for (size_t i = 0; i < loot.GetMaxSlotInLootFor(owner); ++i)
            {
                QuestItem* qitem = NULL;

                LootItem* item = loot.LootItemInSlot(i, owner, &qitem);
                if (!item)
                {
                    continue;
                }

                // questitems use the blocked field for other purposes
                if (!qitem && item->is_blocked)
                {
                    continue;
                }

                stmt.addUInt32(GetGUIDLow());
                stmt.addUInt32(owner->GetGUIDLow());
                stmt.addUInt32(item->itemid);
                stmt.addUInt8(item->count);
                stmt.addInt32(item->randomPropertyId);

                stmt.Execute();
            }
        }
    }

    if (m_lootState != ITEM_LOOT_NONE && m_lootState != ITEM_LOOT_TEMPORARY)
    {
        SetLootState(ITEM_LOOT_UNCHANGED);
    }

    SetState(ITEM_UNCHANGED);
}

/**
 * @brief Loads an item instance from database fields.
 *
 * @param guidLow The low GUID of the item.
 * @param fields The database row fields.
 * @param ownerGuid The expected owner GUID.
 * @return true if the item loaded successfully; otherwise, false.
 */
bool Item::LoadFromDB(uint32 guidLow, Field* fields, ObjectGuid ownerGuid)
{
    // create item before any checks for store correct guid
    // and allow use "FSetState(ITEM_REMOVED); SaveToDB();" for deleting item from DB
    Object::_Create(guidLow, 0, HIGHGUID_ITEM);

    if (!LoadValues(fields[0].GetString()))
    {
        sLog.outError("Item #%d have broken data in `data` field. Can't be loaded.", guidLow);
        return false;
    }

    SetText(fields[1].GetCppString());

    bool need_save = false;                                 // need explicit save data at load fixes

    // overwrite possible wrong/corrupted guid
    ObjectGuid new_item_guid = ObjectGuid(HIGHGUID_ITEM, guidLow);
    if (GetGuidValue(OBJECT_FIELD_GUID) != new_item_guid)
    {
        SetGuidValue(OBJECT_FIELD_GUID, new_item_guid);
        need_save = true;
    }

    ItemPrototype const* proto = GetProto();
    if (!proto)
    {
        return false;
    }

    // update max durability (and durability) if need
    if (proto->MaxDurability != GetUInt32Value(ITEM_FIELD_MAXDURABILITY))
    {
        SetUInt32Value(ITEM_FIELD_MAXDURABILITY, proto->MaxDurability);
        if (GetUInt32Value(ITEM_FIELD_DURABILITY) > proto->MaxDurability)
        {
            SetUInt32Value(ITEM_FIELD_DURABILITY, proto->MaxDurability);
        }

        need_save = true;
    }

    // Remove bind flag for items vs NO_BIND set
    if (IsSoulBound() && proto->Bonding == NO_BIND)
    {
        ApplyModFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_BINDED, false);
        need_save = true;
    }

    // update duration if need, and remove if not need
    if ((proto->Duration == 0) != (GetUInt32Value(ITEM_FIELD_DURATION) == 0))
    {
        SetUInt32Value(ITEM_FIELD_DURATION, proto->Duration);
        need_save = true;
    }

    // set correct owner
    if (ownerGuid && GetOwnerGuid() != ownerGuid)
    {
        SetOwnerGuid(ownerGuid);
        need_save = true;
    }

    // set correct wrapped state
    if (HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
    {
        // wrapped item must be wrapper (used version that not stackable)
        if (!(proto->Flags & ITEM_FLAG_WRAPPER) || GetMaxStackCount() > 1)
        {
            RemoveFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED);
            need_save = true;

            static SqlStatementID delGifts ;

            // also cleanup for sure gift table
            SqlStatement stmt = CharacterDatabase.CreateStatement(delGifts, "DELETE FROM `character_gifts` WHERE `item_guid` = ?");
            stmt.PExecute(GetGUIDLow());
        }
    }

    if (need_save)                                          // normal item changed state set not work at loading
    {
        static SqlStatementID updItem ;

        SqlStatement stmt = CharacterDatabase.CreateStatement(updItem, "UPDATE `item_instance` SET `data` = ?, `owner_guid` = ? WHERE `guid` = ?");

        std::ostringstream ss;
        for (uint16 i = 0; i < m_valuesCount; ++i)
        {
            ss << GetUInt32Value(i) << " ";
        }

        stmt.addString(ss);
        stmt.addUInt32(GetOwnerGuid().GetCounter());
        stmt.addUInt32(guidLow);
        stmt.Execute();
    }

    return true;
}

/**
 * @brief Loads persisted item loot data from the database.
 *
 * @param fields The loot database row fields.
 */
void Item::LoadLootFromDB(Field* fields)
{
    uint32 item_id     = fields[1].GetUInt32();
    uint32 item_amount = fields[2].GetUInt32();
    int32  item_propid = fields[3].GetInt32();

    // money value special case
    if (item_id == 0)
    {
        loot.gold = item_amount;
        SetLootState(ITEM_LOOT_UNCHANGED);
        return;
    }

    // normal item case
    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

    if (!proto)
    {
        CharacterDatabase.PExecute("DELETE FROM `item_loot` WHERE `guid` = '%u' AND `itemid` = '%u'", GetGUIDLow(), item_id);
        sLog.outError("Item::LoadLootFromDB: %s has an unknown item (id: #%u) in item_loot, deleted.", GetOwnerGuid().GetString().c_str(), item_id);
        return;
    }

    loot.items.push_back(LootItem(item_id, item_amount, item_propid));
    ++loot.unlootedCount;

    SetLootState(ITEM_LOOT_UNCHANGED);
}

/**
 * @brief Deletes the item instance record from the database.
 */
void Item::DeleteFromDB()
{
    static SqlStatementID delItem ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delItem, "DELETE FROM `item_instance` WHERE `guid` = ?");
    stmt.PExecute(GetGUIDLow());
}

/**
 * @brief Deletes the character inventory link for this item.
 */
void Item::DeleteFromInventoryDB()
{
    static SqlStatementID delInv ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(delInv, "DELETE FROM `character_inventory` WHERE `item` = ?");
    stmt.PExecute(GetGUIDLow());
}
