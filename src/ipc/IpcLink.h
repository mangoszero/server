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

#ifndef AH_IPC_LINK_H
#define AH_IPC_LINK_H

#include "Common.h"
#include "IpcMessage.h"

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>

/**
 * @brief Thread-safe coupling object between a facade (caller thread) and its
 *        socket handler (which runs a receiver thread).
 *
 * The facade (IpcServer/IpcClient) is used by mangosd's world thread (or the
 * child's service loop) while the socket handler runs on its own thread. The
 * link couples them race-free:
 *
 *   - @ref live, @ref runId, @ref writeAuthority are std::atomics published by
 *     the handler thread and read by the caller thread.
 *   - The send target is a shared_ptr to the live handler, published under
 *     @ref sendMtx when the handshake completes and cleared under the same
 *     mutex on close. SendFrame() copies the shared_ptr under the lock and then
 *     sends outside it; the copied reference keeps the handler alive for the
 *     duration of the send even if the receiver thread closes concurrently.
 *     The handler's own SendFrame serialises the socket write with its own mutex.
 *   - The reliable lane (@ref reliableInbound) is an UNBOUNDED, never-dropped
 *     queue for mutation-class frames; the handler thread pushes, the caller
 *     thread drains it to exhaustion before the bounded inbound queue each pass.
 *
 * Lifetime: heap-allocated and reference-counted (one ref held by the facade,
 * one by the handler thread), so it outlives both.
 */
template<class HandlerT>
struct IpcLink
{
    IpcLink()
        : live(false),
          runId(0),
          writeAuthority(0),
          handlerActive(false),
          refCount(0)
    {
    }

    /// Published true by the handler thread on handshake completion, cleared on
    /// close. Read by the caller thread via Connected().
    std::atomic<bool> live;

    /// Per-spawn run-id received in IPC_HELLO_ACK (client) / to send in the ACK
    /// (server). Zero until the handshake completes.
    std::atomic<uint32> runId;

    /// [SP-2] Write-authority bit exchanged in IPC_HELLO_ACK (0 until then).
    std::atomic<uint8> writeAuthority;

    /// Single-owner guard for the server: the FIRST accepted connection claims
    /// it (test-and-set); a second concurrent local connection finds it set and
    /// refuses itself without touching the live handler. Cleared by the owner
    /// on close.
    std::atomic<bool> handlerActive;

    // --- reliable inbound lane (unbounded, never dropped) ---

    std::deque<IpcMessage> reliableInbound;
    std::mutex             reliableMx;

    void PushReliable(const IpcMessage& m)
    {
        std::lock_guard<std::mutex> g(reliableMx);
        reliableInbound.push_back(m);
    }

    bool PopReliable(IpcMessage& out)
    {
        std::lock_guard<std::mutex> g(reliableMx);
        if (reliableInbound.empty())
        {
            return false;
        }
        out = reliableInbound.front();
        reliableInbound.pop_front();
        return true;
    }

    size_t ClearReliable()
    {
        std::lock_guard<std::mutex> g(reliableMx);
        const size_t removed = reliableInbound.size();
        reliableInbound.clear();
        return removed;
    }

    size_t ReliableSize()
    {
        std::lock_guard<std::mutex> g(reliableMx);
        return reliableInbound.size();
    }

    // --- send target (guarded shared_ptr to the live handler) ---

    void SetSendTarget(const std::shared_ptr<HandlerT>& h)
    {
        std::lock_guard<std::mutex> g(sendMtx);
        sendTarget = h;
    }

    void ClearSendTarget(const HandlerT* who)
    {
        std::lock_guard<std::mutex> g(sendMtx);
        // Only the current target clears itself, so a refused/older handler
        // cannot wipe the live one.
        if (sendTarget.get() == who)
        {
            sendTarget.reset();
        }
    }

    std::shared_ptr<HandlerT> GetSendTarget()
    {
        std::lock_guard<std::mutex> g(sendMtx);
        return sendTarget;
    }

    std::mutex                sendMtx;
    std::shared_ptr<HandlerT> sendTarget;

    // --- reference counting ---

    std::atomic<int> refCount;

    void AddRef() { refCount.fetch_add(1, std::memory_order_relaxed); }

    void Release()
    {
        if (refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }
};

class IpcServerHandler;
class IpcClientHandler;

typedef IpcLink<IpcServerHandler> IpcServerLink;
typedef IpcLink<IpcClientHandler> IpcClientLink;

#endif // AH_IPC_LINK_H
