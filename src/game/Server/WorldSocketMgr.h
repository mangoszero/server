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

/** \addtogroup u2w User to World Communication
 *  @{
 *  \file WorldSocketMgr.h
 *  \author Derex <derex101@gmail.com>
 */

#ifndef MANGOS_H_WORLDSOCKETMGR
#define MANGOS_H_WORLDSOCKETMGR

#include <ace/Basic_Types.h>
#include <ace/Singleton.h>
#include <ace/TSS_T.h>
#include <ace/INET_Addr.h>
#include <ace/Task.h>
#include <ace/Acceptor.h>

class WorldSocket;

/**
 * @brief World socket manager class
 *
 * This is a pool of threads designed to be used by an ACE_TP_Reactor.
 * Manages all sockets connected to peers.
 */
class WorldSocketMgr : public ACE_Task_Base
{
    friend class ACE_Singleton<WorldSocketMgr, ACE_Thread_Mutex>;
    friend class WorldSocket;

    public:
        /**
         * @brief Start network
         * @param addr Internet address
         * @return Result code
         */
        int StartNetwork(ACE_INET_Addr& addr);

        /**
         * @brief Stop network
         */
        void StopNetwork();

    private:
        /**
         * @brief Handle socket open
         * @param sock World socket
         * @return Result code
         */
        int OnSocketOpen(WorldSocket* sock);

        /**
         * @brief Service method (ACE thread pool)
         * @return Result code
         */
        virtual int svc();

        /**
         * @brief Constructor
         */
        WorldSocketMgr();

        /**
         * @brief Virtual destructor
         */
        virtual ~WorldSocketMgr();

    private:
        int m_SockOutKBuff; ///< Socket output kernel buffer size
        int m_SockOutUBuff; ///< Socket output user buffer size
        bool m_UseNoDelay; ///< Use TCP_NODELAY

        ACE_Reactor* reactor_; ///< ACE reactor
        WorldAcceptor* acceptor_; ///< World acceptor
};

#define sWorldSocketMgr ACE_Singleton<WorldSocketMgr, ACE_Thread_Mutex>::instance()

#endif
/// @}
