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
 * @file SQLStorage.cpp
 * @brief Implementation of SQL storage classes for database caching
 *
 * This file provides the implementation for SQL storage containers that cache
 * database query results in memory for fast access. It includes:
 * - SQLStorageBase: Base class for all storage types
 * - SQLStorage: Array-based indexed storage
 * - SQLHashStorage: Hash map based storage
 * - SQLMultiStorage: Multi-map based storage for duplicate keys
 *
 * The storage system supports various data formats including integers,
 * floats, strings, and special field types for DBC-like data structures.
 */

#include "SQLStorage.h"

// -----------------------------------  SQLStorageBase  ---------------------------------------- //

/**
 * @brief Default constructor for SQLStorageBase
 *
 * Initializes all member variables to their default values (NULL or 0).
 * The actual initialization of table name and formats is done via
 * the Initialize() method.
 */
SQLStorageBase::SQLStorageBase()
    : m_tableName(NULL),
    m_entry_field(NULL),
    m_src_format(NULL),
    m_dst_format(NULL),
    m_dstFieldCount(0),
    m_srcFieldCount(0),
    m_recordCount(0),
    m_maxEntry(0),
    m_recordSize(0),
    m_data(NULL)
{}

/**
 * @brief Initialize the storage with table information and field formats
 * @param tableName Name of the SQL table to load data from
 * @param entry_field Name of the primary key field
 * @param src_format Format string describing source data layout
 * @param dst_format Format string describing destination data layout
 *
 * The format strings use characters to represent field types:
 * - 's': String (char*)
 * - 'f': Float
 * - 'i': Integer (uint32)
 * - 'b': Byte (uint8)
 * - 'p': Pointer (void*)
 * - 'n': Not applicable/placeholder
 *
 * Different source and destination formats allow for data transformation
 * during loading, such as converting strings to pointers or reformatting.
 */
void SQLStorageBase::Initialize(const char* tableName, const char* entry_field, const char* src_format, const char* dst_format)
{
    m_tableName = tableName;
    m_entry_field = entry_field;
    m_src_format = src_format;
    m_dst_format = dst_format;

    m_srcFieldCount = strlen(m_src_format);
    m_dstFieldCount = strlen(m_dst_format);
}

/**
 * @brief Create a new record in the data buffer
 * @param recordId The ID of the record to create
 * @return Pointer to the newly created record's memory
 *
 * Allocates space for a new record at the end of the data buffer and
 * calls JustCreatedRecord() to perform any initialization. The record
 * is not automatically indexed - that must be done by derived classes.
 */
char* SQLStorageBase::createRecord(uint32 recordId)
{
    char* newRecord = &m_data[m_recordCount * m_recordSize];
    ++m_recordCount;

    JustCreatedRecord(recordId, newRecord);
    return newRecord;
}

/**
 * @brief Prepare the storage for loading data
 * @param maxEntry Maximum entry ID that will be stored
 * @param recordCount Total number of records to be loaded
 * @param recordSize Size of each record in bytes
 *
 * Allocates the data buffer and clears any existing data.
 * This must be called before loading records into the storage.
 * The buffer is sized to hold all records with their specified size.
 */
void SQLStorageBase::prepareToLoad(uint32 maxEntry, uint32 recordCount, uint32 recordSize)
{
    m_maxEntry = maxEntry;
    m_recordSize = recordSize;

    delete[] m_data;
    m_data = new char[recordCount * m_recordSize];
    memset(m_data, 0, recordCount * m_recordSize);

    m_recordCount = 0;
}

/**
 * @brief Free all allocated memory and clean up the storage
 *
 * Releases all memory allocated for data storage, including:
 * - String data (char* fields)
 * - Pointer data (NA_POINTER fields)
 * - Main data buffer
 *
 * This properly handles all field types defined in the destination
 * format string to ensure no memory leaks occur.
 */
void SQLStorageBase::Free()
{
    if (!m_data)
    {
        return;
    }

    uint32 offset = 0;
    for (uint32 x = 0; x < m_dstFieldCount; ++x)
    {
        switch (m_dst_format[x])
        {
            case DBC_FF_LOGIC:
                offset += sizeof(bool);
                break;
            case DBC_FF_STRING:
            {
                for (uint32 recordItr = 0; recordItr < m_recordCount; ++recordItr)
                {
                    delete[] *(char**)((char*)(m_data + (recordItr * m_recordSize)) + offset);
                }

                offset += sizeof(char*);
                break;
            }
            case DBC_FF_NA:
            case DBC_FF_INT:
                offset += sizeof(uint32);
                break;
            case DBC_FF_BYTE:
            case DBC_FF_NA_BYTE:
                offset += sizeof(char);
                break;
            case DBC_FF_FLOAT:
            case DBC_FF_NA_FLOAT:
                offset += sizeof(float);
                break;
            case DBC_FF_NA_POINTER:
            {
                // Free allocated memory for NA_POINTER fields
                for (uint32 recordItr = 0; recordItr < m_recordCount; ++recordItr)
                {
                    char** ptrPtr = (char**)((char*)(m_data + (recordItr * m_recordSize)) + offset);
                    if (*ptrPtr)
                    {
                        delete[] *ptrPtr;
                        *ptrPtr = NULL;
                    }
                }
                offset += sizeof(char*);
                break;
            }
            case DBC_FF_IND:
            case DBC_FF_SORT:
                assert(false && "SQL storage not have sort field types");
                break;
            default:
                assert(false && "unknown format character");
                break;
        }
    }
    delete[] m_data;
    m_data = NULL;
    m_recordCount = 0;
}

// -----------------------------------  SQLStorage  -------------------------------------------- //

/**
 * @class SQLStorage
 * @brief Array-based indexed SQL data storage
 *
 * SQLStorage provides fast array-based access to database records using
 * record IDs as array indices. It's ideal for data with contiguous or
 * near-contiguous ID ranges where direct indexing is efficient.
 *
 * The index array stores pointers to records, allowing O(1) access time.
 * Records are stored in a contiguous data buffer for cache efficiency.
 *
 * @note This storage type is not suitable for sparse data where IDs have
 * large gaps, as it wastes memory on unused indices.
 */

/**
 * @brief Remove an entry from the index
 * @param id The record ID to remove from the index
 *
 * Sets the index entry to NULL, effectively removing the record
 * from fast lookup. The record data remains in memory until Free() is called.
 * This is useful for marking records as deleted without reallocating memory.
 */
void SQLStorage::EraseEntry(uint32 id)
{
    m_Index[id] = NULL;
}

/**
 * @brief Free all resources and clean up the storage
 *
 * Calls the base class Free() to release data memory, then
 * deletes the index array. After this call, the storage is empty
 * and ready for reloading.
 */
void SQLStorage::Free()
{
    SQLStorageBase::Free();
    delete[] m_Index;
    m_Index = NULL;
}

/**
 * @brief Load data from the database
 * @param error_at_empty If true, log an error if no records are loaded
 *
 * Uses SQLStorageLoader to execute the query and populate the storage.
 * The storage must be initialized before calling this method.
 * Records are indexed by their entry field value for fast access.
 */
void SQLStorage::Load(bool error_at_empty /*= true*/)
{
    SQLStorageLoader loader;
    loader.Load(*this, error_at_empty);
}

/**
 * @brief Constructor with same source and destination format
 * @param fmt Format string for both source and destination data
 * @param _entry_field Name of the entry/ID field
 * @param sqlname Name of the SQL table
 *
 * Creates a SQLStorage where the source and destination formats are identical.
 * The index pointer is initialized to NULL and will be allocated during Load().
 */
SQLStorage::SQLStorage(const char* fmt, const char* _entry_field, const char* sqlname)
{
    Initialize(sqlname, _entry_field, fmt, fmt);
    m_Index = NULL;
}

/**
 * @brief Constructor with different source and destination formats
 * @param src_fmt Format string describing source data layout
 * @param dst_fmt Format string describing destination data layout
 * @param _entry_field Name of the entry/ID field
 * @param sqlname Name of the SQL table
 *
 * Creates a SQLStorage that can transform data during loading.
 * This is useful for converting strings to pointers or changing data types.
 * The index pointer is initialized to NULL and will be allocated during Load().
 */
SQLStorage::SQLStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname)
{
    Initialize(sqlname, _entry_field, src_fmt, dst_fmt);
    m_Index = NULL;
}

/**
 * @brief Prepare the storage for loading records
 * @param maxRecordId Maximum record ID (determines index array size)
 * @param recordCount Expected number of records
 * @param recordSize Size of each record in bytes
 *
 * Clears any existing data and allocates the index array.
 * The index is a char* array where index[recordId] points to the record data.
 * The index is initialized with NULL pointers before data is loaded.
 */
void SQLStorage::prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize)
{
    // Clear (possible) old data and old index array
    Free();

    // Set index array
    m_Index = new char*[maxRecordId];
    memset(m_Index, 0, maxRecordId * sizeof(char*));

    SQLStorageBase::prepareToLoad(maxRecordId, recordCount, recordSize);
}

/**
 * @class SQLHashStorage
 * @brief Hash map based SQL data storage
 *
 * SQLHashStorage uses a std::map for indexing, making it suitable for
 * sparse data with non-contiguous IDs. Unlike SQLStorage, it doesn't
 * waste memory on unused indices and can handle any ID range efficiently.
 *
 * This storage type is ideal for data with:
 * - Sparse or non-contiguous ID ranges
 * - Large ID values that would waste memory in array-based storage
 * - Data where memory efficiency is more important than raw access speed
 *
 * @note Access is O(log n) compared to O(1) for SQLStorage, but memory
 * usage is proportional to actual record count rather than max ID.
 */

/**
 * @brief Load data from the database using hash storage loader
 *
 * Uses SQLHashStorageLoader to execute the query and populate the storage.
 * Records are stored in a hash map keyed by their entry field value.
 */
void SQLHashStorage::Load()
{
    SQLHashStorageLoader loader;
    loader.Load(*this);
}

/**
 * @brief Free all resources and clean up the hash storage
 *
 * Calls the base class Free() to release data memory, then
 * clears the hash map index. After this call, the storage is empty
 * and ready for reloading.
 */
void SQLHashStorage::Free()
{
    SQLStorageBase::Free();
    m_indexMap.clear();
}

/**
 * @brief Prepare the hash storage for loading
 * @param maxRecordId Maximum expected record ID (for reference)
 * @param recordCount Expected number of records
 * @param recordSize Size of each record in bytes
 *
 * Unlike SQLStorage, this doesn't pre-allocate the index since
 * the hash map grows dynamically. Just clears existing data
 * and prepares the base storage buffer.
 */
void SQLHashStorage::prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize)
{
    // Clear (possible) old data and old index array
    Free();

    SQLStorageBase::prepareToLoad(maxRecordId, recordCount, recordSize);
}

/**
 * @brief Remove an entry from the hash map index
 * @param id The record ID to mark as removed
 *
 * Sets the hash map entry to NULL without removing the key.
 * The record data remains allocated until Free() is called.
 * This marks the record as deleted while preserving the map structure.
 */
void SQLHashStorage::EraseEntry(uint32 id)
{
    // do not erase from m_records
    RecordMap::iterator find = m_indexMap.find(id);
    if (find != m_indexMap.end())
    {
        find->second = NULL;
    }
}

/**
 * @brief Constructor with same source and destination format
 * @param fmt Format string for both source and destination data
 * @param _entry_field Name of the entry/ID field
 * @param sqlname Name of the SQL table
 *
 * Creates a SQLHashStorage where source and destination formats are identical.
 * The hash map is default-constructed and will be populated during Load().
 */
SQLHashStorage::SQLHashStorage(const char* fmt, const char* _entry_field, const char* sqlname)
{
    Initialize(sqlname, _entry_field, fmt, fmt);
}

/**
 * @brief Constructor with different source and destination formats
 * @param src_fmt Format string describing source data layout
 * @param dst_fmt Format string describing destination data layout
 * @param _entry_field Name of the entry/ID field
 * @param sqlname Name of the SQL table
 *
 * Creates a SQLHashStorage that can transform data during loading.
 * Useful for converting strings to pointers or other data transformations.
 */
SQLHashStorage::SQLHashStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname)
{
    Initialize(sqlname, _entry_field, src_fmt, dst_fmt);
}

/**
 * @class SQLMultiStorage
 * @brief Multi-map based SQL data storage for duplicate keys
 *
 * SQLMultiStorage extends SQLHashStorage to support multiple records
 * with the same ID. This is useful for one-to-many relationships where
 * a single key maps to multiple values (e.g., spells by class, items by vendor).
 *
 * Uses std::multimap internally, allowing:
 * - Multiple records with the same key
 * - Range-based iteration over all records with a given key
 * - Efficient insertion and lookup
 *
 * @note This storage is ideal for relational data where duplicates are expected.
 */

/**
 * @brief Load data from the database using multi-storage loader
 *
 * Uses SQLMultiStorageLoader to execute the query and populate the storage.
 * Multiple records with the same entry ID are all stored in the multi-map.
 */
void SQLMultiStorage::Load()
{
    SQLMultiStorageLoader loader;
    loader.Load(*this);
}

/**
 * @brief Free all resources and clean up the multi-storage
 *
 * Calls the base class Free() to release data memory, then
 * clears the multi-map index. After this call, the storage is empty
 * and ready for reloading.
 */
void SQLMultiStorage::Free()
{
    SQLStorageBase::Free();
    m_indexMultiMap.clear();
}

/**
 * @brief Prepare the multi-storage for loading
 * @param maxRecordId Maximum expected record ID (for reference)
 * @param recordCount Expected number of records
 * @param recordSize Size of each record in bytes
 *
 * The multi-map grows dynamically as records are inserted.
 * This method just clears existing data and prepares the base buffer.
 */
void SQLMultiStorage::prepareToLoad(uint32 maxRecordId, uint32 recordCount, uint32 recordSize)
{
    // Clear (possible) old data and old index array
    Free();

    SQLStorageBase::prepareToLoad(maxRecordId, recordCount, recordSize);
}

/**
 * @brief Remove all entries with the given ID from the multi-map
 * @param id The record ID to remove
 *
 * Unlike SQLHashStorage which marks entries as NULL, this removes
 * all records with the specified ID from the multi-map completely.
 * This is the standard multimap erase behavior.
 */
void SQLMultiStorage::EraseEntry(uint32 id)
{
    m_indexMultiMap.erase(id);
}

/**
 * @brief Constructor with same source and destination format
 * @param fmt Format string for both source and destination data
 * @param _entry_field Name of the entry/ID field
 * @param sqlname Name of the SQL table
 *
 * Creates a SQLMultiStorage where source and destination formats are identical.
 * The multi-map supports multiple records with the same key.
 */
SQLMultiStorage::SQLMultiStorage(const char* fmt, const char* _entry_field, const char* sqlname)
{
    Initialize(sqlname, _entry_field, fmt, fmt);
}

/**
 * @brief Constructor with different source and destination formats
 * @param src_fmt Format string describing source data layout
 * @param dst_fmt Format string describing destination data layout
 * @param _entry_field Name of the entry/ID field
 * @param sqlname Name of the SQL table
 *
 * Creates a SQLMultiStorage that can transform data during loading.
 * The multi-map will store all records, including those with duplicate keys.
 */
SQLMultiStorage::SQLMultiStorage(const char* src_fmt, const char* dst_fmt, const char* _entry_field, const char* sqlname)
{
    Initialize(sqlname, _entry_field, src_fmt, dst_fmt);
}
