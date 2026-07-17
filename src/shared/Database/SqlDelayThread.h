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

#ifndef MANGOS_H_SQLDELAYTHREAD
#define MANGOS_H_SQLDELAYTHREAD

#include "LockedQueue/LockedQueue.h"
#include "Threading/Threading.h"

class Database;
class SqlOperation;
class SqlConnection;

/**
 * @brief
 *
 */
class SqlDelayThread : public MaNGOS::Runnable
{
    /**
     * @brief
     *
     */
    typedef MaNGOS::LockedQueue<SqlOperation*> SqlQueue;

    private:
        SqlQueue m_sqlQueue;                                /**< Queue of SQL statements */
        Database* m_dbEngine;                               /**< Pointer to used Database engine */
        SqlConnection* m_dbConnection;                      /**< Pointer to DB connection */
        volatile bool m_running; /**< TODO */

        /**
         * @brief process all enqueued requests
         *
         */
        void ProcessRequests();

    public:
        /**
         * @brief
         *
         * @param db
         * @param conn
         */
        SqlDelayThread(Database* db, SqlConnection* conn);

        /**
         * @brief
         *
         */
        ~SqlDelayThread();

        /**
         * @brief Put sql statement to delay queue
         *
         * @param sql
         * @return bool
         */
        bool Delay(SqlOperation* sql) { m_sqlQueue.add(sql); return true; }

        /**
         * @brief Whether the worker loop is still running (not yet stopped)
         *
         * Used by Database::CommitTransactionChecked() to avoid enqueuing a
         * blocking transaction onto a thread that will never drain it (which
         * would block the caller forever). m_running is a volatile bool toggled
         * only by Stop(); this best-effort cross-thread read is sufficient for a
         * pure shutdown gate (we only ever transition true -> false).
         *
         * @return bool true while the thread loop is running
         */
        bool IsRunning() const { return m_running; }

        /**
         * @brief Stop event
         *
         */
        virtual void Stop();

        /**
         * @brief Main Thread loop
         *
         */
        virtual void run();
};
#endif                                                      //__SQLDELAYTHREAD_H
