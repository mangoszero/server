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

#include <algorithm>
#include "TestHarness.h"

#include "PacketCodec.h"
#include "net/Server.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   typedef SOCKET socket_t;
#  define CLOSESOCKET closesocket
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <sys/socket.h>
#  include <unistd.h>
   typedef int socket_t;
#  define INVALID_SOCKET (-1)
#  define CLOSESOCKET ::close
#endif

/**
 * @file
 * @brief Stress coverage for the shared networking engine, over real sockets.
 *
 * This is the only test in the tree that opens a port. That is deliberate: the
 * defects this engine actually had -- a use-after-free when a connection was
 * closed while its pointer was still live in the poller's event array, a
 * closeAfterDrain path that stranded connections, a completion loop that spun on
 * a dead handle -- are all concurrency and teardown bugs. None of them can be
 * reached by calling methods on an object; they need many connections doing
 * different things at once, and they need connections dying at inconvenient
 * moments.
 *
 * So the cases below are hostile on purpose: clients that vanish mid-write,
 * connections closed while the server still has queued output, and a server
 * stopped with traffic in flight. What is being asserted is mostly not a value
 * but an absence -- no crash, no hang, no leak, and no byte out of order in the
 * streams that were allowed to finish.
 *
 * Ports are chosen from a high range and a bind failure skips rather than fails,
 * because a busy port on a shared CI machine is not a defect in this code.
 */

namespace
{
    /// Payload byte at a given offset. Any reordering or duplication shows up.
    inline uint8 PatternByte(size_t index)
    {
        return uint8((index * 131u) ^ (index >> 7));
    }

    struct Totals
    {
        std::atomic<uint64> bytesReceived{0};
        std::atomic<uint64> connections{0};
        std::atomic<uint64> closes{0};
        std::atomic<uint64> corruptions{0};
    };

    /**
     * @brief Server-side session: validates the pattern and echoes it back.
     *
     * Echoing matters -- it drives SendQueue, its coalescing and its
     * backpressure gate, which is where the outbound half of the engine lives.
     */
    class StressSession : public net::ISession
    {
        public:

            explicit StressSession(Totals& totals) : m_totals(totals)
            {
                m_totals.connections.fetch_add(1);
            }

            void setSender(net::Sender sender) override { m_sender = std::move(sender); }
            void setCloser(net::Closer closer) override { m_closer = std::move(closer); }

            std::vector<uint8_t> onData(const uint8_t* data, size_t len) override
            {
                for (size_t i = 0; i < len; ++i)
                {
                    if (data[i] != PatternByte(m_offset + i))
                    {
                        m_totals.corruptions.fetch_add(1);
                    }
                }
                m_offset += len;
                m_totals.bytesReceived.fetch_add(len);

                if (m_sender)
                {
                    m_sender(data, len);
                }
                return std::vector<uint8_t>();
            }

            void onClose() override
            {
                m_closed.store(true);
                m_totals.closes.fetch_add(1);
            }

            bool closed() const override { return m_closed.load(); }

        private:

            Totals&           m_totals;
            net::Sender       m_sender;
            net::Closer       m_closer;
            size_t            m_offset = 0;
            std::atomic<bool> m_closed{false};
    };

    struct SocketLayer
    {
        SocketLayer()
        {
#ifdef _WIN32
            WSADATA wsa{};
            WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
        }
        ~SocketLayer()
        {
#ifdef _WIN32
            WSACleanup();
#endif
        }
    };

    socket_t ConnectTo(uint16 port)
    {
        socket_t s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET)
        {
            return INVALID_SOCKET;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (::connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
        {
            CLOSESOCKET(s);
            return INVALID_SOCKET;
        }

        int one = 1;
        ::setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
                     reinterpret_cast<const char*>(&one), sizeof(one));
        return s;
    }

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

    bool SendAll(socket_t s, const uint8* data, size_t len)
    {
        size_t sent = 0;
        while (sent < len)
        {
            // MSG_NOSIGNAL: these tests deliberately keep writing into sockets
            // the peer has already reset. Without it the default SIGPIPE
            // disposition kills the test process outright -- which is what
            // FreeBSD did here (exit 141 = 128 + SIGPIPE), while Linux happened
            // to survive. The server engine already guards its own sends the
            // same way; this is the client side of the test doing likewise.
            const int n = ::send(s, reinterpret_cast<const char*>(data + sent),
                                 int(len - sent), MSG_NOSIGNAL);
            if (n <= 0)
            {
                return false;
            }
            sent += size_t(n);
        }
        return true;
    }

    /// Ports are picked per test to avoid a TIME_WAIT collision between cases.
    uint16 NextPort()
    {
        static std::atomic<uint16> port{47700};
        return port.fetch_add(1);
    }

    /// Waits for a condition, up to a deadline. Returns false on timeout, which
    /// is how a hang is reported as a failure instead of stalling the suite.
    template <typename Predicate>
    bool WaitFor(Predicate pred, int milliseconds = 15000)
    {
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);

        while (std::chrono::steady_clock::now() < deadline)
        {
            if (pred())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return pred();
    }
}

TEST(NetStress_many_connections_random_fragmentation)
{
    // Forty clients, each streaming a quarter-megabyte in randomly sized writes.
    // The random sizing is the point: it makes the server see split and coalesced
    // reads that a tidy request/response test never produces.
    const int    CLIENTS       = 40;
    const size_t BYTES_PER_CLIENT = 256 * 1024;

    SocketLayer layer;
    Totals totals;

    const uint16 port = NextPort();
    net::Server server;

    const bool started = server.start(port,
        [&totals]() -> std::shared_ptr<net::ISession>
        {
            return std::make_shared<StressSession>(totals);
        },
        "127.0.0.1");

    if (!started)
    {
        std::printf("    (skipped: could not bind 127.0.0.1:%u)\n", unsigned(port));
        return;
    }

    std::atomic<int>    clientFailures{0};
    std::atomic<uint64> echoedBack{0};
    std::vector<std::thread> clients;

    for (int c = 0; c < CLIENTS; ++c)
    {
        clients.emplace_back([&, c]
        {
            std::mt19937 rng(0xC0FFEEu + unsigned(c));
            std::uniform_int_distribution<size_t> chunkSize(1, 8192);

            socket_t s = ConnectTo(port);
            if (s == INVALID_SOCKET)
            {
                clientFailures.fetch_add(1);
                return;
            }

            // Drain the echo on a helper thread so a full socket buffer cannot
            // deadlock the writer against the server's own send path.
            std::atomic<bool> readerStop{false};
            uint64            received = 0;
            std::thread reader([&]
            {
                std::vector<uint8> buf(16384);
                while (!readerStop.load())
                {
                    const int n = ::recv(s, reinterpret_cast<char*>(buf.data()),
                                         int(buf.size()), 0);
                    if (n <= 0)
                    {
                        break;
                    }
                    received += uint64(n);
                }
            });

            std::vector<uint8> payload(BYTES_PER_CLIENT);
            for (size_t i = 0; i < BYTES_PER_CLIENT; ++i)
            {
                payload[i] = PatternByte(i);
            }

            size_t offset = 0;
            while (offset < BYTES_PER_CLIENT)
            {
                const size_t chunk =
                    std::min(chunkSize(rng), BYTES_PER_CLIENT - offset);

                if (!SendAll(s, &payload[offset], chunk))
                {
                    clientFailures.fetch_add(1);
                    break;
                }
                offset += chunk;
            }

            // Let the echo drain before tearing down.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            readerStop.store(true);
#ifdef _WIN32
            ::shutdown(s, SD_BOTH);
#else
            ::shutdown(s, SHUT_RDWR);
#endif
            if (reader.joinable())
            {
                reader.join();
            }
            CLOSESOCKET(s);
            echoedBack.fetch_add(received);
        });
    }

    for (std::thread& t : clients)
    {
        t.join();
    }

    const uint64 expected = uint64(CLIENTS) * BYTES_PER_CLIENT;
    const bool arrived = WaitFor([&] { return totals.bytesReceived.load() >= expected; });

    CHECK_EQ(int(clientFailures.load()), 0);
    CHECK(arrived);
    CHECK_EQ(int(totals.corruptions.load()), 0);
    CHECK_EQ(int(totals.connections.load()), CLIENTS);

    server.stop();

    // Every accepted connection must have been told it was closing.
    CHECK(WaitFor([&] { return totals.closes.load() == totals.connections.load(); }, 5000));
}

TEST(NetStress_clients_vanish_mid_write)
{
    // Half-written streams and sockets dropped without a shutdown. This is the
    // shape that produced the reactor use-after-free: a connection torn down
    // while its pointer was still sitting in the current batch of poller events.
    const int CLIENTS = 60;

    SocketLayer layer;
    Totals totals;

    const uint16 port = NextPort();
    net::Server server;

    if (!server.start(port,
            [&totals]() -> std::shared_ptr<net::ISession>
            {
                return std::make_shared<StressSession>(totals);
            },
            "127.0.0.1"))
    {
        std::printf("    (skipped: could not bind 127.0.0.1:%u)\n", unsigned(port));
        return;
    }

    std::vector<std::thread> clients;
    for (int c = 0; c < CLIENTS; ++c)
    {
        clients.emplace_back([&, c]
        {
            std::mt19937 rng(0xBADu + unsigned(c));
            std::uniform_int_distribution<int> howFar(1, 40);

            socket_t s = ConnectTo(port);
            if (s == INVALID_SOCKET)
            {
                return;
            }

            std::vector<uint8> chunk(1024);

            // Write a bit, then disappear without shutdown() -- an abrupt reset,
            // not a graceful close.
            //
            // The pattern has to run continuously across writes: the receiver
            // checks PatternByte(offset + i) against its running offset for the
            // whole connection, so refilling the buffer from index 0 each time
            // would restart the sequence and read as corruption from the second
            // write onward.
            const int writes = howFar(rng);
            size_t sent = 0;
            for (int w = 0; w < writes; ++w)
            {
                for (size_t i = 0; i < chunk.size(); ++i)
                {
                    chunk[i] = PatternByte(sent + i);
                }

                if (!SendAll(s, chunk.data(), chunk.size()))
                {
                    break;
                }
                sent += chunk.size();
            }

            CLOSESOCKET(s);
        });
    }

    for (std::thread& t : clients)
    {
        t.join();
    }

    // The assertion is survival: every connection accepted must also be reported
    // closed, and the engine must still be healthy enough to stop cleanly.
    const bool allClosed =
        WaitFor([&] { return totals.closes.load() == totals.connections.load(); });

    CHECK(allClosed);
    CHECK_EQ(int(totals.corruptions.load()), 0);

    server.stop();
}

TEST(NetStress_rapid_connect_disconnect_churn)
{
    // Connections that live for microseconds. Exercises accept and teardown
    // against each other, which is where a leaked descriptor or a double free
    // would show up as the run progresses.
    const int ROUNDS  = 300;
    const int PARALLEL = 8;

    SocketLayer layer;
    Totals totals;

    const uint16 port = NextPort();
    net::Server server;

    if (!server.start(port,
            [&totals]() -> std::shared_ptr<net::ISession>
            {
                return std::make_shared<StressSession>(totals);
            },
            "127.0.0.1"))
    {
        std::printf("    (skipped: could not bind 127.0.0.1:%u)\n", unsigned(port));
        return;
    }

    std::atomic<int> connected{0};
    std::vector<std::thread> workers;

    for (int w = 0; w < PARALLEL; ++w)
    {
        workers.emplace_back([&]
        {
            for (int r = 0; r < ROUNDS / PARALLEL; ++r)
            {
                socket_t s = ConnectTo(port);
                if (s == INVALID_SOCKET)
                {
                    continue;
                }
                connected.fetch_add(1);

                // Some send a byte, some send nothing at all.
                if ((r & 1) == 0)
                {
                    const uint8 b = PatternByte(0);
                    SendAll(s, &b, 1);
                }
                CLOSESOCKET(s);
            }
        });
    }

    for (std::thread& t : workers)
    {
        t.join();
    }

    CHECK(connected.load() > 0);
    CHECK(WaitFor([&] { return totals.closes.load() == totals.connections.load(); }));
    CHECK_EQ(int(totals.corruptions.load()), 0);

    server.stop();
}

TEST(NetStress_stop_with_traffic_in_flight)
{
    // Stops the server while clients are still writing. Shutdown has to close
    // live connections, join its workers and disarm the send channels without
    // deadlocking against a producer -- the case the FlowGate exists for.
    SocketLayer layer;
    Totals totals;

    const uint16 port = NextPort();

    std::atomic<bool> keepWriting{true};
    std::vector<std::thread> clients;

    {
        net::Server server;

        if (!server.start(port,
                [&totals]() -> std::shared_ptr<net::ISession>
                {
                    return std::make_shared<StressSession>(totals);
                },
                "127.0.0.1"))
        {
            std::printf("    (skipped: could not bind 127.0.0.1:%u)\n", unsigned(port));
            return;
        }

        for (int c = 0; c < 12; ++c)
        {
            clients.emplace_back([&]
            {
                socket_t s = ConnectTo(port);
                if (s == INVALID_SOCKET)
                {
                    return;
                }

                std::vector<uint8> chunk(4096);

                // Same continuity requirement as above, so the corruption
                // counter stays meaningful here even though this test asserts
                // only on survival.
                size_t sent = 0;
                while (keepWriting.load())
                {
                    for (size_t i = 0; i < chunk.size(); ++i)
                    {
                        chunk[i] = PatternByte(sent + i);
                    }

                    if (!SendAll(s, chunk.data(), chunk.size()))
                    {
                        break;
                    }
                    sent += chunk.size();
                }
                CLOSESOCKET(s);
            });
        }

        // Let traffic build up, then pull the floor out.
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
        server.stop();
    }

    keepWriting.store(false);
    for (std::thread& t : clients)
    {
        t.join();
    }

    // Reaching here at all is the result: stop() returned, the destructor ran,
    // and nothing deadlocked or faulted.
    CHECK(totals.connections.load() > 0);
}

TEST(NetStress_codec_never_faults_on_hostile_input)
{
    // Fuzzes the framing layer with random bytes delivered in random chunks. The
    // contract is narrow and absolute: every call returns Ok or Malformed, and
    // nothing else may happen -- no crash, no hang, no read past the buffer. A
    // client can send whatever it likes, so this is the input the server really
    // faces.
    std::mt19937 rng(0xF0FEu);
    std::uniform_int_distribution<int> byteDist(0, 255);
    std::uniform_int_distribution<size_t> chunkDist(1, 64);

    for (int iteration = 0; iteration < 3000; ++iteration)
    {
        proto::PacketCodec codec;
        std::vector<WorldPacket> out;

        std::vector<uint8> noise(512);
        for (uint8& b : noise)
        {
            b = uint8(byteDist(rng));
        }

        size_t offset = 0;
        bool   rejected = false;

        while (offset < noise.size() && !rejected)
        {
            const size_t chunk = std::min(chunkDist(rng), noise.size() - offset);
            const proto::DecodeStatus status =
                codec.Feed(&noise[offset], chunk, out);

            if (status == proto::DecodeStatus::Malformed)
            {
                rejected = true;
            }
            offset += chunk;
        }

        // Whatever it decided, every emitted packet must be self-consistent.
        for (const WorldPacket& p : out)
        {
            CHECK(p.size() <= proto::MAX_CLIENT_PACKET_SIZE);
        }
    }
}
