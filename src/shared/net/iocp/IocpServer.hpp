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

// Windows-only: a completion-based (proactor) server built on I/O Completion
// Ports. It deliberately does NOT implement the reactor Poller interface — IOCP
// is a different I/O model — but exposes the same start()/stop() facade so the
// rest of the program is platform-agnostic. The readiness-based backends live
// under net/reactor/. On non-Windows platforms this header collapses to nothing.
#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

#include "net/ISession.hpp"
#include "net/SendQueue.hpp"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace net {

class OutstandingOperations {
public:
    bool startSubmissions()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_count != 0)
            return false;
        m_acceptingSubmissions = true;
        return true;
    }

    bool tryBegin()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_acceptingSubmissions)
            return false;
        ++m_count;
        return true;
    }

    void stopSubmissions()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_acceptingSubmissions = false;
        if (m_count == 0)
            m_zero.notify_all();
    }

    void complete()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assert(m_count != 0);
        --m_count;
        if (m_count == 0)
            m_zero.notify_all();
    }

    void waitForZero()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_zero.wait(lock, [&] { return m_count == 0; });
    }

    std::size_t count() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_count;
    }

    bool acceptingSubmissions() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_acceptingSubmissions;
    }

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_zero;
    std::size_t m_count = 0;
    bool m_acceptingSubmissions = true;
};

// ── Per-operation type tag ────────────────────────────────────────────────────
enum class IoType : uint8_t { Accept, Recv, Send };

// Overlapped structs — OVERLAPPED must be the first member, and IoType the second,
// so the worker can recover the op type from a bare OVERLAPPED* completion.

struct AcceptOv {
    OVERLAPPED ov{};
    IoType     type{IoType::Accept};
    SOCKET     clientSock{INVALID_SOCKET};
    // Buffer holds local + remote addresses for AcceptEx
    char       addrbuf[(sizeof(SOCKADDR_IN) + 16) * 2]{};
};

struct RecvOv {
    OVERLAPPED ov{};
    IoType     type{IoType::Recv};
    WSABUF     wsabuf{};
    char       buf[8192]{};
    DWORD      flags{};
};

// No buffer of its own: a send is posted directly out of the SendQueue's in-flight
// buffer, whose storage is guaranteed not to move while the write is outstanding.
struct SendOv {
    OVERLAPPED ov{};
    IoType     type{IoType::Send};
    WSABUF     wsabuf{};
};

// ── Per-connection context ────────────────────────────────────────────────────
class IocpServer;
struct ConnCtx;

// Lifetime-safe handle the session uses to send from any thread (e.g. the world
// update thread). While armed it forwards to ConnCtx::enqueue; disarm() (called
// once on teardown, under the same lock) makes every later post() a no-op, so a
// world thread that still holds a reference can never touch a freed ConnCtx.
//
// The SendQueue lives here rather than in ConnCtx because the session holds this
// channel by shared_ptr: the outbound buffer (and the FlowGate inside it) must
// outlive the socket, so that a bulk producer parked on backpressure is woken into
// live memory rather than a freed connection.
struct SendChannel {
    std::mutex mu;
    ConnCtx*   ctx = nullptr;
    bool       closeRequested = false;
    bool       sendShutdown = false;
    SendQueue  out;                        // coalescing buffer + byte backpressure

    void post(const uint8_t* data, size_t len);  // append + kick a write while armed
    void requestClose();                   // close the socket -> triggers teardown
    void disarm();                         // detach from the ctx, forever
};

struct ConnCtx {
    SOCKET   sock{INVALID_SOCKET};
    RecvOv   recvOv;
    SendOv   sendOv;
    OVERLAPPED closeOv{};
    std::shared_ptr<ISession>    session;
    std::shared_ptr<SendChannel> channel;
    IocpServer* owner = nullptr;

    // Lifetime: the ConnCtx must outlive every overlapped op posted on it, because
    // their completions arrive (keyed by this pointer) on an IOCP worker possibly
    // long after the socket closed. We refcount: the ctx starts with one "alive"
    // reference, every posted recv/send adds one, every completion (and the single
    // teardown) releases one, and the release that hits zero frees it. `dead` makes
    // teardown idempotent across the recv/send/close paths that can all race to it.
    std::atomic<long> refs{1};
    std::atomic<bool> dead{false};
    std::atomic<bool> controlPending{false};

    ConnCtx(const SessionFactory& factory, IocpServer* server)
        : session(factory()), owner(server) {}

    void addRef()  { refs.fetch_add(1, std::memory_order_relaxed); }
    void release() { if (refs.fetch_sub(1, std::memory_order_acq_rel) == 1) delete this; }

    // Append bytes to the outbound buffer and start a write if none is in flight.
    // Thread-safe; callable from any thread.
    void enqueue(const uint8_t* data, size_t len);
    // Post the next contiguous span from the SendQueue, if any. Exactly one write is
    // ever in flight, which is what keeps the byte stream ordered.
    void startSend();
    // A WSASend completed, having transferred `bytes`. Honouring `bytes` is what makes
    // a short write safe: the remainder is re-posted instead of being dropped.
    void onSendComplete(DWORD bytes);
    // Post a WSASend of [data,len); refs++ on success, returns false if it could not
    // be started (e.g. the socket is already closed).
    bool postSend(const uint8_t* data, size_t len);
    void close();
};

// ── IOCP TCP server ───────────────────────────────────────────────────────────
class IocpServer {
public:
    IocpServer() = default;
    ~IocpServer();

    // Bind and start accepting on the given port. `factory` mints one ISession
    // per accepted connection. `bindIp` is the configured BindIP option: empty
    // (or "0.0.0.0") listens on every local interface, otherwise the listener is
    // bound to that single IPv4/hostname (see net::ResolveBindAddress).
    bool start(uint16_t port, SessionFactory factory,
               const std::string& bindIp = std::string());
    // Signal all worker threads to stop and join them.
    void stop();

private:
    friend struct SendChannel;
    friend struct ConnCtx;

    HANDLE   m_iocp{nullptr};
    SOCKET   m_listen{INVALID_SOCKET};
    SessionFactory m_factory;

    LPFN_ACCEPTEX               m_fnAcceptEx{nullptr};
    LPFN_GETACCEPTEXSOCKADDRS   m_fnGetSockaddrs{nullptr};

    std::vector<std::thread>    m_workers;
    std::atomic<bool>           m_running{false};
    OutstandingOperations       m_operations;
    bool                        m_wsaStarted{false};   // owns one WSAStartup ref

    static constexpr int PENDING_ACCEPTS = 4;
    static constexpr ULONG_PTR SHUTDOWN_KEY = ~(ULONG_PTR)0;

    // Tracks live connections for cleanup on shutdown
    std::mutex                             m_connsMu;
    std::unordered_set<ConnCtx*>           m_conns;

    void workerThread();
    void postAccept();
    bool postRecv  (ConnCtx* ctx);              // refs++ on success
    bool postControl(ConnCtx* ctx);              // refs++ on success
    void handleAccept(AcceptOv* aov, DWORD bytes);
    void handleRecv (ConnCtx* ctx, DWORD bytes);
    void handleSend (ConnCtx* ctx, DWORD bytes);// `bytes` MUST be honoured (short writes)
    void handleControl(ConnCtx* ctx);
    bool closeIfDrained(ConnCtx* ctx);
    void markDead   (ConnCtx* ctx);             // idempotent teardown; releases alive ref
};

} // namespace net

#endif // _WIN32
