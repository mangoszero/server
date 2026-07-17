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

#include "net/reactor/ReactorServer.hpp"

#include "net/BindAddress.hpp"
#include "Log.h"

// Reactor server is POSIX-only; on Windows the proactor IocpServer is used and
// this translation unit collapses to nothing.
#ifndef _WIN32

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include <cerrno>
#include <utility>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

namespace net {
namespace {

bool setNonBlocking(int fd) {
    int f = ::fcntl(fd, F_GETFL, 0);
    return f >= 0 && ::fcntl(fd, F_SETFL, f | O_NONBLOCK) == 0;
}

} // namespace

ReactorServer::ReactorServer(PollerFactory factory)
    : m_pollerFactory(std::move(factory)) {}

ReactorServer::~ReactorServer() { stop(); }

bool ReactorServer::start(uint16_t port, SessionFactory factory,
                          const std::string& bindIp) {
    m_factory = std::move(factory);

    // Resolve BindIP before touching the socket so an invalid option fails the
    // bind outright instead of silently listening on every interface.
    uint32_t bindAddr = htonl(INADDR_ANY);
    if (!ResolveBindAddress(bindIp, bindAddr)) return false;

    m_listen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen < 0) return false;

    int one = 1;
    ::setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = bindAddr;
    addr.sin_port        = htons(port);

    if (::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
        ::listen(m_listen, SOMAXCONN) < 0 ||
        !setNonBlocking(m_listen)) {
        ::close(m_listen); m_listen = -1;
        return false;
    }

    // Acceptor poller: watches only the listen socket. We tag it with &m_listen
    // so the accept loop can recognise its events without an fd field.
    m_acceptPoller = m_pollerFactory();
    if (!m_acceptPoller || !m_acceptPoller->init() ||
        !m_acceptPoller->add(m_listen, EvRead, &m_listen)) {
        ::close(m_listen); m_listen = -1;
        m_acceptPoller.reset();
        return false;
    }

    unsigned nWorkers = std::thread::hardware_concurrency();
    if (nWorkers == 0) nWorkers = 1;

    // Mark running before any thread starts so a failure below can route through
    // stop() for cleanup.
    m_running.store(true);

    for (unsigned i = 0; i < nWorkers; ++i) {
        auto w = std::make_unique<Worker>();
        w->poller = m_pollerFactory();
        if (!w->poller || !w->poller->init()) { stop(); return false; }
        m_workers.push_back(std::move(w));
    }

    // Start threads only once every Worker exists, so the vector can't reallocate
    // under a running thread's Worker& reference.
    for (auto& w : m_workers)
        w->thread = std::thread([this, wp = w.get()] { workerLoop(*wp); });

    m_acceptThread = std::thread([this] { acceptLoop(); });

    sLog.outString("WorldSocket: listening on %s:%u with %u worker threads (%s)",
                   (bindIp.empty() ? "0.0.0.0" : bindIp.c_str()), (unsigned)port,
                   (unsigned)nWorkers, m_acceptPoller->name());
    return true;
}

void ReactorServer::stop() {
    if (!m_running.exchange(false)) return;

    // 1) Stop accepting first so no new connection is handed off mid-teardown.
    if (m_acceptPoller) m_acceptPoller->wake();
    if (m_acceptThread.joinable()) m_acceptThread.join();

    // 2) Wake and join workers; each closes the connections it owns.
    for (auto& w : m_workers)
        if (w->poller) w->poller->wake();
    for (auto& w : m_workers)
        if (w->thread.joinable()) w->thread.join();

    // 3) Reap connections handed off but never registered, and close pollers.
    for (auto& w : m_workers) {
        for (auto* c : w->incoming) {
            if (c->channel) c->channel->disarm();   // wake any parked producer
            ::close(c->fd);
            delete c;
        }
        w->incoming.clear();
        if (w->poller) w->poller->shutdown();
    }
    m_workers.clear();

    if (m_acceptPoller) { m_acceptPoller->shutdown(); m_acceptPoller.reset(); }
    if (m_listen >= 0)  { ::close(m_listen); m_listen = -1; }
}

void ReactorServer::acceptLoop() {
    constexpr int MAXEV = 16;
    PollerEvent evs[MAXEV];

    while (true) {
        int n = m_acceptPoller->wait(evs, MAXEV);
        if (n < 0) break;
        if (!m_running.load()) break;

        for (int i = 0; i < n; ++i) {
            if (evs[i].udata != &m_listen) continue;

            // Drain the backlog (listen socket is non-blocking).
            for (;;) {
                sockaddr_in peer{};
                socklen_t peerLen = sizeof(peer);
                int cfd = ::accept(m_listen, reinterpret_cast<sockaddr*>(&peer), &peerLen);
                if (cfd < 0) {
                    if (errno == EINTR) continue;
                    break; // EAGAIN/EWOULDBLOCK: drained
                }
                if (!setNonBlocking(cfd)) { ::close(cfd); continue; }

                char peerIp[INET_ADDRSTRLEN] = {};
                inet_ntop(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp));

                int one = 1;
                ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
#ifdef SO_NOSIGPIPE
                ::setsockopt(cfd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
                auto* conn = new Connection(m_factory);
                conn->fd = cfd;
                conn->session->setPeerAddress(peerIp);

                // Pick the owning worker up front so the SendChannel can target
                // it before the session (in onConnect) registers with the world
                // loop and could be ticked. The Worker lives in a unique_ptr, so
                // its address (and its reqMu/reqQueue/poller) is stable.
                Worker& w = *m_workers[m_rr.fetch_add(1) % m_workers.size()];
                conn->channel = std::make_shared<SendChannel>();
                conn->channel->conn     = conn;
                conn->channel->reqMu    = &w.reqMu;
                conn->channel->reqQueue = &w.reqQueue;
                conn->channel->poller   = w.poller.get();
                conn->session->setSender(
                    [ch = conn->channel](const uint8_t* d, size_t n) { ch->post(d, n); });
                conn->session->setCloser([ch = conn->channel] { ch->requestClose(); });
                conn->session->setFlowControl(
                    std::shared_ptr<net::FlowControl>(conn->channel, &conn->channel->out.gate()));

                // Server-initiated greeting (e.g. SMSG_AUTH_CHALLENGE). Safe to
                // run here: no worker owns the connection until handoff().
                auto greeting = conn->session->onConnect();
                if (!greeting.empty()) {
                    conn->channel->out.append(greeting.data(), greeting.size());
                }
                handoff(w, conn);
            }
        }
    }
}

void ReactorServer::handoff(Worker& w, Connection* conn) {
    // Hand to the pre-chosen worker, then wake it so it registers the fd on its
    // own poller (single-owner: no locking on the Connection itself thereafter).
    {
        std::lock_guard<std::mutex> lock(w.incomingMu);
        w.incoming.push_back(conn);
    }
    w.poller->wake();
}

// ── SendChannel (cross-thread send / close) ───────────────────────────────────

void SendChannel::notifyWorker() {
    // mu must NOT be held here. reqMu/reqQueue/poller are set once at hand-off and
    // the worker outlives every connection, so they are safe to touch unlocked.
    {
        std::lock_guard<std::mutex> lock(*reqMu);
        reqQueue->push_back(shared_from_this());
    }
    poller->wake();
}

void SendChannel::post(const uint8_t* data, size_t len) {
    {
        std::lock_guard<std::mutex> lock(mu);
        if (!alive) return;
        // Append straight into the shared outbound buffer (coalescing with whatever
        // is already queued) and count the bytes against backpressure here, at
        // hand-off, so a producer cannot outrun a lagging worker. The worker drains
        // the very same buffer — no second hand-off, no per-packet allocation.
        out.append(data, len);
    }
    notifyWorker();
}

void SendChannel::requestClose() {
    {
        std::lock_guard<std::mutex> lock(mu);
        if (!alive) return;
        closeRequested = true;
    }
    notifyWorker();
}

void SendChannel::disarm() {
    std::lock_guard<std::mutex> lock(mu);
    alive = false;
    conn  = nullptr;
    out.close();  // release any bulk producer parked on backpressure
}

void ReactorServer::drainIncoming(Worker& w) {
    std::vector<Connection*> pending;
    {
        std::lock_guard<std::mutex> lock(w.incomingMu);
        pending.swap(w.incoming);
    }
    for (auto* conn : pending) {
        if (!w.poller->add(conn->fd, EvRead, conn)) {
            ::close(conn->fd);
            delete conn;
            continue;
        }
        w.conns.insert(conn);
        // Push any greeting queued by onConnect() now that we own the fd.
        if (!conn->channel->out.empty() && !flush(w, conn))
            closeConn(w, conn);
    }
}

void ReactorServer::setWriteInterest(Worker& w, Connection* conn, bool want) {
    if (want == conn->writeArmed) return;
    w.poller->mod(conn->fd, want ? (EvRead | EvWrite) : EvRead, conn);
    conn->writeArmed = want;
}

bool ReactorServer::flush(Worker& w, Connection* conn) {
    SendQueue& out = conn->channel->out;

    for (;;) {
        const uint8_t* p   = nullptr;
        size_t         rem = 0;
        if (!out.nextSpan(p, rem)) {
            setWriteInterest(w, conn, false); // fully drained
            return true;
        }

        ssize_t n = ::send(conn->fd, p, rem, MSG_NOSIGNAL);
        if (n > 0) {
            // Short writes are the norm on a non-blocking socket; consume() just
            // advances the cursor and the next span resumes from there.
            out.consume(static_cast<size_t>(n));
            continue;
        }
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) { setWriteInterest(w, conn, true); return true; }
            return false; // fatal
        }
        setWriteInterest(w, conn, true); // n == 0: wait for writability
        return true;
    }
}

// Runs on the worker thread: apply the sends/closes the world thread queued.
void ReactorServer::drainSendRequests(Worker& w) {
    std::deque<std::shared_ptr<SendChannel>> reqs;
    {
        std::lock_guard<std::mutex> lock(w.reqMu);
        reqs.swap(w.reqQueue);
    }
    for (auto& ch : reqs) {
        Connection* conn = nullptr;
        bool        wantClose = false;
        {
            std::lock_guard<std::mutex> lock(ch->mu);
            if (!ch->alive)
                continue;  // connection already torn down; channel kept alive only by us
            conn      = ch->conn;
            wantClose = ch->closeRequested;
        }
        if (!conn)
            continue;

        // The bytes are already in conn->channel->out — post() appended them there
        // directly, so there is nothing to move across; just push them at the socket.
        if (!flush(w, conn)) {
            closeConn(w, conn);
            continue;
        }
        if (wantClose) {
            if (conn->channel->out.empty())
                closeConn(w, conn);
            else
                conn->closeAfterDrain = true;  // flush() will close once drained
        }
    }
}

void ReactorServer::closeConn(Worker& w, Connection* conn) {
    // Disarm the channel first so a world thread still holding the Sender no-ops
    // instead of touching this Connection, then let the session detach from the
    // world loop.
    if (conn->channel)
        conn->channel->disarm();
    if (conn->session)
        conn->session->onClose();

    w.poller->del(conn->fd);
    w.conns.erase(conn);
    ::close(conn->fd);
    delete conn;
}

void ReactorServer::workerLoop(Worker& w) {
    constexpr int MAXEV = 64;
    PollerEvent evs[MAXEV];
    uint8_t     rbuf[8192];

    while (true) {
        int n = w.poller->wait(evs, MAXEV);
        if (n < 0) break;

        drainIncoming(w);                 // register any newly handed-off conns
        drainSendRequests(w);             // apply world-thread sends/closes
        if (!m_running.load()) break;

        for (int i = 0; i < n; ++i) {
            auto* conn = static_cast<Connection*>(evs[i].udata);
            if (!conn) continue;          // wakeup-only event

            bool dead = false;

            if (evs[i].flags & EvRead) {
                ssize_t r = ::recv(conn->fd, rbuf, sizeof(rbuf), 0);
                if (r > 0) {
                    auto resp = conn->session->onData(rbuf, static_cast<size_t>(r));
                    if (!resp.empty()) {
                        conn->channel->out.append(resp.data(), resp.size());
                        if (!flush(w, conn)) dead = true;
                    }
                    if (!dead && conn->session->closed()) {
                        if (conn->channel->out.empty()) dead = true;
                        else                            conn->closeAfterDrain = true;
                    }
                } else if (r == 0) {
                    dead = true;          // peer closed
                } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    dead = true;          // hard error
                }
            }

            if (!dead && (evs[i].flags & EvWrite)) {
                if (!flush(w, conn)) dead = true;
                else if (conn->channel->out.empty() && conn->closeAfterDrain) dead = true;
            }

            if (dead) closeConn(w, conn);
        }
    }

    // Shutdown: close every connection this worker still owns. disarm() first so a
    // bulk producer parked on backpressure (patch stream) wakes and stops instead
    // of blocking forever / posting into a freed connection.
    for (auto* c : w.conns) {
        if (c->channel) c->channel->disarm();
        ::close(c->fd);
        delete c;
    }
    w.conns.clear();
}

} // namespace net

#endif // !_WIN32
