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

#ifndef DATABASE_H
#define DATABASE_H

#include "Threading/Threading.h"
#include "Utilities/UnorderedMapSet.h"
#include "Database/SqlDelayThread.h"
#include <ace/Recursive_Thread_Mutex.h>
#include "Policies/ThreadingModel.h"
#include <ace/TSS_T.h>
#include <ace/Atomic_Op.h>
#include "SqlPreparedStatement.h"

class SqlTransaction;
class SqlResultQueue;
class SqlQueryHolder;
class SqlStmtParameters;
class SqlParamBinder;
class Database;

#define MAX_QUERY_LEN   (32*1024)

enum DatabaseTypes
{
    DATABASE_WORLD,
    DATABASE_REALMD,
    DATABASE_CHARACTER,
    COUNT_DATABASES,
};

/**
 * @brief
 *
 */
class SqlConnection
{
    public:
        /**
         * @brief
         *
         */
        virtual ~SqlConnection() {}

        /**
         * @brief method for initializing DB connection
         *
         * @param infoString
         * @return bool
         */
        virtual bool Initialize(const char* infoString) = 0;
        /**
         * @brief public methods for making queries
         *
         * @param sql
         * @return QueryResult
         */
        virtual QueryResult* Query(const char* sql) = 0;
        /**
         * @brief
         *
         * @param sql
         * @return QueryNamedResult
         */
        virtual QueryNamedResult* QueryNamed(const char* sql) = 0;

        /**
         * @brief public methods for making requests
         *
         * @param sql
         * @return bool
         */
        virtual bool Execute(const char* sql) = 0;

        /**
         * @brief escape string generation
         *
         * @param to
         * @param from
         * @param length
         * @return unsigned long
         */
        virtual unsigned long escape_string(char* to, const char* from, unsigned long length) { strncpy(to, from, length); return length; }

        /**
         * @brief nothing do if DB not support transactions
         *
         * @return bool
         */
        virtual bool BeginTransaction() { return true; }
        /**
         * @brief
         *
         * @return bool
         */
        virtual bool CommitTransaction() { return true; }
        /**
         * @brief can't rollback without transaction support
         *
         * @return bool
         */
        virtual bool RollbackTransaction() { return true; }

        /**
         * @brief methods to work with prepared statements
         *
         * @param nIndex
         * @param id
         * @return bool
         */
        bool ExecuteStmt(int nIndex, const SqlStmtParameters& id);

        /**
         * @brief SqlConnection object lock
         *
         */
        class Lock
        {
            public:
                /**
                 * @brief
                 *
                 * @param conn
                 */
                Lock(SqlConnection* conn) : m_pConn(conn) { m_pConn->m_mutex.acquire(); }
                /**
                 * @brief
                 *
                 */
                ~Lock() { m_pConn->m_mutex.release(); }

                /**
                 * @brief
                 *
                 * @return SqlConnection *operator ->
                 */
                SqlConnection* operator->() const { return m_pConn; }

            private:
                SqlConnection* const m_pConn; /**< TODO */
        };

        /**
         * @brief get DB object
         *
         * @return Database
         */
        Database& DB() { return m_db; }

    protected:
        /**
         * @brief
         *
         * @param db
         */
        SqlConnection(Database& db) : m_db(db) {}

        /**
         * @brief
         *
         * @param fmt
         * @return SqlPreparedStatement
         */
        virtual SqlPreparedStatement* CreateStatement(const std::string& fmt);
        /**
         * @brief allocate prepared statement and return statement ID
         *
         * @param nIndex
         * @return SqlPreparedStatement
         */
        SqlPreparedStatement* GetStmt(uint32 nIndex);

        Database& m_db; /**< TODO */

        /**
         * @brief free prepared statements objects
         *
         */
        void FreePreparedStatements();

    private:
        /**
         * @brief
         *
         */
        typedef ACE_Recursive_Thread_Mutex LOCK_TYPE;
        LOCK_TYPE m_mutex; /**< TODO */

        /**
         * @brief
         *
         */
        typedef std::vector<SqlPreparedStatement* > StmtHolder;
        StmtHolder m_holder; /**< TODO */
};

/**
 * @brief
 *
 */
class Database
{
    public:
        /**
         * @brief
         *
         */
        virtual ~Database();

        /**
         * @brief
         *
         * @param infoString
         * @param nConns
         * @return bool
         */
        virtual bool Initialize(const char* infoString, int nConns = 1);
        /**
         * @brief start worker thread for async DB request execution
         *
         */
        virtual void InitDelayThread();
        /**
         * @brief stop worker thread
         *
         */
        virtual void HaltDelayThread();

        /**
         * @brief Synchronous DB queries
         *
         * @param sql
         * @return QueryResult
         */
        inline QueryResult* Query(const char* sql)
        {
            SqlConnection::Lock guard(getQueryConnection());
            return guard->Query(sql);
        }

        /**
         * @brief
         *
         * @param sql
         * @return QueryNamedResult
         */
        inline QueryNamedResult* QueryNamed(const char* sql)
        {
            SqlConnection::Lock guard(getQueryConnection());
            return guard->QueryNamed(sql);
        }

        /**
         * @brief
         *
         * @param format...
         * @return QueryResult
         */
        QueryResult* PQuery(const char* format, ...) ATTR_PRINTF(2, 3);
        /**
         * @brief
         *
         * @param format...
         * @return QueryNamedResult
         */
        QueryNamedResult* PQueryNamed(const char* format, ...) ATTR_PRINTF(2, 3);

        /**
         * @brief
         *
         * @param sql
         * @return bool
         */
        inline bool DirectExecute(const char* sql)
        {
            if (!m_pAsyncConn)
                { return false; }

            SqlConnection::Lock guard(m_pAsyncConn);
            return guard->Execute(sql);
        }

        /**
         * @brief
         *
         * @param format...
         * @return bool
         */
        bool DirectPExecute(const char* format, ...) ATTR_PRINTF(2, 3);

        /// Async queries and query holders, implemented in DatabaseImpl.h

        // Query / member
        template<class Class>
        /**
         * @brief
         *
         * @param object
         * @param )
         * @param sql
         * @return bool
         */
        bool AsyncQuery(Class* object, void (Class::*method)(QueryResult*), const char* sql);
        template<class Class, typename ParamType1>
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
        bool AsyncQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1), ParamType1 param1, const char* sql);
        template<class Class, typename ParamType1, typename ParamType2>
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
        bool AsyncQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* sql);
        template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
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
        bool AsyncQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* sql);
        // Query / static
        template<typename ParamType1>
        /**
         * @brief
         *
         * @param
         * @param ParamType1)
         * @param param1
         * @param sql
         * @return bool
         */
        bool AsyncQuery(void (*method)(QueryResult*, ParamType1), ParamType1 param1, const char* sql);
        template<typename ParamType1, typename ParamType2>
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
        bool AsyncQuery(void (*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* sql);
        template<typename ParamType1, typename ParamType2, typename ParamType3>
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
        bool AsyncQuery(void (*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* sql);
        // PQuery / member
        template<class Class>
        /**
         * @brief
         *
         * @param object
         * @param )
         * @param format...
         * @return bool
         */
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*), const char* format, ...) ATTR_PRINTF(4, 5);
        template<class Class, typename ParamType1>
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
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...) ATTR_PRINTF(5, 6);
        template<class Class, typename ParamType1, typename ParamType2>
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
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...) ATTR_PRINTF(6, 7);
        template<class Class, typename ParamType1, typename ParamType2, typename ParamType3>
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
        bool AsyncPQuery(Class* object, void (Class::*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...) ATTR_PRINTF(7, 8);
        // PQuery / static
        template<typename ParamType1>
        /**
         * @brief
         *
         * @param
         * @param ParamType1)
         * @param param1
         * @param format...
         * @return bool
         */
        bool AsyncPQuery(void (*method)(QueryResult*, ParamType1), ParamType1 param1, const char* format, ...) ATTR_PRINTF(4, 5);
        template<typename ParamType1, typename ParamType2>
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
        bool AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2), ParamType1 param1, ParamType2 param2, const char* format, ...) ATTR_PRINTF(5, 6);
        template<typename ParamType1, typename ParamType2, typename ParamType3>
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
        bool AsyncPQuery(void (*method)(QueryResult*, ParamType1, ParamType2, ParamType3), ParamType1 param1, ParamType2 param2, ParamType3 param3, const char* format, ...) ATTR_PRINTF(6, 7);
        template<class Class>
        // QueryHolder
        /**
         * @brief
         *
         * @param object
         * @param
         * @param )
         * @param holder
         * @return bool
         */
        bool DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*), SqlQueryHolder* holder);
        template<class Class, typename ParamType1>
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
        bool DelayQueryHolder(Class* object, void (Class::*method)(QueryResult*, SqlQueryHolder*, ParamType1), SqlQueryHolder* holder, ParamType1 param1);

        /**
         * @brief
         *
         * @param sql
         * @return bool
         */
        bool Execute(const char* sql);
        /**
         * @brief
         *
         * @param format...
         * @return bool
         */
        bool PExecute(const char* format, ...) ATTR_PRINTF(2, 3);

        /**
         * @brief Writes SQL commands to a LOG file (see mangosd.conf "LogSQL")
         *
         * @param format...
         * @return bool
         */
        bool PExecuteLog(const char* format, ...) ATTR_PRINTF(2, 3);

        /**
         * @brief
         *
         * @return bool
         */
        bool BeginTransaction();
        /**
         * @brief
         *
         * @return bool
         */
        bool CommitTransaction();
        /**
         * @brief
         *
         * @return bool
         */
        bool RollbackTransaction();
        /**
         * @brief for sync transaction execution
         *
         * @return bool
         */
        bool CommitTransactionDirect();

        // PREPARED STATEMENT API
        /**
         * @brief allocate index for prepared statement with SQL request 'fmt'
         *
         * @param index
         * @param fmt
         * @return SqlStatement
         */
        SqlStatement CreateStatement(SqlStatementID& index, const char* fmt);
        /**
         * @brief get prepared statement format string
         *
         * @param stmtId
         * @return std::string
         */
        std::string GetStmtString(const int stmtId) const;

        /**
         * @brief
         *
         * @return operator
         */
        operator bool () const { return m_pQueryConnections.size() && m_pAsyncConn != 0; }

        /**
         * @brief escape string generation
         *
         * @param str
         */
        void escape_string(std::string& str);

        /**
         * @brief must be called before first query in thread (one time for thread using one from existing Database objects)
         *
         */
        virtual void ThreadStart();
        /**
         * @brief must be called before finish thread run (one time for thread using one from existing Database objects)
         *
         */
        virtual void ThreadEnd();

        /**
         * @brief set database-wide result queue. also we should use object-bases and not thread-based result queues
         *
         */
        void ProcessResultQueue();

        /**
        * @brief Function to check that the database version matches expected core version
        *
        * @param DatabaseTypes 
        * @return bool
        */
        bool CheckDatabaseVersion(DatabaseTypes database);
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetPingIntervall() { return m_pingIntervallms; }

        /**
         * @brief function to ping database connections
         *
         */
        void Ping();

        /**
         * @brief set this to allow async transactions
         *
         * you should call it explicitly after your server successfully started
         * up.
         * NO ASYNC TRANSACTIONS DURING SERVER STARTUP - ONLY DURING RUNTIME!!!
         *
         */
        void AllowAsyncTransactions() { m_bAllowAsyncTransactions = true; }

    protected:
        /**
         * @brief
         *
         */
        Database() :
            m_TransStorage(NULL),m_nQueryConnPoolSize(1), m_pAsyncConn(NULL), m_pResultQueue(NULL),
            m_threadBody(NULL), m_delayThread(NULL), m_bAllowAsyncTransactions(false),
            m_iStmtIndex(-1), m_logSQL(false), m_pingIntervallms(0)
        {
            m_nQueryCounter = -1;
        }

        /**
         * @brief
         *
         */
        void StopServer();

        /**
         * @brief factory method to create SqlConnection objects
         *
         * @return SqlConnection
         */
        virtual SqlConnection* CreateConnection() = 0;
        /**
         * @brief factory method to create SqlDelayThread objects
         *
         * @return SqlDelayThread
         */
        virtual SqlDelayThread* CreateDelayThread();

        /**
         * @brief
         *
         */
        class TransHelper
        {
            public:
                /**
                 * @brief
                 *
                 */
                TransHelper() : m_pTrans(NULL) {}
                /**
                 * @brief
                 *
                 */
                ~TransHelper();

                /**
                 * @brief initializes new SqlTransaction object
                 *
                 * @return SqlTransaction
                 */
                SqlTransaction* init();
                /**
                 * @brief gets pointer on current transaction object. Returns NULL if transaction was not initiated
                 *
                 * @return SqlTransaction
                 */
                SqlTransaction* get() const { return m_pTrans; }

                /**
                 * @brief detaches SqlTransaction object allocated by init() function
                 *
                 * next call to get() function will return NULL!
                 * do not forget to destroy obtained SqlTransaction object!
                 *
                 * @return SqlTransaction
                 */
                SqlTransaction* detach();
                /**
                 * @brief destroyes SqlTransaction allocated by init() function
                 *
                 */
                void reset();

            private:
                SqlTransaction* m_pTrans; /**< TODO */
        };

        /**
         * @brief per-thread based storage for SqlTransaction object initialization - no locking is required
         *
         */
        typedef ACE_TSS<Database::TransHelper> DBTransHelperTSS;
        Database::DBTransHelperTSS *m_TransStorage; /**< TODO */

        ///< DB connections
        /**
         * @brief round-robin connection selection
         *
         * @return SqlConnection
         */
        SqlConnection* getQueryConnection();
        /**
         * @brief for now return one single connection for async requests
         *
         * @return SqlConnection
         */
        SqlConnection* getAsyncConnection() const { return m_pAsyncConn; }

        friend class SqlStatement;
        // PREPARED STATEMENT API
        /**
         * @brief query function for prepared statements
         *
         * @param id
         * @param params
         * @return bool
         */
        bool ExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);
        /**
         * @brief
         *
         * @param id
         * @param params
         * @return bool
         */
        bool DirectExecuteStmt(const SqlStatementID& id, SqlStmtParameters* params);

        // connection helper counters
        int m_nQueryConnPoolSize;                               /**< current size of query connection pool */
        ACE_Atomic_Op<ACE_Thread_Mutex, long> m_nQueryCounter;  /**< counter for connection selection */

        /**
         * @brief lets use pool of connections for sync queries
         *
         */
        typedef std::vector< SqlConnection* > SqlConnectionContainer;
        SqlConnectionContainer m_pQueryConnections; /**< TODO */

        // only one single DB connection for transactions
        SqlConnection* m_pAsyncConn; /**< TODO */

        SqlResultQueue*     m_pResultQueue;                 /**< Transaction queues from diff. threads */
        SqlDelayThread*     m_threadBody;                   /**< Pointer to delay sql executer (owned by m_delayThread) */
        ACE_Based::Thread*  m_delayThread;                  /**< Pointer to executer thread */

        bool m_bAllowAsyncTransactions;                     /**< flag which specifies if async transactions are enabled */

        // PREPARED STATEMENT REGISTRY
        /**
         * @brief
         *
         */
        typedef ACE_Thread_Mutex LOCK_TYPE;
        /**
         * @brief
         *
         */
        typedef ACE_Guard<LOCK_TYPE> LOCK_GUARD;

        mutable LOCK_TYPE m_stmtGuard; /**< TODO */

        /**
         * @brief
         *
         */
        typedef UNORDERED_MAP<std::string, int> PreparedStmtRegistry;
        PreparedStmtRegistry m_stmtRegistry;                ///< /**< TODO */

        int m_iStmtIndex; /**< TODO */

    private:

        bool m_logSQL; /**< TODO */
        std::string m_logsDir; /**< TODO */
        uint32 m_pingIntervallms; /**< TODO */
};
#endif
