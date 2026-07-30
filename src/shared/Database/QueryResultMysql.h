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

#ifndef DO_POSTGRESQL

#if !defined(QUERYRESULTMYSQL_H)
#define QUERYRESULTMYSQL_H

#include "QueryResult.h"
#include "Platform/Define.h"

#ifdef WIN32
#include <winsock2.h>
#endif

#include <mysql.h>

/**
 * @brief
 *
 */
class QueryResultMysql : public QueryResult
{
    public:
        /**
         * @brief
         *
         * @param result
         * @param fields
         * @param rowCount
         * @param fieldCount
         */
        QueryResultMysql(MYSQL_RES* result, MYSQL_FIELD* fields, uint64 rowCount, uint32 fieldCount);

        /**
         * @brief
         *
         */
        ~QueryResultMysql();

        /**
         * @brief
         *
         * @return bool
         */
        bool NextRow() override;

        /**
         * @brief
         *
         * @param type
         * @return Field::SimpleDataTypes
         */
        static Field::SimpleDataTypes GetSimpleType(enum_field_types type);

    private:
        /**
         * @brief
         *
         */
        void EndQuery();

        MYSQL_RES* mResult; /**< TODO */
};
#endif

#endif
