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
#include "net/ISession.hpp"
#include "net/SendQueue.hpp"
#include "net/reactor/Poller.hpp"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

namespace net {

struct Connection;

// Lifetime-safe bridge that lets the world thread send to / close a connection
// that is otherwise owned end-to-end by one worker thread. The session captures
// this (shared_ptr) as its Sender/Closer. post()/requestClose() run on the world
// thread; they append bytes (or set a close flag) and hand the channel to the
// owning worker's request queue, then wake its poller. The worker drains the queue
// on its own thread, where touching the Connection is race-free. Once the worker
// tears the connection down it calls disarm(), after which every later
// post()/requestClose() is a no-op — so a freed Connection is never read.
//
// The outbound bytes live in the SendQueue held HERE rather than in the Connection,
// for two reasons: producers can append to it directly (it is internally locked, and
// the worker drains the same buffer), which removes the old double hand-off where
// every payload was moved into `pending` and then again into the connection's queue;
// and because this channel is a shared_ptr owned by the session, the buffer and its
// FlowGate outlive the socket, so a bulk producer parked on backpressure is always
// woken into live memory.
//
// The reqMu/reqQueue/poller pointers alias the owning Worker's members; they
// stay valid because workers are shut down only after every connection is gone
// (and the world loop is stopped before the network layer at shutdown).
struct SendChannel : public std::enable_shared_from_this<SendChannel> {
    std::mutex  mu;
    bool        alive = true;
    Connection* conn  = nullptr;
    bool        closeRequested = false;
    SendQueue   out;                        // coalescing buffer + byte backpressure

    // Owning worker's wake plumbing (set at hand-off; valid while alive).
    std::mutex*                                 reqMu    = nullptr;
    std::deque<std::shared_ptr<SendChannel>>*   reqQueue = nullptr;
    Poller*                                     poller   = nullptr;

    void post(const uint8_t* data, size_t len);  // world thread
    void requestClose();                         // world thread
    void disarm();                               // worker thread

private:
    void notifyWorker();                    // enqueue self + wake the worker
};

// Passive per-connection state. A Connection is owned end-to-end by exactly one
// worker thread (the one whose Poller its fd is registered on). ReactorServer
// performs the recv/send syscalls and drives the Poller; the outbound bytes
// themselves live in channel->out (which is separately locked, since producers on
// other threads append to it).
struct Connection {
    int      fd = -1;
    std::shared_ptr<ISession>    session;
    std::shared_ptr<SendChannel> channel;

    bool     writeArmed      = false; // is EvWrite interest currently registered?
    bool     closeAfterDrain = false; // session asked to close once output drains

    explicit Connection(const SessionFactory& factory) : session(factory()) {}
};

} // namespace net
