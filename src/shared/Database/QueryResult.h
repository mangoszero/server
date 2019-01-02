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

#ifndef QUERYRESULT_H
#define QUERYRESULT_H

#include "Common/Common.h"
#include "Utilities/Errors.h"
#include "Field.h"

/**
 * @brief
 *
 */
class QueryResult
{
    public:
        /**
         * @brief
         *
         * @param rowCount
         * @param fieldCount
         */
        QueryResult(uint64 rowCount, uint32 fieldCount)
            : mFieldCount(fieldCount), mRowCount(rowCount) {}

        /**
         * @brief
         *
         */
        virtual ~QueryResult() {}

        /**
         * @brief
         *
         * @return bool
         */
        virtual bool NextRow() = 0;

        /**
         * @brief
         *
         * @return Field
         */
        Field* Fetch() const { return mCurrentRow; }

        /**
         * @brief
         *
         * @param index
         * @return const Field &operator
         */
        const Field& operator [](int index) const { return mCurrentRow[index]; }

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetFieldCount() const { return mFieldCount; }
        /**
         * @brief
         *
         * @return uint64
         */
        uint64 GetRowCount() const { return mRowCount; }

    protected:
        Field* mCurrentRow; /**< TODO */
        uint32 mFieldCount; /**< TODO */
        uint64 mRowCount; /**< TODO */
};

/**
 * @brief
 *
 */
typedef std::vector<std::string> QueryFieldNames;

/**
 * @brief
 *
 */
class QueryNamedResult
{
    public:
        /**
         * @brief
         *
         * @param query
         * @param names
         */
        explicit QueryNamedResult(QueryResult* query, QueryFieldNames const& names) : mQuery(query), mFieldNames(names) {}
        /**
         * @brief
         *
         */
        ~QueryNamedResult() { delete mQuery; }

        // compatible interface with QueryResult
        /**
         * @brief
         *
         * @return bool
         */
        bool NextRow() { return mQuery->NextRow(); }
        /**
         * @brief
         *
         * @return Field
         */
        Field* Fetch() const { return mQuery->Fetch(); }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetFieldCount() const { return mQuery->GetFieldCount(); }
        /**
         * @brief
         *
         * @return uint64
         */
        uint64 GetRowCount() const { return mQuery->GetRowCount(); }
        /**
         * @brief
         *
         * @param index
         * @return const Field &operator
         */
        Field const& operator[](int index) const { return (*mQuery)[index]; }

        /**
         * @brief named access
         *
         * @param name
         * @return const Field &operator
         */
        Field const& operator[](const std::string& name) const { return mQuery->Fetch()[GetField_idx(name)]; }
        /**
         * @brief
         *
         * @return const QueryFieldNames
         */
        QueryFieldNames const& GetFieldNames() const { return mFieldNames; }

        /**
         * @brief
         *
         * @param name
         * @return uint32
         */
        uint32 GetField_idx(const std::string& name) const
        {
            for (size_t idx = 0; idx < mFieldNames.size(); ++idx)
            {
                if (mFieldNames[idx] == name)
                    { return idx; }
            }
            MANGOS_ASSERT(false && "unknown field name");
            return uint32(-1);
        }

    protected:
        QueryResult* mQuery; /**< TODO */
        QueryFieldNames mFieldNames; /**< TODO */
};

#endif
