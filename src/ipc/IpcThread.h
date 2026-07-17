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
 */

#ifndef AH_IPC_THREAD_H
#define AH_IPC_THREAD_H

#include "Threading/Threading.h"
#include "IpcLink.h"
#include "BoundedQueue.h"
#include "IpcMessage.h"

#include <atomic>
#include <string>

/**
 * @brief Server-side driver thread: listen, accept one child at a time, and run
 *        its receive loop. Reconnecting children are accepted in turn.
 *
 * Stop() sets a flag; the accept and receive loops poll it (with a short select
 * timeout) and exit, so no cross-thread socket teardown is needed.
 */
class IpcThread : public MaNGOS::Runnable
{
    public:
        IpcThread(const char* host,
                  uint16 port,
                  const std::string& secret,
                  BoundedQueue<IpcMessage>* inbound,
                  IpcServerLink* link);

        ~IpcThread() override;

        void run() override;
        void Stop();

    private:
        std::string               m_host;
        uint16                    m_port;
        std::string               m_secret;
        BoundedQueue<IpcMessage>* m_inbound;
        IpcServerLink*            m_link;

        std::atomic<bool>         m_stop;
};

/**
 * @brief Client-side driver thread: connect, send IPC_HELLO, run the receive
 *        loop. Symmetric to IpcThread.
 */
class IpcClientThread : public MaNGOS::Runnable
{
    public:
        IpcClientThread(const char* host,
                        uint16 port,
                        const std::string& secret,
                        BoundedQueue<IpcMessage>* inbound,
                        IpcClientLink* link);

        ~IpcClientThread() override;

        void run() override;
        void Stop();

        bool IsReady() const { return m_ready.load(std::memory_order_acquire); }

    private:
        std::string               m_host;
        uint16                    m_port;
        std::string               m_secret;
        BoundedQueue<IpcMessage>* m_inbound;
        IpcClientLink*            m_link;

        std::atomic<bool>         m_stop;
        std::atomic<bool>         m_ready;
};

#endif // AH_IPC_THREAD_H
