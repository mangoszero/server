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

#include "net/iocp/IocpServer.hpp"

#include "net/BindAddress.hpp"
#include "Log.h"

#ifdef _WIN32

#include <cstring>
#include <utility>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <chrono>
#include <thread>

namespace net {

// ── SendChannel ───────────────────────────────────────────────────────────────

void SendChannel::post(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(mu);
    if (ctx)
        ctx->enqueue(data, len);
}

// The session asked to close. Do NOT close the socket here.
//
// Closing it discards whatever is still queued, and what is queued at this exact
// moment is the reason for the disconnect -- the auth rejection, the transfer abort,
// the kick message. The session sets closed() before calling this, so every completion
// path already re-checks "closed and drained"; all this has to do is record the intent
// and kick the drain along.
void SendChannel::requestClose() {
    ConnCtx* c = nullptr;
    {
        std::lock_guard<std::mutex> lock(mu);
        closeRequested = true;
        c = ctx;
    }
    if (!c)
    {
        return;
    }

    // Inside a callback the response has not been queued yet, so the dispatcher makes
    // this call instead, once it has. Outside one -- a world thread closing an idle
    // connection -- no completion may ever arrive, so this is the only chance to act.
    if (!c->dispatching.load(std::memory_order_acquire))
    {
        c->closeIfDrained();
    }
}

void SendChannel::disarm() {
    std::lock_guard<std::mutex> lock(mu);
    ctx = nullptr;
    out.close();  // release any bulk producer parked on backpressure
}

// ── ConnCtx ───────────────────────────────────────────────────────────────────

bool ConnCtx::postSend(const uint8_t* data, size_t len) {
    bool failed = false;
    {
        std::lock_guard<std::mutex> sk(sockMu);   // no concurrent Winsock call on `sock`
        if (sock == INVALID_SOCKET)
            return false;                          // closed under us; never addRef'd
        ZeroMemory(&sendOv.ov, sizeof(OVERLAPPED));
        // Safe to hand the kernel a pointer into the SendQueue's in-flight buffer: only
        // the pending buffer is ever appended to, so this storage cannot move or be
        // reallocated before the completion arrives.
        sendOv.wsabuf.buf = reinterpret_cast<char*>(const_cast<uint8_t*>(data));
        sendOv.wsabuf.len = static_cast<ULONG>(len);
        addRef();  // the completion of this send will release()
        int rc = WSASend(sock, &sendOv.wsabuf, 1, nullptr, 0, &sendOv.ov, nullptr);
        if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
            failed = true;
    }
    if (failed) {
        release();  // no completion for a synchronous failure. Outside sockMu: the last
                    // release routes through removeAndDelete()->m_connsMu, and stop()
                    // takes m_connsMu->sockMu, so releasing under sockMu could deadlock.
        return false;
    }
    return true;
}

void ConnCtx::enqueue(const uint8_t* data, size_t len) {
    // append() returns true only for the caller that finds no write in flight, so
    // exactly one thread starts the write and the stream stays ordered. Everything
    // else queued meanwhile is coalesced into the next span by nextSpan().
    if (channel && channel->out.append(data, len))
        startSend();
}

void ConnCtx::startSend() {
    const uint8_t* data = nullptr;
    size_t         len  = 0;
    if (!channel->out.nextSpan(data, len))
        return;  // nothing left; nextSpan() released ownership of the write

    if (!postSend(data, len)) {
        // Socket already gone. Release ownership so the queue is not stuck believing
        // a write is running; teardown frees us once the recv side completes.
        channel->out.abortWrite();
    }
}

void ConnCtx::onSendComplete(DWORD bytes) {
    // Honour the transferred count: WSASend may complete short, and popping the whole
    // buffer regardless (as this used to) silently truncates the stream and desyncs
    // the protocol. consume() advances the cursor; startSend() re-posts the remainder.
    channel->out.consume(static_cast<size_t>(bytes));
    startSend();
}

bool ConnCtx::drained() const {
    return channel->out.empty();
}

// Half-closes once the queue is empty. shutdown(SD_SEND) rather than closesocket:
// the FIN flushes what Winsock still holds and the peer reads the tail, whereas a
// closesocket on a socket with unsent data is free to reset the connection and throw
// it away. The recv side stays open so the peer's own FIN still arrives and drives the
// normal teardown.
bool ConnCtx::closeIfDrained() {
    {
        std::lock_guard<std::mutex> lock(channel->mu);
        if (!channel->closeRequested)
        {
            return false;
        }
    }
    if (!drained())
    {
        return false;
    }

    std::lock_guard<std::mutex> sk(sockMu);
    if (sock == INVALID_SOCKET || sendShutdown)
    {
        return false;
    }
    sendShutdown = true;
    ::shutdown(sock, SD_SEND);
    return true;
}

void ConnCtx::close() {
    std::lock_guard<std::mutex> sk(sockMu);   // serialise with WSARecv/WSASend and other close()s
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

void ConnCtx::release() {
    if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        // Last reference: hand off to the owner so the erase-from-m_conns and the delete
        // happen together under m_connsMu -- a stop() scan holding that lock then only
        // ever sees live ctxs, and an empty m_conns is genuine I/O quiescence.
        if (owner)
            owner->removeAndDelete(this);
        else
            delete this;   // never adopted by a server (should not happen; owner set at accept)
    }
}

// ── IocpServer ────────────────────────────────────────────────────────────────

IocpServer::~IocpServer() { stop(); }

bool IocpServer::start(uint16_t port, SessionFactory factory,
                       const std::string& bindIp) {
    m_factory = std::move(factory);

    // Own one Winsock reference for this listener's lifetime. realmd (and every
    // module server) links neither gsoap nor g3dlite, so nothing else guarantees
    // WSAStartup has run before the socket calls below.
    if (!m_wsaStarted) {
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            sLog.outError("WorldSocket: WSAStartup failed");
            return false;
        }
        m_wsaStarted = true;
    }

    // Every failure below has to undo the listen socket and the completion port by
    // hand: stop() only cleans up once m_running has been set, which happens after
    // the last of these, so bailing out with a bare `return false` would leak both
    // for the life of the process.
    auto fail = [this]() -> bool {
        if (m_listen != INVALID_SOCKET) { closesocket(m_listen); m_listen = INVALID_SOCKET; }
        if (m_iocp)                     { CloseHandle(m_iocp);   m_iocp   = nullptr; }
        if (m_wsaStarted)               { WSACleanup();          m_wsaStarted = false; }
        return false;
    };

    // Resolve BindIP before touching the socket so an invalid option fails the
    // bind outright instead of silently listening on every interface.
    uint32_t bindAddr = htonl(INADDR_ANY);
    if (!ResolveBindAddress(bindIp, bindAddr)) return fail();

    m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!m_iocp) return fail();

    m_listen = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                          nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (m_listen == INVALID_SOCKET) return fail();

    SOCKADDR_IN addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = bindAddr;
    addr.sin_port        = htons(port);

    if (bind(m_listen, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        return fail();
    if (listen(m_listen, SOMAXCONN) == SOCKET_ERROR)
        return fail();

    // Associate listen socket with IOCP (key=0, used only for AcceptEx completions)
    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_listen), m_iocp, 0, 0))
        return fail();

    // Load AcceptEx extension function
    GUID guidAcceptEx        = WSAID_ACCEPTEX;
    GUID guidGetSockaddrs    = WSAID_GETACCEPTEXSOCKADDRS;
    DWORD bytes = 0;
    WSAIoctl(m_listen, SIO_GET_EXTENSION_FUNCTION_POINTER,
             &guidAcceptEx, sizeof(guidAcceptEx),
             &m_fnAcceptEx, sizeof(m_fnAcceptEx), &bytes, nullptr, nullptr);
    WSAIoctl(m_listen, SIO_GET_EXTENSION_FUNCTION_POINTER,
             &guidGetSockaddrs, sizeof(guidGetSockaddrs),
             &m_fnGetSockaddrs, sizeof(m_fnGetSockaddrs), &bytes, nullptr, nullptr);

    if (!m_fnAcceptEx) return fail();

    m_running = true;

    // Launch one worker thread per logical CPU
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    DWORD nThreads = si.dwNumberOfProcessors;
    for (DWORD i = 0; i < nThreads; ++i)
        m_workers.emplace_back([this] { workerThread(); });

    // Post initial accept operations
    for (int i = 0; i < PENDING_ACCEPTS; ++i)
        postAccept();

    sLog.outString("WorldSocket: listening on %s:%u with %u worker threads (IOCP)",
                   (bindIp.empty() ? "0.0.0.0" : bindIp.c_str()), (unsigned)port, (unsigned)nThreads);
    return true;
}

void IocpServer::stop() {
    if (m_running.exchange(false)) {
        // 1. Close the listener FIRST -- under m_listenMu so it never races a worker's
        //    postAccept()/AcceptEx -- so no further connection is accepted. The pending
        //    AcceptEx ops complete with an error and are reclaimed by the workers.
        {
            std::lock_guard<std::mutex> lk(m_listenMu);
            if (m_listen != INVALID_SOCKET) { closesocket(m_listen); m_listen = INVALID_SOCKET; }
        }

        // 2. Close every live connection's socket -- serialised by the ctx's sockMu so it
        //    never races a worker's WSARecv/WSASend -- which makes its pending ops complete
        //    with an error. The dequeuing worker runs the normal markDead()+release()
        //    teardown, and the ctx is erased from m_conns and freed (in release()) only
        //    once no overlapped op can still complete into its embedded OVERLAPPEDs. Wait
        //    on m_connsCv until BOTH m_conns is empty AND every pending AcceptEx has
        //    completed -- true, drained quiescence, not the "erased before drained" pointer
        //    count the old force-delete relied on -- re-closing any connection that raced
        //    in before the listener closed. The workers are STILL running here, which is
        //    what lets the completions drain.
        {
            std::unique_lock<std::mutex> lock(m_connsMu);
            while (!m_conns.empty() ||
                   m_pendingAccepts.load(std::memory_order_acquire) != 0) {
                for (auto* c : m_conns)
                    c->close();  // idempotent; the closesocket is serialised by c->sockMu
                m_connsCv.wait_for(lock, std::chrono::milliseconds(50));
            }
        }

        // 3. No connection and no accept has a pending overlapped op now; wake the idle
        //    workers and join them.
        for (size_t i = 0; i < m_workers.size(); ++i)
            PostQueuedCompletionStatus(m_iocp, 0, SHUTDOWN_KEY, nullptr);
        for (auto& t : m_workers) t.join();
        m_workers.clear();

        if (m_iocp) { CloseHandle(m_iocp); m_iocp = nullptr; }
    }

    // Balance the WSAStartup from start(). Guarded so a failed start (which may
    // return before m_running is set) still releases the reference exactly once.
    if (m_wsaStarted) { WSACleanup(); m_wsaStarted = false; }
}

// Last-reference cleanup, called from ConnCtx::release(). Erase from m_conns under
// m_connsMu, then delete: a stop() scan holds m_connsMu and touches only ctxs still in the
// set, and each is erased before it is deleted, so the scan never observes freed memory.
void IocpServer::removeAndDelete(ConnCtx* ctx) {
    {
        std::lock_guard<std::mutex> lock(m_connsMu);
        m_conns.erase(ctx);
    }
    m_connsCv.notify_all();
    delete ctx;
}

void IocpServer::postAccept() {
    // Serialise use of the listen socket against stop()'s closesocket(m_listen). Also
    // gates posting once shutdown has closed the listener: AcceptEx on an INVALID_SOCKET
    // (or one being reused) must never happen.
    std::lock_guard<std::mutex> lk(m_listenMu);
    if (m_listen == INVALID_SOCKET)
        return;

    auto* aov = new AcceptOv{};
    aov->clientSock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                 nullptr, 0, WSA_FLAG_OVERLAPPED);
    // A transient socket-creation failure drops this pending-accept slot; the pool is
    // otherwise kept topped up by the re-post on each accept completion. (Recovering the
    // last slot under sustained failure would need a timer/backoff -- left as a follow-up.)
    if (aov->clientSock == INVALID_SOCKET) { delete aov; return; }

    m_pendingAccepts.fetch_add(1, std::memory_order_relaxed);  // balanced in workerThread
    DWORD recvd = 0;
    BOOL ok = m_fnAcceptEx(m_listen, aov->clientSock,
                           aov->addrbuf, 0,
                           sizeof(SOCKADDR_IN) + 16,
                           sizeof(SOCKADDR_IN) + 16,
                           &recvd, &aov->ov);
    // ERROR_IO_PENDING is the normal case: the completion arrives via IOCP when a client
    // connects. Any other immediate failure produces NO completion, so reclaim the socket,
    // the AcceptOv, and the pending-accept count here.
    if (!ok && WSAGetLastError() != ERROR_IO_PENDING) {
        m_pendingAccepts.fetch_sub(1, std::memory_order_relaxed);
        closesocket(aov->clientSock);
        delete aov;
    }
}

void IocpServer::workerThread() {
    while (true) {
        DWORD      bytesXfr  = 0;
        ULONG_PTR  key       = 0;
        OVERLAPPED* ov       = nullptr;

        BOOL ok = GetQueuedCompletionStatus(m_iocp, &bytesXfr, &key, &ov, INFINITE);

        if (key == SHUTDOWN_KEY) break;

        // No OVERLAPPED means no operation completed. With an INFINITE wait the only
        // way that happens is the completion port itself going away, and `continue`
        // would then spin on a dead handle burning a core. The one legitimate
        // null-OVERLAPPED wake-up is our own shutdown post, caught by the key above.
        if (!ov) {
            if (!ok) break;
            continue;
        }

        // Determine operation type from the IoType field embedded right after OVERLAPPED
        auto* base = reinterpret_cast<IoType*>(
            reinterpret_cast<char*>(ov) + sizeof(OVERLAPPED));
        IoType opType = *base;

        if (opType == IoType::Accept) {
            auto* aov = reinterpret_cast<AcceptOv*>(ov);
            if (ok)
                handleAccept(aov, bytesXfr);
            else
                closesocket(aov->clientSock);
            delete aov;
            // This accept operation is done: drop the pending-accept count and wake a
            // waiting stop() if it was the last one, then re-arm the pool while running.
            if (m_pendingAccepts.fetch_sub(1, std::memory_order_acq_rel) == 1)
                m_connsCv.notify_all();
            // Keep the accept pool at PENDING_ACCEPTS: replace the slot that just completed
            // AND recover any slot a previous postAccept() dropped to a transient socket
            // failure (otherwise a few scattered failures shrink the pool toward zero).
            // Bounded to PENDING_ACCEPTS attempts so a persistent failure cannot spin.
            for (int i = 0; m_running &&
                            i < PENDING_ACCEPTS &&
                            m_pendingAccepts.load(std::memory_order_relaxed) < PENDING_ACCEPTS;
                 ++i)
                postAccept();
            continue;
        }

        // Data operation: key == ConnCtx*. Exactly one completion per posted op, so
        // we release() once here no matter which branch runs — that balances the
        // addRef() the post did and is what eventually frees the ctx.
        auto* ctx = reinterpret_cast<ConnCtx*>(key);

        {
            // Serialise callbacks for THIS connection. IOCP hands completions to any
            // worker with no per-handle ordering, so a recv-data completion (onData)
            // and a concurrent send/error completion (which can reach onClose) for the
            // same ctx must not run on two workers at once. Held only around the
            // dispatch: markDead()'s release of the "alive" ref cannot free the ctx here
            // because this completion's own ref is still held, so the free happens at
            // the release() below -- after the lock is already gone.
            std::lock_guard<std::mutex> cbLock(ctx->cb);
            ctx->dispatching.store(true, std::memory_order_release);
            struct DispatchGuard {
                ConnCtx* c;
                ~DispatchGuard() { c->dispatching.store(false, std::memory_order_release); }
            } dispatchGuard{ctx};

            if (!ok || bytesXfr == 0)
                markDead(ctx);                  // closed or error: tear down (idempotent)
            else if (ctx->dead.load(std::memory_order_acquire)) {
                // A prior completion already tore this ctx down. Drop the payload rather
                // than deliver onData after onClose (or re-arm a closed socket); the
                // op-ref is still released below.
            }
            else if (opType == IoType::Recv)
                handleRecv(ctx, bytesXfr);
            else
                handleSend(ctx, bytesXfr);
        }

        ctx->release();
    }
}

void IocpServer::handleAccept(AcceptOv* aov, DWORD /*bytes*/) {
    // Inherit listen socket options (required after AcceptEx). Read m_listen under its
    // mutex, via a snapshot, so it cannot race stop()'s closesocket(m_listen). If the
    // listener has already closed we are shutting down -- drop this accepted socket.
    SOCKET listenSnapshot;
    {
        std::lock_guard<std::mutex> lk(m_listenMu);
        listenSnapshot = m_listen;
    }
    if (listenSnapshot == INVALID_SOCKET) {
        closesocket(aov->clientSock);
        return;
    }
    setsockopt(aov->clientSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
               reinterpret_cast<char*>(&listenSnapshot), sizeof(listenSnapshot));

    auto* ctx  = new ConnCtx(m_factory);
    ctx->sock  = aov->clientSock;
    ctx->owner = this;
    ctx->addRef();  // accept-handler reference: keeps the ctx alive for the whole of this
                    // function even if a send/error completion for it is dispatched on
                    // another worker; released at the very end, after the callback section.

    // Associate the accepted socket with the completion port; key = ctx. If this fails,
    // no completion for the socket can ever arrive, so the ctx would never drain -- tear
    // it down here (it was never inserted into m_conns) rather than publish it.
    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(ctx->sock), m_iocp,
                                reinterpret_cast<ULONG_PTR>(ctx), 0)) {
        ctx->close();
        ctx->release();   // alive ref
        ctx->release();   // accept-handler ref -> frees
        return;
    }

    // Adopt the connection ONLY while still running, under m_connsMu so the decision is
    // atomic against stop()'s scan: either this insert precedes the scan (and stop() then
    // closes the connection) or, once shutting down, we reject it here rather than strand
    // a ctx that stop() has already scanned past.
    bool rejected = false;
    {
        std::lock_guard<std::mutex> lock(m_connsMu);
        if (m_running.load(std::memory_order_acquire))
            m_conns.insert(ctx);
        else
            rejected = true;   // shutting down: do not adopt
    }
    if (rejected) {
        // Tear the rejected connection down OUTSIDE m_connsMu: the accept-handler release
        // drops the last ref and re-enters removeAndDelete(), which locks m_connsMu -- doing
        // it under the lock would self-deadlock (the mutex is not recursive) and hang stop().
        ctx->close();
        ctx->release();   // alive ref
        ctx->release();   // accept-handler ref -> frees (m_conns.erase is a no-op; never inserted)
        return;
    }

    // Resolve the peer address and hand it to the session before onConnect, so protocols
    // that key on the client IP (bans, IP locking) have it available. No overlapped op is
    // posted for this ctx yet, so no completion -- hence no concurrent callback -- can be
    // in flight until onConnect() below.
    if (m_fnGetSockaddrs) {
        SOCKADDR* localAddr  = nullptr;
        SOCKADDR* remoteAddr = nullptr;
        int localLen = 0, remoteLen = 0;
        m_fnGetSockaddrs(aov->addrbuf, 0,
                         sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
                         &localAddr, &localLen, &remoteAddr, &remoteLen);
        if (remoteAddr && remoteAddr->sa_family == AF_INET) {
            char ip[INET_ADDRSTRLEN] = {};
            auto* sin = reinterpret_cast<SOCKADDR_IN*>(remoteAddr);
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
            ctx->session->setPeerAddress(ip);
        }
    }

    // Arm the outbound channel before onConnect so the session can send from any
    // thread (and safely register itself with a world loop) right away.
    ctx->channel = std::make_shared<SendChannel>();
    ctx->channel->ctx = ctx;
    ctx->session->setSender(
        [ch = ctx->channel](const uint8_t* d, size_t n) { ch->post(d, n); });
    ctx->session->setCloser([ch = ctx->channel] { ch->requestClose(); });
    ctx->session->setFlowControl(
        std::shared_ptr<net::FlowControl>(ctx->channel, &ctx->channel->out.gate()));

    {
        // From onConnect() on, the session may register with a world loop and the greeting
        // send posts the first overlapped op -- so a completion (and its onClose) can now
        // race this handler. Run the callbacks under the same per-ctx lock the worker
        // dispatch uses, and skip if a teardown already won.
        std::lock_guard<std::mutex> cbLock(ctx->cb);
        ctx->dispatching.store(true, std::memory_order_release);
        if (!ctx->dead.load(std::memory_order_acquire)) {
            // Server-initiated greeting (e.g. world server's SMSG_AUTH_CHALLENGE).
            auto greeting = ctx->session->onConnect();
            if (!greeting.empty())
                ctx->enqueue(greeting.data(), greeting.size());

            ctx->dispatching.store(false, std::memory_order_release);
            ctx->closeIfDrained();

            if (ctx->session->closed() && ctx->channel->out.empty())
                markDead(ctx);
            else if (!postRecv(ctx))
                markDead(ctx);
        }
        ctx->dispatching.store(false, std::memory_order_release);
    }

    ctx->release();  // drop the accept-handler reference
}

bool IocpServer::postRecv(ConnCtx* ctx) {
    bool failed = false;
    {
        std::lock_guard<std::mutex> sk(ctx->sockMu);  // no concurrent Winsock call on `sock`
        if (ctx->sock == INVALID_SOCKET)
            return false;                              // closed under us; never addRef'd
        ZeroMemory(&ctx->recvOv.ov, sizeof(OVERLAPPED));
        ctx->recvOv.wsabuf.buf = ctx->recvOv.buf;
        ctx->recvOv.wsabuf.len = sizeof(ctx->recvOv.buf);
        ctx->recvOv.flags      = 0;
        ctx->addRef();  // the completion of this recv will release()
        if (WSARecv(ctx->sock, &ctx->recvOv.wsabuf, 1,
                    nullptr, &ctx->recvOv.flags, &ctx->recvOv.ov, nullptr) == SOCKET_ERROR
            && WSAGetLastError() != WSA_IO_PENDING) {
            failed = true;
        }
    }
    if (failed) {
        ctx->release();  // synchronous failure: no completion. Released outside sockMu so
                         // the last release's removeAndDelete()->m_connsMu cannot invert
                         // against stop()'s m_connsMu->sockMu.
        return false;
    }
    return true;
}

void IocpServer::handleRecv(ConnCtx* ctx, DWORD bytes) {
    auto response = ctx->session->onData(
        reinterpret_cast<const uint8_t*>(ctx->recvOv.buf), bytes);

    if (!response.empty())
        ctx->enqueue(response.data(), response.size());

    ctx->closeIfDrained();

    // A session that asks to close having just queued its final bytes (an auth
    // rejection, say) must still get them out, so only tear down once the outbound
    // buffer has actually drained. Otherwise keep a recv posted: it guarantees a
    // completion will arrive to carry the teardown even if the peer goes quiet.
    if (ctx->session->closed() && ctx->channel->out.empty()) {
        markDead(ctx);
        return;
    }

    if (!postRecv(ctx))
        markDead(ctx);
}

void IocpServer::handleSend(ConnCtx* ctx, DWORD bytes) {
    ctx->onSendComplete(bytes);
    ctx->closeIfDrained();
    // Only tear down once the session's remaining output has actually drained —
    // otherwise a session that closes right after queueing its last packet (e.g. an
    // auth rejection followed by a disconnect) loses those bytes.
    if (ctx->session->closed() && ctx->channel->out.empty())
        markDead(ctx);
}

void IocpServer::markDead(ConnCtx* ctx) {
    // Idempotent: recv-error, send-error, and a world-thread requestClose can all
    // race here, but only the first does the teardown and drops the "alive" ref.
    if (ctx->dead.exchange(true))
        return;

    // Disarm the outbound channel first: after this, any world thread still holding
    // the Sender no-ops instead of touching the ctx. Then let the session detach from
    // its world loop. The session object may outlive this ctx (the world loop holds a
    // shared_ptr) — that's fine, it just can't send anymore.
    if (ctx->channel)
        ctx->channel->disarm();
    if (ctx->session)
        ctx->session->onClose();

    ctx->close();  // forces any still-pending recv/send to complete (with error)
    // The ctx is removed from m_conns in release()/removeAndDelete when its last ref drops
    // -- not here -- so an empty m_conns means every overlapped op has actually drained.
    ctx->release();  // drop the initial "alive" reference; frees when ops have drained
}

} // namespace net

#endif // _WIN32
