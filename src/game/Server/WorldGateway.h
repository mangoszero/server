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

#ifndef MANGOS_H_WORLDGATEWAY
#define MANGOS_H_WORLDGATEWAY

#include "IWorldGateway.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class SessionMailbox;

class WorldGateway final : public proto::IWorldGateway
{
public:
    bool FilterAuthPacket(WorldPacket& packet) override;
    void TracePacket(proto::SessionId session, WorldPacket const& packet, bool incoming) override;
    proto::AuthLookup LookupAccount(proto::AuthRequest const& request) override;
    proto::SessionId Attach(proto::AuthRequest const& request,
        std::shared_ptr<proto::IClientLink> const& link,
        std::shared_ptr<proto::AuthContext> const& context) override;
    void Deliver(proto::SessionId session, WorldPacket&& packet) override;
    void Detach(proto::SessionId session) override;

private:
    std::mutex m_lock;
    proto::SessionId m_nextSessionId = proto::INVALID_SESSION_ID;
    std::unordered_map<proto::SessionId, std::shared_ptr<SessionMailbox>> m_routes;
};

#endif
