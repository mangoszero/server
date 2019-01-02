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

#ifndef FIELD_H
#define FIELD_H

#include "Common/Common.h"
#include <mysql.h>

/**
 * @brief
 *
 */
class Field
{
    public:

        /**
         * @brief
         *
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
         * @brief
         *
         */
        Field() : mValue(NULL), mType(MYSQL_TYPE_NULL) {}
        /**
         * @brief
         *
         * @param value
         * @param type
         */
        Field(const char* value, enum_field_types type) : mValue(value), mType(type) {}

        /**
         * @brief
         *
         */
        ~Field() {}

        /**
         * @brief
         *
         * @return enum_field_type
         */
        enum enum_field_types GetType() const { return mType; }
        /**
         * @brief
         *
         * @return bool
         */
        bool IsNULL() const { return mValue == NULL; }

        /**
         * @brief
         *
         * @return const char
         */
        const char* GetString() const { return mValue; }
        /**
         * @brief
         *
         * @return std::string
         */
        std::string GetCppString() const
        {
            return mValue ? mValue : "";                    // std::string s = 0 have undefine result in C++
        }
        /**
         * @brief
         *
         * @return float
         */
        float GetFloat() const { return mValue ? static_cast<float>(atof(mValue)) : 0.0f; }
        /**
         * @brief
         *
         * @return bool
         */
        bool GetBool() const { return mValue ? atoi(mValue) > 0 : false; }
        double GetDouble() const { return mValue ? static_cast<double>(atof(mValue)) : 0.0f; }
        int8 GetInt8() const { return mValue ? static_cast<int8>(atol(mValue)) : int8(0); }
        /**
         * @brief
         *
         * @return int32
         */
        int32 GetInt32() const { return mValue ? static_cast<int32>(atol(mValue)) : int32(0); }
        /**
         * @brief
         *
         * @return uint8
         */
        uint8 GetUInt8() const { return mValue ? static_cast<uint8>(atol(mValue)) : uint8(0); }
        /**
         * @brief
         *
         * @return uint16
         */
        uint16 GetUInt16() const { return mValue ? static_cast<uint16>(atol(mValue)) : uint16(0); }
        /**
         * @brief
         *
         * @return int16
         */
        int16 GetInt16() const { return mValue ? static_cast<int16>(atol(mValue)) : int16(0); }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetUInt32() const { return mValue ? static_cast<uint32>(atol(mValue)) : uint32(0); }
        /**
         * @brief
         *
         * @return uint64
         */
        uint64 GetUInt64() const
        {
            uint64 value = 0;
            if (!mValue || sscanf(mValue, UI64FMTD, &value) == -1)
                { return 0; }

            return value;
        }

        uint64 GetInt64() const
        {
            int64 value = 0;
            if (!mValue || sscanf(mValue, SI64FMTD, &value) == -1)
                return 0;

            return value;
        }

        /**
         * @brief
         *
         * @param type
         */
        void SetType(enum enum_field_types type) { mType = type; }

        /**
         * @brief no need for memory allocations to store resultset field strings
         *
         * all we need is to cache pointers returned by different DBMS APIs
         *
         * @param value
         */
        void SetValue(const char* value) { mValue = value; }

    private:
        /**
         * @brief
         *
         * @param
         */
        Field(Field const&);
        /**
         * @brief
         *
         * @param
         * @return Field &operator
         */
        Field& operator=(Field const&);

        const char* mValue; /**< TODO */
        enum_field_types mType; /**< TODO */
};
#endif
