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

/// \addtogroup mangosd
/// @{
/// \file

#include "RASession.h"

#include "AccountMgr.h"
#include "Config.h"
#include "Language.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "World.h"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

RASession::RASession()
    : m_closed(false),
      m_stage(NONE),
      m_accountId(0),
      m_accessLevel(SEC_PLAYER),
      m_commandsPending(0)
{
    m_secure   = sConfig.GetBoolDefault("RA.Secure", true);
    m_stricted = sConfig.GetBoolDefault("RA.Stricted", false);
    m_minLevel = AccountTypes(sConfig.GetIntDefault("RA.MinLevel", SEC_ADMINISTRATOR));
}

RASession::~RASession()
{
    sLog.outRALog("Connection was closed.");
}

void RASession::Send(const char* message)
{
    if (!message || m_closed.load() || !m_sender)
    {
        return;
    }

    m_sender(reinterpret_cast<const uint8_t*>(message), strlen(message));
}

void RASession::Close()
{
    if (m_closed.exchange(true))
    {
        return;
    }

    if (m_closer)
    {
        m_closer();
    }
}

void RASession::onClose()
{
    m_closed.store(true);
}

std::vector<uint8_t> RASession::onConnect()
{
    sLog.outRALog("Incoming connection from %s.", m_address.c_str());

    Send(sWorld.GetMotd());
    Send("\r\n");
    Send(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_USER));

    return {};
}

std::vector<uint8_t> RASession::onData(const uint8_t* data, size_t len)
{
    if (m_closed.load())
    {
        return {};
    }

    m_input.append(reinterpret_cast<const char*>(data), len);

    for (;;)
    {
        const std::string::size_type eol = m_input.find_first_of("\r\n");
        if (eol == std::string::npos)
        {
            break;
        }

        const std::string line = m_input.substr(0, eol);

        // Swallow the whole line terminator, however the client spells it.
        std::string::size_type next = m_input.find_first_not_of("\r\n", eol);
        m_input.erase(0, next == std::string::npos ? m_input.size() : next);

        HandleLine(line);

        if (m_closed.load())
        {
            break;
        }
    }

    return {};
}

void RASession::HandleLine(const std::string& line)
{
    switch (m_stage)
    {
        case NONE: HandleUsername(line); break;
        case LG:   HandlePassword(line); break;
        case OK:   HandleCommand(line);  break;
    }
}

void RASession::HandleUsername(const std::string& line)
{
    m_accountId = sAccountMgr.GetId(line);

    ///- If the user is not found, deny access
    if (!m_accountId)
    {
        Send("-No such user.\r\n");
        sLog.outRALog("User %s does not exist.", line.c_str());

        if (m_secure)
        {
            Close();
            return;
        }

        Send("\r\n");
        Send(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_USER));
        return;
    }

    m_accessLevel = sAccountMgr.GetSecurity(m_accountId);

    ///- if gmlevel is too low, deny access
    if (m_accessLevel < m_minLevel)
    {
        Send("-Not enough privileges.\r\n");
        sLog.outRALog("User %s has no privilege.", line.c_str());

        if (m_secure)
        {
            Close();
            return;
        }

        Send("\r\n");
        Send(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_USER));
        return;
    }

    ///- allow a remotely connected admin to use console-level commands, per config
    if (m_accessLevel >= SEC_ADMINISTRATOR && !m_stricted)
    {
        m_accessLevel = SEC_CONSOLE;
    }

    m_stage = LG;
    Send(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_PASS));
}

void RASession::HandlePassword(const std::string& line)
{
    if (sAccountMgr.CheckPassword(m_accountId, line))
    {
        m_stage = OK;

        Send("+Logged in.\r\n");
        sLog.outRALog("User account %u has logged in.", m_accountId);
        Send("mangos>");
        return;
    }

    ///- Else deny access
    Send("-Wrong pass.\r\n");
    sLog.outRALog("User account %u has failed to log in.", m_accountId);

    if (m_secure)
    {
        Close();
        return;
    }

    Send("\r\n");
    Send(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_PASS));
}

void RASession::HandleCommand(const std::string& line)
{
    if (line.empty())
    {
        Send("mangos>");
        return;
    }

    sLog.outRALog("Got '%s' cmd.", line.c_str());

    if (line.compare(0, 4, "quit") == 0)
    {
        Close();
        return;
    }

    {
        // The command runs later, on the world thread, and is handed a bare pointer
        // back to us. Keep ourselves alive until it reports finished.
        std::lock_guard<std::mutex> guard(m_commandLock);

        if (m_commandsPending++ == 0)
        {
            m_keepAlive = std::static_pointer_cast<RASession>(shared_from_this());
        }
    }

    sWorld.QueueCliCommand(new CliCommandHolder(m_accountId, m_accessLevel, this, line.c_str(),
                                                &RASession::CommandPrint,
                                                &RASession::CommandFinished));
}

void RASession::ReleaseCommand()
{
    // Drop the self-reference outside the lock: it may be the last one.
    std::shared_ptr<RASession> expiring;

    {
        std::lock_guard<std::mutex> guard(m_commandLock);

        if (--m_commandsPending == 0)
        {
            expiring = std::move(m_keepAlive);
            m_keepAlive.reset();
        }
    }
}

/// Command output (world thread).
void RASession::CommandPrint(void* callbackArg, const char* text)
{
    if (!text)
    {
        return;
    }

    static_cast<RASession*>(callbackArg)->Send(text);
}

/// Command completion (world thread).
void RASession::CommandFinished(void* callbackArg, bool /*success*/)
{
    RASession* session = static_cast<RASession*>(callbackArg);

    session->Send("mangos>");
    session->ReleaseCommand();
}

// ── RaServer ─────────────────────────────────────────────────────────────────────

bool RaServer::Start(uint16_t port, const std::string& bindIp)
{
    if (m_started)
    {
        return false;
    }

    net::SessionFactory factory = []() -> std::shared_ptr<net::ISession>
    {
        return std::make_shared<RASession>();
    };

    if (!m_server.start(port, std::move(factory), bindIp))
    {
        sLog.outError("RaServer: failed to listen on %s:%u",
                      (bindIp.empty() ? "0.0.0.0" : bindIp.c_str()), unsigned(port));
        return false;
    }

    return m_started = true;
}

void RaServer::Stop()
{
    if (!m_started)
    {
        return;
    }

    m_server.stop();
    m_started = false;
}
/// @}
