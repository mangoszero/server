/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_PROTO_CLIENTCONNECTION_H
#define MANGOS_PROTO_CLIENTCONNECTION_H

#include <vector>
#include "Auth/AuthCrypt.h"
#include "IClientLink.h"
#include "IWorldGateway.h"
#include "PacketCodec.h"
#include "net/ISession.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <utility>

namespace proto
{
class ClientConnection final : public net::ISession, public IClientLink
{
public:
    explicit ClientConnection(IWorldGateway& gateway);
    ~ClientConnection() override;

    void setPeerAddress(std::string const& address) override { m_address = address; }
    void setSender(net::Sender sender) override { m_sender = std::move(sender); }
    void setCloser(net::Closer closer) override { m_closer = std::move(closer); }
    std::vector<uint8_t> onConnect() override;
    std::vector<uint8_t> onData(uint8_t const* data, std::size_t len) override;
    void onClose() override;
    bool closed() const override { return m_closed.load(); }

    void SendPacket(WorldPacket const& packet) override;
    void Close() override;
    std::string const& GetRemoteAddress() const override { return m_address; }
    bool IsClosed() const override { return m_closed.load(); }

    static uint32 GetOpenConnectionCount()
    {
        return s_openConnections.load(std::memory_order_relaxed);
    }

private:
    bool HandlePacket(WorldPacket& packet);
    bool HandleAuthSession(WorldPacket& packet);
    SessionId CurrentSession();
    void SendAuthResponse(AuthStatus status);
    std::vector<uint8> EncodePacket(WorldPacket const& packet);

    IWorldGateway& m_gateway;
    std::string m_address;
    PacketCodec m_codec;
    AuthCrypt m_crypt;
    std::mutex m_sendOrderLock;
    std::mutex m_sessionLock;
    uint32 m_seed;
    SessionId m_session = INVALID_SESSION_ID;

    /// The same id, readable without m_sessionLock: SendPacket traces while holding
    /// m_sendOrderLock, and taking m_sessionLock under it would invert the order
    /// HandleAuthSession uses. Tracing only -- m_session stays the authority.
    std::atomic<SessionId> m_traceSession{INVALID_SESSION_ID};
    bool m_authStarted = false;
    std::atomic<bool> m_closed{false};
    net::Sender m_sender;
    net::Closer m_closer;

    static std::atomic<uint32> s_openConnections;
};
}

#endif
