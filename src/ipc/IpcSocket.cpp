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
 */

#include "IpcSocket.h"

#include <atomic>
#include <cstring>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace
{
#ifdef _WIN32
    std::atomic<int> g_wsaRefs{0};

    inline int LastError()      { return ::WSAGetLastError(); }
    inline bool WouldBlock(int e){ return e == WSAEWOULDBLOCK; }
    inline void CloseFd(ipc_socket_t fd) { ::closesocket(fd); }
#else
    inline int LastError()      { return errno; }
    inline bool WouldBlock(int e){ return e == EWOULDBLOCK || e == EAGAIN || e == EINTR; }
    inline void CloseFd(ipc_socket_t fd) { ::close(fd); }
#endif

    // Fill a loopback-friendly IPv4 sockaddr for host:port.
    bool MakeAddr(const std::string& host, uint16 port, sockaddr_in& addr)
    {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        const char* h = host.empty() ? "127.0.0.1" : host.c_str();
        return ::inet_pton(AF_INET, h, &addr.sin_addr) == 1;
    }
}

bool IpcSocket::GlobalInit()
{
#ifdef _WIN32
    if (g_wsaRefs.fetch_add(1) == 0)
    {
        WSADATA data;
        if (::WSAStartup(MAKEWORD(2, 2), &data) != 0)
        {
            g_wsaRefs.fetch_sub(1);
            return false;
        }
    }
#endif
    return true;
}

void IpcSocket::GlobalShutdown()
{
#ifdef _WIN32
    if (g_wsaRefs.fetch_sub(1) == 1)
    {
        ::WSACleanup();
    }
#endif
}

bool IpcSocket::Listen(const std::string& host, uint16 port)
{
    Close();

    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == INVALID)
    {
        return false;
    }

    int yes = 1;
    ::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr;
    if (!MakeAddr(host, port, addr) ||
        ::bind(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 ||
        ::listen(m_fd, 1) != 0)
    {
        Close();
        return false;
    }

    return true;
}

int IpcSocket::AcceptOnce(int timeoutMs, IpcSocket& out)
{
    if (m_fd == INVALID)
    {
        return -1;
    }

    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(m_fd, &rd);

    timeval tv;
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int sel = ::select(static_cast<int>(m_fd) + 1, &rd, nullptr, nullptr, &tv);
    if (sel == 0)
    {
        return 0; // timeout
    }
    if (sel < 0)
    {
        return WouldBlock(LastError()) ? 0 : -1;
    }

    const ipc_socket_t peer = ::accept(m_fd, nullptr, nullptr);
    if (peer == INVALID)
    {
        return WouldBlock(LastError()) ? 0 : -1;
    }

    out = IpcSocket(peer);
    return 1;
}

bool IpcSocket::Connect(const std::string& host, uint16 port)
{
    Close();

    m_fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_fd == INVALID)
    {
        return false;
    }

    sockaddr_in addr;
    if (!MakeAddr(host, port, addr) ||
        ::connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        Close();
        return false;
    }

    return true;
}

std::ptrdiff_t IpcSocket::RecvSome(void* buf, size_t len, int timeoutMs)
{
    if (m_fd == INVALID)
    {
        return -1;
    }

    fd_set rd;
    FD_ZERO(&rd);
    FD_SET(m_fd, &rd);

    timeval tv;
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int sel = ::select(static_cast<int>(m_fd) + 1, &rd, nullptr, nullptr, &tv);
    if (sel == 0)
    {
        return -2; // timeout
    }
    if (sel < 0)
    {
        return WouldBlock(LastError()) ? -2 : -1;
    }

    const int n = ::recv(m_fd, static_cast<char*>(buf), static_cast<int>(len), 0);
    if (n > 0)
    {
        return static_cast<std::ptrdiff_t>(n);
    }
    if (n == 0)
    {
        return 0; // peer closed
    }
    return WouldBlock(LastError()) ? -2 : -1;
}

bool IpcSocket::SendAll(const void* data, size_t len)
{
    if (m_fd == INVALID)
    {
        return false;
    }

    const char* p = static_cast<const char*>(data);
    size_t sent = 0;

    int flags = 0;
#ifdef MSG_NOSIGNAL
    flags = MSG_NOSIGNAL;
#endif

    while (sent < len)
    {
        const int n = ::send(m_fd, p + sent, static_cast<int>(len - sent), flags);
        if (n > 0)
        {
            sent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && WouldBlock(LastError()))
        {
            continue;
        }
        return false;
    }

    return true;
}

void IpcSocket::ShutdownBoth()
{
    if (m_fd != INVALID)
    {
#ifdef _WIN32
        ::shutdown(m_fd, SD_BOTH);
#else
        ::shutdown(m_fd, SHUT_RDWR);
#endif
    }
}

void IpcSocket::Close()
{
    if (m_fd != INVALID)
    {
        CloseFd(m_fd);
        m_fd = INVALID;
    }
}
