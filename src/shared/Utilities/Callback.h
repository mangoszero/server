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

#ifndef MANGOS_CALLBACK_H
#define MANGOS_CALLBACK_H

#include <functional>
#include <utility>

class QueryResult;
class SqlQueryHolder;

/// ---------- QUERY CALLBACKS -----------
///
/// An async query is executed on a worker thread; the result is handed back
/// on the thread that owns it (see SqlResultQueue::Update()) by queuing one
/// of these and calling Execute() on it. Callers bind whatever object/state
/// they need into a std::function, so no per-arity callback classes are
/// needed here.

namespace MaNGOS
{
    /**
     * @brief Type-erased handle for a pending asynchronous query result.
     */
    class IQueryCallback
    {
        public:
            virtual ~IQueryCallback() {}

            /// Invokes the bound callback with whatever result has been set.
            virtual void Execute() = 0;

            /// Called on the worker thread once the query has run.
            virtual void SetResult(QueryResult* result) = 0;

            virtual QueryResult* GetResult() = 0;
    };

    /**
     * @brief Callback for Database::AsyncQuery()/AsyncPQuery().
     */
    class QueryCallback : public IQueryCallback
    {
        public:
            explicit QueryCallback(std::function<void(QueryResult*)> callback)
                : m_callback(std::move(callback)), m_result(nullptr)
            {}

            void Execute() override { m_callback(m_result); }
            void SetResult(QueryResult* result) override { m_result = result; }
            QueryResult* GetResult() override { return m_result; }

        private:
            std::function<void(QueryResult*)> m_callback;
            QueryResult* m_result;
    };

    /**
     * @brief Callback for Database::DelayQueryHolder(). The holder carries its
     * own per-query results (see SqlQueryHolder::GetResult()), so the
     * QueryResult* handed to the callback is always null.
     */
    class QueryHolderCallback : public IQueryCallback
    {
        public:
            QueryHolderCallback(std::function<void(QueryResult*, SqlQueryHolder*)> callback, SqlQueryHolder* holder)
                : m_callback(std::move(callback)), m_holder(holder), m_result(nullptr)
            {}

            void Execute() override { m_callback(m_result, m_holder); }
            void SetResult(QueryResult* result) override { m_result = result; }
            QueryResult* GetResult() override { return m_result; }

        private:
            std::function<void(QueryResult*, SqlQueryHolder*)> m_callback;
            SqlQueryHolder* m_holder;
            QueryResult* m_result;
    };
}

#endif
