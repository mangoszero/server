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

#include "Common.h"
#include "Log.h"
#include "Config/Config.h"
#include "WorldSocket.h"
#include "WorldSocketMgr.h"
#include "Opcodes.h"

#include <memory>
#include <cstdint>
#include <string>
#include <utility>

WorldSocketMgr::WorldSocketMgr()
    : m_started(false)
{
}

WorldSocketMgr::~WorldSocketMgr()
{
    StopNetwork();
}

int WorldSocketMgr::StartNetwork(uint16_t port, const std::string& bindIp)
{
    if (m_started)
    {
        return -1;
    }

    net::SessionFactory factory = []() -> std::shared_ptr<net::ISession>
    {
        return std::make_shared<WorldSocket>();
    };

    if (!m_server.start(port, std::move(factory), bindIp))
    {
        sLog.outError("WorldSocketMgr::StartNetwork: failed to listen on %s:%u",
                      (bindIp.empty() ? "0.0.0.0" : bindIp.c_str()), unsigned(port));
        return -1;
    }

    m_started = true;
    return 0;
}

void WorldSocketMgr::StopNetwork()
{
    if (!m_started)
    {
        return;
    }

    m_server.stop();
    m_started = false;
}
