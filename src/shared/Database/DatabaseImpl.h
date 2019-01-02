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

#include "Database/Database.h"
#include "Database/SqlOperations.h"

/// Function body definitions for the template function members of the Database class

#define ASYNC_QUERY_BODY(sql) if (!sql || !m_pResultQueue) return false;
#define ASYNC_DELAYHOLDER_BODY(holder) if (!holder || !m_pResultQueue) return false;

#define ASYNC_PQUERY_BODY(format, szQuery) \
    if(!format) return false; \
    \
    char szQuery [MAX_QUERY_LEN]; \
    \
    { \
        va_list ap; \
        \
        va_start(ap, format); \
        int res = vsnprintf( szQuery, MAX_QUERY_LEN, format, ap ); \
        va_end(ap); \
        \
        if(res==-1) \
        { \
            sLog.outError("SQL Query truncated (and not execute) for format: %s",format); \
            return false; \
        } \
    }

// -- Query / member --

template<class Class>
bool
/**
 * @brief
 *
 * @param object
 * @param )
 * @param sql
 * @return bool
 */
Database::AsyncQuery(Class* object, void (Class::*method)(QueryResult*), const char* sql)
{
    ASYNC_QUERY_BODY(sql)
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::QueryCallback<Class>(object, method), m_pResultQueue));
}

template<class Class, typename ParamType1>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param ParamType1)
 * @param param1
 * @param sql
 * @return bool
 */
Database::AsyncQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1), ParamType1 param1, const char* sql)
{
    ASYNC_QUERY_BODY(sql)
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::QueryCallback<Class, ParamType1>(object, method, (QueryResult*)NULL, param1), m_pResultQueue));
}

template<class Class, typename ParamType1, typename ParamType2>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param ParamType1
 * @param ParamType2)
 * @param param1
 * @param param2
 * @param sql
 * @return bool
 */
Database::AsyncQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* sql)
{
    ASYNC_QUERY_BODY(sql)
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::QueryCallback<Class, ParamType1, ParamType2>(object, method, (QueryResult*)NULL, param1, param2), m_pResultQueue));
}

template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param ParamType1
 * @param ParamType2
 * @param ParamType3)
 * @param param1
 * @param param2
 * @param param3
 * @param sql
 * @return bool
 */
Database::AsyncQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* sql)
{
    ASYNC_QUERY_BODY(sql)
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::QueryCallback<Class, ParamType1, ParamType2, ParamType3>(object, method, (QueryResult*)NULL, param1, param2, param3), m_pResultQueue));
}

// -- Query / static --
template<typename ParamType1>
bool
/**
 * @brief
 *
 * @param
 * @param ParamType1)
 * @param param1
 * @param sql
 * @return bool
 */
Database::AsyncQuery(void (*method)(QueryResult*, ParamType1), ParamType1 param1, const char* sql)
{
    ASYNC_QUERY_BODY(sql)
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::SQueryCallback<ParamType1>(method, (QueryResult*)NULL, param1), m_pResultQueue));
}

template<typename ParamType1, typename ParamType2>
bool
/**
 * @brief
 *
 * @param
 * @param ParamType1
 * @param ParamType2)
 * @param param1
 * @param param2
 * @param sql
 * @return bool
 */
Database::AsyncQuery(void (*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* sql)
{
    ASYNC_QUERY_BODY(sql)
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::SQueryCallback<ParamType1, ParamType2>(method, (QueryResult*)NULL, param1, param2), m_pResultQueue));
}

template<typename ParamType1, typename ParamType2, typename ParamType3>
bool
/**
 * @brief
 *
 * @param
 * @param ParamType1
 * @param ParamType2
 * @param ParamType3)
 * @param param1
 * @param param2
 * @param param3
 * @param sql
 * @return bool
 */
Database::AsyncQuery(void (*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* sql)
{
    ASYNC_QUERY_BODY(sql)
    return m_threadBody->Delay(new SqlQuery(sql, new MaNGOS::SQueryCallback<ParamType1, ParamType2, ParamType3>(method, (QueryResult*)NULL, param1, param2, param3), m_pResultQueue));
}

// -- PQuery / member --
template<class Class>
bool
/**
 * @brief
 *
 * @param object
 * @param )
 * @param format...
 * @return bool
 */
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*), const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(object, method, szQuery);
}

template<class Class, typename ParamType1>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param ParamType1)
 * @param param1
 * @param format...
 * @return bool
 */
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(object, method, param1, szQuery);
}

template<class Class, typename ParamType1, typename ParamType2>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param ParamType1
 * @param ParamType2)
 * @param param1
 * @param param2
 * @param format...
 * @return bool
 */
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(object, method, param1, param2, szQuery);
}

template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param ParamType1
 * @param ParamType2
 * @param ParamType3)
 * @param param1
 * @param param2
 * @param param3
 * @param format...
 * @return bool
 */
Database::AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(object, method, param1, param2, param3, szQuery);
}

// -- PQuery / static --
template<typename ParamType1>
bool
/**
 * @brief
 *
 * @param
 * @param ParamType1)
 * @param param1
 * @param format...
 * @return bool
 */
Database::AsyncPQuery(void (*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(method, param1, szQuery);
}

template<typename ParamType1, typename ParamType2>
bool
/**
 * @brief
 *
 * @param
 * @param ParamType1
 * @param ParamType2)
 * @param param1
 * @param param2
 * @param format...
 * @return bool
 */
Database::AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(method, param1, param2, szQuery);
}

template<typename ParamType1, typename ParamType2, typename ParamType3>
bool
/**
 * @brief
 *
 * @param
 * @param ParamType1
 * @param ParamType2
 * @param ParamType3)
 * @param param1
 * @param param2
 * @param param3
 * @param format...
 * @return bool
 */
Database::AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...)
{
    ASYNC_PQUERY_BODY(format, szQuery)
    return AsyncQuery(method, param1, param2, param3, szQuery);
}

// -- QueryHolder --
template<class Class>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param )
 * @param holder
 * @return bool
 */
Database::DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*), SqlQueryHolder* holder)
{
    ASYNC_DELAYHOLDER_BODY(holder)
    return holder->Execute(new MaNGOS::QueryCallback<Class, SqlQueryHolder*>(object, method, (QueryResult*)NULL, holder), m_threadBody, m_pResultQueue);
}

template<class Class, typename ParamType1>
bool
/**
 * @brief
 *
 * @param object
 * @param
 * @param
 * @param ParamType1)
 * @param holder
 * @param param1
 * @return bool
 */
Database::DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*, ParamType1), SqlQueryHolder* holder, ParamType1 param1)
{
    ASYNC_DELAYHOLDER_BODY(holder)
    return holder->Execute(new MaNGOS::QueryCallback<Class, SqlQueryHolder*, ParamType1>(object, method, (QueryResult*)NULL, holder, param1), m_threadBody, m_pResultQueue);
}

#undef ASYNC_QUERY_BODY
#undef ASYNC_PQUERY_BODY
#undef ASYNC_DELAYHOLDER_BODY
