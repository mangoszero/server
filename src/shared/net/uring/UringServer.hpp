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

// Linux io_uring: a completion-based (proactor) server, like IocpServer and
// unlike the readiness-based net/reactor/ backends. It is therefore NOT plugged
// into the Poller interface — it sits behind the same start()/stop() facade as a
// sibling proactor. Opt-in: enabled only when CMake's WITH_IO_URING defines
// MANGOS_USE_IO_URING (which also links liburing). Otherwise this header is empty
// and Linux falls back to the epoll reactor.
#ifdef MANGOS_USE_IO_URING

#include "net/ISession.hpp"
#include "net/SendQueue.hpp"

#include <liburing.h>

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace net {

struct UringConn;

// Lifetime-safe bridge letting the world thread send to / close a connection
// owned by one worker. post()/requestClose() append bytes (or set a close flag) and
// wake the owning worker via its eventfd; the worker applies it on its own thread.
// After the worker tears the connection down it calls disarm(), so later calls
// are no-ops and never touch the freed UringConn. (Mirror of the reactor's
// SendChannel, but waking through io_uring's eventfd instead of a poller.)
//
// As on the other backends the SendQueue lives here, in the shared_ptr the session
// holds, so the outbound buffer and its FlowGate outlive the socket — and, because
// a submitted SQE hands the kernel a raw pointer, so that the in-flight span cannot
// be reallocated out from under an outstanding write.
struct UringSendChannel : public std::enable_shared_from_this<UringSendChannel> {
    std::mutex  mu;
    bool        alive = true;
    UringConn*  conn  = nullptr;
    bool        closeRequested = false;
    SendQueue   out;                        // coalescing buffer + byte backpressure

    std::mutex*                                    reqMu    = nullptr;
    std::deque<std::shared_ptr<UringSendChannel>>* reqQueue = nullptr;
    int                                            evfd     = -1;

    void post(const uint8_t* data, size_t len);  // world thread
    void requestClose();                         // world thread
    void disarm();                               // worker thread

private:
    void notifyWorker();
};

// ── Per-connection state ──────────────────────────────────────────────────────
// Owned end-to-end by one worker thread, but FULL-DUPLEX: a recv and a send may
// be in flight at once (needed so the world thread can push packets at any time,
// not only in reply to a read). Because two ops can reference a connection, we
// reference-count in-flight ops (inflight) and free the connection only once it
// is `dead` AND inflight == 0 — markDead() shuts the socket so pending ops drain.
struct UringConn {
    int      fd = -1;
    std::shared_ptr<ISession>        session;
    std::shared_ptr<UringSendChannel> channel;
    // Kernel-filled: io_uring_prep_recv() writes it, and the completion handler
    // reads back exactly the `res` bytes the kernel reported. Deliberately left
    // uninitialised -- nothing reads it before recv fills it, and zeroing 8 KB per
    // accepted connection would only enshrine the idea that its initial value means
    // something. It does not.
    uint8_t  recvBuf[8192];

    bool     recvInFlight  = false;
    bool     sendInFlight  = false;
    int      inflight      = 0;     // submitted-but-not-completed ops
    bool     dead          = false; // teardown started; stop submitting new ops
    bool     closeAfterDrain = false;

    // cppcheck-suppress uninitMemberVar ; recvBuf is a kernel-filled recv buffer, see above
    explicit UringConn(const SessionFactory& factory) : session(factory()) {}
};

// ── io_uring TCP server ───────────────────────────────────────────────────────
class UringServer {
public:
    UringServer() = default;
    ~UringServer();

    // `bindIp` is the configured BindIP: empty (or "0.0.0.0") listens on every
    // local interface, otherwise the listener binds that single IPv4/hostname.
    bool start(uint16_t port, SessionFactory factory,
               const std::string& bindIp = std::string());
    void stop();

private:
    // user_data tagging: connection pointers are 8-byte aligned, so the low bits
    // carry the operation type. A dedicated sentinel marks the wakeup read.
    enum : uint64_t { OP_RECV = 0, OP_SEND = 1, OP_MASK = 3 };
    static constexpr uint64_t WAKE_DATA = ~uint64_t(0);

    static constexpr unsigned RING_ENTRIES = 1024;

    struct Worker {
        io_uring                          ring{};
        int                               evfd = -1;   // wakeup eventfd
        uint64_t                          evbuf = 0;   // read target for the eventfd
        std::thread                       thread;
        std::mutex                        incomingMu;
        std::vector<UringConn*>           incoming;     // handed off by acceptor
        std::unordered_set<UringConn*>    conns;        // owned by this thread

        // Cross-thread send/close requests from the world thread.
        std::mutex                                     reqMu;
        std::deque<std::shared_ptr<UringSendChannel>>  reqQueue;
    };

    int                       m_listen = -1;
    SessionFactory            m_factory;
    std::atomic<bool>         m_running{false};

    std::vector<std::unique_ptr<Worker>> m_workers;
    std::atomic<uint32_t>     m_rr{0};
    std::thread               m_acceptThread;

    void acceptLoop();
    void workerLoop(Worker& w);
    void handoff(Worker& w, UringConn* conn);
    void drainIncoming(Worker& w);
    void drainSendRequests(Worker& w);   // apply world-thread sends/closes

    // Returns true if the completion signalled shutdown (worker should stop).
    bool handleCqe(Worker& w, io_uring_cqe* cqe);

    void submitRecv(Worker& w, UringConn* conn);
    void submitSend(Worker& w, UringConn* conn);
    void submitWakeRead(Worker& w);
    void markDead(UringConn* conn);          // begin teardown (shutdown socket)
    void maybeFree(Worker& w, UringConn* conn);  // free once dead && no ops left

    static io_uring_sqe* getSqe(io_uring* ring);
};

} // namespace net

#endif // MANGOS_USE_IO_URING
