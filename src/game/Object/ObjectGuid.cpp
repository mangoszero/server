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

#include "ObjectGuid.h"

#include "World.h"
#include "ObjectMgr.h"

#include <sstream>

/**
 * @brief Gets a human-readable type name for a high GUID category.
 *
 * @param high The high GUID type.
 * @return The textual type name.
 */
char const* ObjectGuid::GetTypeName(HighGuid high)
{
    switch (high)
    {
        case HIGHGUID_ITEM:         return "Item";
        case HIGHGUID_PLAYER:       return "Player";
        case HIGHGUID_GAMEOBJECT:   return "Gameobject";
        case HIGHGUID_TRANSPORT:    return "Transport";
        case HIGHGUID_UNIT:         return "Creature";
        case HIGHGUID_PET:          return "Pet";
        case HIGHGUID_DYNAMICOBJECT: return "DynObject";
        case HIGHGUID_CORPSE:       return "Corpse";
        case HIGHGUID_MO_TRANSPORT: return "MoTransport";
        default:
            return "<unknown>";
    }
}

/**
 * @brief Builds a readable string representation of the object GUID.
 *
 * @return The formatted GUID string.
 */
std::string ObjectGuid::GetString() const
{
    std::ostringstream str;
    str << GetTypeName();

    if (IsPlayer())
    {
        std::string name;
        if (sObjectMgr.GetPlayerNameByGUID(*this, name))
        {
            str << " " << name;
        }
    }

    str << " (";
    if (HasEntry())
    {
        str << (IsPet() ? "Petnumber: " : "Entry: ") << GetEntry() << " ";
    }
    str << "Guid: " << GetCounter() << ")";
    return str.str();
}

template<HighGuid high>

/**
 * @brief Generates the next GUID counter for a specific high GUID type.
 *
 * @return The generated counter value.
 */
uint32 ObjectGuidGenerator<high>::Generate()
{
    if (m_nextGuid >= ObjectGuid::GetMaxCounter(high) - 1)
    {
        sLog.outError("%s guid overflow!! Can't continue, shutting down server. ", ObjectGuid::GetTypeName(high));
        World::StopNow(ERROR_EXIT_CODE);
        // World::StopNow() only sets a deferred stop flag, so this call still
        // returns. Do NOT hand out m_nextGuid here: at the overflow boundary it
        // equals GetMaxCounter-1, which for players is the reserved AH bot
        // system-owner GUID (0xFFFFFFFE) -- returning it could brand a real
        // character with the forged owner's GUID before the shutdown takes
        // effect. Return the top-of-range value instead (never the reserved one);
        // the server is stopping regardless.
        return ObjectGuid::GetMaxCounter(high);
    }
    return m_nextGuid++;
}

/**
 * @brief Writes an object GUID to a byte buffer.
 *
 * @param buf The destination buffer.
 * @param guid The GUID to write.
 * @return The updated buffer.
 */
ByteBuffer& operator<< (ByteBuffer& buf, ObjectGuid const& guid)
{
    buf << uint64(guid.GetRawValue());
    return buf;
}

/**
 * @brief Reads an object GUID from a byte buffer.
 *
 * @param buf The source buffer.
 * @param guid The GUID to populate.
 * @return The updated buffer.
 */
ByteBuffer& operator>>(ByteBuffer& buf, ObjectGuid& guid)
{
    guid.Set(buf.read<uint64>());
    return buf;
}

/**
 * @brief Writes a packed GUID to a byte buffer.
 *
 * @param buf The destination buffer.
 * @param guid The packed GUID wrapper.
 * @return The updated buffer.
 */
ByteBuffer& operator<< (ByteBuffer& buf, PackedGuid const& guid)
{
    buf.append(guid.m_packedGuid);
    return buf;
}

/**
 * @brief Reads a packed GUID from a byte buffer.
 *
 * @param buf The source buffer.
 * @param guid The packed GUID reader wrapper.
 * @return The updated buffer.
 */
ByteBuffer& operator>>(ByteBuffer& buf, PackedGuidReader const& guid)
{
    guid.m_guidPtr->Set(buf.readPackGUID());
    return buf;
}

template uint32 ObjectGuidGenerator<HIGHGUID_ITEM>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_PLAYER>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_GAMEOBJECT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_TRANSPORT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_UNIT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_PET>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_DYNAMICOBJECT>::Generate();
template uint32 ObjectGuidGenerator<HIGHGUID_CORPSE>::Generate();
