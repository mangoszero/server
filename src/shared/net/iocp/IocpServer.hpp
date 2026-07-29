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
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace net {

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

// The worker recovers the op type from a bare OVERLAPPED* by reading the byte at
// sizeof(OVERLAPPED) past it. That is only valid while the compiler puts `type`
// exactly there, which holds today (OVERLAPPED aligns to a pointer, IoType is a
// uint8_t) but is silent if a member is ever inserted or the tag is widened.
// Assert it instead of trusting it: a layout change becomes a compile error rather
// than completions being dispatched to the wrong handler at runtime.
static_assert(offsetof(AcceptOv, type) == sizeof(OVERLAPPED), "AcceptOv layout");
static_assert(offsetof(RecvOv,   type) == sizeof(OVERLAPPED), "RecvOv layout");
static_assert(offsetof(SendOv,   type) == sizeof(OVERLAPPED), "SendOv layout");

// ── Per-connection context ────────────────────────────────────────────────────
struct ConnCtx;
class IocpServer;

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
    SendQueue  out;                        // coalescing buffer + byte backpressure

    // A close asked for by the session, honoured only once its bytes are out. The
    // flag lives on the CHANNEL rather than the ctx because the channel is what a
    // world thread holds, and it outlives the ctx by design.
    bool closeRequested = false;

    void post(const uint8_t* data, size_t len);  // append + kick a write while armed
    void requestClose();                   // drain, then close
    void disarm();                         // detach from the ctx, forever
};

struct ConnCtx {
    SOCKET   sock{INVALID_SOCKET};
    RecvOv   recvOv;
    SendOv   sendOv;
    std::shared_ptr<ISession>    session;
    std::shared_ptr<SendChannel> channel;

    // Lifetime: the ConnCtx must outlive every overlapped op posted on it, because
    // their completions arrive (keyed by this pointer) on an IOCP worker possibly
    // long after the socket closed. We refcount: the ctx starts with one "alive"
    // reference, every posted recv/send adds one, every completion (and the single
    // teardown) releases one, and the release that hits zero frees it. `dead` makes
    // teardown idempotent across the recv/send/close paths that can all race to it.
    std::atomic<long> refs{1};
    std::atomic<bool> dead{false};

    // Set once the FIN has been sent, so the half-close is not repeated.
    bool sendShutdown = false;

    // True while a worker is inside this connection's session callbacks.
    //
    // A session closes by setting closed() and calling the closer FROM INSIDE onData,
    // before it has returned the bytes that explain the disconnect. Acting on the
    // request there would find an empty queue and shut the send side down a moment
    // before those bytes are queued -- which is precisely the data loss this is meant
    // to prevent. So the request is only recorded while dispatching; the dispatcher
    // honours it once the response is in the queue.
    std::atomic<bool> dispatching{false};

    // Serialises this connection's session callbacks. IOCP places no ordering on the
    // completions for one handle, so without this a recv-data completion (onData) and
    // a concurrent send- or error-completion (which can reach onClose) for the same
    // ctx could run on two workers at once -- a data race on the session's state and a
    // teardown racing a live delivery. The reactor/io_uring backends inherit this from
    // their single I/O thread; here the worker takes it around the dispatch, never
    // across the release() that may free the ctx.
    std::mutex cb;

    // Serialises the Winsock calls on `sock` (WSARecv/WSASend/closesocket). Winsock
    // forbids concurrent calls on one socket, and shutdown closes sockets while workers
    // may still be posting I/O -- this makes every such call mutually exclusive and lets
    // close() run idempotently from teardown, a world-thread requestClose, and stop().
    std::mutex sockMu;

    // The owning server. When the last ref drops, release() hands the ctx to
    // owner->removeAndDelete(), which erases it from m_conns under m_connsMu and only then
    // deletes it. A stop() scan holds m_connsMu and touches only ctxs still in the set, and
    // each is erased before it is deleted, so the scan never sees freed memory -- which is
    // what makes an empty m_conns true I/O quiescence. Set once in handleAccept().
    IocpServer* owner{nullptr};

    explicit ConnCtx(const SessionFactory& factory) : session(factory()) {}

    // True once the session's outbound bytes have all reached the socket, so a
    // requested close can be honoured. Checked wherever a send or recv completes.
    bool drained() const;

    // Closes if a close was asked for and nothing is left to send. Returns true when
    // it closed. The half-close is a shutdown(SD_SEND), not a closesocket: the peer
    // gets a FIN and reads the tail, instead of losing it to a reset.
    bool closeIfDrained();

    void addRef()  { refs.fetch_add(1, std::memory_order_relaxed); }
    // Out of line: the last release routes through owner->removeAndDelete(), which needs
    // the (here still-incomplete) IocpServer definition.
    void release();

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
    HANDLE   m_iocp{nullptr};
    SOCKET   m_listen{INVALID_SOCKET};
    std::mutex m_listenMu;   // serialises AcceptEx / SO_UPDATE_ACCEPT_CONTEXT / close on m_listen
    SessionFactory m_factory;

    LPFN_ACCEPTEX               m_fnAcceptEx{nullptr};
    LPFN_GETACCEPTEXSOCKADDRS   m_fnGetSockaddrs{nullptr};

    std::vector<std::thread>    m_workers;
    std::atomic<bool>           m_running{false};
    bool                        m_wsaStarted{false};   // owns one WSAStartup ref

    static constexpr int PENDING_ACCEPTS = 4;
    static constexpr ULONG_PTR SHUTDOWN_KEY = ~(ULONG_PTR)0;

    // Tracks live connections. A ConnCtx is inserted in handleAccept() and removed by
    // removeAndDelete() (from ConnCtx::release()) only when its last reference drops, so an
    // empty m_conns means every connection's overlapped I/O has completed -- the true
    // quiescence that stop() waits on before it joins the workers and tears the port down.
    std::mutex                             m_connsMu;
    std::condition_variable                m_connsCv;   // signalled when a ConnCtx is freed
    std::unordered_set<ConnCtx*>           m_conns;

    // Posted-but-not-yet-completed AcceptEx operations. m_conns only tracks adopted
    // connections, so shutdown also waits for this to reach 0 before joining the workers,
    // otherwise a still-pending (or cancelled-but-undequeued) accept leaks its AcceptOv.
    std::atomic<int>                       m_pendingAccepts{0};

    friend struct ConnCtx;                      // ConnCtx::release() calls removeAndDelete
    void removeAndDelete(ConnCtx* ctx);         // erase from m_conns (locked) + notify + delete

    void workerThread();
    void postAccept();
    bool postRecv  (ConnCtx* ctx);              // refs++ on success
    void handleAccept(AcceptOv* aov, DWORD bytes);
    void handleRecv (ConnCtx* ctx, DWORD bytes);
    void handleSend (ConnCtx* ctx, DWORD bytes);// `bytes` MUST be honoured (short writes)
    void markDead   (ConnCtx* ctx);             // idempotent teardown; releases alive ref
};

} // namespace net

#endif // _WIN32
