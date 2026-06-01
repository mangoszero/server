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
 * @file UpdateData.cpp
 * @brief Object update packet builder and compressor
 *
 * This file implements UpdateData, which accumulates object creation,
 * destruction, and value updates for efficient network transmission.
 *
 * Features:
 * - Accumulates object update blocks for batch transmission
 * - Tracks out-of-range objects (visibility removal)
 * - zlib compression for large packets (configurable threshold: 100 bytes)
 * - Packed GUID encoding for bandwidth efficiency
 *
 * Packet structure:
 * - Block count
 * - Transport flag
 * - Out-of-range GUID list (optional)
 * - Update data blocks
 *
 * @see UpdateData for the data accumulator
 * @see Object::BuildCreateUpdateBlockForPlayer for object creation
 * @see Object::BuildValuesUpdateBlockForPlayer for value updates
 */

#include <zlib.h>
#include "Common.h"
#include "UpdateData.h"
#include "ByteBuffer.h"
#include "WorldPacket.h"
#include "Log.h"
#include "Opcodes.h"
#include "World.h"
#include "ObjectGuid.h"

/**
 * @brief Construct empty UpdateData
 *
 * Initializes an empty update data accumulator with zero blocks.
 * Data buffers are allocated as needed during block accumulation.
 */
UpdateData::UpdateData() : m_blockCount(0)
{
}

/**
 * @brief Add multiple out-of-range GUIDs
 * @param guids Set of GUIDs to add
 *
 * Adds a set of object GUIDs that the player can no longer see.
 * These will be sent as out-of-range objects in the update packet,
 * causing the client to remove them from the scene.
 */
void UpdateData::AddOutOfRangeGUID(GuidSet& guids)
{
    m_outOfRangeGUIDs.insert(guids.begin(), guids.end());
}

/**
 * @brief Add single out-of-range GUID
 * @param guid Object GUID to add
 *
 * Adds a single object GUID that the player can no longer see.
 * @see AddOutOfRangeGUID(GuidSet&)
 */
void UpdateData::AddOutOfRangeGUID(ObjectGuid const& guid)
{
    m_outOfRangeGUIDs.insert(guid);
}

/**
 * @brief Compress data using zlib deflate
 * @param dst Destination buffer for compressed data
 * @param dst_size In: max destination size, Out: actual compressed size
 * @param src Source data to compress
 * @param src_size Size of source data in bytes
 *
 * Compresses update data using zlib deflate algorithm.
 * Compression level is controlled by CONFIG_UINT32_COMPRESSION config.
 *
 * @note On error, dst_size is set to 0
 * @note Uses Z_BEST_SPEED (level 1) by default for CPU efficiency
 */
void UpdateData::Compress(void* dst, uint32* dst_size, void* src, int src_size)
{
    z_stream c_stream;

    c_stream.zalloc = (alloc_func)0;
    c_stream.zfree = (free_func)0;
    c_stream.opaque = (voidpf)0;

    // default Z_BEST_SPEED (1)
    int z_res = deflateInit(&c_stream, sWorld.getConfig(CONFIG_UINT32_COMPRESSION));
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflateInit) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    c_stream.next_out = (Bytef*)dst;
    c_stream.avail_out = *dst_size;
    c_stream.next_in = (Bytef*)src;
    c_stream.avail_in = (uInt)src_size;

    z_res = deflate(&c_stream, Z_NO_FLUSH);
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflate) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    if (c_stream.avail_in != 0)
    {
        sLog.outError("Can't compress update packet (zlib: deflate not greedy)");
        *dst_size = 0;
        return;
    }

    z_res = deflate(&c_stream, Z_FINISH);
    if (z_res != Z_STREAM_END)
    {
        sLog.outError("Can't compress update packet (zlib: deflate should report Z_STREAM_END instead %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    z_res = deflateEnd(&c_stream);
    if (z_res != Z_OK)
    {
        sLog.outError("Can't compress update packet (zlib: deflateEnd) Error code: %i (%s)", z_res, zError(z_res));
        *dst_size = 0;
        return;
    }

    *dst_size = c_stream.total_out;
}

/**
 * @brief Build final update packet from accumulated data
 * @param packet Output packet to build (must be empty)
 * @param hasTransport If true, packet contains transport position data
 * @return true on success, false on compression failure
 *
 * Builds the final network packet from accumulated update blocks:
 * 1. Calculates buffer size (block count + transport flag + OOR data + blocks)
 * 2. Writes header (block count, transport flag)
 * 3. Writes out-of-range GUID list (if any)
 * 4. Appends accumulated update blocks
 * 5. Compresses if size > 100 bytes
 *
 * Packet format:
 * - uint32: Block count
 * - uint8: Transport flag (1 if transport in update, 0 otherwise)
 * - (Optional) Out-of-range section:
 *   - uint8: Update type (UPDATETYPE_OUT_OF_RANGE_OBJECTS)
 *   - uint32: Count
 *   - PackedGUID[]: GUIDs to remove
 * - Byte[]: Update blocks data
 *
 * @note Packet must be empty before calling (assertion-checked)
 */
bool UpdateData::BuildPacket(WorldPacket* packet, bool hasTransport)
{
    MANGOS_ASSERT(packet->empty());                         // shouldn't happen

    // Calculate buffer size: header + OOR section (optional) + data blocks
    ByteBuffer buf(4 + 1 + (m_outOfRangeGUIDs.empty() ? 0 : 1 + 4 + 9 * m_outOfRangeGUIDs.size()) + m_data.wpos());

    buf << (uint32)(!m_outOfRangeGUIDs.empty() ? m_blockCount + 1 : m_blockCount);
    buf << (uint8)(hasTransport ? 1 : 0);

    // Write out-of-range GUIDs if present
    if (!m_outOfRangeGUIDs.empty())
    {
        buf << (uint8) UPDATETYPE_OUT_OF_RANGE_OBJECTS;
        buf << (uint32) m_outOfRangeGUIDs.size();

        for (GuidSet::const_iterator i = m_outOfRangeGUIDs.begin(); i != m_outOfRangeGUIDs.end(); ++i)
        {
            buf << i->WriteAsPacked();
        }
    }

    buf.append(m_data);

    size_t pSize = buf.wpos();                              // use real used data size

    // Compress packets larger than 100 bytes
    if (pSize > 100)
    {
        uint32 destsize = compressBound(pSize);
        packet->resize(destsize + sizeof(uint32));

        packet->put<uint32>(0, pSize);
        Compress(const_cast<uint8*>(packet->contents()) + sizeof(uint32), &destsize, (void*)buf.contents(), pSize);
        if (destsize == 0)
        {
            return false;
        }

        packet->resize(destsize + sizeof(uint32));
        packet->SetOpcode(SMSG_COMPRESSED_UPDATE_OBJECT);
    }
    else                                                    // send small packets without compression
    {
        packet->append(buf);
        packet->SetOpcode(SMSG_UPDATE_OBJECT);
    }

    return true;
}

/**
 * @brief Clear all accumulated data
 *
 * Resets the UpdateData to empty state:
 * - Clears update data buffer
 * - Clears out-of-range GUID set
 * - Resets block counter to 0
 *
 * Called after the packet is built and sent.
 */
void UpdateData::Clear()
{
    m_data.clear();
    m_outOfRangeGUIDs.clear();
    m_blockCount = 0;
}
