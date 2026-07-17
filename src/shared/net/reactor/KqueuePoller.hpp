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
#include "net/reactor/Poller.hpp"
#include <cstdint>

// kqueue(2): FreeBSD / Darwin / NetBSD / OpenBSD / DragonFly.
#if defined(__FreeBSD__) || defined(__APPLE__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
#define MANGOS_HAVE_KQUEUE 1
#endif

#ifdef MANGOS_HAVE_KQUEUE

namespace net {

class KqueuePoller final : public Poller {
public:
    ~KqueuePoller() override { shutdown(); }

    bool init() override;
    bool add(int fd, uint32_t interest, void* udata) override;
    bool mod(int fd, uint32_t interest, void* udata) override;
    bool del(int fd) override;
    int  wait(PollerEvent* out, int maxEvents) override;
    void wake() override;
    void shutdown() override;
    const char* name() const override { return "kqueue"; }

private:
    int m_kq = -1;
    // EVFILT_USER identifier used as the cross-thread wakeup source.
    static constexpr uintptr_t kWakeIdent = 1;
};

} // namespace net

#endif // MANGOS_HAVE_KQUEUE
