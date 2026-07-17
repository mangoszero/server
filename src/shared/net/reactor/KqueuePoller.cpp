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

#include "net/reactor/KqueuePoller.hpp"

#ifdef MANGOS_HAVE_KQUEUE

#include <sys/types.h>
#include <sys/event.h>
#include <unistd.h>

#include <cerrno>
#include <vector>
#include <cstdint>

namespace net {

bool KqueuePoller::init() {
    m_kq = ::kqueue();
    if (m_kq < 0) return false;

    struct kevent kev;
    EV_SET(&kev, kWakeIdent, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
    return ::kevent(m_kq, &kev, 1, nullptr, 0, nullptr) == 0;
}

bool KqueuePoller::add(int fd, uint32_t interest, void* udata) {
    struct kevent kev[2];
    int n = 0;
    if (interest & EvRead)  EV_SET(&kev[n++], fd, EVFILT_READ,  EV_ADD, 0, 0, udata);
    if (interest & EvWrite) EV_SET(&kev[n++], fd, EVFILT_WRITE, EV_ADD, 0, 0, udata);
    if (n == 0) return true;
    return ::kevent(m_kq, kev, n, nullptr, 0, nullptr) == 0;
}

bool KqueuePoller::mod(int fd, uint32_t interest, void* udata) {
    // EVFILT_READ stays registered for the connection's lifetime; we only toggle
    // the write filter. EV_ADD is idempotent; EV_DELETE on an absent filter
    // returns ENOENT, which we ignore.
    struct kevent kev;
    EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, udata);
    ::kevent(m_kq, &kev, 1, nullptr, 0, nullptr);

    if (interest & EvWrite) EV_SET(&kev, fd, EVFILT_WRITE, EV_ADD,    0, 0, udata);
    else                    EV_SET(&kev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    ::kevent(m_kq, &kev, 1, nullptr, 0, nullptr);
    return true;
}

bool KqueuePoller::del(int fd) {
    struct kevent kev[2];
    EV_SET(&kev[0], fd, EVFILT_READ,  EV_DELETE, 0, 0, nullptr);
    EV_SET(&kev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
    ::kevent(m_kq, kev, 2, nullptr, 0, nullptr); // ignore ENOENT
    return true;
}

int KqueuePoller::wait(PollerEvent* out, int maxEvents) {
    std::vector<struct kevent> raw(static_cast<size_t>(maxEvents));
    int n = ::kevent(m_kq, nullptr, 0, raw.data(), maxEvents, nullptr);
    if (n < 0) return (errno == EINTR) ? 0 : -1;

    // kqueue delivers one event per filter, so a connection ready for both read
    // and write yields two kevents. Coalesce by udata into a single PollerEvent
    // with combined flags — this is the quirk the abstraction hides, and it lets
    // ReactorServer treat every fd uniformly (one event per fd per batch).
    int count = 0;
    for (int i = 0; i < n; ++i) {
        struct kevent& e = raw[i];
        if (e.filter == EVFILT_USER) continue; // wakeup; EV_CLEAR already reset it

        uint32_t fl = 0;
        if (e.filter == EVFILT_READ)  fl = EvRead;
        else if (e.filter == EVFILT_WRITE) fl = EvWrite;
        bool eof = (e.flags & EV_EOF) != 0;

        int found = -1;
        for (int j = 0; j < count; ++j)
            if (out[j].udata == e.udata) { found = j; break; }

        if (found >= 0) {
            out[found].flags |= fl;
            out[found].eof    = out[found].eof || eof;
        } else {
            out[count].udata = e.udata;
            out[count].flags = fl;
            out[count].eof   = eof;
            ++count;
        }
    }
    return count;
}

void KqueuePoller::wake() {
    struct kevent kev;
    EV_SET(&kev, kWakeIdent, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
    ::kevent(m_kq, &kev, 1, nullptr, 0, nullptr);
}

void KqueuePoller::shutdown() {
    if (m_kq >= 0) { ::close(m_kq); m_kq = -1; }
}

} // namespace net

#endif // MANGOS_HAVE_KQUEUE
