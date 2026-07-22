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

namespace net {

// ── SendChannel ───────────────────────────────────────────────────────────────

void SendChannel::post(const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(mu);
    if (ctx && !closeRequested)
        ctx->enqueue(data, len);
}

void SendChannel::requestClose() {
    std::lock_guard<std::mutex> lock(mu);
    if (!ctx || closeRequested)
        return;

    closeRequested = true;
    if (!ctx->owner->postControl(ctx))
        ctx->close();
}

void SendChannel::disarm() {
    std::lock_guard<std::mutex> lock(mu);
    closeRequested = true;
    ctx = nullptr;
    out.close();  // release any bulk producer parked on backpressure
}

// ── ConnCtx ───────────────────────────────────────────────────────────────────

bool ConnCtx::postSend(const uint8_t* data, size_t len) {
    IocpServer* server = owner;
    if (!server || !server->m_operations.tryBegin())
        return false;

    ZeroMemory(&sendOv.ov, sizeof(OVERLAPPED));
    // Safe to hand the kernel a pointer into the SendQueue's in-flight buffer: only
    // the pending buffer is ever appended to, so this storage cannot move or be
    // reallocated before the completion arrives.
    sendOv.wsabuf.buf = reinterpret_cast<char*>(const_cast<uint8_t*>(data));
    sendOv.wsabuf.len = static_cast<ULONG>(len);
    addRef();  // the completion of this send will release()
    int rc = WSASend(sock, &sendOv.wsabuf, 1, nullptr, 0, &sendOv.ov, nullptr);
    if (rc == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        release();  // no completion will arrive for a synchronous failure
        server->m_operations.complete();
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

void ConnCtx::close() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

// ── IocpServer ────────────────────────────────────────────────────────────────

IocpServer::~IocpServer() { stop(); }

bool IocpServer::start(uint16_t port, SessionFactory factory,
                       const std::string& bindIp) {
    if (m_running.load() || !m_operations.startSubmissions())
        return false;

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

    // Resolve BindIP before touching the socket so an invalid option fails the
    // bind outright instead of silently listening on every interface.
    uint32_t bindAddr = htonl(INADDR_ANY);
    if (!ResolveBindAddress(bindIp, bindAddr)) return false;

    m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!m_iocp) return false;

    m_listen = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                          nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (m_listen == INVALID_SOCKET) return false;

    SOCKADDR_IN addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = bindAddr;
    addr.sin_port        = htons(port);

    if (bind(m_listen, reinterpret_cast<SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        return false;
    if (listen(m_listen, SOMAXCONN) == SOCKET_ERROR)
        return false;

    // Associate listen socket with IOCP (key=0, used only for AcceptEx completions)
    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(m_listen), m_iocp, 0, 0))
        return false;

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

    if (!m_fnAcceptEx) return false;

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
    if (m_running.load()) {
        // Close the submission gate while workers are still alive. Every operation
        // accepted before this point is counted and must produce one completion.
        m_operations.stopSubmissions();

        if (m_running.exchange(false)) {
            // Cancels the outstanding AcceptEx operations. Their completions are
            // drained by the workers just like ordinary accepts.
            if (m_listen != INVALID_SOCKET) {
                closesocket(m_listen);
                m_listen = INVALID_SOCKET;
            }

            // Hold a temporary reference while taking each connection through the
            // normal idempotent teardown. closesocket then completes pending I/O.
            std::vector<ConnCtx*> connections;
            {
                std::lock_guard lock(m_connsMu);
                connections.reserve(m_conns.size());
                for (auto* ctx : m_conns) {
                    ctx->addRef();
                    connections.push_back(ctx);
                }
            }
            for (auto* ctx : connections) {
                markDead(ctx);
                ctx->release();
            }

            // Do not stop the workers or close the completion port until all kernel
            // operations have completed and released their ConnCtx references.
            m_operations.waitForZero();

            for (size_t i = 0; i < m_workers.size(); ++i)
                PostQueuedCompletionStatus(m_iocp, 0, SHUTDOWN_KEY, nullptr);
            for (auto& t : m_workers)
                t.join();
            m_workers.clear();

            if (m_iocp) {
                CloseHandle(m_iocp);
                m_iocp = nullptr;
            }
        }
    }

    // Balance the WSAStartup from start(). Guarded so a failed start (which may
    // return before m_running is set) still releases the reference exactly once.
    if (m_wsaStarted) { WSACleanup(); m_wsaStarted = false; }
}

void IocpServer::postAccept() {
    if (!m_operations.tryBegin())
        return;

    auto* aov = new AcceptOv{};
    aov->clientSock = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                 nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (aov->clientSock == INVALID_SOCKET) {
        delete aov;
        m_operations.complete();
        return;
    }

    DWORD recvd = 0;
    BOOL accepted = m_fnAcceptEx(m_listen, aov->clientSock,
                                 aov->addrbuf, 0,
                                 sizeof(SOCKADDR_IN) + 16,
                                 sizeof(SOCKADDR_IN) + 16,
                                 &recvd, &aov->ov);
    if (!accepted && WSAGetLastError() != ERROR_IO_PENDING) {
        closesocket(aov->clientSock);
        delete aov;
        m_operations.complete();
        return;
    }
    // A successfully submitted accept completes through IOCP when a client
    // connects or when shutdown cancels the listener.
}

void IocpServer::workerThread() {
    while (true) {
        DWORD      bytesXfr  = 0;
        ULONG_PTR  key       = 0;
        OVERLAPPED* ov       = nullptr;

        BOOL ok = GetQueuedCompletionStatus(m_iocp, &bytesXfr, &key, &ov, INFINITE);

        if (key == SHUTDOWN_KEY) break;

        if (!ov) continue; // timeout or spurious

        // Control completions use the connection's dedicated OVERLAPPED. Identify
        // them by address before decoding the type tag used by kernel I/O operations.
        auto* ctx = key == 0 ? nullptr : reinterpret_cast<ConnCtx*>(key);
        if (ctx && ov == &ctx->closeOv) {
            handleControl(ctx);
            ctx->release();
            m_operations.complete();
            continue;
        }

        // Determine operation type from the IoType field embedded right after OVERLAPPED
        auto* base = reinterpret_cast<IoType*>(
            reinterpret_cast<char*>(ov) + sizeof(OVERLAPPED));
        IoType opType = *base;

        if (opType == IoType::Accept) {
            auto* aov = reinterpret_cast<AcceptOv*>(ov);
            if (ok && m_running.load())
                handleAccept(aov, bytesXfr);
            else
                closesocket(aov->clientSock);
            delete aov;
            m_operations.complete();
            if (m_running) postAccept(); // always maintain PENDING_ACCEPTS
            continue;
        }

        // Data operation: key == ConnCtx*. Exactly one completion per posted op, so
        // we release() once here no matter which branch runs — that balances the
        // addRef() the post did and is what eventually frees the ctx.
        if (!ok || bytesXfr == 0)
            markDead(ctx);                      // closed or error: tear down (idempotent)
        else if (opType == IoType::Recv)
            handleRecv(ctx, bytesXfr);
        else
            handleSend(ctx, bytesXfr);

        ctx->release();
        m_operations.complete();
    }
}

void IocpServer::handleAccept(AcceptOv* aov, DWORD /*bytes*/) {
    // Inherit listen socket options (required after AcceptEx)
    setsockopt(aov->clientSock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
               reinterpret_cast<char*>(&m_listen), sizeof(m_listen));

    auto* ctx = new ConnCtx(m_factory, this);
    ctx->sock = aov->clientSock;

    // Associate new socket with IOCP; key = ctx pointer
    CreateIoCompletionPort(reinterpret_cast<HANDLE>(ctx->sock), m_iocp,
                           reinterpret_cast<ULONG_PTR>(ctx), 0);

    {
        std::lock_guard lock(m_connsMu);
        m_conns.insert(ctx);
    }

    // Resolve the peer address and hand it to the session before onConnect, so
    // protocols that key on the client IP (bans, IP locking) have it available.
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

    // Server-initiated greeting (e.g. world server's SMSG_AUTH_CHALLENGE).
    auto greeting = ctx->session->onConnect();
    if (!greeting.empty())
        ctx->enqueue(greeting.data(), greeting.size());

    if (closeIfDrained(ctx))
        return;

    // Post initial recv.
    if (!postRecv(ctx))
        markDead(ctx);
}

bool IocpServer::postRecv(ConnCtx* ctx) {
    if (!m_operations.tryBegin())
        return false;

    ZeroMemory(&ctx->recvOv.ov, sizeof(OVERLAPPED));
    ctx->recvOv.wsabuf.buf = ctx->recvOv.buf;
    ctx->recvOv.wsabuf.len = sizeof(ctx->recvOv.buf);
    ctx->recvOv.flags      = 0;
    ctx->addRef();  // the completion of this recv will release()
    if (WSARecv(ctx->sock, &ctx->recvOv.wsabuf, 1,
                nullptr, &ctx->recvOv.flags, &ctx->recvOv.ov, nullptr) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            ctx->release();  // synchronous failure: no completion will arrive
            m_operations.complete();
            return false;
        }
    }
    return true;
}

bool IocpServer::postControl(ConnCtx* ctx) {
    bool expected = false;
    if (!ctx->controlPending.compare_exchange_strong(expected, true))
        return true;

    if (!m_operations.tryBegin()) {
        ctx->controlPending.store(false);
        return false;
    }

    ZeroMemory(&ctx->closeOv, sizeof(OVERLAPPED));
    ctx->addRef();
    if (!PostQueuedCompletionStatus(m_iocp, 0,
            reinterpret_cast<ULONG_PTR>(ctx), &ctx->closeOv)) {
        ctx->controlPending.store(false);
        ctx->release();
        m_operations.complete();
        return false;
    }
    return true;
}

void IocpServer::handleRecv(ConnCtx* ctx, DWORD bytes) {
    auto response = ctx->session->onData(
        reinterpret_cast<const uint8_t*>(ctx->recvOv.buf), bytes);

    if (!response.empty())
        ctx->enqueue(response.data(), response.size());

    // A session that asks to close having just queued its final bytes (an auth
    // rejection, say) must still get them out, so only tear down once the outbound
    // buffer has actually drained. Otherwise keep a recv posted: it guarantees a
    // completion will arrive to carry the teardown even if the peer goes quiet.
    if (closeIfDrained(ctx))
        return;

    if (!postRecv(ctx))
        markDead(ctx);
}

void IocpServer::handleSend(ConnCtx* ctx, DWORD bytes) {
    ctx->onSendComplete(bytes);
    // Only tear down once the session's remaining output has actually drained —
    // otherwise a session that closes right after queueing its last packet (e.g. an
    // auth rejection followed by a disconnect) loses those bytes.
    closeIfDrained(ctx);
}

void IocpServer::handleControl(ConnCtx* ctx) {
    ctx->controlPending.store(false);
    closeIfDrained(ctx);
}

bool IocpServer::closeIfDrained(ConnCtx* ctx) {
    bool const sessionClosed = ctx->session && ctx->session->closed();
    bool shouldClose = false;
    {
        std::lock_guard<std::mutex> lock(ctx->channel->mu);
        if (sessionClosed)
            ctx->channel->closeRequested = true;
        shouldClose = ctx->channel->closeRequested && !ctx->channel->sendShutdown &&
                      ctx->channel->out.empty();
        if (shouldClose)
            ctx->channel->sendShutdown = true;
    }

    if (shouldClose)
    {
        // On Winsock, closing an overlapped socket directly can reset the peer even
        // after the final WSASend completion. Queue the FIN first, then perform the
        // normal idempotent teardown so clients receive all bytes followed by EOF.
        shutdown(ctx->sock, SD_SEND);
        markDead(ctx);
    }
    return shouldClose;
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
    {
        std::lock_guard lock(m_connsMu);
        m_conns.erase(ctx);
    }
    ctx->release();  // drop the initial "alive" reference; frees when ops have drained
}

} // namespace net

#endif // _WIN32
