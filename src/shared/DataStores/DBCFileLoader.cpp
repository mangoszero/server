/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include <cstdint>
#include "Common/Locales.h"
#include <cstring>
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "DBCFileLoader.h"


// Ceilings for the two sizes AutoProduceData takes from file content. Both are far above
// anything a real client ships -- the largest DBC here is a few megabytes -- and exist so
// a corrupt file fails the load instead of sizing an allocation.
static const uint64 MAX_DBC_INDEX       = 16u * 1024u * 1024u;
static const uint64 MAX_DBC_TABLE_BYTES = 512u * 1024u * 1024u;

DBCFileLoader::DBCFileLoader()
{
    data = NULL;
    fieldsOffset = NULL;
}

bool DBCFileLoader::Load(const char* filename, const char* fmt)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        return false;
    }

    fseek(f, 0, SEEK_END);
    const long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (length <= 0)
    {
        fclose(f);
        return false;
    }

    // Not `bytes(size_t(length))`: that is a function declaration, not a vector.
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    const size_t got = fread(bytes.data(), 1, bytes.size(), f);
    fclose(f);
    if (got != bytes.size())
    {
        return false;
    }

    return LoadFromMemory(bytes.data(), bytes.size(), fmt);
}

// The offline baker reads its DBCs out of the client MPQs, where there is no file to
// open. Sharing this path with Load keeps the baker's column layout identical to the
// server's -- one parser, one set of format strings, nothing to drift.
bool DBCFileLoader::LoadFromMemory(const void* bytes, size_t size, const char* fmt)
{
    delete[] data;
    data = NULL;
    delete[] fieldsOffset;
    fieldsOffset = NULL;

    if (!bytes || size < 20)
    {
        return false;
    }

    const unsigned char* p = static_cast<const unsigned char*>(bytes);
    uint32 header;
    memcpy(&header, p, 4);
    EndianConvert(header);
    if (header != 0x43424457)                               //'WDBC'
    {
        return false;
    }

    memcpy(&recordCount, p + 4, 4);
    EndianConvert(recordCount);
    memcpy(&fieldCount, p + 8, 4);
    EndianConvert(fieldCount);
    memcpy(&recordSize, p + 12, 4);
    EndianConvert(recordSize);
    memcpy(&stringSize, p + 16, 4);
    EndianConvert(stringSize);

    if (!fieldCount || strlen(fmt) < fieldCount)
    {
        return false;
    }

    const uint64 payload = uint64(recordSize) * recordCount + stringSize;
    if (payload + 20 > size)
    {
        return false;
    }

    fieldsOffset = new uint32[fieldCount];
    fieldsOffset[0] = 0;
    for (uint32 i = 1; i < fieldCount; ++i)
    {
        fieldsOffset[i] = fieldsOffset[i - 1];
        if (fmt[i - 1] == 'b' || fmt[i - 1] == 'X')         // byte fields
        {
            fieldsOffset[i] += 1;
        }
        else                                                // 4 byte fields (int32/float/strings)
        {
            fieldsOffset[i] += 4;
        }
    }

    data = new unsigned char[size_t(payload)];
    stringTable = data + recordSize * recordCount;
    memcpy(data, p + 20, size_t(payload));
    return true;
}

DBCFileLoader::~DBCFileLoader()
{
    delete[] data;
    delete[] fieldsOffset;
}

DBCFileLoader::Record DBCFileLoader::getRecord(size_t id)
{
    assert(data);
    return Record(*this, data + id * recordSize);
}

uint32 DBCFileLoader::GetFormatRecordSize(const char* format, int32* index_pos)
{
    uint32 recordsize = 0;
    int32 i = -1;
    for (uint32 x = 0; format[x]; ++ x)
    {
        switch (format[x])
        {
            case DBC_FF_FLOAT:
                recordsize += sizeof(float);
                break;
            case DBC_FF_INT:
                recordsize += sizeof(uint32);
                break;
            case DBC_FF_STRING:
                recordsize += sizeof(char*);
                break;
            case DBC_FF_SORT:
                i = x;
                break;
            case DBC_FF_IND:
                i = x;
                recordsize += sizeof(uint32);
                break;
            case DBC_FF_BYTE:
                recordsize += sizeof(uint8);
                break;
            case DBC_FF_LOGIC:
                assert(false && "Attempted to load DBC files that do not have field types that match what is in the core. Check DBCfmt.h or your DBC files.");
                break;
            case DBC_FF_NA:
            case DBC_FF_NA_BYTE:
                break;
            default:
                assert(false && "Unknown field format character in DBCfmt.h");
                break;
        }
    }

    if (index_pos)
    {
        *index_pos = i;
    }

    return recordsize;
}

char* DBCFileLoader::AutoProduceData(const char* format, uint32& records, char**& indexTable)
{
    /*
    format STRING, NA, FLOAT,NA,INT <=>
    struct{
    char* field0,
    float field1,
    int field2
    }entry;

    this func will generate  entry[rows] data;
    */

    typedef char* ptr;
    if (strlen(format) != fieldCount)
    {
        return NULL;
    }

    // get struct size and index pos
    int32 i;
    uint32 recordsize = GetFormatRecordSize(format, &i);

    if (i >= 0)
    {
        uint32 maxi = 0;
        // find max index
        for (uint32 y = 0; y < recordCount; ++y)
        {
            uint32 ind = getRecord(y).getUInt(i);
            if (ind > maxi)
            {
                maxi = ind;
            }
        }

        // maxi comes from record CONTENT, so it is whatever the file says. Refuse a
        // wrapped or absurd count rather than allocating from it: ++maxi on 0xFFFFFFFF
        // is 0, which would allocate nothing and then be written through by every row.
        if (maxi == 0xFFFFFFFFu || uint64(maxi) + 1 > MAX_DBC_INDEX)
        {
            return NULL;
        }

        ++maxi;
        records = maxi;
        indexTable = new ptr[maxi];
        memset(indexTable, 0, size_t(maxi) * sizeof(ptr));
    }
    else
    {
        records = recordCount;
        indexTable = new ptr[recordCount];
    }

    // In 64 bits, so a large record count times a wide format cannot wrap. The header
    // check bounded recordCount against the FILE's record size; recordsize here is the
    // FORMAT's, and the two need not agree.
    const uint64 tableBytes = uint64(recordCount) * recordsize;
    if (tableBytes > MAX_DBC_TABLE_BYTES)
    {
        delete[] indexTable;
        indexTable = NULL;
        return NULL;
    }

    char* dataTable = new char[size_t(tableBytes)];

    uint32 offset = 0;

    for (uint32 y = 0; y < recordCount; ++y)
    {
        if (i >= 0)
        {
            indexTable[getRecord(y).getUInt(i)] = &dataTable[offset];
        }
        else
        {
            indexTable[y] = &dataTable[offset];
        }

        for (uint32 x = 0; x < fieldCount; ++x)
        {
            switch (format[x])
            {
                case DBC_FF_FLOAT:
                    *((float*)(&dataTable[offset])) = getRecord(y).getFloat(x);
                    offset += sizeof(float);
                    break;
                case DBC_FF_IND:
                case DBC_FF_INT:
                    *((uint32*)(&dataTable[offset])) = getRecord(y).getUInt(x);
                    offset += sizeof(uint32);
                    break;
                case DBC_FF_BYTE:
                    *((uint8*)(&dataTable[offset])) = getRecord(y).getUInt8(x);
                    offset += sizeof(uint8);
                    break;
                case DBC_FF_STRING:
                    *((char**)(&dataTable[offset])) = NULL; // will replace non-empty or "" strings in AutoProduceStrings
                    offset += sizeof(char*);
                    break;
                case DBC_FF_LOGIC:
                    assert(false && "Attempted to load DBC files that do not have field types that match what is in the core. Check DBCfmt.h or your DBC files.");
                    break;
                case DBC_FF_NA:
                case DBC_FF_NA_BYTE:
                case DBC_FF_SORT:
                    break;
                default:
                    assert(false && "Unknown field format character in DBCfmt.h");
                    break;
            }
        }
    }

    return dataTable;
}

char* DBCFileLoader::AutoProduceStrings(const char* format, char* dataTable)
{
    if (strlen(format) != fieldCount)
    {
        return NULL;
    }

    char* stringPool = new char[stringSize];
    memcpy(stringPool, stringTable, stringSize);

    uint32 offset = 0;

    for (uint32 y = 0; y < recordCount; ++y)
    {
        for (uint32 x = 0; x < fieldCount; ++x)
        {
            switch (format[x])
            {
                case DBC_FF_FLOAT:
                    offset += sizeof(float);
                    break;
                case DBC_FF_IND:
                case DBC_FF_INT:
                    offset += sizeof(uint32);
                    break;
                case DBC_FF_BYTE:
                    offset += sizeof(uint8);
                    break;
                case DBC_FF_STRING:
                {
                    // fill only not filled entries
                    char** slot = (char**)(&dataTable[offset]);
                    if (!*slot || !** slot)
                    {
                        const char* st = getRecord(y).getString(x);
                        *slot = stringPool + (st - (const char*)stringTable);
                    }
                    offset += sizeof(char*);
                    break;
                }
                case DBC_FF_LOGIC:
                    assert(false && "Attempted to load DBC files that does not have field types that match what is in the core. Check DBCfmt.h or your DBC files.");
                    break;
                case DBC_FF_NA:
                case DBC_FF_NA_BYTE:
                case DBC_FF_SORT:
                    break;
                default:
                    assert(false && "Unknown field format character in DBCfmt.h");
                    break;
            }
        }
    }

    return stringPool;
}
