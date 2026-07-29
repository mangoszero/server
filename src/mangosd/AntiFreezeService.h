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

#ifndef MANGOS_H_ANTIFREEZESERVICE
#define MANGOS_H_ANTIFREEZESERVICE

#include "Service.h"

#include "Platform/Define.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

/**
 * @brief Watchdog that terminates the process if the world loop stops turning.
 *
 * World::m_worldLoopCounter is incremented once per world tick. If it does not
 * change for the configured interval, the world thread is wedged: no player is
 * being served, nothing will recover on its own, and the process is only holding
 * its port and its database connections. The watchdog aborts so that whatever
 * supervises the server (systemd, a container runtime, a restart script) can put
 * it back.
 *
 * That termination is the part the predecessor never implemented. It logged
 * "World Thread hangs, kicking out server!" and then carried on looping, while
 * mangosd.conf described the option as "force crash after the specified amount of
 * seconds". Both the message and the documentation promised an action nothing
 * performed, which is worse than not having the feature: an operator who
 * configured MaxCoreStuckTime believed they had a recovery mechanism.
 *
 * Off by default (MaxCoreStuckTime = 0), so terminating only ever happens
 * because an operator explicitly asked for it.
 */
class AntiFreezeService : public IService
{
    public:

        /**
         * @param maxStuckMs How long the loop counter may stand still before the
         *                   process is terminated. Zero disables the watchdog.
         */
        explicit AntiFreezeService(uint32 maxStuckMs);
        ~AntiFreezeService() override;

        const char* Name() const override { return "anti-freeze watchdog"; }

        void Start() override;
        void RequestStop() override;
        void Join() override;

    private:

        void Run();

        const uint32 m_maxStuckMs;

        std::thread             m_thread;
        std::mutex              m_mutex;
        std::condition_variable m_wake;
        std::atomic<bool>       m_stop;
};

#endif
