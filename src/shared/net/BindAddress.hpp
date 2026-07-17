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

#pragma once

// Shared helper that turns a configured BindIP string into the address a
// backend hands to bind(). Kept in one place so the IOCP, reactor and io_uring
// servers all resolve the option identically (and log identical diagnostics).

#include <cstdint>
#include <string>

namespace net {

// Resolve a BindIP option to a network-byte-order IPv4 address for bind().
//
//   - empty or "0.0.0.0" -> INADDR_ANY (listen on every local interface); always
//     succeeds, no diagnostics.
//   - a dotted-quad IPv4 ("10.0.0.5") -> that address.
//   - anything else -> resolved as a hostname (the conf documents "IP/hostname").
//
// On a non-empty value that is neither a valid IPv4 nor a resolvable hostname the
// function logs an error and returns false, so the caller fails the bind rather
// than silently listening on every interface (which the operator did not ask for).
//
// On Windows the Winsock stack must already be initialised (WSAStartup) before
// this is called, exactly like the bind() that follows it.
bool ResolveBindAddress(const std::string& bindIp, uint32_t& outAddrNet);

} // namespace net
