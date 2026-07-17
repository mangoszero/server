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
 * @file SqlDelayThread.cpp
 * @brief Implementation of asynchronous SQL execution thread
 *
 * This file implements the SqlDelayThread class which provides background
 * processing of SQL operations. This allows the main thread to queue
 * non-blocking SQL operations that will be executed asynchronously,
 * improving server responsiveness.
 */

#include "Database/SqlDelayThread.h"
#include "Database/SqlOperations.h"
#include "DatabaseEnv.h"
#include "Timer.h"

/**
 * @brief Constructor for SqlDelayThread
 * @param db Pointer to the Database engine
 * @param conn Pointer to the SqlConnection for this thread
 *
 * Initializes the delay thread with the database connection it will use
 * for executing queued operations. The thread starts in running state
 * but doesn't begin execution until run() is called.
 */
SqlDelayThread::SqlDelayThread(Database* db, SqlConnection* conn) : m_dbEngine(db), m_dbConnection(conn), m_running(true)
{
}

/**
 * @brief Destructor for SqlDelayThread
 *
 * Processes any remaining queued requests before destruction.
 * This ensures that no SQL operations are lost when the thread
 * is destroyed, even if they were queued during shutdown.
 */
SqlDelayThread::~SqlDelayThread()
{
    // process all requests which might have been queued while thread was stopping
    ProcessRequests();
}

/**
 * @brief Main execution loop for the delay thread
 *
 * The thread runs in a loop until stopped:
 * 1. Sleeps for a short interval (10ms) to prevent CPU spinning
 * 2. Processes any queued SQL requests
 * 3. Periodically pings the database to keep the connection alive
 *
 * The loop interval and ping frequency are calculated based on the
 * database's configured ping interval.
 *
 * @note This method is called when the thread starts. It should not
 * be called directly - use MaNGOS::Thread::Start() instead.
 */
void SqlDelayThread::run()
{
#ifndef DO_POSTGRESQL
    // Initialize MySQL thread-local data for this thread
    mysql_thread_init();
#endif

    const uint32 loopSleepms = 10; /**< Sleep interval between processing cycles in milliseconds */

    // Calculate how many loops to run before sending a ping
    // This keeps the database connection alive
    const uint32 pingEveryLoop = m_dbEngine->GetPingIntervall() / loopSleepms;

    uint32 loopCounter = 0;
    while (m_running)
    {
        // if the running state gets turned off while sleeping
        // empty the queue before exiting
        MaNGOS::Thread::Sleep(loopSleepms);

        uint32 start = getMSTime();
        ProcessRequests();
        uint32 elapsed = getMSTimeDiff(start, getMSTime());
        if (elapsed > 5000)
        {
            sLog.outError("SqlDelayThread: ProcessRequests took %u ms", elapsed);
        }

        // Send periodic ping to keep connection alive
        if ((loopCounter++) >= pingEveryLoop)
        {
            loopCounter = 0;
            m_dbEngine->Ping();
        }
    }

#ifndef DO_POSTGRESQL
    // Clean up MySQL thread-local data
    mysql_thread_end();
#endif

}

/**
 * @brief Signal the thread to stop running
 *
 * Sets the running flag to false, which will cause the main loop
 * to exit after the current sleep cycle. The thread will process
 * any remaining queued operations before terminating.
 *
 * This is a graceful shutdown mechanism - the thread will not
 * terminate immediately but will finish its current operations.
 */
void SqlDelayThread::Stop()
{
    m_running = false;
}

/**
 * @brief Process all queued SQL operations
 *
 * Dequeues and executes all pending SQL operations from the queue.
 * Each operation is executed using the thread's database connection,
 * then deleted. This method processes the entire queue before returning.
 *
 * This is thread-safe as it uses the queue's internal synchronization
 * mechanisms. Multiple threads can safely enqueue operations while
 * this thread processes them.
 *
 * @note This method should only be called from the delay thread itself
 * or during thread shutdown in the destructor.
 */
void SqlDelayThread::ProcessRequests()
{
    SqlOperation* s = NULL;
    while (m_sqlQueue.next(s))
    {
        s->Execute(m_dbConnection);
        delete s;
    }
}
