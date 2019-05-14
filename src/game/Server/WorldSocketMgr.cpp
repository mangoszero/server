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

/** \file WorldSocketMgr.cpp
*  \ingroup u2w
*  \author Derex <derex101@gmail.com>
*/

#include "Common.h"
#include "Log.h"
#include "Config/Config.h"
#include "WorldSocket.h"
#include "WorldSocketMgr.h"

#include <ace/ACE.h>
#include <ace/TP_Reactor.h>
#include <ace/os_include/arpa/os_inet.h>
#include <ace/os_include/netinet/os_tcp.h>
#include <ace/os_include/sys/os_types.h>
#include <ace/os_include/sys/os_socket.h>

#include <set>

WorldSocketMgr::WorldSocketMgr()
  : m_SockOutKBuff(-1), m_SockOutUBuff(65536), m_UseNoDelay(true),
    reactor_(NULL), acceptor_(NULL)
{
}

WorldSocketMgr::~WorldSocketMgr()
{
    if (reactor_) delete reactor_;
    if (acceptor_) delete acceptor_;
}


int WorldSocketMgr::svc()
{
    DEBUG_LOG("Starting Network Thread");

    reactor_->run_reactor_event_loop();

    DEBUG_LOG("Network Thread Exitting");
    return 0;
}



int WorldSocketMgr::StartNetwork(ACE_INET_Addr& addr)
{
    int num_threads = sConfig.GetIntDefault("Network.Threads", 1);
    if (num_threads <= 0)
    {
        sLog.outError("Network.Threads is wrong in your config file");
        return -1;
    }

    m_SockOutUBuff = sConfig.GetIntDefault("Network.OutUBuff", 65536);
    if (m_SockOutUBuff <= 0)
    {
        sLog.outError("Network.OutUBuff is wrong in your config file");
        return -1;
    }

    // -1 means use default
    m_SockOutKBuff = sConfig.GetIntDefault("Network.OutKBuff", -1);
    m_UseNoDelay = sConfig.GetBoolDefault("Network.TcpNodelay", true);


    ACE_Reactor_Impl* imp = 0;
    imp = new ACE_TP_Reactor();
    imp->max_notify_iterations(128);
    reactor_ = new ACE_Reactor(imp, 1);

    acceptor_ = new WorldAcceptor;

    if (acceptor_->open(addr, reactor_, ACE_NONBLOCK) == -1)
    {
        sLog.outError("Failed to open acceptor, check if the port is free");
        return -1;
    }

    if (activate(THR_NEW_LWP | THR_JOINABLE, num_threads) == -1)
      return -1;

    sLog.outString("Max allowed socket connections: %d", ACE::max_handles());
    return 0;
}

void WorldSocketMgr::StopNetwork()
{
    if (acceptor_) acceptor_->close();
    if (reactor_)  reactor_->end_reactor_event_loop();
    wait();
}

int WorldSocketMgr::OnSocketOpen(WorldSocket* sock)
{
    // set some options here
    if (m_SockOutKBuff >= 0)
    {
        if (sock->peer().set_option(SOL_SOCKET, SO_SNDBUF, (void*)&m_SockOutKBuff, sizeof(int)) == -1 && errno != ENOTSUP)
        {
            sLog.outError("WorldSocketMgr::OnSocketOpen set_option SO_SNDBUF");
            return -1;
        }
    }

    static const int ndoption = 1;

    // Set TCP_NODELAY.
    if (m_UseNoDelay)
    {
        if (sock->peer().set_option(ACE_IPPROTO_TCP, TCP_NODELAY, (void*)&ndoption, sizeof(int)) == -1)
        {
            sLog.outError("WorldSocketMgr::OnSocketOpen: peer().set_option TCP_NODELAY errno = %s", ACE_OS::strerror(errno));
            return -1;
        }
    }

    sock->m_OutBufferSize = static_cast<size_t>(m_SockOutUBuff);
    sock->reactor(reactor_);

    return 0;
}
