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
#include <cstdint>
#include <memory>

namespace net {

// Interest / readiness flags. Combined with bitwise OR.
enum EventMask : uint32_t {
    EvNone  = 0,
    EvRead  = 1u << 0,
    EvWrite = 1u << 1,
};

// One ready connection, as reported by Poller::wait(). A connection appears at
// most once per wait() batch even on backends that report read/write
// separately (kqueue) — the Poller coalesces them, so flags may carry both
// EvRead and EvWrite. `udata` is the opaque pointer handed to add()/mod().
struct PollerEvent {
    void*    udata = nullptr;
    uint32_t flags = EvNone;
    bool     eof   = false; // best-effort peer-hangup/error hint
};

// ── Readiness-based demultiplexer ─────────────────────────────────────────────
// The single OS-specific seam of the reactor server. epoll / kqueue / poll each
// implement this; ReactorServer holds the connection, send-queue and Session
// logic and never names a platform API.
//
// Threading: every method is called from the one thread that owns the Poller,
// EXCEPT wake(), which is thread-safe and used by another thread to unblock a
// wait() in progress.
class Poller {
public:
    virtual ~Poller() = default;

    // Create the underlying OS handle and its wakeup source. False on failure.
    virtual bool init() = 0;

    // Register / change / remove interest in a file descriptor. `udata` is
    // echoed back in the matching PollerEvent.
    virtual bool add(int fd, uint32_t interest, void* udata) = 0;
    virtual bool mod(int fd, uint32_t interest, void* udata) = 0;
    virtual bool del(int fd) = 0;

    // Block until at least one fd is ready or wake() is called, then fill up to
    // `maxEvents` entries. Returns the event count (0 is valid — e.g. a bare
    // wakeup, which is consumed internally), or -1 on fatal error.
    virtual int  wait(PollerEvent* out, int maxEvents) = 0;

    // Unblock a thread sitting in wait(). Safe to call from any thread.
    virtual void wake() = 0;

    // Close OS handles. Idempotent.
    virtual void shutdown() = 0;

    // Concrete mechanism name for diagnostics ("epoll", "kqueue", ...).
    virtual const char* name() const = 0;
};

// Platform-selected factory (defined in PollerFactory.cpp). Returns nullptr on
// a platform with no reactor backend.
std::unique_ptr<Poller> makePoller();

} // namespace net
