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

// A server that closes a connection must still deliver what it already queued.
//
// The last thing written before a disconnect is the one that explains it: the auth
// rejection that says WHY the login failed, the transfer abort, the kick reason. Drop
// it and the client shows a generic "disconnected from server" -- a support ticket
// nobody can answer, and a bug that never reproduces under a debugger because the
// window is a few microseconds wide.
//
// These drive a REAL loopback server through a real socket. Nothing is mocked: the
// bytes have to survive the kernel, the send queue and the teardown path.

#include "TestHarness.h"

#include "net/ISession.hpp"
#include "net/Server.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define CLOSESOCK closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET (-1)
#define CLOSESOCK ::close
#endif

namespace
{
    constexpr uint16_t TEST_PORT_BASE = 41730;

    // Throttles the reader without going below the peer's MSS: a sub-MSS SO_RCVBUF
    // wedges the transfer under silly-window-syndrome avoidance (no window update is
    // ever sent, so the send queue never drains) -- deadlocks on FreeBSD, whose
    // loopback MSS is ~16 KiB, and merely hidden on Linux's 64 KiB loopback.
    constexpr int THROTTLE_RCVBUF = 64 * 1024;

    uint16_t NextPort()
    {
        static std::atomic<uint16_t> next{TEST_PORT_BASE};
        return next.fetch_add(1);
    }

    struct WinsockGuard
    {
        WinsockGuard()
        {
#if defined(_WIN32)
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        }
    };

    // A session that answers one request with `payload`, then closes -- the shape of
    // every "reject and disconnect" the server does. With `alsoClose` false it stays
    // open, which separates "the close lost the bytes" from "the send never finished".
    class FarewellSession : public net::ISession
    {
    public:
        explicit FarewellSession(std::vector<uint8_t> payload, bool alsoClose = true)
            : m_payload(std::move(payload)), m_alsoClose(alsoClose) {}

        std::vector<uint8_t> onData(const uint8_t*, size_t) override
        {
            if (m_spoken.exchange(true))
            {
                return {};
            }
            std::vector<uint8_t> out = m_payload;
            if (!m_alsoClose)
            {
                return out;
            }
            m_closed.store(true, std::memory_order_release);
            if (m_closer)
            {
                m_closer();   // exactly what ClientConnection::Close does
            }
            return out;
        }

        void setCloser(net::Closer c) override { m_closer = std::move(c); }
        bool closed() const override { return m_closed.load(std::memory_order_acquire); }

    private:
        std::vector<uint8_t> m_payload;
        bool m_alsoClose = true;
        std::atomic<bool> m_spoken{false};
        std::atomic<bool> m_closed{false};
        net::Closer m_closer;
    };

    // Reads until the peer sends FIN, or the deadline passes. Returns everything read.
    std::vector<uint8_t> ReadToEof(socket_t s, int deadlineMs)
    {
        std::vector<uint8_t> got;
#if defined(_WIN32)
        DWORD tv = 250;
#else
        timeval tv{0, 250 * 1000};
#endif
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv),
                   sizeof(tv));

        const auto start = std::chrono::steady_clock::now();
        for (;;)
        {
            char buf[4096];
            const int n = int(recv(s, buf, sizeof(buf), 0));
            if (n > 0)
            {
                got.insert(got.end(), buf, buf + n);
                continue;
            }
            if (n == 0)
            {
                break;   // clean FIN
            }
            const auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >
                deadlineMs)
            {
                break;
            }
        }
        return got;
    }

    socket_t ConnectLoopback(uint16_t port)
    {
        socket_t s = socket(AF_INET, SOCK_STREAM, 0);
        if (s == INVALID_SOCKET)
        {
            return s;
        }
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            CLOSESOCK(s);
            return INVALID_SOCKET;
        }
        return s;
    }

    // Starts a server whose sessions all answer with `payload` and then close.
    std::unique_ptr<net::Server> StartFarewellServer(uint16_t port,
                                                     std::vector<uint8_t> payload,
                                                     bool alsoClose = true)
    {
        auto server = std::make_unique<net::Server>();
        const bool ok = server->start(port,
                                      [payload, alsoClose]() -> std::shared_ptr<net::ISession>
                                      {
                                          return std::make_shared<FarewellSession>(payload,
                                                                                   alsoClose);
                                      },
                                      std::string());
        if (!ok)
        {
            return nullptr;
        }
        return server;
    }
}

TEST(GracefulCloseDeliversTheLastPacketBeforeDisconnecting)
{
    // The whole point. One small farewell, queued and then closed in the same breath.
    WinsockGuard winsock;
    const uint16_t port = NextPort();

    const std::vector<uint8_t> farewell = {'A', 'U', 'T', 'H', '_', 'R', 'E', 'J',
                                           'E', 'C', 'T', 'E', 'D', '!'};
    auto server = StartFarewellServer(port, farewell);
    REQUIRE(server != nullptr);

    socket_t c = ConnectLoopback(port);
    REQUIRE(c != INVALID_SOCKET);

    const char ping = 'x';
    REQUIRE(send(c, &ping, 1, 0) == 1);

    const std::vector<uint8_t> got = ReadToEof(c, 3000);
    CLOSESOCK(c);
    server->stop();

    CHECK_EQ(got.size(), farewell.size());
    CHECK(got == farewell);
}

TEST(GracefulCloseDeliversAFarewellTooBigForOneWrite)
{
    // A single small write can survive an abortive close by accident -- it may already
    // be inside the kernel's buffer when the socket is closed. A payload larger than
    // the socket buffer cannot: part of it is still in the send queue, and only a
    // drain-then-close delivers it. This is the case that separates the two designs.
    WinsockGuard winsock;
    const uint16_t port = NextPort();

    std::vector<uint8_t> farewell(1u << 20);   // 1 MiB
    for (size_t i = 0; i < farewell.size(); ++i)
    {
        farewell[i] = uint8_t(i * 31u + (i >> 8));
    }

    auto server = StartFarewellServer(port, farewell);
    REQUIRE(server != nullptr);

    socket_t c = ConnectLoopback(port);
    REQUIRE(c != INVALID_SOCKET);

    // A small receive buffer keeps the server's queue from emptying instantly, so the
    // close request genuinely races data that has not left user space.
    int rcvbuf = THROTTLE_RCVBUF;
    setsockopt(c, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf),
               sizeof(rcvbuf));

    const char ping = 'x';
    REQUIRE(send(c, &ping, 1, 0) == 1);

    // Let the server queue and start closing before the reader drains anything.
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    const std::vector<uint8_t> got = ReadToEof(c, 15000);
    CLOSESOCK(c);
    server->stop();

    CHECK_EQ(got.size(), farewell.size());
    if (got.size() == farewell.size())
    {
        CHECK(std::memcmp(got.data(), farewell.data(), farewell.size()) == 0);
    }
}

TEST(GracefulCloseHoldsUnderManyConnectionsAtOnce)
{
    // The window is microseconds wide, so one connection proves little. Sixty-four at
    // once, each queueing a farewell too large for a single write and closing on the
    // spot, is what turns a rare race into a reproducible one -- and what would catch
    // a teardown that frees the queue while another worker is still draining it.
    WinsockGuard winsock;
    const uint16_t port = NextPort();

    std::vector<uint8_t> farewell(96 * 1024);
    for (size_t i = 0; i < farewell.size(); ++i)
    {
        farewell[i] = uint8_t(i ^ (i >> 5));
    }

    auto server = StartFarewellServer(port, farewell);
    REQUIRE(server != nullptr);

    constexpr int CLIENTS = 64;
    std::atomic<int> whole{0};
    std::atomic<int> truncated{0};
    std::atomic<int> corrupt{0};

    std::vector<std::thread> peers;
    peers.reserve(CLIENTS);
    for (int i = 0; i < CLIENTS; ++i)
    {
        peers.emplace_back([&]()
        {
            socket_t c = ConnectLoopback(port);
            if (c == INVALID_SOCKET)
            {
                return;
            }
            int rcvbuf = THROTTLE_RCVBUF;
            setsockopt(c, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf),
                       sizeof(rcvbuf));

            const char ping = 'x';
            if (send(c, &ping, 1, 0) != 1)
            {
                CLOSESOCK(c);
                return;
            }

            const std::vector<uint8_t> got = ReadToEof(c, 20000);
            CLOSESOCK(c);

            if (got.size() != farewell.size())
            {
                ++truncated;
            }
            else if (std::memcmp(got.data(), farewell.data(), farewell.size()) != 0)
            {
                ++corrupt;
            }
            else
            {
                ++whole;
            }
        });
    }
    for (std::thread& t : peers)
    {
        t.join();
    }
    server->stop();

    CHECK_EQ(corrupt.load(), 0);
    CHECK_EQ(truncated.load(), 0);
    CHECK_EQ(whole.load(), CLIENTS);
}

TEST(ServerStopDoesNotHangOrCrashWithConnectionsMidFlight)
{
    // stop() has to reach quiescence while workers are still completing I/O. Cutting
    // clients off mid-transfer -- some mid-read, some never reading at all -- is the
    // state that strands a pending operation and either hangs the join or frees a
    // buffer the kernel still owns.
    WinsockGuard winsock;
    const uint16_t port = NextPort();

    std::vector<uint8_t> farewell(256 * 1024, 0xA5);
    auto server = StartFarewellServer(port, farewell);
    REQUIRE(server != nullptr);

    std::vector<socket_t> clients;
    for (int i = 0; i < 24; ++i)
    {
        socket_t c = ConnectLoopback(port);
        if (c == INVALID_SOCKET)
        {
            continue;
        }
        int rcvbuf = THROTTLE_RCVBUF;
        setsockopt(c, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf),
                   sizeof(rcvbuf));
        const char ping = 'x';
        send(c, &ping, 1, 0);
        clients.push_back(c);
    }
    CHECK(!clients.empty());

    // Half read a little, half read nothing at all.
    for (size_t i = 0; i < clients.size(); i += 2)
    {
        char buf[512];
        recv(clients[i], buf, sizeof(buf), 0);
    }

    const auto begin = std::chrono::steady_clock::now();
    server->stop();
    const auto took = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - begin).count();

    for (socket_t c : clients)
    {
        CLOSESOCK(c);
    }

    // A drain that cannot finish is a hang, and a hang in shutdown is a server that
    // has to be killed. Ten seconds is far past any legitimate drain of loopback.
    CHECK(took < 10000);
}

TEST(LargeSendCompletesWithoutAClose)
{
    // The control for the test above. Same 1 MiB, same throttled reader, but the
    // session never closes -- so anything missing here is the SEND path failing to
    // resume after the socket buffer filled, not the close discarding bytes. Keeping
    // the two apart is the difference between fixing a teardown and fixing a stall.
    WinsockGuard winsock;
    const uint16_t port = NextPort();

    std::vector<uint8_t> payload(1u << 20);
    for (size_t i = 0; i < payload.size(); ++i)
    {
        payload[i] = uint8_t(i * 7u + (i >> 9));
    }

    auto server = StartFarewellServer(port, payload, /*alsoClose*/ false);
    REQUIRE(server != nullptr);

    socket_t c = ConnectLoopback(port);
    REQUIRE(c != INVALID_SOCKET);

    int rcvbuf = THROTTLE_RCVBUF;
    setsockopt(c, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf),
               sizeof(rcvbuf));

    const char ping = 'x';
    REQUIRE(send(c, &ping, 1, 0) == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    // Read exactly what was promised; the peer never sends FIN here.
    std::vector<uint8_t> got;
    got.reserve(payload.size());
#if defined(_WIN32)
    DWORD tv = 500;
#else
    timeval tv{0, 500 * 1000};
#endif
    setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
    const auto start = std::chrono::steady_clock::now();
    while (got.size() < payload.size())
    {
        char buf[4096];
        const int n = int(recv(c, buf, sizeof(buf), 0));
        if (n > 0)
        {
            got.insert(got.end(), buf, buf + n);
            continue;
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 15)
        {
            break;
        }
    }
    CLOSESOCK(c);
    server->stop();

    CHECK_EQ(got.size(), payload.size());
}
