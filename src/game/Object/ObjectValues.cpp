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
 * @file Object.cpp
 * @brief Base implementation for all game objects
 *
 * This file implements the Object class, which is the base class for all
 * entities in the game world. It provides:
 * - Update field management (synchronized with clients)
 * - Object GUID handling
 * - Update data building for network transmission
 * - Object visibility and spawning
 * - Type identification
 *
 * The Object class uses an array of uint32 values (update fields) that
 * mirror the client's object state. Changes to these values are sent to
 * players who can see the object.
 */



#include "Object.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "MapManager.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "VMapFactory.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Set signed 32-bit value
 * @param index Field index
 * @param value Value to set
 *
 * Sets a signed 32-bit field value and marks it as changed.
 */
void Object::SetInt32Value(uint16 index, int32 value)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (m_int32Values[index] != value)
    {
        m_int32Values[index] = value;
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Set unsigned 32-bit value
 * @param index Field index
 * @param value Value to set
 *
 * Sets an unsigned 32-bit field value and marks it as changed.
 */
void Object::SetUInt32Value(uint16 index, uint32 value)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (m_uint32Values[index] != value)
    {
        m_uint32Values[index] = value;
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Update unsigned 32-bit value
 * @param index Field index
 * @param value Value to set
 *
 * Sets an unsigned 32-bit field value and marks it as changed.
 * Does not check if value differs from current.
 */
void Object::UpdateUInt32Value(uint16 index, uint32 value)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    m_uint32Values[index] = value;
    m_changedValues[index] = true;
}

/**
 * @brief Set unsigned 64-bit value
 * @param index Field index (uses two consecutive fields)
 * @param value Value to set
 *
 * Sets a 64-bit field value across two consecutive 32-bit fields
 * and marks both as changed.
 */
void Object::SetUInt64Value(uint16 index, const uint64& value)
{
    MANGOS_ASSERT(index + 1 < m_valuesCount || PrintIndexError(index, true));
    if (*((uint64*) & (m_uint32Values[index])) != value)
    {
        m_uint32Values[index] = *((uint32*)&value);
        m_uint32Values[index + 1] = *(((uint32*)&value) + 1);
        m_changedValues[index] = true;
        m_changedValues[index + 1] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Set float value
 * @param index Field index
 * @param value Value to set
 *
 * Sets a floating-point field value and marks it as changed.
 */
void Object::SetFloatValue(uint16 index, float value)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (m_floatValues[index] != value)
    {
        m_floatValues[index] = value;
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Set byte value
 * @param index Field index
 * @param offset Byte offset within the field
 * @param value Value to set
 *
 * Sets a single byte within a 32-bit field and marks it as changed.
 */
void Object::SetByteValue(uint16 index, uint8 offset, uint8 value)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 4)
    {
        sLog.outError("Object::SetByteValue: wrong offset %u", offset);
        return;
    }

    if (uint8(m_uint32Values[index] >> (offset * 8)) != value)
    {
        m_uint32Values[index] &= ~uint32(uint32(0xFF) << (offset * 8));
        m_uint32Values[index] |= uint32(uint32(value) << (offset * 8));
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Set unsigned 16-bit value
 * @param index Field index
 * @param offset 16-bit offset within the field (0 or 1)
 * @param value Value to set
 *
 * Sets a 16-bit value within a 32-bit field and marks it as changed.
 */
void Object::SetUInt16Value(uint16 index, uint8 offset, uint16 value)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 2)
    {
        sLog.outError("Object::SetUInt16Value: wrong offset %u", offset);
        return;
    }

    if (uint16(m_uint32Values[index] >> (offset * 16)) != value)
    {
        m_uint32Values[index] &= ~uint32(uint32(0xFFFF) << (offset * 16));
        m_uint32Values[index] |= uint32(uint32(value) << (offset * 16));
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Set stat float value
 * @param index Field index
 * @param value Value to set
 *
 * Sets a floating-point stat value, clamping to minimum 0.
 */
void Object::SetStatFloatValue(uint16 index, float value)
{
    if (value < 0)
    {
        value = 0.0f;
    }

    SetFloatValue(index, value);
}

/**
 * @brief Set stat int32 value
 * @param index Field index
 * @param value Value to set
 *
 * Sets an integer stat value, clamping to minimum 0.
 */
void Object::SetStatInt32Value(uint16 index, int32 value)
{
    if (value < 0)
    {
        value = 0;
    }

    SetUInt32Value(index, uint32(value));
}

/**
 * @brief Apply modifier to unsigned 32-bit value
 * @param index Field index
 * @param val Modifier value
 * @param apply If true, add modifier; if false, subtract
 *
 * Applies a modifier to a field value, clamping to minimum 0.
 */
void Object::ApplyModUInt32Value(uint16 index, int32 val, bool apply)
{
    int32 cur = GetUInt32Value(index);
    cur += (apply ? val : -val);
    if (cur < 0)
    {
        cur = 0;
    }
    SetUInt32Value(index, cur);
}

/**
 * @brief Apply modifier to signed 32-bit value
 * @param index Field index
 * @param val Modifier value
 * @param apply If true, add modifier; if false, subtract
 *
 * Applies a modifier to a signed field value.
 */
void Object::ApplyModInt32Value(uint16 index, int32 val, bool apply)
{
    int32 cur = GetInt32Value(index);
    cur += (apply ? val : -val);
    SetInt32Value(index, cur);
}

/**
 * @brief Apply modifier to signed float value
 * @param index Field index
 * @param val Modifier value
 * @param apply If true, add modifier; if false, subtract
 *
 * Applies a modifier to a floating-point field value.
 */
void Object::ApplyModSignedFloatValue(uint16 index, float  val, bool apply)
{
    float cur = GetFloatValue(index);
    cur += (apply ? val : -val);
    SetFloatValue(index, cur);
}

/**
 * @brief Apply modifier to positive float value
 * @param index Field index
 * @param val Modifier value
 * @param apply If true, add modifier; if false, subtract
 *
 * Applies a modifier to a floating-point field value,
 * clamping to minimum 0.
 */
void Object::ApplyModPositiveFloatValue(uint16 index, float  val, bool apply)
{
    float cur = GetFloatValue(index);
    cur += (apply ? val : -val);
    if (cur < 0)
    {
        cur = 0;
    }
    SetFloatValue(index, cur);
}

/**
 * @brief Set flag in field
 * @param index Field index
 * @param newFlag Flag to set
 *
 * Sets a flag bit in a field using OR operation.
 */
void Object::SetFlag(uint16 index, uint32 newFlag)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));
    uint32 oldval = m_uint32Values[index];
    uint32 newval = oldval | newFlag;

    if (oldval != newval)
    {
        m_uint32Values[index] = newval;
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Remove flag from field
 * @param index Field index
 * @param oldFlag Flag to remove
 *
 * Removes a flag bit from a field using AND with inverted mask.
 */
void Object::RemoveFlag(uint16 index, uint32 oldFlag)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));
    uint32 oldval = m_uint32Values[index];
    uint32 newval = oldval & ~oldFlag;

    if (oldval != newval)
    {
        m_uint32Values[index] = newval;
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Set byte flag in field
 * @param index Field index
 * @param offset Byte offset within the field
 * @param newFlag Flag to set
 *
 * Sets a flag bit within a byte of a field.
 */
void Object::SetByteFlag(uint16 index, uint8 offset, uint8 newFlag)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 4)
    {
        sLog.outError("Object::SetByteFlag: wrong offset %u", offset);
        return;
    }

    if (!(uint8(m_uint32Values[index] >> (offset * 8)) & newFlag))
    {
        m_uint32Values[index] |= uint32(uint32(newFlag) << (offset * 8));
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Remove byte flag from field
 * @param index Field index
 * @param offset Byte offset within the field
 * @param oldFlag Flag to remove
 *
 * Removes a flag bit within a byte of a field.
 */
void Object::RemoveByteFlag(uint16 index, uint8 offset, uint8 oldFlag)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (offset > 4)
    {
        sLog.outError("Object::RemoveByteFlag: wrong offset %u", offset);
        return;
    }

    if (uint8(m_uint32Values[index] >> (offset * 8)) & oldFlag)
    {
        m_uint32Values[index] &= ~uint32(uint32(oldFlag) << (offset * 8));
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Set short flag in field
 * @param index Field index
 * @param highpart If true, use high 16 bits; if false, use low 16 bits
 * @param newFlag Flag to set
 *
 * Sets a flag bit within a 16-bit portion of a field.
 */
void Object::SetShortFlag(uint16 index, bool highpart, uint16 newFlag)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (!(uint16(m_uint32Values[index] >> (highpart ? 16 : 0)) & newFlag))
    {
        m_uint32Values[index] |= uint32(uint32(newFlag) << (highpart ? 16 : 0));
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Remove short flag from field
 * @param index Field index
 * @param highpart If true, use high 16 bits; if false, use low 16 bits
 * @param oldFlag Flag to remove
 *
 * Removes a flag bit within a 16-bit portion of a field.
 */
void Object::RemoveShortFlag(uint16 index, bool highpart, uint16 oldFlag)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    if (uint16(m_uint32Values[index] >> (highpart ? 16 : 0)) & oldFlag)
    {
        m_uint32Values[index] &= ~uint32(uint32(oldFlag) << (highpart ? 16 : 0));
        m_changedValues[index] = true;
        MarkForClientUpdate();
    }
}

/**
 * @brief Print index error
 * @param index Field index that caused error
 * @param set If true, was a set operation; if false, was a get operation
 * @return Always false
 *
 * Logs an error when attempting to access a nonexistent field.
 */
bool Object::PrintIndexError(uint32 index, bool set) const
{
    sLog.outError("Attempt %s nonexistent value field: %u (count: %u) for object typeid: %u type mask: %u", (set ? "set value to" : "get value from"), index, m_valuesCount, GetTypeId(), m_objectType);

    // ASSERT must fail after function call
    return false;
}

/**
 * @brief Print entry error
 * @param descr Description of the invalid operation
 * @return Always false
 *
 * Logs an error when an invalid operation is performed on this object.
 */
bool Object::PrintEntryError(char const* descr) const
{
    sLog.outError("Object Type %u, Entry %u (lowguid %u) with invalid call for %s", GetTypeId(), GetEntry(), GetObjectGuid().GetCounter(), descr);

    // always false for continue assert fail
    return false;
}

/**
 * @brief Build update data for player
 * @param pl Target player
 * @param update_players Map of players to their update data
 *
 * Builds update data for the specified player, adding them
 * to the update map if not already present.
 */
void Object::BuildUpdateDataForPlayer(Player* pl, UpdateDataMapType& update_players)
{
    UpdateDataMapType::iterator iter = update_players.find(pl);

    if (iter == update_players.end())
    {
        std::pair<UpdateDataMapType::iterator, bool> p = update_players.insert(UpdateDataMapType::value_type(pl, UpdateData()));
        MANGOS_ASSERT(p.second);
        iter = p.first;
    }

    BuildValuesUpdateBlockForPlayer(&iter->second, iter->first);
}

/**
 * @brief Add to client update list
 *
 * Base implementation logs error and asserts.
 * Derived classes should override this method.
 */
void Object::AddToClientUpdateList()
{
    sLog.outError("Unexpected call of Object::AddToClientUpdateList for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
    MANGOS_ASSERT(false);
}

/**
 * @brief Remove from client update list
 *
 * Base implementation logs error and asserts.
 * Derived classes should override this method.
 */
void Object::RemoveFromClientUpdateList()
{
    sLog.outError("Unexpected call of Object::RemoveFromClientUpdateList for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
    MANGOS_ASSERT(false);
}

/**
 * @brief Build update data
 * @param update_players Map of players to their update data
 *
 * Base implementation logs error and asserts.
 * Derived classes should override this method.
 */
void Object::BuildUpdateData(UpdateDataMapType& /*update_players */)
{
    sLog.outError("Unexpected call of Object::BuildUpdateData for object (TypeId: %u Update fields: %u)", GetTypeId(), m_valuesCount);
    MANGOS_ASSERT(false);
}

/**
 * @brief Mark object for client update
 *
 * Adds the object to the client update list if it's in world
 * and not already marked for update.
 */
void Object::MarkForClientUpdate()
{
    if (m_inWorld)
    {
        if (!m_objectUpdated)
        {
            AddToClientUpdateList();
            m_objectUpdated = true;
        }
    }
}
