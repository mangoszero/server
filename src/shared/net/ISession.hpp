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

// Protocol-agnostic per-connection contract used by every networking backend. The
// transport owns the socket and byte plumbing; a concrete ISession owns the protocol.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace net {

// Thread-safe outbound channel. The session may call send() from any thread; once the
// connection is torn down the transport disarms it and send() becomes a no-op. The
// span need only stay valid for the duration of the call.
using Sender = std::function<void(const uint8_t* data, size_t len)>;

// Lets a session ask the transport to tear the connection down. No-op once gone.
using Closer = std::function<void()>;

// Backpressure handle for bulk producers (realmd's patch stream): awaitWritable()
// blocks until this connection's outbound backlog drains to at most
// maxOutstandingBytes, or returns false once the connection is gone.
class FlowControl {
public:
    virtual ~FlowControl() = default;
    virtual bool awaitWritable(uint64_t maxOutstandingBytes) = 0;
};

class ISession : public std::enable_shared_from_this<ISession> {
public:
    virtual ~ISession() = default;

    // Hands the session the peer's remote address as a printable string (net
    // thread, once, right after accept and before onConnect). Default: ignored.
    // Protocols that need the client IP (bans, IP locking, logging) override it.
    virtual void setPeerAddress(const std::string&) {}

    // Called once, on the net thread, right after accept and before any client
    // bytes. Returns bytes to push immediately (e.g. SMSG_AUTH_CHALLENGE). This
    // is also where a session can register itself with a world loop.
    virtual std::vector<uint8_t> onConnect() { return {}; }

    // Newly received bytes (net thread). Returns bytes to send back synchronously
    // (used by the request/response auth gateway). Stateful protocols that defer
    // work to a world thread instead enqueue here and return {}. TCP is a stream,
    // so implementations must tolerate partial/coalesced packets.
    virtual std::vector<uint8_t> onData(const uint8_t* data, size_t len) = 0;

    // Hands the session its outbound channel (net thread, once, before onConnect).
    // Default: ignored (request/response sessions only ever use onData's return).
    virtual void setSender(Sender) {}

    // Hands the session a way to request its own teardown (net thread, once).
    virtual void setCloser(Closer) {}

    // Hands the session a backpressure handle for this connection (net thread,
    // once, before onConnect). Default: ignored — only bulk producers (the patch
    // stream) use it; request/response and world sessions never need to throttle.
    virtual void setFlowControl(std::shared_ptr<FlowControl>) {}

    // Called by the world loop once per tick (world thread). Default: nothing —
    // request/response sessions do all their work inline in onData.
    virtual void update() {}

    // Second phase of a world tick (world thread), after every session's update(), so
    // packets one session queued into another's buffer are delivered the same tick.
    virtual void flush() {}

    // Called by the transport when the socket is being torn down (net thread),
    // before the session is dropped. Lets the session mark itself dead so a world
    // loop stops ticking it. Default: nothing.
    virtual void onClose() {}

    // When true, the transport flushes any queued output then closes the socket.
    virtual bool closed() const = 0;

    // When true, the world loop may drop its reference (reap the session). Distinct
    // from closed(): a session can be closed() yet still need a world-thread tick to
    // unwind its game-state (leave the map, destroy itself for nearby players) before
    // it is safe to free. Defaulting to closed() keeps request/response sessions —
    // which hold no world state — reaped immediately.
    virtual bool reapable() const { return closed(); }
};

// Each accepted connection gets a fresh session from this factory. Shared
// ownership lets a world loop keep a session alive past socket teardown until it
// has finished any in-flight tick. Capture protocol state (config, key store,
// world handle, ...) in the closure; the transport stays oblivious to it.
using SessionFactory = std::function<std::shared_ptr<ISession>()>;

}  // namespace net
