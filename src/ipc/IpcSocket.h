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

#ifndef AH_IPC_SOCKET_H
#define AH_IPC_SOCKET_H

#include "Common.h"

#include <cstddef>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET ipc_socket_t;
#else
typedef int ipc_socket_t;
#endif

/**
 * @brief Minimal cross-platform blocking TCP socket for the loopback IPC channel.
 *
 * One connection per side; both handshake and framing run over a single stream.
 * Reads and accepts are made interruptible by polling with a timeout (select),
 * so an owning thread can observe a stop flag without a signal or a self-pipe.
 */
class IpcSocket
{
    public:
        static const ipc_socket_t INVALID = (ipc_socket_t)(~0);

        /// Reference-counted Winsock startup/cleanup (no-op elsewhere).
        static bool GlobalInit();
        static void GlobalShutdown();

        IpcSocket() : m_fd(INVALID) {}
        explicit IpcSocket(ipc_socket_t fd) : m_fd(fd) {}
        ~IpcSocket() { Close(); }

        IpcSocket(IpcSocket&& o) noexcept : m_fd(o.m_fd) { o.m_fd = INVALID; }
        IpcSocket& operator=(IpcSocket&& o) noexcept
        {
            if (this != &o) { Close(); m_fd = o.m_fd; o.m_fd = INVALID; }
            return *this;
        }

        bool Valid() const { return m_fd != INVALID; }

        /// Bind + listen on host:port. Sets SO_REUSEADDR. False on any error.
        bool Listen(const std::string& host, uint16 port);

        /**
         * @brief Accept a single connection, waiting up to @p timeoutMs.
         * @return 1 accepted (out holds the peer), 0 timeout, -1 error.
         */
        int AcceptOnce(int timeoutMs, IpcSocket& out);

        /// Blocking connect to host:port. False on any error.
        bool Connect(const std::string& host, uint16 port);

        /**
         * @brief Receive up to @p len bytes, waiting up to @p timeoutMs.
         * @return >0 bytes read, 0 peer closed, -1 error, -2 timeout.
         */
        std::ptrdiff_t RecvSome(void* buf, size_t len, int timeoutMs);

        /// Send exactly @p len bytes (loops on partial writes). False on error.
        bool SendAll(const void* data, size_t len);

        /// Wake a blocked peer (half-close both directions) without freeing the fd.
        void ShutdownBoth();

        void Close();

    private:
        ipc_socket_t m_fd;

        IpcSocket(const IpcSocket&);
        IpcSocket& operator=(const IpcSocket&);
};

#endif // AH_IPC_SOCKET_H
