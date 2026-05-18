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

#ifndef QUERYRESULT_H
#define QUERYRESULT_H

#include "Common/Common.h"
#include "Utilities/Errors.h"
#include "Field.h"

/**
 * @brief Abstract base class for database query results
 *
 * QueryResult provides the interface for accessing database query results.
 * It supports row iteration and field access. Concrete implementations
 * (like QueryResultMysql) provide the actual database-specific functionality.
 */
class QueryResult
{
    public:
        /**
         * @brief Constructor with row and field count
         * @param rowCount Number of rows in result set
         * @param fieldCount Number of fields per row
         */
        QueryResult(uint64 rowCount, uint32 fieldCount)
            : mFieldCount(fieldCount), mRowCount(rowCount), mCurrentRow{} {}

        /**
         * @brief Virtual destructor
         */
        virtual ~QueryResult() {}

        /**
         * @brief Move to next row in result set
         * @return True if moved to next row, false if no more rows
         */
        virtual bool NextRow() = 0;

        /**
         * @brief Get current row fields
         * @return Pointer to current row field array
         */
        Field* Fetch() const { return mCurrentRow; }

        /**
         * @brief Get field by index from current row
         * @param index Field index
         * @return Reference to field at index
         */
        const Field& operator [](int index) const { return mCurrentRow[index]; }

        /**
         * @brief Get number of fields per row
         * @return Field count
         */
        uint32 GetFieldCount() const { return mFieldCount; }
        /**
         * @brief Get number of rows in result set
         * @return Row count
         */
        uint64 GetRowCount() const { return mRowCount; }

    protected:
        Field* mCurrentRow; /**< Current row field array */
        uint32 mFieldCount; /**< Number of fields per row */
        uint64 mRowCount; /**< Total number of rows */
};

/**
 * @brief Vector of field names for named query results
 */
typedef std::vector<std::string> QueryFieldNames;

/**
 * @brief Query result with named field access
 *
 * QueryNamedResult wraps a QueryResult and adds the ability to
 * access fields by name instead of just by index. This provides
 * more readable and maintainable code.
 */
class QueryNamedResult
{
    public:
        /**
         * @brief Constructor from QueryResult and field names
         * @param query QueryResult to wrap (takes ownership)
         * @param names Vector of field names
         */
        explicit QueryNamedResult(QueryResult* query, QueryFieldNames const& names) : mQuery(query), mFieldNames(names) {}
        /**
         * @brief Destructor - deletes wrapped QueryResult
         */
        ~QueryNamedResult() { delete mQuery; }

        // compatible interface with QueryResult
        /**
         * @brief Move to next row
         * @return True if moved to next row, false if no more rows
         */
        bool NextRow() { return mQuery->NextRow(); }
        /**
         * @brief Get current row fields
         * @return Pointer to current row field array
         */
        Field* Fetch() const { return mQuery->Fetch(); }
        /**
         * @brief Get field count
         * @return Number of fields per row
         */
        uint32 GetFieldCount() const { return mQuery->GetFieldCount(); }
        /**
         * @brief Get row count
         * @return Total number of rows
         */
        uint64 GetRowCount() const { return mQuery->GetRowCount(); }
        /**
         * @brief Get field by index
         * @param index Field index
         * @return Reference to field at index
         */
        Field const& operator[](int index) const { return (*mQuery)[index]; }

        /**
         * @brief Get field by name (named access)
         * @param name Field name
         * @return Reference to field with given name
         */
        Field const& operator[](const std::string& name) const { return mQuery->Fetch()[GetField_idx(name)]; }
        /**
         * @brief Get all field names
         * @return Const reference to field names vector
         */
        QueryFieldNames const& GetFieldNames() const { return mFieldNames; }

        /**
         * @brief Get field index by name
         * @param name Field name to look up
         * @return Field index, or -1 if not found
         */
        uint32 GetField_idx(const std::string& name) const
        {
            for (size_t idx = 0; idx < mFieldNames.size(); ++idx)
            {
                if (mFieldNames[idx] == name)
                {
                    return idx;
                }
            }
            MANGOS_ASSERT(false && "unknown field name");
            return uint32(-1);
        }

    protected:
        QueryResult* mQuery; /**< Wrapped QueryResult (owned) */
        QueryFieldNames mFieldNames; /**< Field name to index mapping */
};

#endif
