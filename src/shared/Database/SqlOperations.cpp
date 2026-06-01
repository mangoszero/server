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
 * @file SqlOperations.cpp
 * @brief Implementation of asynchronous SQL operations
 *
 * This file implements various SQL operation classes for asynchronous
 * database access. These include:
 * - Plain SQL statements (SqlPlainRequest)
 * - Transactions (SqlTransaction)
 * - Prepared statements (SqlPreparedRequest)
 * - Queries (SqlQuery, SqlQueryHolder)
 *
 * The operations support both immediate execution and deferred execution
 * through the delay thread system for improved server performance.
 */

#include "SqlOperations.h"
#include "SqlDelayThread.h"
#include "DatabaseEnv.h"
#include "DatabaseImpl.h"

/**
 * @def LOCK_DB_CONN
 * @brief RAII lock macro for database connections
 * @param conn The SqlConnection to lock
 *
 * Creates a SqlConnection::Lock guard that ensures thread-safe
 * access to the database connection during operation execution.
 */
#define LOCK_DB_CONN(conn) SqlConnection::Lock guard(conn)

/// ---- ASYNC STATEMENTS / TRANSACTIONS ----

/**
 * @brief Execute a plain SQL request
 * @param conn The database connection to use
 * @return true if execution succeeded, false otherwise
 *
 * Executes the stored SQL statement directly on the connection.
 * The connection is locked during execution to ensure thread safety.
 * This is the simplest form of SQL operation with no parameters or results.
 */
bool SqlPlainRequest::Execute(SqlConnection* conn)
{
    /// just do it
    LOCK_DB_CONN(conn);
    return conn->Execute(m_sql);
}

/**
 * @brief Destructor for SqlTransaction
 *
 * Cleans up any operations remaining in the transaction queue.
 * This should normally not contain any operations (they should be
 * executed or the transaction rolled back), but handles the edge case
 * where the transaction is destroyed before completion.
 */
SqlTransaction::~SqlTransaction()
{
    while (!m_queue.empty())
    {
        delete m_queue.back();
        m_queue.pop_back();
    }
}

/**
 * @brief Execute a transaction containing multiple operations
 * @param conn The database connection to use
 * @return true if transaction committed successfully, false if rolled back
 *
 * Executes all queued operations atomically within a transaction:
 * 1. Begins a database transaction
 * 2. Executes each operation in order
 * 3. If any operation fails, rolls back all changes
 * 4. If all succeed, commits the transaction
 *
 * @note Empty transactions return true without doing anything.
 */
bool SqlTransaction::Execute(SqlConnection* conn)
{
    if (m_queue.empty())
    {
        return true;
    }

    LOCK_DB_CONN(conn);

    conn->BeginTransaction();

    const int nItems = m_queue.size();
    for (int i = 0; i < nItems; ++i)
    {
        SqlOperation* pStmt = m_queue[i];

        if (!pStmt->Execute(conn))
        {
            conn->RollbackTransaction();
            return false;
        }
    }

    return conn->CommitTransaction();
}

/**
 * @brief Constructor for SqlPreparedRequest
 * @param nIndex Index of the prepared statement
 * @param arg Pointer to the statement parameters
 *
 * Creates a prepared statement request with the given statement index
 * and parameters. The parameters will be deleted when this request is
 * destroyed or executed.
 */
SqlPreparedRequest::SqlPreparedRequest(int nIndex, SqlStmtParameters* arg) : m_nIndex(nIndex), m_param(arg)
{
}

/**
 * @brief Destructor for SqlPreparedRequest
 *
 * Deletes the stored statement parameters to prevent memory leaks.
 */
SqlPreparedRequest::~SqlPreparedRequest()
{
    delete m_param;
}

/**
 * @brief Execute a prepared statement
 * @param conn The database connection to use
 * @return true if execution succeeded, false otherwise
 *
 * Executes the prepared statement with the stored parameters on the
 * given connection. Prepared statements are more efficient than plain
 * SQL for repeated execution with different parameters.
 */
bool SqlPreparedRequest::Execute(SqlConnection* conn)
{
    LOCK_DB_CONN(conn);
    return conn->ExecuteStmt(m_nIndex, *m_param);
}

/// ---- ASYNC QUERIES ----

/**
 * @brief Execute an asynchronous SQL query
 * @param conn The database connection to use
 * @return true if query executed and callback queued, false otherwise
 *
 * Executes a SELECT query and stores the results. The callback will be
 * invoked on the originating thread when results are available.
 * This provides asynchronous query execution to prevent blocking.
 *
 * @note The callback and result queue must be valid for this to succeed.
 */
bool SqlQuery::Execute(SqlConnection* conn)
{
    if (!m_callback || !m_queue)
    {
        return false;
    }

    LOCK_DB_CONN(conn);
    /// execute the query and store the result in the callback
    m_callback->SetResult(conn->Query(m_sql));
    /// add the callback to the sql result queue of the thread it originated from
    m_queue->add(m_callback);

    return true;
}

/**
 * @brief Process pending query callbacks
 *
 * Executes all callbacks that have been queued by completed queries.
 * This should be called regularly (typically each server tick) to
 * handle asynchronous query results on the main thread.
 *
 * Each callback is executed once and then deleted.
 */
void SqlResultQueue::Update()
{
    /// execute the callbacks waiting in the synchronization queue
    MaNGOS::IQueryCallback* callback = NULL;
    while (next(callback))
    {
        callback->Execute();
        delete callback;
    }
}

/**
 * @brief Execute all queries in the holder asynchronously
 * @param callback Callback to invoke when all queries complete
 * @param thread The delay thread to execute queries on
 * @param queue The result queue for callback synchronization
 * @return true if execution was scheduled, false if parameters invalid
 *
 * Schedules all queries for execution on the delay thread. When complete,
 * the callback will be invoked via the result queue on the original thread.
 * This batches multiple queries efficiently in a single operation.
 */
bool SqlQueryHolder::Execute(MaNGOS::IQueryCallback* callback, SqlDelayThread* thread, SqlResultQueue* queue)
{
    if (!callback || !thread || !queue)
    {
        return false;
    }

    /// delay the execution of the queries, sync them with the delay thread
    /// which will in turn resync on execution (via the queue) and call back
    SqlQueryHolderEx* holderEx = new SqlQueryHolderEx(this, callback, queue);
    thread->Delay(holderEx);
    return true;
}

/**
 * @brief Store a SQL query at a specific index
 * @param index The slot to store the query in
 * @param sql The SQL query string
 * @return true if stored successfully, false if slot invalid or occupied
 *
 * Stores a query string for later execution. The string is duplicated
 * so the original can be safely freed. The query is not executed until
 * Execute() is called on the holder.
 *
 * @note The holder must have been sized appropriately with SetSize() first.
 */
bool SqlQueryHolder::SetQuery(size_t index, const char* sql)
{
    if (m_queries.size() <= index)
    {
        sLog.outError("Query index (%zu) out of range (size: %zu) for query: %s", index, m_queries.size(), sql);
        return false;
    }

    if (m_queries[index].first != NULL)
    {
        sLog.outError("Attempt assign query to holder index (%zu) where other query stored (Old: [%s] New: [%s])",
                      index, m_queries[index].first, sql);
        return false;
    }

    /// not executed yet, just stored (it's not called a holder for nothing)
    m_queries[index] = SqlResultPair(mangos_strdup(sql), (QueryResult*)NULL);
    return true;
}

/**
 * @brief Store a parameterized SQL query at a specific index
 * @param index The slot to store the query in
 * @param format printf-style format string
 * @param ... Variable arguments for formatting
 * @return true if stored successfully, false if formatting failed
 *
 * Creates a query string from the format and arguments, then stores it
 * at the specified index. Handles variable argument formatting safely.
 *
 * @warning If the formatted query exceeds MAX_QUERY_LEN, it will fail.
 */
bool SqlQueryHolder::SetPQuery(size_t index, const char* format, ...)
{
    if (!format)
    {
        sLog.outError("Query (index: %zu) is empty.", index);
        return false;
    }

    va_list ap;
    char szQuery [MAX_QUERY_LEN];
    va_start(ap, format);
    int res = vsnprintf(szQuery, MAX_QUERY_LEN, format, ap);
    va_end(ap);

    if (res == -1)
    {
        sLog.outError("SQL Query truncated (and not execute) for format: %s", format);
        return false;
    }

    return SetQuery(index, szQuery);
}

/**
 * @brief Get the query result at a specific index
 * @param index The slot to retrieve the result from
 * @return Pointer to the QueryResult, or NULL if index invalid or no result
 *
 * Retrieves the result of a previously executed query. The query string
 * is freed on first access (transferred to caller responsibility). The
 * QueryResult pointer is also transferred to the caller who must delete it.
 *
 * @warning The caller is responsible for deleting the returned QueryResult!
 * @note Query strings are freed on first GetResult call or in destructor.
 */
QueryResult* SqlQueryHolder::GetResult(size_t index)
{
    if (index < m_queries.size())
    {
        /// the query strings are freed on the first GetResult or in the destructor
        if (m_queries[index].first != NULL)
        {
            delete[](const_cast<char*>(m_queries[index].first));
            m_queries[index].first = NULL;
        }
        /// when you get a result aways remember to delete it!
        return m_queries[index].second;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Store a query result at a specific index
 * @param index The slot to store the result in
 * @param result Pointer to the QueryResult
 *
 * Stores the result of a query execution. This is called internally
 * by the execution system. If the index is out of range, the result
 * is ignored.
 */
void SqlQueryHolder::SetResult(size_t index, QueryResult* result)
{
    /// store the result in the holder
    if (index < m_queries.size())
    {
        m_queries[index].second = result;
    }
}

/**
 * @brief Destructor for SqlQueryHolder
 *
 * Cleans up all queries and results that weren't retrieved via GetResult().
 * Any query strings still present are deleted, and any unretrieved results
 * are also deleted to prevent memory leaks.
 *
 * @note Already-retrieved results (where first == NULL) are NOT deleted
 * as they were transferred to the caller.
 */
SqlQueryHolder::~SqlQueryHolder()
{
    for (size_t i = 0; i < m_queries.size(); ++i)
    {
        /// if the result was never used, free the resources
        /// results used already (getresult called) are expected to be deleted
        if (m_queries[i].first != NULL)
        {
            delete[](const_cast<char*>(m_queries[i].first));
            delete m_queries[i].second;
        }
    }
}

/**
 * @brief Set the number of queries this holder will contain
 * @param size The number of query slots to allocate
 *
 * Resizes the internal storage to accommodate the specified number
 * of queries. This should be called before adding queries with SetQuery()
 * to avoid reallocations.
 *
 * @note This does not execute any queries, it only prepares storage.
 */
void SqlQueryHolder::SetSize(size_t size)
{
    /// to optimize push_back, reserve the number of queries about to be executed
    m_queries.resize(size);
}

/**
 * @brief Execute all queries in the extended holder
 * @param conn The database connection to use
 * @return true if all queries executed, false if parameters invalid
 *
 * Executes all queries stored in the holder and stores their results.
 * This is the internal execution method called by the delay thread.
 * After execution, the callback is queued for the originating thread.
 *
 * @note This is called internally by SqlDelayThread, not directly by users.
 */
bool SqlQueryHolderEx::Execute(SqlConnection* conn)
{
    if (!m_holder || !m_callback || !m_queue)
    {
        return false;
    }

    LOCK_DB_CONN(conn);
    /// we can do this, we are friends
    std::vector<SqlQueryHolder::SqlResultPair>& queries = m_holder->m_queries;
    for (size_t i = 0; i < queries.size(); ++i)
    {
        /// execute all queries in the holder and pass the results
        char const* sql = queries[i].first;
        if (sql)
        {
            m_holder->SetResult(i, conn->Query(sql));
        }
    }

    /// sync with the caller thread
    m_queue->add(m_callback);

    return true;
}
