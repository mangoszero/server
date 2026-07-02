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
 */

#ifndef AH_WORKER_ITEM_INSTANCE_FIELDS_H
#define AH_WORKER_ITEM_INSTANCE_FIELDS_H

#include "Common.h"
#include <string>

/// Decoded subset of the item_instance.data UpdateFields blob the AH wire needs.
struct ItemInstanceFields
{
    uint32 entry;         ///< item entry (word 3, OBJECT_FIELD_ENTRY)
    uint32 enchantId;     ///< perm-enchant id (word 22)
    uint32 suffixFactor;  ///< property seed (word 43)
    int32  randomPropId;  ///< random-properties id (word 44, signed)
    int32  charges;       ///< spell charges index 0 (word 16, signed)
    uint32 stackCount;    ///< stack count (word 14) - cross-check vs auction.item_count
    bool   valid;         ///< false if the blob was too short to read every field
};

namespace AhItemBlob
{
    /// Parse the space-separated decimal-uint32 item_instance.data blob exactly
    /// as Object::LoadValues does (StrSplit + atol), reading the 1.12 word
    /// offsets. Classic-only seam; a future expansion changes only the offsets.
    ItemInstanceFields Decode(const std::string& dataBlob);
}

#endif // AH_WORKER_ITEM_INSTANCE_FIELDS_H
