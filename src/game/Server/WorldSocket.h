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

/** \addtogroup u2w User to World Communication
 * @{
 * \file WorldSocket.h
 * \author Derex <derex101@gmail.com>
 */

#ifndef MANGOS_H_WORLDSOCKET
#define MANGOS_H_WORLDSOCKET

#include "Common.h"
#include "Auth/AuthCrypt.h"
#include "Auth/Sha1.h"
#include "Threading/LeasedPtr.h"

#include "net/ISession.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class WorldSession;
class WorldPacket;

/**
 * The world protocol spoken over one client connection.
 *
 * The shared networking engine (net::Server) owns the socket and byte plumbing and
 * hands bytes here. onData() reassembles the TCP stream into WorldPackets and routes
 * them to the WorldSession; SendPacket() encrypts a header and pushes bytes back
 * through the Sender, and may be called from any thread. The transport disarms the
 * Sender on teardown, so a world thread still ticking a dying session merely sends
 * into a no-op.
 */
class WorldSocket : public net::ISession
{
    public:

        WorldSocket();
        ~WorldSocket() override;

        // net::ISession
        void setPeerAddress(const std::string& address) override { m_Address = address; }
        void setSender(net::Sender sender) override { m_sender = std::move(sender); }
        void setCloser(net::Closer closer) override { m_closer = std::move(closer); }
        std::vector<uint8_t> onConnect() override;
        std::vector<uint8_t> onData(const uint8_t* data, size_t len) override;
        void onClose() override;
        bool closed() const override { return m_closed.load(); }

        /// Check if the socket is closed.
        bool IsClosed() const { return m_closed.load(); }

        /// Mark the connection dead and ask the transport to close it. Any thread.
        void CloseSocket();

        /// Address of the connected peer.
        const std::string& GetRemoteAddress() const { return m_Address; }

        /// Send a packet. Reentrant, callable from any thread. Returns -1 on failure.
        int SendPacket(const WorldPacket& pct);

#ifdef _WIN32
        /// Number of currently open world TCP connections (observability).
        static uint32 GetOpenConnectionCount() { return s_openConnections.load(std::memory_order_relaxed); }
#endif

    private:

        friend class WorldSession;
        using SessionLease = LeasedPtr<WorldSession>::Lease;

        std::vector<uint8_t> EncodePacket(const WorldPacket& pct);
        int  ProcessIncoming(WorldPacket* new_pct);
        int  HandleAuthSession(WorldPacket& recvPacket);
        int  HandlePing(WorldPacket& recvPacket);

        SessionLease GetSession();
        void SetSession(WorldSession* session);
        void DetachSessionAndWait();

        std::string m_Address;

        AuthCrypt  m_Crypt;
        std::mutex m_CryptSendLock;        ///< serialises header encryption on send

        LeasedPtr<WorldSession> m_session;

        std::atomic<bool> m_closed;

        net::Sender m_sender;
        net::Closer m_closer;

        // Inbound reassembly (network thread only). m_headerPending records that the
        // current header has already been decrypted, so it is never decrypted twice.
        std::vector<uint8_t> m_recvBuf;
        bool                 m_headerPending;
        uint16               m_recvOpcode;
        uint32               m_recvSize;

        const uint32 m_Seed;

        // Ping-flood tracking.
        std::chrono::steady_clock::time_point m_LastPingTime;
        bool   m_hasPinged;
        uint32 m_OverSpeedPings;

#ifdef _WIN32
        static std::atomic<uint32> s_openConnections;
#endif
};

#endif  /* MANGOS_H_WORLDSOCKET */

/// @}
