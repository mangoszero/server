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
 * @file DelayExecutor.cpp
 * @brief Asynchronous method execution queue using ACE Task
 *
 * This file implements DelayExecutor, a thread pool-based executor for
 * asynchronous method calls. It uses the ACE framework's Task/ActivationQueue
 * pattern to safely execute callbacks in worker threads.
 *
 * Features:
 * - Thread pool for concurrent execution
 * - Thread-safe method request queue
 * - Graceful activation/deactivation
 * - Singleton pattern for global access
 *
 * Used primarily for:
 * - Async database query callbacks
 * - Deferred work execution
 * - Offloading work from main thread
 *
 * @see DelayExecutor for the main executor class
 * @see ACE_Method_Request for the request interface
 */

#include <ace/Singleton.h>
#include <ace/Thread_Mutex.h>
#include <ace/Log_Msg.h>

#include "DelayExecutor.h"
#include "Log.h"

/**
 * @brief Get singleton instance
 * @return Pointer to the DelayExecutor singleton
 */
DelayExecutor* DelayExecutor::instance()
{
    return ACE_Singleton<DelayExecutor, ACE_Thread_Mutex>::instance();
}

/**
 * @brief Construct DelayExecutor
 *
 * Initializes the executor in inactive state.
 * Must call _activate() before use.
 */
DelayExecutor::DelayExecutor()
    : activated_(false)
{
}

/**
 * @brief Destroy DelayExecutor
 *
 * Automatically deactivates and waits for worker threads to complete.
 */
DelayExecutor::~DelayExecutor()
{
    deactivate();
}

/**
 * @brief Deactivate the executor and stop worker threads
 * @return 0 on success, -1 if already inactive
 *
 * Signals the queue to stop accepting new requests, waits for
 * all worker threads to complete, and marks as inactive.
 */
int DelayExecutor::deactivate()
{
    if (!activated())
    {
        return -1;
    }

    activated(false);
    sLog.outString("[shutdown] DelayExecutor::deactivate: deactivating activation queue + joining workers...");
    queue_.queue()->deactivate();
    wait();
    sLog.outString("[shutdown] DelayExecutor::deactivate: workers joined");
    return 0;
}

/**
 * @brief Worker thread service function
 * @return Always returns 0
 *
 * Each worker thread runs this loop:
 * 1. Dequeue next method request (blocks if empty)
 * 2. Execute the request via call()
 * 3. Delete the request
 * 4. Exit when NULL received (queue deactivated)
 */
int DelayExecutor::svc()
{
    for (;;)
    {
        ACE_Method_Request* rq = queue_.dequeue();

        if (!rq)
        {
            break;
        }

        rq->call();
        delete rq;
    }

    return 0;
}

/**
 * @brief Activate the executor with worker threads
 * @param num_threads Number of worker threads to create
 * @return true on success, -1 on failure
 *
 * Activates the internal queue and spawns the specified number of
 * worker threads to process method requests.
 */
int DelayExecutor::_activate(int num_threads)
{
    if (activated())
    {
        return -1;
    }

    if (num_threads < 1)
    {
        return -1;
    }

    queue_.queue()->activate();

    if (ACE_Task_Base::activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED, num_threads) == -1)
    {
        return -1;
    }

    activated(true);

    return true;
}

/**
 * @brief Enqueue a method request for async execution
 * @param new_req Request to execute (ownership transferred)
 * @return 0 on success, -1 on failure
 *
 * Adds a method request to the queue for execution by a worker thread.
 * The executor takes ownership of the request and deletes it after
 * execution. If enqueue fails, the request is deleted immediately.
 */
int DelayExecutor::execute(ACE_Method_Request* new_req)
{
    if (new_req == NULL)
    {
        return -1;
    }

    if (queue_.enqueue(new_req, (ACE_Time_Value*)&ACE_Time_Value::zero) == -1)
    {
        delete new_req;
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT("(%t) %p\n"), ACE_TEXT("DelayExecutor::execute enqueue")), -1);
    }

    return 0;
}

/**
 * @brief Check if executor is active
 * @return true if active, false otherwise
 */
bool DelayExecutor::activated()
{
    return activated_;
}

/**
 * @brief Set activation state
 * @param s New activation state
 */
void DelayExecutor::activated(bool s)
{
    activated_ = s;
}
