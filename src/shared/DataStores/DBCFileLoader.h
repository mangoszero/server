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

#ifndef DBC_FILE_LOADER_H
#define DBC_FILE_LOADER_H

#include "Platform/Define.h"
#include "Utilities/ByteConverter.h"
#include <cassert>

/**
 * @brief Field format enumeration for DBC file parsing
 *
 * Defines the format codes used in DBC (Database Client) files
 * for specifying field types during data loading and conversion.
 */
enum FieldFormat
{
    DBC_FF_NA = 'x',                                        // ignore/ default, 4 byte size, in Source String means field is ignored, in Dest String means field is filled with default value
    DBC_FF_NA_BYTE = 'X',                                   // ignore/ default, 1 byte size, see above
    DBC_FF_NA_FLOAT = 'F',                                  // ignore/ default,  float size, see above
    DBC_FF_NA_POINTER = 'p',                                // fill default value into dest, pointer size, Use this only with static data (otherwise mem-leak)
    DBC_FF_STRING = 's',                                    // char*
    DBC_FF_FLOAT = 'f',                                     // float
    DBC_FF_INT = 'i',                                       // uint32
    DBC_FF_BYTE = 'b',                                      // uint8
    DBC_FF_SORT = 'd',                                      // sorted by this field, field is not included
    DBC_FF_IND = 'n',                                       // the same,but parsed to data
    DBC_FF_LOGIC = 'l'                                          // Logical (boolean)
};

/**
 * @brief DBC (Database Client) file loader
 *
 * DBCFileLoader loads and parses World of Warcraft DBC files,
 * which contain static game data. It provides access to records
 * and handles endianness conversion for cross-platform compatibility.
 */
class DBCFileLoader
{
    public:
        /**
         * @brief Constructor
         */
        DBCFileLoader();

        /**
         * @brief Destructor - frees loaded data
         */
        ~DBCFileLoader();

        /**
         * @brief Load a DBC file from disk
         * @param filename Path to the DBC file
         * @param fmt Format string describing field types
         * @return True on success, false on failure
         */
        bool Load(const char* filename, const char* fmt);

        /**
         * @brief Represents a single record in the DBC file
         *
         * Record provides access to individual fields within a DBC record,
         * with automatic endianness conversion.
         */
        class Record
        {
            public:
                /**
                 * @brief Get float value from field
                 * @param field Field index
                 * @return Float value
                 */
                float getFloat(size_t field) const
                {
                    assert(field < file.fieldCount);
                    float val = *reinterpret_cast<float*>(offset + file.GetOffset(field));
                    EndianConvert(val);
                    return val;
                }

                /**
                 * @brief Get unsigned 32-bit integer value from field
                 * @param field Field index
                 * @return Unsigned 32-bit integer value
                 */
                uint32 getUInt(size_t field) const
                {
                    assert(field < file.fieldCount);
                    uint32 val = *reinterpret_cast<uint32*>(offset + file.GetOffset(field));
                    EndianConvert(val);
                    return val;
                }

                /**
                 * @brief Get unsigned 8-bit integer value from field
                 * @param field Field index
                 * @return Unsigned 8-bit integer value
                 */
                uint8 getUInt8(size_t field) const
                {
                    assert(field < file.fieldCount);
                    return *reinterpret_cast<uint8*>(offset + file.GetOffset(field));
                }

                /**
                 * @brief Get string value from field
                 * @param field Field index
                 * @return Pointer to string in string table
                 */
                const char* getString(size_t field) const
                {
                    assert(field < file.fieldCount);
                    size_t stringOffset = getUInt(field);
                    assert(stringOffset < file.stringSize);
                    return reinterpret_cast<char*>(file.stringTable + stringOffset);
                }

            private:
                /**
                 * @brief Constructor (private, for DBCFileLoader use only)
                 * @param file_ Parent DBCFileLoader reference
                 * @param offset_ Offset to record data
                 */
                Record(DBCFileLoader& file_, unsigned char* offset_): offset(offset_), file(file_) {}
                unsigned char* offset; /**< Offset to record data */
                DBCFileLoader& file; /**< Parent DBCFileLoader reference */

                friend class DBCFileLoader;
        };

        /**
         * @brief Get record by index
         * @param id Record index
         * @return Record object
         */
        Record getRecord(size_t id);

        /**
         * @brief Get number of records in the file
         * @return Record count
         */
        uint32 GetNumRows() const { return recordCount;}

        /**
         * @brief Get number of fields per record
         * @return Field count
         */
        uint32 GetCols() const { return fieldCount; }

        /**
         * @brief Get offset of a field within a record
         * @param id Field index
         * @return Byte offset from record start
         */
        uint32 GetOffset(size_t id) const { return (fieldsOffset != NULL && id < fieldCount) ? fieldsOffset[id] : 0; }

        /**
         * @brief Check if file is loaded
         * @return True if loaded, false otherwise
         */
        bool IsLoaded() const {return (data != NULL);}

        /**
         * @brief Automatically produce data array from DBC file
         * @param fmt Format string for conversion
         * @param count Output record count
         * @param indexTable Output index table
         * @return Allocated data array
         */
        char* AutoProduceData(const char* fmt, uint32& count, char**& indexTable);

        /**
         * @brief Automatically produce string table from DBC file
         * @param fmt Format string for conversion
         * @param dataTable Data table to reference
         * @return Allocated string table
         */
        char* AutoProduceStrings(const char* fmt, char* dataTable);

        /**
         * Calculate and return the total amount of memory required by the types specified within the format string
         *
         * @param format the format string passed to it (see DBCfmt.h)
         * @param index_pos
         * @return uint32 the total amount of memory required for all the data types
         */
        static uint32 GetFormatRecordSize(const char* format, int32* index_pos = NULL);
    private:

        uint32 recordSize; /**< Size of each record in bytes */
        uint32 recordCount; /**< Number of records in file */
        uint32 fieldCount; /**< Number of fields per record */
        uint32 stringSize; /**< Size of string table in bytes */
        uint32* fieldsOffset; /**< Array of field offsets */
        unsigned char* data; /**< Raw record data */
        unsigned char* stringTable; /**< String table data */
};
#endif
