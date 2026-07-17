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

#pragma once

// Single entry point to the shared networking. net::Server is the backend chosen for
// the current platform, behind a uniform start(port, factory) / stop() facade; the
// factory mints one ISession per accepted connection.
//
//   - Windows                     -> IocpServer    (proactor, I/O completion ports)
//   - Linux + MANGOS_USE_IO_URING -> UringServer   (proactor, io_uring)
//   - Linux (default)             -> ReactorServer (epoll)
//   - BSD / macOS                 -> ReactorServer (kqueue)

#include "net/ISession.hpp"

#ifdef _WIN32

#include "net/iocp/IocpServer.hpp"

namespace net { using Server = IocpServer; }

#elif defined(MANGOS_USE_IO_URING)

#include "net/uring/UringServer.hpp"

namespace net { using Server = UringServer; }

#else

#include "net/reactor/Poller.hpp"
#include "net/reactor/ReactorServer.hpp"

#include <cstdint>
#include <string>
#include <utility>

namespace net {

// Thin wrapper that binds ReactorServer to the platform's Poller factory while
// keeping the same value-type, start()/stop() shape as IocpServer.
class Server {
public:
    bool start(uint16_t port, SessionFactory factory,
               const std::string& bindIp = std::string()) {
        return m_server.start(port, std::move(factory), bindIp);
    }
    void stop() { m_server.stop(); }

private:
    ReactorServer m_server{ &makePoller };
};

} // namespace net

#endif
