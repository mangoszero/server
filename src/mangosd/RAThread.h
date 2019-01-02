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

/// \addtogroup mangosd
/// @{
/// \file

#ifndef MANGOS_H_RATHREAD
#define MANGOS_H_RATHREAD

#include <ace/SOCK_Acceptor.h>
#include <ace/Acceptor.h>
#include <ace/Task.h>
#include <ace/INET_Addr.h>

#include "Common.h"


class RASocket;
class ACE_Reactor;

typedef ACE_Acceptor < RASocket, ACE_SOCK_ACCEPTOR > RAAcceptor;

class RAThread : public ACE_Task_Base
{
    private:
        ACE_Reactor    *m_Reactor;
        RAAcceptor     *m_Acceptor;
        ACE_INET_Addr  listen_addr;

    public:
        explicit RAThread(uint16 port, const char* host);
        virtual ~RAThread();

        virtual int open(void* unused) override;
        virtual int svc() override;
};

#endif

