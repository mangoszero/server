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

/// \addtogroup mangosd
/// @{
/// \file

#ifndef MANGOS_H_GDBSERVERTHREAD
#define MANGOS_H_GDBSERVERTHREAD

#include <ace/SOCK_Acceptor.h>
#include <ace/Acceptor.h>
#include <ace/Task.h>
#include <ace/INET_Addr.h>

#include "Common.h"

class GdbRspSocket;
class GdbMonSocket;
class ACE_Reactor;

typedef ACE_Acceptor < GdbRspSocket, ACE_SOCK_ACCEPTOR > GdbRspAcceptor;
typedef ACE_Acceptor < GdbMonSocket, ACE_SOCK_ACCEPTOR > GdbMonAcceptor;

/**
 * Listener thread for the GDB-server debug endpoint. Accepts a GDB RSP
 * connection (for gdb / lldb / IDA) on one port and an optional plain-text
 * monitor bridge (for AI / non-RSP debuggers) on another, both serviced by a
 * single reactor. Network I/O only — all protocol/state work is handed to
 * GdbServer on the world thread. Modeled on RAThread.
 */
class GdbServerThread : public ACE_Task_Base
{
    private:
        ACE_Reactor*    m_Reactor;
        GdbRspAcceptor* m_RspAcceptor;
        GdbMonAcceptor* m_MonAcceptor;
        ACE_INET_Addr   m_rspAddr;
        ACE_INET_Addr   m_monAddr;
        bool            m_monEnabled;

    public:
        /// @param rspPort GDB RSP listen port.
        /// @param monPort Plain-text monitor port (0 to disable).
        /// @param host    Bind address (default 127.0.0.1 in config).
        GdbServerThread(uint16 rspPort, uint16 monPort, const char* host);
        virtual ~GdbServerThread();

        int open(void* unused) override;
        int svc() override;
};

#endif
/// @}
