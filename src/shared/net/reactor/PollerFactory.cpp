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

// The single place that maps an OS to its reactor backend. Adding a new platform
// (poll, /dev/poll, event ports, ...) means: write the Poller subclass, then add
// one branch here. The rest of the server never changes.
#ifndef _WIN32

#include "net/reactor/Poller.hpp"

#include <memory>

#if defined(__linux__)
#include "net/reactor/EpollPoller.hpp"
#elif defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__) || \
      defined(__OpenBSD__) || defined(__DragonFly__)
#include "net/reactor/KqueuePoller.hpp"
#endif

namespace net {

std::unique_ptr<Poller> makePoller() {
#if defined(__linux__)
    return std::make_unique<EpollPoller>();
#elif defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__) || \
      defined(__OpenBSD__) || defined(__DragonFly__)
    return std::make_unique<KqueuePoller>();
#else
    return nullptr; // no reactor backend for this platform
#endif
}

} // namespace net

#endif // !_WIN32
