#ifndef AH_WORKER_ITEM_INSTANCE_FIELDS_H
#define AH_WORKER_ITEM_INSTANCE_FIELDS_H

#include "Common.h"
#include <string>

/// Decoded subset of the item_instance.data UpdateFields blob the AH wire needs.
struct ItemInstanceFields
{
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
