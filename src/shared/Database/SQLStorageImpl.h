/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef SQLSTORAGE_IMPL_H
#define SQLSTORAGE_IMPL_H

#include "Utilities/ProgressBar.h"
#include "Log/Log.h"
#include "DataStores/DBCFileLoader.h"

template<class DerivedLoader, class StorageClass>
template<class S, class D>
/**
 * @brief S source-type, D destination-type
 *
 * @param uint32
 * @param src
 * @param dst
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert(uint32 /*field_pos*/, S src, D& dst)
{
#if defined(__arm__)
    if (((unsigned) &dst) % sizeof(D)) {
        //The address is not aligned. Use memcpy to avoid ARM unaligned trap
       D converted(src);
       memcpy((void*) &dst, (void*) &converted, sizeof(D));
    }
    else
#endif

    dst = D(src);
}

template<class DerivedLoader, class StorageClass>
/**
 * @brief
 *
 * @param uint32
 * @param src
 * @param dst
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_str_to_str(uint32 /*field_pos*/, char const* src, char*& dst)
{
    if (!src)
    {
        dst = new char[1];
        *dst = 0;
    }
    else
    {
        uint32 l = strlen(src) + 1;
        dst = new char[l];
        memcpy(dst, src, l);
    }
}

template<class DerivedLoader, class StorageClass>
template<class S>
/**
 * @brief S source-type
 *
 * @param uint32
 * @param S
 * @param dst
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_to_str(uint32 /*field_pos*/, S /*src*/, char*& dst)
{
    dst = new char[1];
    *dst = 0;
}

template<class DerivedLoader, class StorageClass>
template<class D>
/**
 * @brief D destination-type
 *
 * @param uint32
 * @param
 * @param dst
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::convert_from_str(uint32 /*field_pos*/, char const* /*src*/, D& dst)
{
#if defined(__arm__)
    if (((unsigned) &dst) % sizeof(D)) {
       //The address is not aligned. Use memcpy to avoid ARM unaligned trap
       D converted(0);
       memcpy((void*) &dst, (void*) &converted, sizeof(D));
    }
    else
#endif

    dst = 0;
}

template<class DerivedLoader, class StorageClass>
template<class S, class D>
/**
 * @brief S source-type, D destination-type
 *
 * @param uint32
 * @param src
 * @param dst
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::default_fill(uint32 /*field_pos*/, S src, D& dst)
{
#if defined(__arm__)
    if (((unsigned) &dst) % sizeof(D)) {
       //The address is not aligned. Use memcpy to avoid ARM unaligned trap
       D converted(src);
       memcpy((void*) &dst, (void*) &converted, sizeof(D));
    }
    else
#endif

    dst = D(src);
}

template<class DerivedLoader, class StorageClass>
/**
 * @brief
 *
 * @param uint32
 * @param
 * @param dst
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::default_fill_to_str(uint32 /*field_pos*/, char const* /*src*/, char*& dst)
{
    dst = new char[1];
    *dst = 0;
}

template<class DerivedLoader, class StorageClass>
template<class V>
/**
 * @brief V value-type
 *
 * @param value
 * @param store
 * @param p
 * @param x
 * @param offset
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::storeValue(V value, StorageClass& store, char* p, uint32 x, uint32& offset)
{
    DerivedLoader* subclass = (static_cast<DerivedLoader*>(this));
    switch (store.GetDstFormat(x))
    {
        case DBC_FF_LOGIC:
            subclass->convert(x, value, *((bool*)(&p[offset])));
            offset += sizeof(bool);
            break;
        case DBC_FF_BYTE:
            subclass->convert(x, value, *((char*)(&p[offset])));
            offset += sizeof(char);
            break;
        case DBC_FF_INT:
            subclass->convert(x, value, *((uint32*)(&p[offset])));
            offset += sizeof(uint32);
            break;
        case DBC_FF_FLOAT:
            subclass->convert(x, value, *((float*)(&p[offset])));
            offset += sizeof(float);
            break;
        case DBC_FF_STRING:
            subclass->convert_to_str(x, value, *((char**)(&p[offset])));
            offset += sizeof(char*);
            break;
        case DBC_FF_NA:
            subclass->default_fill(x, value, *((uint32*)(&p[offset])));
            offset += sizeof(uint32);
            break;
        case DBC_FF_NA_BYTE:
            subclass->default_fill(x, value, *((char*)(&p[offset])));
            offset += sizeof(char);
            break;
        case DBC_FF_NA_FLOAT:
            subclass->default_fill(x, value, *((float*)(&p[offset])));
            offset += sizeof(float);
            break;
        case DBC_FF_IND:
        case DBC_FF_SORT:
            assert(false && "SQL storage does not have sort field types");
            break;
        default:
            assert(false && "unknown format character");
            break;
    }
}

template<class DerivedLoader, class StorageClass>
/**
 * @brief
 *
 * @param value
 * @param store
 * @param p
 * @param x
 * @param offset
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::storeValue(char const* value, StorageClass& store, char* p, uint32 x, uint32& offset)
{
    DerivedLoader* subclass = (static_cast<DerivedLoader*>(this));
    switch (store.GetDstFormat(x))
    {
        case DBC_FF_LOGIC:
            subclass->convert_from_str(x, value, *((bool*)(&p[offset])));
            offset += sizeof(bool);
            break;
        case DBC_FF_BYTE:
            subclass->convert_from_str(x, value, *((char*)(&p[offset])));
            offset += sizeof(char);
            break;
        case DBC_FF_INT:
            subclass->convert_from_str(x, value, *((uint32*)(&p[offset])));
            offset += sizeof(uint32);
            break;
        case DBC_FF_FLOAT:
            subclass->convert_from_str(x, value, *((float*)(&p[offset])));
            offset += sizeof(float);
            break;
        case DBC_FF_STRING:
            subclass->convert_str_to_str(x, value, *((char**)(&p[offset])));
            offset += sizeof(char*);
            break;
        case DBC_FF_NA_POINTER:
            subclass->default_fill_to_str(x, value, *((char**)(&p[offset])));
            offset += sizeof(char*);
            break;
        case DBC_FF_IND:
        case DBC_FF_SORT:
            assert(false && "SQL storage does not have sort field types");
            break;
        default:
            assert(false && "unknown format character");
            break;
    }
}

template<class DerivedLoader, class StorageClass>
/**
 * @brief
 *
 * @param store
 * @param error_at_empty
 */
void SQLStorageLoaderBase<DerivedLoader, StorageClass>::Load(StorageClass& store, bool error_at_empty /*= true*/)
{
    Field* fields = NULL;
    QueryResult* result  = WorldDatabase.PQuery("SELECT MAX(`%s`) FROM `%s`", store.EntryFieldName(), store.GetTableName());
    if (!result)
    {
        sLog.outError("Error loading %s table (not exist?)\n", store.GetTableName());
        Log::WaitBeforeContinueIfNeed();
        exit(1);                                            // Stop server at loading non existent table or inaccessible table
    }

    uint32 maxRecordId = (*result)[0].GetUInt32() + 1;
    uint32 recordCount = 0;
    uint32 recordsize = 0;
    delete result;

    result = WorldDatabase.PQuery("SELECT COUNT(*) FROM `%s`", store.GetTableName());
    if (result)
    {
        fields = result->Fetch();
        recordCount = fields[0].GetUInt32();
        delete result;
    }

    result = WorldDatabase.PQuery("SELECT * FROM `%s`", store.GetTableName());

    if (!result)
    {
        if (error_at_empty)
        {
            sLog.outError("%s table is empty!\n", store.GetTableName());
        }
        else
        {
            sLog.outString("%s table is empty!\n", store.GetTableName());
        }

        recordCount = 0;
        return;
    }

    if (store.GetSrcFieldCount() != result->GetFieldCount())
    {
        recordCount = 0;
        sLog.outError("Error in %s table.Perhaps the table structure was changed. There should be %d fields in the table.\n", store.GetTableName(), store.GetSrcFieldCount());
        delete result;
        Log::WaitBeforeContinueIfNeed();
        exit(1);                                            // Stop server at loading broken or non-compatible table.
    }

    // get struct size
    uint32 offset = 0;
    for (uint32 x = 0; x < store.GetDstFieldCount(); ++x)
    {
        switch (store.GetDstFormat(x))
        {
            case DBC_FF_LOGIC:
                recordsize += sizeof(bool);   break;
            case DBC_FF_BYTE:
                recordsize += sizeof(char);   break;
            case DBC_FF_INT:
                recordsize += sizeof(uint32); break;
            case DBC_FF_FLOAT:
                recordsize += sizeof(float);  break;
            case DBC_FF_STRING:
                recordsize += sizeof(char*);  break;
            case DBC_FF_NA:
                recordsize += sizeof(uint32); break;
            case DBC_FF_NA_BYTE:
                recordsize += sizeof(char);   break;
            case DBC_FF_NA_FLOAT:
                recordsize += sizeof(float);  break;
            case DBC_FF_NA_POINTER:
                recordsize += sizeof(char*);  break;
            case DBC_FF_IND:
            case DBC_FF_SORT:
                assert(false && "SQL storage not have sort field types");
                break;
            default:
                assert(false && "unknown format character");
                break;
        }
    }

    // Prepare data storage and lookup storage
    store.prepareToLoad(maxRecordId, recordCount, recordsize);

    BarGoLink bar(recordCount);
    do
    {
        fields = result->Fetch();
        bar.step();

        char* record = store.createRecord(fields[0].GetUInt32());
        offset = 0;

        // dependend on dest-size
        // iterate two indexes: x over dest, y over source
        //                      y++ If and only If x != FT_NA*
        //                      x++ If and only If a value is stored
        for (uint32 x = 0, y = 0; x < store.GetDstFieldCount();)
        {
            switch (store.GetDstFormat(x))
            {
                // For default fill continue and do not increase y
                case DBC_FF_NA:         storeValue((uint32)0, store, record, x, offset);         ++x; continue;
                case DBC_FF_NA_BYTE:    storeValue((char)0, store, record, x, offset);           ++x; continue;
                case DBC_FF_NA_FLOAT:   storeValue((float)0.0f, store, record, x, offset);       ++x; continue;
                case DBC_FF_NA_POINTER: storeValue((char const*)NULL, store, record, x, offset); ++x; continue;
                default:
                    break;
            }

            // It is required that the input has at least as many columns set as the output requires
            if (y >= store.GetSrcFieldCount())
            {
                assert(false && "SQL storage has too few columns!");
            }

            switch (store.GetSrcFormat(y))
            {
                case DBC_FF_LOGIC:  storeValue((bool)(fields[y].GetUInt32() > 0), store, record, x, offset);  ++x; break;
                case DBC_FF_BYTE:   storeValue((char)fields[y].GetUInt8(), store, record, x, offset);         ++x; break;
                case DBC_FF_INT:    storeValue((uint32)fields[y].GetUInt32(), store, record, x, offset);      ++x; break;
                case DBC_FF_FLOAT:  storeValue((float)fields[y].GetFloat(), store, record, x, offset);        ++x; break;
                case DBC_FF_STRING: storeValue((char const*)fields[y].GetString(), store, record, x, offset); ++x; break;
                case DBC_FF_NA:
                case DBC_FF_NA_BYTE:
                case DBC_FF_NA_FLOAT:
                    // Do Not increase x
                    break;
                case DBC_FF_IND:
                case DBC_FF_SORT:
                case DBC_FF_NA_POINTER:
                    assert(false && "SQL storage not have sort or pointer field types");
                    break;
                default:
                    assert(false && "unknown format character");
            }
            ++y;
        }
    }
    while (result->NextRow());

    delete result;
}

#endif
