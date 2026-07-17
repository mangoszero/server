/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

// One connection's outbound byte stream, shared by every backend. Producers append
// bytes from any thread into m_pending; the transport drains m_inflight to the socket
// and swaps the two once drained, coalescing everything queued between writes into the
// next single write. m_inflight's storage stays put for the duration of a write, so a
// proactor may hand the kernel a raw pointer into it; m_off resumes a partial write
// from where the kernel stopped.

#include "net/FlowControl.hpp"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace net {

class SendQueue {
public:
    /// Producer (any thread): copy `len` bytes into the pending buffer.
    ///
    /// Returns true iff this call took ownership of the write — that is, no write
    /// was in flight and the caller is now responsible for starting one. Proactor
    /// backends (IOCP, io_uring) use this to kick off a WSASend/SQE exactly once.
    /// The reactor ignores it: its worker owns the fd and simply flushes on the
    /// next poll wake.
    ///
    /// `data` need only stay valid for the duration of the call.
    bool append(const uint8_t* data, size_t len)
    {
        if (data == nullptr || len == 0)
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(m_mu);
        m_pending.insert(m_pending.end(), data, data + len);
        m_gate.onQueued(len);

        if (m_writing)
        {
            return false;
        }
        m_writing = true;
        return true;
    }

    /// Transport (the thread that owns the write): hand back the next contiguous
    /// span to write to the socket.
    ///
    /// If the in-flight span has been fully consumed, the pending buffer is swapped
    /// in to take its place — this is where coalescing happens. Returns false when
    /// there is nothing left to write, and in that case also releases ownership of
    /// the write, so the next append() will hand it to whoever calls next.
    ///
    /// The returned pointer stays valid until the matching consume() — producers
    /// cannot invalidate it, because they only ever touch the pending buffer.
    bool nextSpan(const uint8_t*& data, size_t& len)
    {
        std::lock_guard<std::mutex> lock(m_mu);

        if (m_off == m_inflight.size())
        {
            // Fully drained: promote whatever accumulated while we were writing.
            // swap() keeps both buffers' capacity, so this never allocates once
            // the connection has reached its steady state.
            m_inflight.swap(m_pending);
            m_pending.clear();
            m_off = 0;
        }

        if (m_inflight.empty())
        {
            m_writing = false;
            return false;
        }

        data = m_inflight.data() + m_off;
        len  = m_inflight.size() - m_off;
        return true;
    }

    /// Transport: `n` bytes of the span handed out by nextSpan() reached the socket.
    /// A short write is normal (and, on IOCP, was previously dropped on the floor);
    /// the next nextSpan() simply resumes from the new offset.
    void consume(size_t n)
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_off += n;
        m_gate.onSent(n);
    }

    /// Transport: the write could not be started (socket already gone). Releases
    /// ownership so the queue is not left permanently believing a write is running.
    void abortWrite()
    {
        std::lock_guard<std::mutex> lock(m_mu);
        m_writing = false;
    }

    /// True when nothing is queued anywhere. Used to decide whether a session that
    /// asked to close can be torn down now or must first drain.
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_off == m_inflight.size() && m_pending.empty();
    }

    /// Teardown: wake any producer parked on backpressure so it stops producing.
    void close() { m_gate.onClosed(); }

    FlowGate& gate() { return m_gate; }

private:
    mutable std::mutex   m_mu;
    std::vector<uint8_t> m_pending;        ///< producers append here
    std::vector<uint8_t> m_inflight;       ///< the socket is draining this
    size_t               m_off = 0;        ///< bytes of m_inflight already written
    bool                 m_writing = false;///< a write is in flight (proactors)
    FlowGate             m_gate;           ///< byte-counted backpressure
};

} // namespace net
