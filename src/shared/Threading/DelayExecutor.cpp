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

#include <ace/Singleton.h>
#include <ace/Thread_Mutex.h>
#include <ace/Log_Msg.h>

#include "DelayExecutor.h"

DelayExecutor* DelayExecutor::instance()
{
    return ACE_Singleton<DelayExecutor, ACE_Thread_Mutex>::instance();
}

DelayExecutor::DelayExecutor()
    : activated_(false)
{
}

DelayExecutor::~DelayExecutor()
{
    deactivate();
}

int DelayExecutor::deactivate()
{
    if (!activated())
        return -1;

    activated(false);
    queue_.queue()->deactivate();
    wait();

    return 0;
}

int DelayExecutor::svc()
{
    for (;;)
    {
        ACE_Method_Request* rq = queue_.dequeue();

        if (!rq)
            break;

        rq->call();
        delete rq;
    }

    return 0;
}

int DelayExecutor::_activate(int num_threads)
{
    if (activated())
        return -1;

    if (num_threads < 1)
        return -1;

    queue_.queue()->activate();

    if (ACE_Task_Base::activate(THR_NEW_LWP | THR_JOINABLE | THR_INHERIT_SCHED, num_threads) == -1)
        return -1;

    activated(true);

    return true;
}

int DelayExecutor::execute(ACE_Method_Request* new_req)
{
    if (new_req == NULL)
        return -1;

    if (queue_.enqueue(new_req, (ACE_Time_Value*)&ACE_Time_Value::zero) == -1)
    {
        delete new_req;
        ACE_ERROR_RETURN((LM_ERROR, ACE_TEXT("(%t) %p\n"), ACE_TEXT("DelayExecutor::execute enqueue")), -1);
    }

    return 0;
}

bool DelayExecutor::activated()
{
    return activated_;
}

void DelayExecutor::activated(bool s)
{
    activated_ = s;
}