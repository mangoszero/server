/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#include "net/BindAddress.hpp"

#include "Log.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstdint>
#include <string>
#endif

namespace net {

bool ResolveBindAddress(const std::string& bindIp, uint32_t& outAddrNet)
{
    // Empty / "0.0.0.0" keeps the historical default: every local interface.
    if (bindIp.empty() || bindIp == "0.0.0.0")
    {
        outAddrNet = htonl(INADDR_ANY);
        return true;
    }

    // Dotted-quad IPv4 is the common, cheap case; try it before any DNS work.
    in_addr a{};
    if (inet_pton(AF_INET, bindIp.c_str(), &a) == 1)
    {
        outAddrNet = a.s_addr;
        return true;
    }

    // Otherwise treat it as a hostname (the conf documents "IP/hostname"), taking
    // the first IPv4 result.
    addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* res = nullptr;
    if (getaddrinfo(bindIp.c_str(), nullptr, &hints, &res) == 0 && res != nullptr)
    {
        outAddrNet = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr.s_addr;
        freeaddrinfo(res);
        return true;
    }

    sLog.outError("Network: BindIP '%s' is not a valid IPv4 address or resolvable hostname; refusing to bind",
                  bindIp.c_str());
    return false;
}

} // namespace net
