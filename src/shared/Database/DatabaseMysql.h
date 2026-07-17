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

#ifndef DO_POSTGRESQL

#ifndef MANGOS_H_DATABASEMYSQL
#define MANGOS_H_DATABASEMYSQL

//#include "Common.h"
#include "Database.h"
#include "Policies/Singleton.h"
#include <mysql.h>

#ifdef WIN32
#include <winsock2.h>
#endif

/**
 * @brief MySQL-specific prepared statement implementation
 *
 * MySqlPreparedStatement provides MySQL-specific implementation of
 * prepared statements for efficient SQL query execution with parameter binding.
 */
class MySqlPreparedStatement : public SqlPreparedStatement
{
    public:
        /**
         * @brief Constructor with SQL format and connection
         * @param fmt SQL format string
         * @param conn Database connection reference
         * @param mysql MySQL connection handle
         */
        MySqlPreparedStatement(const std::string& fmt, SqlConnection& conn, MYSQL* mysql);

        /**
         * @brief Destructor - cleans up MySQL resources
         */
        ~MySqlPreparedStatement();

        /**
         * @brief Prepare the statement for execution
         * @return True on success, false on failure
         */
        bool prepare() override;

        /**
         * @brief Bind input parameters to the statement
         * @param holder Parameter holder containing values
         */
        void bind(const SqlStmtParameters& holder) override;

        /**
         * @brief Execute the prepared DML statement
         * @return True on success, false on failure
         */
        bool execute() override;

    protected:
        /**
         * @brief Add a parameter to the bind array
         * @param nIndex Parameter index
         * @param data Field data to bind
         */
        void addParam(unsigned int nIndex, const SqlStmtFieldData& data);

        /**
         * @brief Convert field data type to MySQL type
         * @param data Field data to convert
         * @param bUnsigned Set to true if type is unsigned
         * @return MySQL field type
         */
        static enum_field_types ToMySQLType(const SqlStmtFieldData& data, bool& bUnsigned);

    private:
        /**
         * @brief Remove and free parameter bindings
         */
        void RemoveBinds();

        MYSQL* m_pMySQLConn; /**< MySQL connection handle */
        MYSQL_STMT* m_stmt; /**< MySQL prepared statement handle */
        MYSQL_BIND* m_pInputArgs; /**< Input parameter bindings */
        MYSQL_BIND* m_pResult; /**< Result bindings */
        MYSQL_RES* m_pResultMetadata; /**< Result metadata */
};

/**
 * @brief MySQL-specific database connection implementation
 *
 * MySQLConnection provides MySQL-specific implementation of database
 * connection, query execution, and transaction management.
 */
class MySQLConnection : public SqlConnection
{
    public:
        /**
         * @brief Constructor
         * @param db Database reference
         */
        MySQLConnection(Database& db) : SqlConnection(db), mMysql(NULL) {}

        /**
         * @brief Destructor - closes MySQL connection
         */
        ~MySQLConnection();

        /**
         * @brief Initializes Mysql and connects to a server.
         *
         * @param infoString infoString should be formated like hostname;username;password;database
         * @return bool
         */
        bool Initialize(const char* infoString) override;

        /**
         * @brief Execute SELECT query and return results
         * @param sql SQL query string
         * @return QueryResult pointer or NULL on error
         */
        QueryResult* Query(const char* sql) override;

        /**
         * @brief Execute SELECT query with named field access
         * @param sql SQL query string
         * @return QueryNamedResult pointer or NULL on error
         */
        QueryNamedResult* QueryNamed(const char* sql) override;

        /**
         * @brief Execute non-SELECT query (INSERT, UPDATE, DELETE)
         * @param sql SQL query string
         * @return True on success, false on failure
         */
        bool Execute(const char* sql) override;

        /**
         * @brief Escape string for SQL safety
         * @param to Destination buffer
         * @param from Source string
         * @param length Length of source string
         * @return Length of escaped string
         */
        unsigned long escape_string(char* to, const char* from, unsigned long length) override;

        /**
         * @brief Begin a transaction
         * @return True on success, false on failure
         */
        bool BeginTransaction() override;

        /**
         * @brief Commit the current transaction
         * @return True on success, false on failure
         */
        bool CommitTransaction() override;

        /**
         * @brief Rollback the current transaction
         * @return True on success, false on failure
         */
        bool RollbackTransaction() override;

    protected:
        /**
         * @brief Create a MySQL prepared statement
         * @param fmt SQL format string
         * @return SqlPreparedStatement pointer
         */
        SqlPreparedStatement* CreateStatement(const std::string& fmt) override;

    private:
        /**
         * @brief Execute a transaction command
         * @param sql Transaction SQL command
         * @return True on success, false on failure
         */
        bool _TransactionCmd(const char* sql);

        /**
         * @brief Internal query execution
         * @param sql SQL query string
         * @param pResult Output result set pointer
         * @param pFields Output field array pointer
         * @param pRowCount Output row count
         * @param pFieldCount Output field count
         * @return True on success, false on failure
         */
        bool _Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, uint64* pRowCount, uint32* pFieldCount);

        MYSQL* mMysql; /**< MySQL connection handle */
};

/**
 * @brief MySQL database implementation
 *
 * DatabaseMysql provides the MySQL-specific implementation of the
 * Database interface, managing MySQL connections and thread initialization.
 */
class DatabaseMysql : public Database
{

    public:
        /**
         * @brief Constructor
         */
        DatabaseMysql();

        /**
         * @brief Destructor
         */
        ~DatabaseMysql();

        /**
         * @brief Initialize MySQL library for current thread
         * Must be called before first query in thread
         */
        void ThreadStart() override;

        /**
         * @brief Cleanup MySQL library for current thread
         * Must be called before thread termination
         */
        void ThreadEnd() override;

    protected:
        /**
         * @brief Create a new MySQL connection
         * @return SqlConnection pointer
         */
        SqlConnection* CreateConnection() override;

    private:
        static size_t db_count; /**< Number of active database instances */
};

#endif

#endif
