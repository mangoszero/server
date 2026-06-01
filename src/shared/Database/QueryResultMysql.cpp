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
 * @file QueryResultMysql.cpp
 * @brief MySQL-specific query result implementation
 *
 * This file implements the QueryResultMysql class which wraps MySQL
 * query results (MYSQL_RES) and provides MaNGOS-compatible access
 * to row data. It handles:
 * - Result set iteration
 * - Field type mapping from MySQL to MaNGOS types
 * - Memory management for MySQL result structures
 *
 * @note This file is only compiled when DO_POSTGRESQL is not defined
 */

#ifndef DO_POSTGRESQL

#include "DatabaseEnv.h"
#include "Utilities/Errors.h"

/**
 * @brief Construct a MySQL query result wrapper
 * @param result MySQL result handle (MYSQL_RES*)
 * @param fields MySQL field metadata array
 * @param rowCount Number of rows in result set
 * @param fieldCount Number of fields per row
 *
 * Initializes the query result with MySQL data. Creates a Field array
 * for the current row and sets up field type information for proper
 * data conversion.
 *
 * @note The MYSQL_RES* ownership is transferred to this object
 */
QueryResultMysql::QueryResultMysql(MYSQL_RES* result, MYSQL_FIELD* fields, uint64 rowCount, uint32 fieldCount) :
    QueryResult(rowCount, fieldCount), mResult(result)
{
    mCurrentRow = new Field[mFieldCount];
    MANGOS_ASSERT(mCurrentRow);

    for (uint32 i = 0; i < mFieldCount; ++i)
    {
        mCurrentRow[i].SetType(fields[i].type);
    }
}

/**
 * @brief Destroy the MySQL query result
 *
 * Cleans up all associated resources by calling EndQuery(), which:
 * - Deletes the current row Field array
 * - Frees the MySQL result structure
 */
QueryResultMysql::~QueryResultMysql()
{
    EndQuery();
}

/**
 * @brief Fetch the next row from the result set
 * @return true if a row was fetched, false if no more rows
 *
 * Retrieves the next row from the MySQL result set using mysql_fetch_row().
 * If successful, populates the mCurrentRow Field array with the row data.
 * Automatically cleans up when no more rows are available.
 *
 * @note Must be called before accessing row data after construction
 */
bool QueryResultMysql::NextRow()
{
    MYSQL_ROW row;

    if (!mResult)
    {
        return false;
    }

    row = mysql_fetch_row(mResult);
    if (!row)
    {
        EndQuery();
        return false;
    }

    for (uint32 i = 0; i < mFieldCount; ++i)
    {
        mCurrentRow[i].SetValue(row[i]);
    }

    return true;
}

/**
 * @brief Clean up query resources
 *
 * Frees all memory associated with this query result:
 * - Deletes the Field array for current row
 * - Frees the MySQL result structure with mysql_free_result()
 *
 * Called automatically by destructor and NextRow() when done.
 * Safe to call multiple times (idempotent).
 */
void QueryResultMysql::EndQuery()
{
    delete[] mCurrentRow;
    mCurrentRow = 0;

    if (mResult)
    {
        mysql_free_result(mResult);
        mResult = 0;
    }
}

/**
 * @brief Convert MySQL field type to simplified MaNGOS type
 * @param type MySQL field type enum (enum_field_types)
 * @return Simplified type classification (STRING, INTEGER, FLOAT, RAW)
 *
 * Maps the various MySQL field types to four basic categories used
 * by MaNGOS for data handling:
 * - DB_TYPE_STRING: Text types, dates, blobs
 * - DB_TYPE_INTEGER: Integer numeric types
 * - DB_TYPE_FLOAT: Floating point types
 * - DB_TYPE_RAW: Binary data (decimal, bit fields)
 *
 * This mapping ensures consistent handling across different MySQL versions.
 */
Field::SimpleDataTypes QueryResultMysql::GetSimpleType(enum_field_types type)
{
    switch (type)
    {
        case FIELD_TYPE_TIMESTAMP:
        case FIELD_TYPE_DATE:
        case FIELD_TYPE_TIME:
        case FIELD_TYPE_DATETIME:
        case FIELD_TYPE_YEAR:
        case FIELD_TYPE_STRING:
        case FIELD_TYPE_VAR_STRING:
        case FIELD_TYPE_BLOB:
        case FIELD_TYPE_SET:
        case FIELD_TYPE_NULL:
            return Field::DB_TYPE_STRING;
        case FIELD_TYPE_TINY:
        case FIELD_TYPE_SHORT:
        case FIELD_TYPE_LONG:
        case FIELD_TYPE_INT24:
        case FIELD_TYPE_LONGLONG:
        case FIELD_TYPE_ENUM:
            return Field::DB_TYPE_INTEGER;
        case FIELD_TYPE_DECIMAL:
        case FIELD_TYPE_FLOAT:
        case FIELD_TYPE_DOUBLE:
            return Field::DB_TYPE_FLOAT;
        default:
            return Field::DB_TYPE_UNKNOWN;
    }
}
#endif
