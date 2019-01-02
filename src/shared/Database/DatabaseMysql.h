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

#ifndef DO_POSTGRESQL

#ifndef MANGOS_H_DATABASEMYSQL
#define MANGOS_H_DATABASEMYSQL

//#include "Common.h"
#include "Database.h"
#include "Policies/Singleton.h"
#include <ace/Thread_Mutex.h>
#include <ace/Guard_T.h>
#include <mysql.h>

#ifdef WIN32
#include <winsock2.h>
#endif

/**
 * @brief MySQL prepared statement class
 *
 */
class MySqlPreparedStatement : public SqlPreparedStatement
{
    public:
        /**
         * @brief
         *
         * @param fmt
         * @param conn
         * @param mysql
         */
        MySqlPreparedStatement(const std::string& fmt, SqlConnection& conn, MYSQL* mysql);
        /**
         * @brief
         *
         */
        ~MySqlPreparedStatement();

        /**
         * @brief prepare statement
         *
         * @return bool
         */
        virtual bool prepare() override;

        /**
         * @brief bind input parameters
         *
         * @param holder
         */
        virtual void bind(const SqlStmtParameters& holder) override;

        /**
         * @brief execute DML statement
         *
         * @return bool
         */
        virtual bool execute() override;

    protected:
        /**
         * @brief bind parameters
         *
         * @param nIndex
         * @param data
         */
        void addParam(unsigned int nIndex, const SqlStmtFieldData& data);

        /**
         * @brief
         *
         * @param data
         * @param bUnsigned
         * @return enum_field_types
         */
        static enum_field_types ToMySQLType(const SqlStmtFieldData& data, bool& bUnsigned);

    private:
        /**
         * @brief
         *
         */
        void RemoveBinds();

        MYSQL* m_pMySQLConn; /**< TODO */
        MYSQL_STMT* m_stmt; /**< TODO */
        MYSQL_BIND* m_pInputArgs; /**< TODO */
        MYSQL_BIND* m_pResult; /**< TODO */
        MYSQL_RES* m_pResultMetadata; /**< TODO */
};

/**
 * @brief
 *
 */
class MySQLConnection : public SqlConnection
{
    public:
        /**
         * @brief
         *
         * @param db
         */
        MySQLConnection(Database& db) : SqlConnection(db), mMysql(NULL) {}
        /**
         * @brief
         *
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
         * @brief
         *
         * @param sql
         * @return QueryResult
         */
        QueryResult* Query(const char* sql) override;
        /**
         * @brief
         *
         * @param sql
         * @return QueryNamedResult
         */
        QueryNamedResult* QueryNamed(const char* sql) override;
        /**
         * @brief
         *
         * @param sql
         * @return bool
         */
        bool Execute(const char* sql) override;

        /**
         * @brief
         *
         * @param to
         * @param from
         * @param length
         * @return unsigned long
         */
        unsigned long escape_string(char* to, const char* from, unsigned long length) override;

        /**
         * @brief
         *
         * @return bool
         */
        bool BeginTransaction() override;
        /**
         * @brief
         *
         * @return bool
         */
        bool CommitTransaction() override;
        /**
         * @brief
         *
         * @return bool
         */
        bool RollbackTransaction() override;

    protected:
        /**
         * @brief
         *
         * @param fmt
         * @return SqlPreparedStatement
         */
        SqlPreparedStatement* CreateStatement(const std::string& fmt) override;

    private:
        /**
         * @brief
         *
         * @param sql
         * @return bool
         */
        bool _TransactionCmd(const char* sql);
        /**
         * @brief
         *
         * @param sql
         * @param pResult
         * @param pFields
         * @param pRowCount
         * @param pFieldCount
         * @return bool
         */
        bool _Query(const char* sql, MYSQL_RES** pResult, MYSQL_FIELD** pFields, uint64* pRowCount, uint32* pFieldCount);

        MYSQL* mMysql; /**< TODO */
};

/**
 * @brief
 *
 */
class DatabaseMysql : public Database
{
        friend class MaNGOS::OperatorNew<DatabaseMysql>;

    public:
        /**
         * @brief
         *
         */
        DatabaseMysql();
        /**
         * @brief
         *
         */
        ~DatabaseMysql();

        /**
         * @brief must be call before first query in thread
         *
         */
        void ThreadStart() override;
        /**
         * @brief must be call before finish thread run
         *
         */
        void ThreadEnd() override;

    protected:
        /**
         * @brief
         *
         * @return SqlConnection
         */
        virtual SqlConnection* CreateConnection() override;

    private:
        static size_t db_count; /**< TODO */
};

#endif
#endif
