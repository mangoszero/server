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
 */

#include "ServiceDatabase.h"
#include "Config/Config.h"
#include "Log/Log.h"

#include <string>

// ---------------------------------------------------------------------------
// ParseDbName - extract the database name from host;port;user;pass;db
// ---------------------------------------------------------------------------

std::string ServiceDatabase::ParseDbName(const std::string& dbstring)
{
    // Format: host;port;user;pass;db  (5 semicolon-delimited fields)
    // Return the 5th field (index 4).
    size_t pos = 0;
    int    field = 0;
    while (field < 4)
    {
        size_t semi = dbstring.find(';', pos);
        if (semi == std::string::npos)
        {
            return std::string();   // malformed
        }
        pos = semi + 1;
        ++field;
    }
    // pos now points at the start of the 5th field.
    size_t end = dbstring.find(';', pos);
    if (end == std::string::npos)
    {
        end = dbstring.size();
    }
    return dbstring.substr(pos, end - pos);
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ServiceDatabase::ServiceDatabase()
    : m_initialized(false),
      m_charInitialized(false)
{
}

ServiceDatabase::~ServiceDatabase()
{
    Shutdown();
}

bool ServiceDatabase::Init()
{
    if (m_initialized)
    {
        return true;
    }

    // Connection string + pool size from the loaded ah-service.conf, same
    // format and key names as mangosd.conf.
    std::string dbstring = sConfig.GetStringDefault("WorldDatabaseInfo", "");
    int nConnections = sConfig.GetIntDefault("WorldDatabaseConnections", 1);

    if (dbstring.empty())
    {
        sLog.outError("ah-service: WorldDatabaseInfo not specified in"
                      " configuration file");
        return false;
    }

    sLog.outString("ah-service: world database total connections: %i",
                   nConnections + 1);

    // Constructing the DatabaseType (DatabaseMysql) already performs the
    // ref-counted mysql_library_init(); only Initialize() is needed here.
    if (!m_worldDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("ah-service: cannot connect to world database %s",
                      dbstring.c_str());
        return false;
    }

    // Version-tolerant: skip CheckDatabaseVersion (read-only advisory child).
    // Stand in a single trivial connectivity query and fail clearly on null.
    QueryResult* result = m_worldDatabase.Query("SELECT 1");
    if (!result)
    {
        sLog.outError("ah-service: world database connectivity check failed"
                      " (SELECT 1 returned null)");
        m_worldDatabase.HaltDelayThread();
        return false;
    }
    delete result;

    sLog.outString("ah-service: world database connection established"
                   " (read-only)");
    m_worldDbName = ParseDbName(dbstring);
    m_initialized = true;
    return true;
}

void ServiceDatabase::Shutdown()
{
    if (m_charInitialized)
    {
        m_characterDatabase.HaltDelayThread();
        m_charInitialized = false;
    }

    if (!m_initialized)
    {
        return;
    }

    m_worldDatabase.HaltDelayThread();
    m_initialized = false;
}

bool ServiceDatabase::InitCharacter()
{
    if (m_charInitialized)
    {
        return true;
    }

    std::string dbstring =
        sConfig.GetStringDefault("CharacterDatabaseInfo", "");
    int nConnections =
        sConfig.GetIntDefault("CharacterDatabaseConnections", 1);

    if (dbstring.empty())
    {
        sLog.outError("ah-service: CharacterDatabaseInfo not specified"
                      " in configuration file");
        return false;
    }

    sLog.outString("ah-service: character database total connections: %i",
                   nConnections + 1);

    if (!m_characterDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("ah-service: cannot connect to character database %s",
                      dbstring.c_str());
        return false;
    }

    QueryResult* result = m_characterDatabase.Query("SELECT 1");
    if (!result)
    {
        sLog.outError("ah-service: character database connectivity check"
                      " failed (SELECT 1 returned null)");
        m_characterDatabase.HaltDelayThread();
        return false;
    }
    delete result;

    sLog.outString("ah-service: character database connection established"
                   " (read-only)");
    m_charDbName      = ParseDbName(dbstring);
    m_charInitialized = true;
    return true;
}

// ---------------------------------------------------------------------------
// ResolveCharacterGuid - faithful port of ObjectMgr::GetPlayerGuidByName
// ---------------------------------------------------------------------------

uint32 ServiceDatabase::ResolveCharacterGuid(const std::string& name)
{
    if (name.empty() || !m_charInitialized)
    {
        return 0;
    }

    // Escape with the character DB (matches the in-process bot exactly).
    std::string escaped = name;
    m_characterDatabase.escape_string(escaped);

    uint32 guid = 0;
    QueryResult* result = m_characterDatabase.PQuery(
        "SELECT `guid` FROM `characters` WHERE `name` = '%s'",
        escaped.c_str());
    if (result)
    {
        guid = (*result)[0].GetUInt32();
        delete result;
    }

    return guid;
}
