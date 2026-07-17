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
#include "net/reactor/Connection.hpp"
#include "net/reactor/Poller.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
#include <deque>

namespace net {

// ── Generic readiness-based (reactor) TCP server ──────────────────────────────
// Holds all the platform-independent machinery: a non-blocking acceptor, a pool
// of worker threads (one Poller each), single-owner connections, async send
// queues and dispatch into the auth Session. The only OS-specific code lives
// behind the Poller interface, supplied via the factory — so a new platform is
// just a new Poller subclass plus a line in PollerFactory.cpp.
class ReactorServer {
public:
    using PollerFactory = std::function<std::unique_ptr<Poller>()>;

    explicit ReactorServer(PollerFactory factory);
    ~ReactorServer();

    // `bindIp` is the configured BindIP: empty (or "0.0.0.0") listens on every
    // local interface, otherwise the listener binds that single IPv4/hostname.
    bool start(uint16_t port, SessionFactory factory,
               const std::string& bindIp = std::string());
    void stop();

private:
    struct Worker {
        std::unique_ptr<Poller>         poller;
        std::thread                     thread;
        std::mutex                      incomingMu;
        std::vector<Connection*>        incoming; // handed off by the acceptor
        std::unordered_set<Connection*> conns;    // owned by this thread

        // Cross-thread send/close requests posted by the world thread via a
        // connection's SendChannel; drained on this worker's own thread.
        std::mutex                                reqMu;
        std::deque<std::shared_ptr<SendChannel>>  reqQueue;
    };

    PollerFactory             m_pollerFactory;
    int                       m_listen = -1;
    SessionFactory            m_factory;
    std::atomic<bool>         m_running{false};

    std::unique_ptr<Poller>   m_acceptPoller;
    std::thread               m_acceptThread;

    std::vector<std::unique_ptr<Worker>> m_workers;
    std::atomic<uint32_t>     m_rr{0}; // round-robin handoff counter

    void acceptLoop();
    void workerLoop(Worker& w);
    void handoff(Worker& w, Connection* conn);
    void drainIncoming(Worker& w);
    void drainSendRequests(Worker& w);   // world-thread sends, run on the worker

    // Push as much of conn->sendQ as the socket accepts; arms/disarms EvWrite via
    // the worker's Poller. Returns false on fatal send error.
    bool flush(Worker& w, Connection* conn);
    void setWriteInterest(Worker& w, Connection* conn, bool want);
    void closeConn(Worker& w, Connection* conn);
};

} // namespace net
