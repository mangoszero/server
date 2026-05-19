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

#ifndef FIELD_H
#define FIELD_H

#include "Common/Common.h"
#include <mysql.h>

/**
 * @brief Wrapper for database field values
 *
 * Field provides type-safe access to database query result values.
 * It handles NULL values and converts between string representations
 * and various C++ types (int, float, bool, string, etc.).
 */
class Field
{
    public:

        /**
         * @brief Simple data type enumeration for field classification
         */
        enum SimpleDataTypes
        {
            DB_TYPE_UNKNOWN = 0x00,
            DB_TYPE_STRING  = 0x01,
            DB_TYPE_INTEGER = 0x02,
            DB_TYPE_FLOAT   = 0x03,
            DB_TYPE_BOOL    = 0x04
        };

        /**
         * @brief Default constructor - creates NULL field
         */
        Field() : mValue(NULL), mType(MYSQL_TYPE_NULL) {}
        /**
         * @brief Constructor with value and type
         * @param value Pointer to string value
         * @param type MySQL field type
         */
        Field(const char* value, enum_field_types type) : mValue(value), mType(type) {}

        /**
         * @brief Destructor
         */
        ~Field() {}

        /**
         * @brief Get the MySQL field type
         * @return MySQL field type enumeration
         */
        enum enum_field_types GetType() const { return mType; }
        /**
         * @brief Check if field value is NULL
         * @return True if NULL, false otherwise
         */
        bool IsNULL() const { return mValue == NULL; }

        /**
         * @brief Get raw string value
         * @return Pointer to string value (may be NULL)
         */
        const char* GetString() const { return mValue; }
        /**
         * @brief Get C++ string value
         * @return String value (empty if NULL)
         */
        std::string GetCppString() const
        {
            return mValue ? mValue : "";                    // std::string s = 0 have undefine result in C++
        }
        /**
         * @brief Get float value
         * @return Float value (0.0 if NULL)
         */
        float GetFloat() const { return mValue ? static_cast<float>(atof(mValue)) : 0.0f; }
        /**
         * @brief Get boolean value
         * @return Boolean value (false if NULL or 0)
         */
        bool GetBool() const { return mValue ? atoi(mValue) > 0 : false; }
        /**
        * @brief Get double value
        * @return Double value (0.0 if NULL)
        */
        double GetDouble() const
        {
             return mValue ? static_cast<double>(atof(mValue)) : 0.0f;
        }

        /**
        * @brief Get 8-bit signed integer value
        * @return 8-bit signed integer (0 if NULL)
        */
        int8 GetInt8() const { return mValue ? static_cast<int8>(atol(mValue)) : int8(0); }
        /**
         * @brief Get 32-bit signed integer value
         * @return 32-bit signed integer (0 if NULL)
         */
        int32 GetInt32() const { return mValue ? static_cast<int32>(atol(mValue)) : int32(0); }
        /**
         * @brief Get 8-bit unsigned integer value
         * @return 8-bit unsigned integer (0 if NULL)
         */
        uint8 GetUInt8() const { return mValue ? static_cast<uint8>(atol(mValue)) : uint8(0); }
        /**
         * @brief Get 16-bit unsigned integer value
         * @return 16-bit unsigned integer (0 if NULL)
         */
        uint16 GetUInt16() const { return mValue ? static_cast<uint16>(atol(mValue)) : uint16(0); }
        /**
         * @brief Get 16-bit signed integer value
         * @return 16-bit signed integer (0 if NULL)
         */
        int16 GetInt16() const { return mValue ? static_cast<int16>(atol(mValue)) : int16(0); }
        /**
         * @brief Get 32-bit unsigned integer value
         * @return 32-bit unsigned integer (0 if NULL)
         */
        uint32 GetUInt32() const { return mValue ? static_cast<uint32>(atol(mValue)) : uint32(0); }
        /**
         * @brief Get 64-bit unsigned integer value
         * @return 64-bit unsigned integer (0 if NULL)
         */
        uint64 GetUInt64() const
        {
            uint64 value = 0;
            if (!mValue || sscanf(mValue, UI64FMTD, &value) == -1)
            {
                return 0;
            }

            return value;
        }
        /**
        * @brief Get 64-bit signed integer value
        * @return 64-bit signed integer (0 if NULL)
        */
        // TODO: should this be int64 not uint64
        uint64 GetInt64() const
        {
            int64 value = 0;
            if (!mValue || sscanf(mValue, SI64FMTD, &value) == -1)
            {
                return 0;
            }

            return value;
        }

        /**
         * @brief Set the MySQL field type
         * @param type MySQL field type
         */
        void SetType(enum enum_field_types type) { mType = type; }

        /**
         * @brief Set the field value (no memory allocation, pointer only)
         *
         * This caches pointers returned by DBMS APIs without memory allocation.
         * The caller must ensure the pointer remains valid.
         *
         * @param value Pointer to string value
         */
        void SetValue(const char* value) { mValue = value; }

    private:
        /**
         * @brief Copy constructor (disabled)
         */
        Field(Field const&);
        /**
         * @brief Assignment operator (disabled)
         */
        Field& operator=(Field const&);

        const char* mValue; /**< Pointer to field value string */
        enum_field_types mType; /**< MySQL field type */
};
#endif
