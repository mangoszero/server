/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOS_H_UPDATEDATA
#define MANGOS_H_UPDATEDATA

#include "ByteBuffer.h"
#include "ObjectGuid.h"

class WorldPacket;

enum ObjectUpdateType
{
    UPDATETYPE_VALUES               = 0,
    UPDATETYPE_MOVEMENT             = 1,
    UPDATETYPE_CREATE_OBJECT        = 2,
    UPDATETYPE_CREATE_OBJECT2       = 3,
    UPDATETYPE_OUT_OF_RANGE_OBJECTS = 4,
    UPDATETYPE_NEAR_OBJECTS         = 5
};

// checked for 1.12.1
enum ObjectUpdateFlags
{
    UPDATEFLAG_NONE                 = 0x0000,
    UPDATEFLAG_SELF                 = 0x0001,
    UPDATEFLAG_TRANSPORT            = 0x0002,
    UPDATEFLAG_FULLGUID             = 0x0004,
    UPDATEFLAG_HIGHGUID             = 0x0008,
    UPDATEFLAG_ALL                  = 0x0010,
    UPDATEFLAG_LIVING               = 0x0020,
    UPDATEFLAG_HAS_POSITION         = 0x0040
};

class UpdateData
{
    public:
        UpdateData();

        void AddOutOfRangeGUID(GuidSet& guids);
        void AddOutOfRangeGUID(ObjectGuid const& guid);
        void AddUpdateBlock(const ByteBuffer& block);
        bool BuildPacket(WorldPacket* packet, bool hasTransport = false);
        bool HasData() { return m_blockCount > 0 || !m_outOfRangeGUIDs.empty(); }
        void Clear();

        GuidSet const& GetOutOfRangeGUIDs() const { return m_outOfRangeGUIDs; }

    protected:
        uint32 m_blockCount;
        GuidSet m_outOfRangeGUIDs;
        ByteBuffer m_data;

        void Compress(void* dst, uint32* dst_size, void* src, int src_size);
};
#endif
