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
#include "BoundedQueue.h"

#include <ace/Reactor.h>
#include <ace/Event_Handler.h>
#include <atomic>
#include <deque>
#include <mutex>

class IpcServerHandler;
class IpcClientHandler;

/**
 * @brief Capacity of the outbound (caller-thread -> reactor) queue per side.
 */
static const size_t IPC_OUTBOUND_QUEUE_CAP = 256;

/**
 * @brief Thread-safe coupling object between a facade (caller thread) and its
 *        reactor thread / live handler.
 *
 * The whole point of this object is that mangosd's world thread calls
 * SendFrame/Connected on the facade while a separate reactor thread runs the
 * socket handler. The previous design handed a raw handler pointer across that
 * boundary, which is a use-after-free + data race once cross-thread use begins.
 *
 * Concurrency contract:
 *   - @ref outbound and @ref live are the ONLY lock-free members the caller
 *     thread may touch. @ref outbound is internally synchronised (BoundedQueue)
 *     and @ref live is a std::atomic - both are safe to read/write from any
 *     thread.
 *   - @ref handler is owned by, and may only be read/written from, the REACTOR
 *     thread. The caller thread never dereferences it.
 *   - @ref reactor and @ref notifier form the notify target. The caller thread
 *     reads them (to call ACE_Reactor::notify) ONLY while holding
 *     @ref m_notifyMtx; teardown nulls them ONLY while holding the same mutex,
 *     and only AFTER the reactor thread has been joined. This serialises the
 *     caller's notify() against destruction of the reactor/notifier objects,
 *     closing the teardown TOCTOU/use-after-free. See @ref m_notifyMtx.
 *
 * Send path (caller thread): encode into @ref outbound, then take
 * @ref m_notifyMtx and, if @ref reactor is non-null, poke the reactor via
 * ACE_Reactor::notify(<the side's notifier>). The notifier's
 * handle_exception() runs on the reactor thread, where it drains @ref outbound
 * and performs the actual send through @ref handler.
 *
 * IMPORTANT: the reactor-thread drain (IpcOutboundNotifier::handle_exception)
 * must NEVER take @ref m_notifyMtx. The drain runs on the reactor thread BEFORE
 * the teardown join completes; taking the mutex there while teardown waits on
 * the join would deadlock. The drain only needs @ref outbound and @ref handler,
 * both of which it already uses.
 *
 * Lifetime: the link is heap-allocated and reference-counted (one ref held by
 * the facade, one by the reactor thread). It outlives both the handler and the
 * reactor so the reactor-thread notifier can always touch it safely.
 */
template<class HandlerT>
struct IpcLink
{
    IpcLink()
        : outbound(IPC_OUTBOUND_QUEUE_CAP),
          live(false),
          runId(0),
          writeAuthority(0),
          handlerActive(false),
          handler(nullptr),
          reactor(nullptr),
          notifier(nullptr),
          refCount(0)
    {
    }

    // --- caller-thread-safe members ---

    /// Frames enqueued by the caller thread, drained on the reactor thread.
    BoundedQueue<IpcMessage> outbound;

    /// Published true by the reactor thread on handshake completion, cleared on
    /// handle_close(). Read by the caller thread via Connected().
    std::atomic<bool> live;

    /// Per-spawn run-id received in IPC_HELLO_ACK (set by handler, read by
    /// IpcClient::RunId()). Zero until the handshake completes.
    std::atomic<uint32> runId;

    /// [SP-2] Write-authority bit received in IPC_HELLO_ACK (0 until then).
    std::atomic<uint8> writeAuthority;

    /// [SP-2 decision 10] UNBOUNDED reliable inbound lane for mutation-class
    /// frames (IpcIsReliableOpcode) that must never be dropped under inbound
    /// pressure - they carry money/item value or their outcome. The reactor
    /// thread pushes (PushReliable); the caller/facade thread drains
    /// (PopReliable) to EXHAUSTION before the bounded inbound queue each pass,
    /// so a browse flood on the bounded queue can neither drop nor starve a
    /// value-bearing frame. Genuinely unbounded (no drop policy): the frame
    /// rate is naturally bounded (one per player action / worker resolve) and
    /// each accepted frame already passed the per-opcode size validation. Its
    /// own mutex, independent of the notify/outbound paths.
    std::deque<IpcMessage> reliableInbound;
    std::mutex             reliableMx;

    /// Push a reliable frame (reactor thread). Never drops.
    void PushReliable(const IpcMessage& m)
    {
        std::lock_guard<std::mutex> g(reliableMx);
        reliableInbound.push_back(m);
    }

    /// Pop the oldest reliable frame (caller thread). False when empty.
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

    /// Discard every queued reliable frame; returns the count discarded. Used
    /// by the supervisor purge on child death/respawn (mirrors ClearInbound).
    size_t ClearReliable()
    {
        std::lock_guard<std::mutex> g(reliableMx);
        const size_t removed = reliableInbound.size();
        reliableInbound.clear();
        return removed;
    }

    /// Approximate number of queued reliable frames.
    size_t ReliableSize()
    {
        std::lock_guard<std::mutex> g(reliableMx);
        return reliableInbound.size();
    }

    /// Single-owner guard. Set true (test-and-set) by the FIRST handler that
    /// opens on the reactor thread; any additional accepted connection finds
    /// it already set and refuses itself WITHOUT touching the live handler,
    /// reactor, or routing state. Cleared by the owning handler's
    /// handle_close(). Only the loopback child should ever connect; a second
    /// concurrent local connection is hostile and must not be able to steal
    /// outbound routing by overwriting @ref handler. Atomic because the
    /// test-and-set is the ownership decision; in practice all touches are on
    /// the reactor thread, but the atomic makes the intent explicit and is
    /// free on this path.
    std::atomic<bool> handlerActive;

    // --- reactor-thread-only members ---

    /// Live handler pointer. Set on the reactor thread when the handler goes
    /// live; cleared on the reactor thread in handle_close(). NEVER read off
    /// the reactor thread.
    HandlerT* handler;

    /// The reactor owning the handler. Set once by the reactor thread before it
    /// enters run_reactor_event_loop(); read by the caller thread ONLY to call
    /// ACE_Reactor::notify(), which is documented thread-safe.
    ///
    /// This pointer is the publication point: it is stored LAST (after
    /// @ref notifier, with release ordering) on the reactor thread, and
    /// Connected()/SendFrame() on the caller thread load it with acquire
    /// ordering and treat null as "not ready". A non-null acquire-load
    /// therefore guarantees @ref notifier is also visible.
    ///
    /// TEARDOWN: the caller thread's SendFrame() additionally reads this under
    /// @ref m_notifyMtx and only calls notify() while the mutex is held, so
    /// the notify cannot race the reactor/notifier objects being destroyed.
    std::atomic<ACE_Reactor*> reactor;

    /// The reactor-thread notifier whose handle_exception() drains @ref
    /// outbound. Set before @ref reactor is published; passed to
    /// ACE_Reactor::notify() by the caller thread (under @ref m_notifyMtx).
    /// Lifetime owned by the reactor thread (the IpcThread / IpcClientThread).
    ACE_Event_Handler* notifier;

    /// Guards ONLY the (@ref reactor, @ref notifier) validity for the caller
    /// thread's notify() call. SendFrame() takes this around the notify;
    /// teardown takes it to null @ref reactor + @ref notifier AFTER joining the
    /// reactor thread and BEFORE destroying those objects. The reactor-thread
    /// drain path must NEVER take this mutex (see the class note above) - it
    /// would deadlock against the teardown join.
    std::mutex m_notifyMtx;

    // --- reference counting (simple, mutex-free via atomic) ---

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

typedef IpcLink<IpcServerHandler> IpcServerLink;
typedef IpcLink<IpcClientHandler> IpcClientLink;

#endif // AH_IPC_LINK_H
