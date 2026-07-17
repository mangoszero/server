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

#include "net/reactor/EpollPoller.hpp"

#ifdef MANGOS_HAVE_EPOLL

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <vector>

namespace net {

uint32_t EpollPoller::toEpoll(uint32_t interest) {
    uint32_t e = 0;
    if (interest & EvRead)  e |= EPOLLIN;
    if (interest & EvWrite) e |= EPOLLOUT;
    return e;
}

bool EpollPoller::init() {
    m_epfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (m_epfd < 0) return false;

    m_eventfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (m_eventfd < 0) return false;

    // Register the wakeup eventfd, tagged with `this` so wait() can tell it apart
    // from connection fds.
    struct epoll_event ev{};
    ev.events   = EPOLLIN;
    ev.data.ptr = this;
    return ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, m_eventfd, &ev) == 0;
}

bool EpollPoller::add(int fd, uint32_t interest, void* udata) {
    struct epoll_event ev{};
    ev.events   = toEpoll(interest);
    ev.data.ptr = udata;
    return ::epoll_ctl(m_epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
}

bool EpollPoller::mod(int fd, uint32_t interest, void* udata) {
    struct epoll_event ev{};
    ev.events   = toEpoll(interest);
    ev.data.ptr = udata;
    return ::epoll_ctl(m_epfd, EPOLL_CTL_MOD, fd, &ev) == 0;
}

bool EpollPoller::del(int fd) {
    return ::epoll_ctl(m_epfd, EPOLL_CTL_DEL, fd, nullptr) == 0;
}

int EpollPoller::wait(PollerEvent* out, int maxEvents) {
    std::vector<struct epoll_event> raw(static_cast<size_t>(maxEvents));
    int n = ::epoll_wait(m_epfd, raw.data(), maxEvents, -1);
    if (n < 0) return (errno == EINTR) ? 0 : -1;

    int count = 0;
    for (int i = 0; i < n; ++i) {
        struct epoll_event& e = raw[i];
        if (e.data.ptr == this) {           // wakeup: drain the eventfd counter
            uint64_t v;
            while (::read(m_eventfd, &v, sizeof(v)) > 0) {}
            continue;
        }

        uint32_t fl = 0;
        // Report HUP/ERR as readable so ReactorServer's recv() observes the
        // close (recv == 0 / error) through its normal path.
        if (e.events & (EPOLLIN | EPOLLERR | EPOLLHUP)) fl |= EvRead;
        if (e.events & EPOLLOUT)                        fl |= EvWrite;

        out[count].udata = e.data.ptr;
        out[count].flags = fl;
        out[count].eof   = (e.events & (EPOLLHUP | EPOLLERR)) != 0;
        ++count;
    }
    return count;
}

void EpollPoller::wake() {
    uint64_t v = 1;
    ssize_t r = ::write(m_eventfd, &v, sizeof(v));
    (void)r; // EAGAIN only if the 64-bit counter is saturated — wakeup still pending
}

void EpollPoller::shutdown() {
    if (m_eventfd >= 0) { ::close(m_eventfd); m_eventfd = -1; }
    if (m_epfd    >= 0) { ::close(m_epfd);    m_epfd    = -1; }
}

} // namespace net

#endif // MANGOS_HAVE_EPOLL
