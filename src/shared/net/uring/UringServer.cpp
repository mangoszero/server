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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // accept4(), SOCK_CLOEXEC
#endif

#include "net/uring/UringServer.hpp"

#include "net/BindAddress.hpp"
#include "Log.h"

#ifdef MANGOS_USE_IO_URING

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <utility>
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

io_uring_sqe* UringServer::getSqe(io_uring* ring) {
    io_uring_sqe* sqe = io_uring_get_sqe(ring);
    if (!sqe) {                       // SQ ring full: flush, then retry once
        io_uring_submit(ring);
        sqe = io_uring_get_sqe(ring);
    }
    return sqe;
}

UringServer::~UringServer() { stop(); }

bool UringServer::start(uint16_t port, SessionFactory factory,
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
        ::listen(m_listen, SOMAXCONN) < 0) {
        ::close(m_listen); m_listen = -1;
        return false;
    }

    unsigned nWorkers = std::thread::hardware_concurrency();
    if (nWorkers == 0) nWorkers = 1;

    m_running.store(true);

    for (unsigned i = 0; i < nWorkers; ++i) {
        auto w = std::make_unique<Worker>();
        if (io_uring_queue_init(RING_ENTRIES, &w->ring, 0) < 0) { stop(); return false; }
        w->evfd = ::eventfd(0, EFD_CLOEXEC);
        if (w->evfd < 0) { io_uring_queue_exit(&w->ring); stop(); return false; }
        m_workers.push_back(std::move(w));
    }

    for (auto& w : m_workers)
        w->thread = std::thread([this, wp = w.get()] { workerLoop(*wp); });

    m_acceptThread = std::thread([this] { acceptLoop(); });

    sLog.outString("WorldSocket: listening on %s:%u with %u worker threads (io_uring)",
                   (bindIp.empty() ? "0.0.0.0" : bindIp.c_str()), (unsigned)port, (unsigned)nWorkers);
    return true;
}

void UringServer::stop() {
    if (!m_running.exchange(false)) return;

    // Unblock the acceptor's blocking accept() by tearing down the listen socket.
    if (m_listen >= 0) {
        ::shutdown(m_listen, SHUT_RDWR);
        ::close(m_listen);
        m_listen = -1;
    }
    if (m_acceptThread.joinable()) m_acceptThread.join();

    // Wake every worker (eventfd read completes -> worker observes !running).
    for (auto& w : m_workers) {
        if (w->evfd >= 0) { uint64_t one = 1; ssize_t r = ::write(w->evfd, &one, sizeof(one)); (void)r; }
    }
    for (auto& w : m_workers)
        if (w->thread.joinable()) w->thread.join();

    // Workers leave their connections allocated; tear down the ring FIRST so the
    // kernel stops referencing any in-flight recv/send buffers, THEN free.
    for (auto& w : m_workers) {
        io_uring_queue_exit(&w->ring);
        // disarm() first so a bulk producer parked on backpressure (patch stream)
        // wakes and stops instead of blocking forever / posting into a freed conn.
        for (auto* c : w->conns)    { if (c->channel) c->channel->disarm(); ::close(c->fd); delete c; }
        for (auto* c : w->incoming) { if (c->channel) c->channel->disarm(); ::close(c->fd); delete c; }
        w->conns.clear();
        w->incoming.clear();
        if (w->evfd >= 0) { ::close(w->evfd); w->evfd = -1; }
    }
    m_workers.clear();
}

void UringServer::acceptLoop() {
    while (true) {
        sockaddr_in peer{};
        socklen_t peerLen = sizeof(peer);
        int cfd = ::accept4(m_listen, reinterpret_cast<sockaddr*>(&peer),
                            &peerLen, SOCK_CLOEXEC);
        if (cfd < 0) {
            if (errno == EINTR) continue;
            break; // listen socket closed on shutdown
        }

        char peerIp[INET_ADDRSTRLEN] = {};
        if (peer.sin_family != AF_INET ||
            !inet_ntop(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp))) {
            ::close(cfd);
            continue;
        }

        int one = 1;
        ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        auto* conn = new UringConn(m_factory);
        conn->fd = cfd;
        conn->session->setPeerAddress(peerIp);

        // Pick the owning worker up front so the channel can target its eventfd
        // before the session (in onConnect) registers with the world loop.
        Worker& w = *m_workers[m_rr.fetch_add(1) % m_workers.size()];
        conn->channel = std::make_shared<UringSendChannel>();
        conn->channel->conn     = conn;
        conn->channel->reqMu    = &w.reqMu;
        conn->channel->reqQueue = &w.reqQueue;
        conn->channel->evfd     = w.evfd;
        conn->session->setSender(
            [ch = conn->channel](const uint8_t* d, size_t n) { ch->post(d, n); });
        conn->session->setCloser([ch = conn->channel] { ch->requestClose(); });
        conn->session->setFlowControl(
            std::shared_ptr<net::FlowControl>(conn->channel, &conn->channel->out.gate()));

        // Server-initiated greeting (e.g. SMSG_AUTH_CHALLENGE). No worker owns
        // the connection yet, so touching the session here is race-free.
        auto greeting = conn->session->onConnect();
        if (!greeting.empty()) {
            conn->channel->out.append(greeting.data(), greeting.size());
        }
        handoff(w, conn);
    }
}

void UringServer::handoff(Worker& w, UringConn* conn) {
    {
        std::lock_guard<std::mutex> lock(w.incomingMu);
        w.incoming.push_back(conn);
    }
    uint64_t one = 1;
    ssize_t r = ::write(w.evfd, &one, sizeof(one)); // wake the worker's eventfd read
    (void)r;
}

// ── UringSendChannel (cross-thread send / close) ──────────────────────────────

void UringSendChannel::notifyWorker() {
    {
        std::lock_guard<std::mutex> lock(*reqMu);
        reqQueue->push_back(shared_from_this());
    }
    uint64_t one = 1;
    ssize_t r = ::write(evfd, &one, sizeof(one));  // wake the worker
    (void)r;
}

void UringSendChannel::post(const uint8_t* data, size_t len) {
    {
        std::lock_guard<std::mutex> lock(mu);
        if (!alive) return;
        // Append straight into the shared outbound buffer (coalescing with whatever
        // is already queued) and count the bytes against backpressure here, at
        // hand-off, so a producer cannot outrun a lagging worker. The worker submits
        // its SQEs out of the very same buffer — no second hand-off, no per-packet
        // allocation.
        out.append(data, len);
    }
    notifyWorker();
}

void UringSendChannel::requestClose() {
    {
        std::lock_guard<std::mutex> lock(mu);
        if (!alive) return;
        closeRequested = true;
    }
    notifyWorker();
}

void UringSendChannel::disarm() {
    std::lock_guard<std::mutex> lock(mu);
    alive = false;
    conn  = nullptr;
    out.close();  // release any bulk producer parked on backpressure
}

void UringServer::drainIncoming(Worker& w) {
    std::vector<UringConn*> pending;
    {
        std::lock_guard<std::mutex> lock(w.incomingMu);
        pending.swap(w.incoming);
    }
    for (auto* conn : pending) {
        w.conns.insert(conn);
        // Full-duplex: keep a recv pending so reads/closes are always noticed, and
        // also kick a send if onConnect() queued a greeting.
        submitRecv(w, conn);
        if (!conn->channel->out.empty())
            submitSend(w, conn);
    }
}

// Apply the sends/closes the world thread queued (runs on the worker thread).
void UringServer::drainSendRequests(Worker& w) {
    std::deque<std::shared_ptr<UringSendChannel>> reqs;
    {
        std::lock_guard<std::mutex> lock(w.reqMu);
        reqs.swap(w.reqQueue);
    }
    for (auto& ch : reqs) {
        UringConn* conn = nullptr;
        bool       wantClose = false;
        {
            std::lock_guard<std::mutex> lock(ch->mu);
            if (!ch->alive)
                continue;
            conn      = ch->conn;
            wantClose = ch->closeRequested;
        }
        if (!conn || conn->dead)
            continue;

        // The bytes are already in ch->out — post() appended them there directly, so
        // there is nothing to move across; just get a write going.
        if (wantClose)
            conn->closeAfterDrain = true;

        if (!conn->sendInFlight && !ch->out.empty())
            submitSend(w, conn);
        else if (conn->closeAfterDrain && ch->out.empty())
            markDead(conn);  // nothing to flush, close now (recv op drains via shutdown)

        maybeFree(w, conn);
    }
}

void UringServer::submitRecv(Worker& w, UringConn* conn) {
    if (conn->dead || conn->recvInFlight) return;
    io_uring_sqe* sqe = getSqe(&w.ring);
    if (!sqe) return;
    io_uring_prep_recv(sqe, conn->fd, conn->recvBuf, sizeof(conn->recvBuf), 0);
    io_uring_sqe_set_data64(sqe, reinterpret_cast<uint64_t>(conn) | OP_RECV);
    conn->recvInFlight = true;
    ++conn->inflight;
}

void UringServer::submitSend(Worker& w, UringConn* conn) {
    if (conn->dead || conn->sendInFlight) return;

    // Safe to hand the kernel a raw pointer into the in-flight span: producers only
    // ever append to the queue's *pending* buffer, so this storage cannot move
    // before the completion arrives. nextSpan() also coalesces everything queued
    // since the last write into this one SQE.
    const uint8_t* data = nullptr;
    size_t         len  = 0;
    if (!conn->channel->out.nextSpan(data, len)) return;   // nothing to write

    io_uring_sqe* sqe = getSqe(&w.ring);
    if (!sqe) { conn->channel->out.abortWrite(); return; }
    io_uring_prep_send(sqe, conn->fd, data, len, MSG_NOSIGNAL);
    io_uring_sqe_set_data64(sqe, reinterpret_cast<uint64_t>(conn) | OP_SEND);
    conn->sendInFlight = true;
    ++conn->inflight;
}

void UringServer::submitWakeRead(Worker& w) {
    io_uring_sqe* sqe = getSqe(&w.ring);
    if (!sqe) return;
    io_uring_prep_read(sqe, w.evfd, &w.evbuf, sizeof(w.evbuf), 0);
    io_uring_sqe_set_data64(sqe, WAKE_DATA);
}

void UringServer::markDead(UringConn* conn) {
    if (conn->dead) return;
    conn->dead = true;
    if (conn->channel) conn->channel->disarm();   // world thread stops posting
    if (conn->session) conn->session->onClose();
    ::shutdown(conn->fd, SHUT_RDWR);               // force pending recv/send to complete
}

void UringServer::maybeFree(Worker& w, UringConn* conn) {
    if (!conn->dead || conn->inflight != 0) return;
    w.conns.erase(conn);
    ::close(conn->fd);
    delete conn;
}

bool UringServer::handleCqe(Worker& w, io_uring_cqe* cqe) {
    const uint64_t data = cqe->user_data;
    const int      res  = cqe->res;

    if (data == WAKE_DATA) {
        drainIncoming(w);
        drainSendRequests(w);
        if (m_running.load()) { submitWakeRead(w); return false; }
        return true; // shutdown
    }

    auto*          conn = reinterpret_cast<UringConn*>(data & ~OP_MASK);
    const uint64_t tag  = data & OP_MASK;

    if (tag == OP_RECV) {
        conn->recvInFlight = false;
        --conn->inflight;

        if (res <= 0) {
            markDead(conn);            // 0 = peer closed; <0 = error/cancelled
        } else {
            auto resp = conn->session->onData(conn->recvBuf, static_cast<size_t>(res));
            if (!resp.empty())
                conn->channel->out.append(resp.data(), resp.size());

            // A session that closes having just queued its final bytes (an auth
            // rejection, say) must still get them out, so only tear down once the
            // outbound buffer has drained; otherwise let the send completion do it.
            if (conn->session->closed()) {
                if (conn->channel->out.empty())
                    markDead(conn);
                else
                    conn->closeAfterDrain = true;
            }
            if (!conn->dead) {
                submitRecv(w, conn);             // keep reading
                submitSend(w, conn);             // flush any reply/queued bytes
            }
        }
        maybeFree(w, conn);
        return false;
    }

    // OP_SEND
    conn->sendInFlight = false;
    --conn->inflight;

    if (res <= 0) {
        markDead(conn);
    } else {
        // Honour the transferred count: io_uring may complete a send short, and
        // dropping the remainder would silently truncate the stream. consume()
        // advances the cursor; the next submitSend() resumes from there.
        conn->channel->out.consume(static_cast<size_t>(res));

        if (!conn->dead) {
            if (!conn->channel->out.empty())
                submitSend(w, conn);             // more queued / partial remainder
            else if (conn->closeAfterDrain)
                markDead(conn);                  // delivered everything, now close
        }
    }
    maybeFree(w, conn);
    return false;
}

void UringServer::workerLoop(Worker& w) {
    submitWakeRead(w);
    io_uring_submit(&w.ring);

    bool stopping = false;
    while (!stopping) {
        int ret = io_uring_submit_and_wait(&w.ring, 1);
        if (ret < 0 && ret == -EINTR) continue;

        unsigned        head;
        io_uring_cqe*   cqe;
        unsigned        count = 0;
        io_uring_for_each_cqe(&w.ring, head, cqe) {
            if (handleCqe(w, cqe)) stopping = true;
            ++count;
        }
        io_uring_cq_advance(&w.ring, count);
    }
    // Connections are freed in stop(), after io_uring_queue_exit() guarantees the
    // kernel no longer touches their buffers.
}

} // namespace net

#endif // MANGOS_USE_IO_URING
