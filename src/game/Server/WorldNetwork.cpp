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

#include <string>
#include "WorldNetwork.h"

#include "ClientConnection.h"
#include "Log.h"
#include "OpcodeTable.h"

WorldNetwork::WorldNetwork()
    : m_listener(m_gateway)
{
}

WorldNetwork::~WorldNetwork()
{
    Stop();
}

bool WorldNetwork::Start(uint16 port, std::string const& bindIp)
{
    if (m_started)
    {
        return false;
    }

    InitializeOpcodes();
    if (!m_listener.Start(port, bindIp))
    {
        sLog.outError("WorldNetwork::Start: failed to listen on %s:%u",
            bindIp.empty() ? "0.0.0.0" : bindIp.c_str(), unsigned(port));
        return false;
    }

    m_started = true;
    return true;
}

void WorldNetwork::Stop()
{
    if (!m_started)
    {
        return;
    }

    m_listener.Stop();
    m_started = false;
}

uint32 WorldNetwork::GetOpenConnectionCount() const
{
    return proto::ClientConnection::GetOpenConnectionCount();
}
