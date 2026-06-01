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
 * @file Threading.cpp
 * @brief ACE-based threading implementation
 *
 * This file provides a platform-independent threading abstraction using
 * the ACE (Adaptive Communication Environment) library. Features:
 *
 * - Thread priority management (Idle to Realtime)
 * - Runnable-based task execution
 * - Thread lifecycle management (start, wait, destroy, suspend, resume)
 * - Reference counting for task safety
 *
 * The Thread class wraps ACE threads and provides a C++ interface for
 * spawning and managing worker threads throughout the server.
 *
 * @see Thread for the main thread class
 * @see Runnable for the task interface
 * @see ThreadPriority for priority level management
 */

#include "Threading.h"
#include "Utilities/Errors.h"
#include <ace/OS_NS_unistd.h>
#include <ace/Sched_Params.h>
#include <vector>

using namespace ACE_Based;

/**
 * @brief Construct thread priority enumerator
 *
 * Initializes the priority mapping table by querying the OS scheduler
 * for available priority levels. Maps the 7 priority enum values to
 * OS-specific priority values:
 * - Idle: Minimum priority
 * - Lowest, Low: Below normal
 * - Normal: Default priority
 * - High, Highest: Above normal
 * - Realtime: Maximum priority
 */
ThreadPriority::ThreadPriority()
{
    for (int i = Idle; i < MAXPRIORITYNUM; ++i)
    {
        m_priority[i] = ACE_THR_PRI_OTHER_DEF;
    }

    m_priority[Idle] = ACE_Sched_Params::priority_min(ACE_SCHED_OTHER);
    m_priority[Realtime] = ACE_Sched_Params::priority_max(ACE_SCHED_OTHER);

    std::vector<int> _tmp;

    ACE_Sched_Params::Policy _policy = ACE_SCHED_OTHER;
    ACE_Sched_Priority_Iterator pr_iter(_policy);

    while (pr_iter.more())
    {
        _tmp.push_back(pr_iter.priority());
        pr_iter.next();
    }

    MANGOS_ASSERT(!_tmp.empty());

    if (_tmp.size() >= MAXPRIORITYNUM)
    {
        const size_t max_pos = _tmp.size();
        size_t min_pos = 1;
        size_t norm_pos = 0;
        for (size_t i = 0; i < max_pos; ++i)
        {
            if (_tmp[i] == ACE_THR_PRI_OTHER_DEF)
            {
                norm_pos = i + 1;
                break;
            }
        }

        // since we have only 7(seven) values in enum Priority
        // and 3 we know already (Idle, Normal, Realtime) so
        // we need to split each list [Idle...Normal] and [Normal...Realtime]
        // into pieces
        const size_t _divider = 4;
        size_t _div = (norm_pos - min_pos) / _divider;
        if (_div == 0)
        {
            _div = 1;
        }

        min_pos = (norm_pos - 1);

        m_priority[Low] = _tmp[min_pos -= _div];
        m_priority[Lowest] = _tmp[min_pos -= _div ];

        _div = (max_pos - norm_pos) / _divider;
        if (_div == 0)
        {
            _div = 1;
        }

        min_pos = norm_pos - 1;

        m_priority[High] = _tmp[min_pos += _div];
        m_priority[Highest] = _tmp[min_pos += _div];
    }
}

/**
 * @brief Get OS-specific priority value
 * @param p Priority level from enum
 * @return OS-specific priority value for ACE_Thread
 *
 * Returns the platform-specific priority value mapped to the abstract
 * priority level. Values are clamped to valid range.
 */
int ThreadPriority::getPriority(Priority p) const
{
    if (p < Idle)
    {
        p = Idle;
    }

    if (p > Realtime)
    {
        p = Realtime;
    }

    return m_priority[p];
}

#ifndef __sun__
# define THREADFLAG (THR_NEW_LWP | THR_JOINABLE | THR_SCHED_DEFAULT)
#else
# define THREADFLAG (THR_NEW_LWP | THR_JOINABLE)
#endif

/**
 * @brief Construct empty thread object
 *
 * Creates a thread object without an associated task.
 * Use start() to assign and launch a runnable task later.
 */
Thread::Thread() : m_iThreadId(0), m_hThreadHandle(0), m_task(0)
{
}

/**
 * @brief Construct and start thread with runnable task
 * @param instance Runnable task to execute (will be reference counted)
 *
 * Creates a thread and immediately starts it executing the provided
 * Runnable. The runnable is reference-counted to prevent deletion
 * while the thread is running.
 *
 * @note Assertion failure if start fails
 */
Thread::Thread(Runnable* instance) : m_iThreadId(0), m_hThreadHandle(0), m_task(instance)
{
    // Register reference to m_task to prevent deletion until destructor
    if (m_task)
    {
        m_task->incReference();
    }

    bool _start = start();
    MANGOS_ASSERT(_start);
}

/**
 * @brief Destroy thread object
 *
 * Decrements reference count on the associated task (if any).
 * Note: Does NOT wait for thread completion - caller must
 * ensure proper synchronization before destruction.
 */
Thread::~Thread()
{
    // Note: No automatic wait() - caller must manage lifecycle

    // Delete runnable object if no other references exist
    if (m_task)
    {
        m_task->decReference();
    }
}

// initialize Thread's class static member
ThreadPriority Thread::m_TpEnum;

/**
 * @brief Start the thread executing its task
 * @return true on success, false if already running or no task
 *
 * Spawns a new OS thread that begins executing the ThreadTask
 * function with the Runnable as parameter. The runnable's
 * reference count is managed during the spawn process.
 */
bool Thread::start()
{
    if (m_task == 0 || m_iThreadId != 0)
    {
        return false;
    }

    // incRef before spawning - otherwise Thread::ThreadTask() might call decRef and delete m_task
    m_task->incReference();

    bool res = (ACE_Thread::spawn(&Thread::ThreadTask, (void*)m_task, THREADFLAG, &m_iThreadId, &m_hThreadHandle) == 0);

    if (res)
    {
        m_task->decReference();
    }

    return res;
}

/**
 * @brief Wait for thread completion
 * @return true if successfully joined, false otherwise
 *
 * Blocks until the thread completes execution. After joining,
 * the thread handle is reset to allow reuse or proper cleanup.
 */
bool Thread::wait()
{
    if (!m_hThreadHandle || !m_task)
    {
        return false;
    }

    ACE_THR_FUNC_RETURN _value = ACE_THR_FUNC_RETURN(-1);
    int _res = ACE_Thread::join(m_hThreadHandle, &_value);

    m_iThreadId = 0;
    m_hThreadHandle = 0;

    return (_res == 0);
}

/**
 * @brief Forcefully terminate the thread
 *
 * Sends a kill signal to force thread termination.
 * @warning This is dangerous and should be avoided if possible
 * @warning Does not allow proper cleanup of the task
 */
void Thread::destroy()
{
    if (!m_iThreadId || !m_task)
    {
        return;
    }

    if (ACE_Thread::kill(m_iThreadId, -1) != 0)
    {
        return;
    }

    m_iThreadId = 0;
    m_hThreadHandle = 0;

    // Decrement reference added at ACE_Thread::spawn
    m_task->decReference();
}

/**
 * @brief Suspend thread execution
 *
 * Pauses the thread's execution. Not supported on all platforms.
 * @see resume() to continue execution
 */
void Thread::suspend()
{
    ACE_Thread::suspend(m_hThreadHandle);
}

/**
 * @brief Resume thread execution
 *
 * Continues a previously suspended thread.
 * @see suspend() to pause execution
 */
void Thread::resume()
{
    ACE_Thread::resume(m_hThreadHandle);
}

/**
 * @brief Thread entry point function
 * @param param Pointer to Runnable task
 * @return Always returns 0
 *
 * Static function passed to ACE_Thread::spawn. Increments the
 * task's reference count, calls run(), then decrements the count.
 */
ACE_THR_FUNC_RETURN Thread::ThreadTask(void* param)
{
    Runnable* _task = static_cast<Runnable*>(param);
    _task->incReference();

    _task->run();

    // Task execution complete, free reference added at start
    _task->decReference();

    return (ACE_THR_FUNC_RETURN)0;
}

/**
 * @brief Set thread priority
 * @param type Priority level (Idle to Realtime)
 *
 * Changes the OS scheduling priority for this thread.
 * Not supported on Solaris (__sun__).
 *
 * @warning Assertion failure if priority change fails
 */
void Thread::setPriority(Priority type)
{
#ifndef __sun__
    int _priority = m_TpEnum.getPriority(type);
    int _ok = ACE_Thread::setprio(m_hThreadHandle, _priority);
    // Remove this ASSERT if you don't need to know if priority change succeeded
    MANGOS_ASSERT(_ok == 0);
#endif
}

/**
 * @brief Sleep current thread
 * @param msecs Milliseconds to sleep
 *
 * Suspends execution of the calling thread for the specified duration.
 * Uses ACE_OS::sleep for cross-platform compatibility.
 */
void Thread::Sleep(unsigned long msecs)
{
    ACE_OS::sleep(ACE_Time_Value(0, 1000 * msecs));
}
