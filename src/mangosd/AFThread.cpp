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

#include "ace/OS.h"
#include "AFThread.h"
#include "World.h"
#include "Log.h"


AntiFreezeThread::AntiFreezeThread(uint32 delay) : delaytime_(delay)
{
    m_loops = 0;
    w_loops = 0;
    m_lastchange = 0;
    w_lastchange = 0;
}

int AntiFreezeThread::open(void* unused)
{
    activate();
    return 0;
}

int AntiFreezeThread::svc(void)
{
    if (!delaytime_)
      { return 0; }

    sLog.outString("AntiFreeze Thread started (%u seconds max stuck time)", delaytime_/1000);
    while (!World::IsStopped())
    {
        ACE_OS::sleep(1);

        uint32 curtime = WorldTimer::getMSTime();

        // normal work
        if (w_loops != World::m_worldLoopCounter.value())
        {
              w_lastchange = curtime;
              w_loops = World::m_worldLoopCounter.value();
        }
        // possible freeze
        else if (WorldTimer::getMSTimeDiff(w_lastchange, curtime) > delaytime_)
        {
            sLog.outError("World Thread hangs, kicking out server!");
            *((uint32 volatile*)NULL) = 0;          // bang crash
        }
    }
    sLog.outString("AntiFreeze Thread stopped.");
    return 0;
}

