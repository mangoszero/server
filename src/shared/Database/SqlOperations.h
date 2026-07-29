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

#ifndef MANGOS_H_SQLOPERATIONS
#define MANGOS_H_SQLOPERATIONS

#include <utility>
#include "Platform/Define.h"
#include "Utilities/Util.h"
#include <vector>

#include "LockedQueue/LockedQueue.h"
#include <future>
#include <queue>
#include "Utilities/Callback.h"

/// ---- BASE ---

class Database;
class SqlConnection;
class SqlDelayThread;
class SqlStmtParameters;

/**
 * @brief
 *
 */
class SqlOperation
{
    public:
        /**
         * @brief
         *
         */
        virtual void OnRemove() { delete this; }
        /**
         * @brief
         *
         * @param conn
         * @return bool
         */
        /**
         * @brief Run this operation, taking the connection's lock.
         *
         * Entry point for a standalone operation (i.e. from SqlDelayThread).
         */
        bool Execute(SqlConnection* conn);

        /**
         * @brief Run this operation with the connection's lock ALREADY held.
         *
         * SqlTransaction takes the lock once for the whole transaction and then runs
         * each queued statement through here. That split is what lets a connection use
         * a plain mutex: previously the transaction locked the connection and then every
         * statement inside it locked the same connection again, which only worked because
         * the mutex was recursive -- and would have deadlocked the first transaction the
         * moment it stopped being.
         */
        virtual bool ExecuteLocked(SqlConnection* conn) = 0;
        /**
         * @brief
         *
         */
        virtual ~SqlOperation() {}
};

/// ---- ASYNC STATEMENTS / TRANSACTIONS ----

/**
 * @brief
 *
 */
class SqlPlainRequest : public SqlOperation
{
    private:
        const char* m_sql; /**< TODO */
    public:
        /**
         * @brief
         *
         * @param sql
         */
        SqlPlainRequest(const char* sql) : m_sql(mangos_strdup(sql)) {}
        /**
         * @brief
         *
         */
        ~SqlPlainRequest() { char* tofree = const_cast<char*>(m_sql); delete[] tofree; }
        /**
         * @brief
         *
         * @param conn
         * @return bool
         */
        bool ExecuteLocked(SqlConnection* conn) override;
};

/**
 * @brief
 *
 */
class SqlTransaction : public SqlOperation
{
    private:
        std::vector<SqlOperation* > m_queue; /**< TODO */

    public:
        /**
         * @brief
         *
         */
        SqlTransaction() {}
        /**
         * @brief
         *
         */
        ~SqlTransaction();

        /**
         * @brief
         *
         * @param sql
         */
        void DelayExecute(SqlOperation* sql) { m_queue.push_back(sql); }

        /**
         * @brief
         *
         * @param conn
         * @return bool
         */
        bool ExecuteLocked(SqlConnection* conn) override;
};

/**
 * @brief
 *
 */
class SqlPreparedRequest : public SqlOperation
{
    public:
        /**
         * @brief
         *
         * @param nIndex
         * @param arg
         */
        SqlPreparedRequest(int nIndex, SqlStmtParameters* arg);
        /**
         * @brief
         *
         */
        ~SqlPreparedRequest();

        /**
         * @brief
         *
         * @param conn
         * @return bool
         */
        bool ExecuteLocked(SqlConnection* conn) override;

    private:
        const int m_nIndex; /**< TODO */
        SqlStmtParameters* m_param; /**< TODO */
};

/// ---- ASYNC QUERIES ----

class SqlQuery;                                             /// contains a single async query
class QueryResult;                                          /// the result of one
class SqlResultQueue;                                       /// queue for thread sync
/**
 * @brief A transaction that reports whether it actually committed.
 *
 * Owns the transaction detached from the TSS slot; the promise is owned by the caller,
 * which is parked on the matching future and therefore outlives this operation.
 */
class SqlTransactionResultSignal : public SqlOperation
{
    private:
        SqlTransaction* m_trans;        ///< owned wrapped transaction
        std::promise<bool>* m_result;   ///< caller-owned result channel

    public:

        SqlTransactionResultSignal(SqlTransaction* trans, std::promise<bool>* result)
            : m_trans(trans), m_result(result) {}

        bool ExecuteLocked(SqlConnection* conn) override;
};

class SqlQueryHolder;                                       /// groups several async quries
class SqlQueryHolderEx;                                     /// points to a holder, added to the delay thread

/**
 * @brief
 *
 */
class SqlResultQueue : public MaNGOS::LockedQueue<MaNGOS::IQueryCallback*>
{
    public:
        /**
         * @brief
         *
         */
        SqlResultQueue() {}
        /**
         * @brief
         *
         */
        void Update();
};

/**
 * @brief
 *
 */
class SqlQuery : public SqlOperation
{
    private:
        const char* m_sql; /**< TODO */
        MaNGOS::IQueryCallback* m_callback; /**< TODO */
        SqlResultQueue* m_queue; /**< TODO */
    public:
        /**
         * @brief
         *
         * @param sql
         * @param callback
         * @param queue
         */
        SqlQuery(const char* sql, MaNGOS::IQueryCallback* callback, SqlResultQueue* queue)
            : m_sql(mangos_strdup(sql)), m_callback(callback), m_queue(queue) {}
        /**
         * @brief
         *
         */
        ~SqlQuery() { char* tofree = const_cast<char*>(m_sql); delete[] tofree; }
        /**
         * @brief
         *
         * @param conn
         * @return bool
         */
        bool ExecuteLocked(SqlConnection* conn) override;
};

/**
 * @brief
 *
 */
class SqlQueryHolder
{
        friend class SqlQueryHolderEx;
    private:
        /**
         * @brief
         *
         */
        typedef std::pair<const char*, QueryResult*> SqlResultPair;
        std::vector<SqlResultPair> m_queries; /**< TODO */
    public:
        /**
         * @brief
         *
         */
        SqlQueryHolder() {}
        /**
         * @brief
         *
         */
        ~SqlQueryHolder();
        /**
         * @brief
         *
         * @param index
         * @param sql
         * @return bool
         */
        bool SetQuery(size_t index, const char* sql);
        /**
         * @brief
         *
         * @param index
         * @param format...
         * @return bool
         */
        bool SetPQuery(size_t index, const char* format, ...) ATTR_PRINTF(3, 4);
        /**
         * @brief
         *
         * @param size
         */
        void SetSize(size_t size);
        /**
         * @brief
         *
         * @param index
         * @return QueryResult
         */
        QueryResult* GetResult(size_t index);
        /**
         * @brief
         *
         * @param index
         * @param result
         */
        void SetResult(size_t index, QueryResult* result);
        /**
         * @brief
         *
         * @param callback
         * @param thread
         * @param queue
         * @return bool
         */
        bool Execute(MaNGOS::IQueryCallback* callback, SqlDelayThread* thread, SqlResultQueue* queue);
};

/**
 * @brief
 *
 */
class SqlQueryHolderEx : public SqlOperation
{
    private:
        SqlQueryHolder* m_holder; /**< TODO */
        MaNGOS::IQueryCallback* m_callback; /**< TODO */
        SqlResultQueue* m_queue; /**< TODO */
    public:
        /**
         * @brief
         *
         * @param holder
         * @param callback
         * @param queue
         */
        SqlQueryHolderEx(SqlQueryHolder* holder, MaNGOS::IQueryCallback* callback, SqlResultQueue* queue)
            : m_holder(holder), m_callback(callback), m_queue(queue) {}
        /**
         * @brief
         *
         * @param conn
         * @return bool
         */
        bool ExecuteLocked(SqlConnection* conn) override;
};
#endif                                                      //__SQLOPERATIONS_H
