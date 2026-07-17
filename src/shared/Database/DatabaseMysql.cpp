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
 * @file DatabaseMysql.cpp
 * @brief MySQL database driver implementation
 *
 * This file implements the MySQL-specific database layer:
 * - DatabaseMysql: Factory for MySQL connections
 * - MySQLConnection: Per-thread MySQL connection handling
 *
 * Features:
 * - Thread-safe MySQL library initialization
 * - Connection pooling with multiple threads
 * - UTF-8 encoding enforcement
 * - Automatic reconnection support
 * - Prepared statement caching
 *
 * @note This file is only compiled when DO_POSTGRESQL is not defined
 */

#ifndef DO_POSTGRESQL

#include "Utilities/Util.h"
#include "Policies/Singleton.h"
#include "Platform/Define.h"
#include "Threading/Threading.h"
#include "DatabaseEnv.h"
#include "Utilities/Timer.h"

/**
 * @var DatabaseMysql::db_count
 * @brief Reference counter for MySQL library initialization
 *
 * Tracks how many DatabaseMysql instances exist. The MySQL client
 * library is initialized when the first instance is created and
 * cleaned up when the last instance is destroyed.
 */
size_t DatabaseMysql::db_count = 0;

/**
 * @brief Initialize MySQL thread-local data
 *
 * Must be called in each thread that will use MySQL before
 * performing any MySQL operations. Sets up thread-local
 * variables required by the MySQL client library.
 *
 * @note Called automatically by database worker threads
 */
void DatabaseMysql::ThreadStart()
{
    mysql_thread_init();
}

/**
 * @brief Clean up MySQL thread-local data
 *
 * Releases thread-local resources allocated by mysql_thread_init().
 * Must be called before a thread exits to prevent memory leaks.
 *
 * @note Called automatically when database worker threads exit
 */
void DatabaseMysql::ThreadEnd()
{
    mysql_thread_end();
}

/**
 * @brief Construct MySQL database interface
 *
 * Initializes the MySQL library if this is the first instance:
 * - Calls mysql_library_init() for global initialization
 * - Verifies MySQL library is thread-safe (critical for MaNGOS)
 * - Exits the process if initialization fails
 *
 * Uses reference counting to ensure library is only initialized once
 * and cleaned up only after all instances are destroyed.
 */
DatabaseMysql::DatabaseMysql()
{
    // before first connection
    if (db_count++ == 0)
    {
        // Mysql Library Init
        if (mysql_library_init(-1, NULL, NULL))
        {
            sLog.outError("Could not initialize MySQL client library\n");
            exit(1);
        }
        if (!mysql_thread_safe())
        {
            sLog.outError("FATAL ERROR: Used MySQL library isn't thread-safe.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }
    }
}

/**
 * @brief Destroy MySQL database interface
 *
 * Stops all worker threads and cleans up connections.
 * If this is the last DatabaseMysql instance, also calls
 * mysql_library_end() to clean up global MySQL resources.
 */
DatabaseMysql::~DatabaseMysql()
{
    StopServer();

    // Free Mysql library pointers for last ~DB
    if (--db_count == 0)
    {
        mysql_library_end();
    }
}

/**
 * @brief Create a new MySQL connection
 * @return New MySQLConnection instance
 *
 * Factory method used by the database connection pool to create
 * new MySQL-specific connections on demand.
 */
SqlConnection* DatabaseMysql::CreateConnection()
{
    return new MySQLConnection(*this);
}

/**
 * @brief Destroy MySQL connection
 *
 * Cleans up all prepared statements and closes the MySQL connection.
 * Called when a database worker thread exits or connection is lost.
 */
MySQLConnection::~MySQLConnection()
{
    FreePreparedStatements();
    mysql_close(mMysql);
}

/**
 * @brief Initialize MySQL connection from connection string
 * @param infoString Connection string in format: host;port_or_socket;user;password;database
 * @return true on successful connection, false otherwise
 *
 * Parses the connection string and establishes a MySQL connection.
 * Supports both TCP connections (hostname:port) and Unix sockets/socket files.
 * Configures UTF-8 encoding and enables auto-reconnect.
 *
 * Connection string format:
 *   hostname;port_or_socket;username;password;database
 *
 * Special cases:
 *   - host="." uses named pipe (Windows) or Unix socket (Linux)
 *   - Auto-commit is enabled for atomic operations
 *   - Character set is set to UTF-8
 */
bool MySQLConnection::Initialize(const char* infoString)
{
    MYSQL* mysqlInit = mysql_init(NULL);
    if (!mysqlInit)
    {
        sLog.outError("Could not initialize Mysql connection");
        return false;
    }

    Tokens tokens = StrSplit(infoString, ";");

    Tokens::iterator iter;

    std::string host, port_or_socket, user, password, database;
    int port;
    char const* unix_socket;

    iter = tokens.begin();

    if (iter != tokens.end())
    {
        host = *iter++;
    }
    if (iter != tokens.end())
    {
        port_or_socket = *iter++;
    }
    if (iter != tokens.end())
    {
        user = *iter++;
    }
    if (iter != tokens.end())
    {
        password = *iter++;
    }
    if (iter != tokens.end())
    {
        database = *iter++;
    }

    mysql_options(mysqlInit, MYSQL_SET_CHARSET_NAME, "utf8");
    mysql_options(mysqlInit, MYSQL_OPT_RECONNECT, "1");
#ifdef WIN32
    if (host == ".")                                        // named pipe use option (Windows)
    {
        unsigned int opt = MYSQL_PROTOCOL_PIPE;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        port = 0;
        unix_socket = 0;
    }
    else                                                    // generic case
    {
        port = atoi(port_or_socket.c_str());
        unix_socket = 0;
    }
#else
    if (host == ".")                                        // socket use option (Unix/Linux)
    {
        unsigned int opt = MYSQL_PROTOCOL_SOCKET;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        host = "localhost";
        port = 0;
        unix_socket = port_or_socket.c_str();
    }
    else                                                    // generic case
    {
        port = atoi(port_or_socket.c_str());
        unix_socket = 0;
    }
#endif

    mMysql = mysql_real_connect(mysqlInit, host.c_str(), user.c_str(),
        password.c_str(), database.c_str(), port, unix_socket, 0);

    if (!mMysql)
    {
        sLog.outError("Could not connect to MySQL database at %s: %s\n",
            host.c_str(), mysql_error(mysqlInit));
        mysql_close(mysqlInit);
        return false;
    }

    DETAIL_LOG("Connected to MySQL database %s@%s:%s/%s", user.c_str(), host.c_str(), port_or_socket.c_str(), database.c_str());
    sLog.outString("MySQL client library: %s", mysql_get_client_info());
    sLog.outString("MySQL server ver: %s ", mysql_get_server_info(mMysql));

    /*----------SET AUTOCOMMIT ON---------*/
    // It seems mysql 5.0.x have enabled this feature
    // by default. In crash case you can lose data!!!
    // So better to turn this off
    // ---
    // This is wrong since mangos use transactions,
    // autocommit is turned of during it.
    // Setting it to on makes atomic updates work
    // ---
    // LEAVE 'AUTOCOMMIT' MODE ALWAYS ENABLED!!!
    // W/O IT EVEN 'SELECT' QUERIES WOULD REQUIRE TO BE WRAPPED INTO 'START TRANSACTION'<>'COMMIT' CLAUSES!!!
    if (!mysql_autocommit(mMysql, 1))
    {
        DETAIL_LOG("AUTOCOMMIT SUCCESSFULLY SET TO 1");
    }
    else
    {
        DETAIL_LOG("AUTOCOMMIT NOT SET TO 1");
    }
    /*-------------------------------------*/

    // set connection properties to UTF8 to properly handle locales for different
    // server configs - core sends data in UTF8, so MySQL must expect UTF8 too
    Execute("SET NAMES `utf8`");
    Execute("SET CHARACTER SET `utf8`");

    return true;
}

/**
 * @brief Execute a SQL query and return raw MySQL results
 * @param sql SQL query string
 * @param pResult Output: MySQL result handle
 * @param pFields Output: Field metadata array
 * @param pRowCount Output: Number of rows in result
 * @param pFieldCount Output: Number of fields per row
 * @return true if query succeeded and has rows, false otherwise
 *
 * Internal query execution method that handles:
 * - Query timing and logging
 * - Error reporting
 * - Result set extraction
 *
 * @note Caller must free the result with mysql_free_result()
 */
bool MySQLConnection::_Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, uint64* pRowCount, uint32* pFieldCount)
{
    if (!mMysql)
    {
        return 0;
    }

    uint32 _s = getMSTime();

    if (mysql_query(mMysql, sql))
    {
        sLog.outErrorDb("SQL: %s", sql);
        sLog.outErrorDb("query ERROR: %s", mysql_error(mMysql));
        return false;
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "[%u ms] SQL: %s", getMSTimeDiff(_s, getMSTime()), sql);
    }

    *pResult = mysql_store_result(mMysql);
    *pRowCount = mysql_affected_rows(mMysql);
    *pFieldCount = mysql_field_count(mMysql);

    if (!*pResult)
    {
        return false;
    }

    if (!*pRowCount)
    {
        mysql_free_result(*pResult);
        return false;
    }

    *pFields = mysql_fetch_fields(*pResult);
    return true;
}

/**
 * @brief Execute a SELECT query and return results
 * @param sql SELECT query string
 * @return QueryResult with row data, or NULL on failure/no rows
 *
 * Executes a SQL query and returns a QueryResult object for iterating
 * over the results. Automatically fetches the first row.
 *
 * @note Caller is responsible for deleting the returned QueryResult
 */
QueryResult* MySQLConnection::Query(const char* sql)
{
    MYSQL_RES* result = NULL;
    MYSQL_FIELD* fields = NULL;
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    if (!_Query(sql, &result, &fields, &rowCount, &fieldCount))
    {
        return NULL;
    }

    QueryResultMysql* queryResult = new QueryResultMysql(result, fields, rowCount, fieldCount);

    queryResult->NextRow();
    return queryResult;
}

/**
 * @brief Execute a SELECT query with named field access
 * @param sql SELECT query string
 * @return QueryNamedResult with row data and field names, or NULL on failure
 *
 * Similar to Query() but returns a QueryNamedResult that allows accessing
 * fields by name rather than index. Field names are extracted from the
 * MySQL result metadata.
 *
 * @note Caller is responsible for deleting the returned QueryNamedResult
 */
QueryNamedResult* MySQLConnection::QueryNamed(const char* sql)
{
    MYSQL_RES* result = NULL;
    MYSQL_FIELD* fields = NULL;
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    if (!_Query(sql, &result, &fields, &rowCount, &fieldCount))
    {
        return NULL;
    }

    QueryFieldNames names(fieldCount);
    for (uint32 i = 0; i < fieldCount; ++i)
    {
        names[i] = fields[i].name;
    }

    QueryResultMysql* queryResult = new QueryResultMysql(result, fields, rowCount, fieldCount);

    queryResult->NextRow();
    return new QueryNamedResult(queryResult, names);
}

/**
 * @brief Execute a non-SELECT SQL statement
 * @param sql SQL statement (INSERT, UPDATE, DELETE, etc.)
 * @return true on success, false on error
 *
 * Executes a SQL statement that doesn't return a result set.
 * Commonly used for data modification operations.
 *
 * Execution time is logged when SQL text filtering is enabled.
 * Errors are logged to the database error log.
 */
bool MySQLConnection::Execute(const char* sql)
{
    if (!mMysql)
    {
        return false;
    }

    {
        uint32 _s = getMSTime();

        if (mysql_query(mMysql, sql))
        {
            sLog.outErrorDb("SQL: %s", sql);
            sLog.outErrorDb("SQL ERROR: %s", mysql_error(mMysql));
            return false;
        }
        else
        {
            DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "[%u ms] SQL: %s", getMSTimeDiff(_s, getMSTime()), sql);
        }
        // end guarded block
    }

    return true;
}

/**
 * @brief Execute a transaction command
 * @param sql Transaction SQL command (START TRANSACTION, COMMIT, ROLLBACK)
 * @return true on success, false on error
 *
 * Internal helper for executing transaction control statements.
 * Used by BeginTransaction(), CommitTransaction(), and RollbackTransaction().
 */
bool MySQLConnection::_TransactionCmd(const char* sql)
{
    if (mysql_query(mMysql, sql))
    {
        sLog.outError("SQL: %s", sql);
        sLog.outError("SQL ERROR: %s", mysql_error(mMysql));
        return false;
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SQL_TEXT, "SQL: %s", sql);
    }
    return true;
}

/**
 * @brief Begin a database transaction
 * @return true on success, false on error
 *
 * Starts a new transaction. All subsequent SQL statements will be
 * part of this transaction until CommitTransaction() or RollbackTransaction()
 * is called.
 *
 * @note Requires table storage engine with transaction support (InnoDB)
 */
bool MySQLConnection::BeginTransaction()
{
    return _TransactionCmd("START TRANSACTION");
}

/**
 * @brief Commit the current transaction
 * @return true on success, false on error
 *
 * Commits all changes made since BeginTransaction() was called.
 * Changes become permanent in the database.
 */
bool MySQLConnection::CommitTransaction()
{
    return _TransactionCmd("COMMIT");
}

/**
 * @brief Rollback the current transaction
 * @return true on success, false on error
 *
 * Reverts all changes made since BeginTransaction() was called.
 * The database state is restored to what it was before the transaction.
 */
bool MySQLConnection::RollbackTransaction()
{
    return _TransactionCmd("ROLLBACK");
}

/**
 * @brief Escape a string for safe SQL usage
 * @param to Destination buffer for escaped string
 * @param from Source string to escape
 * @param length Length of source string
 * @return Length of escaped string
 *
 * Escapes special characters in a string to prevent SQL injection attacks.
 * The destination buffer must be at least (length * 2 + 1) bytes.
 *
 * @note Uses MySQL's mysql_real_escape_string for proper escaping
 */
unsigned long MySQLConnection::escape_string(char* to, const char* from, unsigned long length)
{
    if (!mMysql || !to || !from || !length)
    {
        return 0;
    }

    return(mysql_real_escape_string(mMysql, to, from, length));
}

//////////////////////////////////////////////////////////////////////////

/**
 * @brief Create a prepared statement
 * @param fmt SQL statement format string with ? placeholders
 * @return New prepared statement instance
 *
 * Factory method that creates MySQL-specific prepared statements.
 * The format string uses ? for parameter placeholders.
 */
SqlPreparedStatement* MySQLConnection::CreateStatement(const std::string& fmt)
{
    return new MySqlPreparedStatement(fmt, *this, mMysql);
}

//////////////////////////////////////////////////////////////////////////

/**
 * @brief Construct MySQL prepared statement
 * @param fmt SQL format string with ? placeholders
 * @param conn Database connection reference
 * @param mysql MySQL connection handle
 *
 * Initializes a prepared statement for the given SQL format.
 * The statement is not actually prepared until prepare() is called.
 */
MySqlPreparedStatement::MySqlPreparedStatement(const std::string& fmt, SqlConnection& conn, MYSQL* mysql) : SqlPreparedStatement(fmt, conn),
    m_pMySQLConn(mysql), m_stmt(NULL), m_pInputArgs(NULL), m_pResult(NULL), m_pResultMetadata(NULL)
{
}

/**
 * @brief Destroy MySQL prepared statement
 *
 * Cleans up all allocated resources including bound parameters,
 * statement handle, and result metadata.
 */
MySqlPreparedStatement::~MySqlPreparedStatement()
{
    RemoveBinds();
}

/**
 * @brief Prepare the statement for execution
 * @return true on success, false on error
 *
 * Prepares the SQL statement on the MySQL server. This parses the SQL,
 * creates a statement handle, and allocates resources for parameter binding.
 *
 * @note Safe to call multiple times - returns true if already prepared
 */
bool MySqlPreparedStatement::prepare()
{
    if (isPrepared())
    {
        return true;
    }

    // remove old binds
    RemoveBinds();

    // create statement object
    m_stmt = mysql_stmt_init(m_pMySQLConn);
    if (!m_stmt)
    {
        sLog.outError("SQL: mysql_stmt_init() failed ");
        return false;
    }

    // prepare statement
    if (mysql_stmt_prepare(m_stmt, m_szFmt.c_str(), m_szFmt.length()))
    {
        sLog.outError("SQL: mysql_stmt_prepare() failed for '%s'", m_szFmt.c_str());
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
        return false;
    }

    /* Get the parameter count from the statement */
    m_nParams = mysql_stmt_param_count(m_stmt);

    /* Fetch result set meta information */
    m_pResultMetadata = mysql_stmt_result_metadata(m_stmt);
    // if we do not have result metadata
    if (!m_pResultMetadata && strnicmp(m_szFmt.c_str(), "select", 6) == 0)
    {
        sLog.outError("SQL: no meta information for '%s'", m_szFmt.c_str());
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
        return false;
    }

    // bind input buffers
    if (m_nParams)
    {
        m_pInputArgs = new MYSQL_BIND[m_nParams];
        memset(m_pInputArgs, 0, sizeof(MYSQL_BIND) * m_nParams);
    }

    // check if we have a statement which returns result sets
    if (m_pResultMetadata)
    {
        // our statement is query
        m_bIsQuery = true;
        /* Get total columns in the query */
        m_nColumns = mysql_num_fields(m_pResultMetadata);

        // bind output buffers
    }

    m_bPrepared = true;
    return true;
}

/**
 * Binds the prepared statement input parameters from the supplied holder.
 */
void MySqlPreparedStatement::bind(const SqlStmtParameters& holder)
{
    if (!isPrepared())
    {
        MANGOS_ASSERT(false);
        return;
    }

    // finalize adding params
    if (!m_pInputArgs)
    {
        return;
    }

    // verify if we bound all needed input parameters
    if (m_nParams != holder.boundParams())
    {
        MANGOS_ASSERT(false);
        return;
    }

    unsigned int nIndex = 0;
    SqlStmtParameters::ParameterContainer const& _args = holder.params();

    SqlStmtParameters::ParameterContainer::const_iterator iter_last = _args.end();
    for (SqlStmtParameters::ParameterContainer::const_iterator iter = _args.begin(); iter != iter_last; ++iter)
    {
        // bind parameter
        addParam(nIndex++, (*iter));
    }

    // bind input arguments
    if (mysql_stmt_bind_param(m_stmt, m_pInputArgs))
    {
        sLog.outError("SQL ERROR: mysql_stmt_bind_param() failed\n");
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
    }
}

/**
 * Adds a single bound parameter to the MySQL bind array at the given index.
 */
void MySqlPreparedStatement::addParam(unsigned int nIndex, const SqlStmtFieldData& data)
{
    MANGOS_ASSERT(m_pInputArgs);
    MANGOS_ASSERT(nIndex < m_nParams);

    MYSQL_BIND& pData = m_pInputArgs[nIndex];

    bool bUnsigned = 0;
    enum_field_types dataType = ToMySQLType(data, bUnsigned);

    // setup MYSQL_BIND structure
    pData.buffer_type = dataType;
    pData.is_unsigned = bUnsigned;
    pData.buffer = data.buff();
    pData.length = 0;
    pData.buffer_length = data.type() == FIELD_STRING ? data.size() : 0;
}

/**
 * Releases MySQL statement handles and bind buffers.
 */
void MySqlPreparedStatement::RemoveBinds()
{
    if (!m_stmt)
    {
        return;
    }

    delete[] m_pInputArgs;
    delete[] m_pResult;

    mysql_free_result(m_pResultMetadata);
    mysql_stmt_close(m_stmt);

    m_stmt = NULL;
    m_pResultMetadata = NULL;
    m_pResult = NULL;
    m_pInputArgs = NULL;

    m_bPrepared = false;
}

/**
 * Executes the prepared MySQL statement.
 */
bool MySqlPreparedStatement::execute()
{
    if (!isPrepared())
    {
        return false;
    }

    if (mysql_stmt_execute(m_stmt))
    {
        sLog.outError("SQL: can not execute '%s'", m_szFmt.c_str());
        sLog.outError("SQL ERROR: %s", mysql_stmt_error(m_stmt));
        return false;
    }

    return true;
}

/**
 * Maps an internal prepared statement field type to a MySQL field type.
 */
enum_field_types MySqlPreparedStatement::ToMySQLType(const SqlStmtFieldData& data, bool& bUnsigned)
{
    bUnsigned = 0;
    enum_field_types dataType = MYSQL_TYPE_NULL;

    switch (data.type())
    {
        case FIELD_NONE:    dataType = MYSQL_TYPE_NULL;                     break;
        // MySQL does not support MYSQL_TYPE_BIT as input type
        case FIELD_BOOL:    // dataType = MYSQL_TYPE_BIT;      bUnsigned = 1;  break;
        case FIELD_UI8:     dataType = MYSQL_TYPE_TINY;     bUnsigned = 1;  break;
        case FIELD_I8:      dataType = MYSQL_TYPE_TINY;                     break;
        case FIELD_I16:     dataType = MYSQL_TYPE_SHORT;                    break;
        case FIELD_UI16:    dataType = MYSQL_TYPE_SHORT;    bUnsigned = 1;  break;
        case FIELD_I32:     dataType = MYSQL_TYPE_LONG;                     break;
        case FIELD_UI32:    dataType = MYSQL_TYPE_LONG;     bUnsigned = 1;  break;
        case FIELD_I64:     dataType = MYSQL_TYPE_LONGLONG;                 break;
        case FIELD_UI64:    dataType = MYSQL_TYPE_LONGLONG; bUnsigned = 1;  break;
        case FIELD_FLOAT:   dataType = MYSQL_TYPE_FLOAT;                    break;
        case FIELD_DOUBLE:  dataType = MYSQL_TYPE_DOUBLE;                   break;
        case FIELD_STRING:  dataType = MYSQL_TYPE_STRING;                   break;
    }

    return dataType;
}
#endif
