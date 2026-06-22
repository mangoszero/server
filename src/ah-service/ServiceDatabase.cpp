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

ServiceDatabase::ServiceDatabase()
    : m_initialized(false)
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
    m_initialized = true;
    return true;
}

void ServiceDatabase::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    m_worldDatabase.HaltDelayThread();
    m_initialized = false;
}
