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

#ifndef DO_POSTGRESQL

#if !defined(QUERYRESULTMYSQL_H)
#define QUERYRESULTMYSQL_H

#include "Common/Common.h"

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
