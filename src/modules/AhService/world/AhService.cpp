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

#include "AhService.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "SystemConfig.h"
#include "WorkerSupervisor.h"

namespace
{
    char const* const AHSERVICE_CONFIG_NAME = "ah-service.conf";
}

AhService& AhService::Instance()
{
    static AhService instance;
    return instance;
}

bool AhService::IsWorkerActive() const
{
    return m_supervisor && m_supervisor->ServiceActive();
}

bool AhService::LoadConfig(bool reload)
{
    if (!reload)
    {
        m_configPath = std::string(SYSCONFDIR) + AHSERVICE_CONFIG_NAME;
        if (!m_file.SetSource(m_configPath.c_str()))
        {
            m_configPath = AHSERVICE_CONFIG_NAME;
            if (!m_file.SetSource(m_configPath.c_str()))
            {
                sLog.outString("AH service is disabled: unable to open %s", AHSERVICE_CONFIG_NAME);
                return false;
            }
        }
    }
    else if (!m_file.Reload())
    {
        sLog.outError("AH service: unable to reload %s; keeping the previous settings", m_configPath.c_str());
        return false;
    }

    AhServiceSettings settings;
    settings.enable              = m_file.GetBoolDefault("Enable", false);
    settings.worker              = m_file.GetBoolDefault("AH.Service.Worker", false);
    settings.path                = m_file.GetStringDefault("AH.Service.Path", "service-workers/ah-service/ah-service");
    settings.port                = uint16(m_file.GetIntDefault("AH.Service.Port", 5760));
    settings.secret              = m_file.GetStringDefault("AH.Service.Secret", "changeme");
    settings.intentTtlSec        = m_file.GetIntDefault("AH.Service.IntentTtlSec", 900);
    settings.custody             = m_file.GetBoolDefault("AH.Service.Custody", false);
    settings.custodyCrashAt      = m_file.GetStringDefault("AH.Service.CustodyCrashAt", "");
    settings.custodyFailCommitAt = m_file.GetStringDefault("AH.Service.CustodyFailCommitAt", "");

    if (reload)
    {
        // The worker, its address and the write authority hold for the whole run.
        settings.enable         = m_settings.enable;
        settings.worker         = m_settings.worker;
        settings.path           = m_settings.path;
        settings.port           = m_settings.port;
        settings.secret         = m_settings.secret;
        settings.writeAuthority = m_settings.writeAuthority;
        m_settings = settings;
        return true;
    }

    settings.writeAuthority = m_file.GetBoolDefault("AH.Service.WriteAuthority", false);
    if (settings.writeAuthority && !settings.custody)
    {
        sLog.outError("AH.Service.WriteAuthority = 1 requires AH.Service.Custody = 1; forcing WriteAuthority OFF.");
        settings.writeAuthority = false;
    }

    // Reconcile-on-reconnect reads the worker journal; without the table a
    // committed bid would read as absent and be refunded twice.
    if (settings.writeAuthority)
    {
        QueryResult* journal = CharacterDatabase.Query("SHOW TABLES LIKE 'ah_worker_journal'");
        if (!journal)
        {
            sLog.outError("AH.Service.WriteAuthority = 1 but the `ah_worker_journal` table is missing; "
                          "apply the SP-2 migration first. Forcing WriteAuthority OFF.");
            settings.writeAuthority = false;
        }
        delete journal;
    }

    if (settings.writeAuthority && !settings.worker)
    {
        sLog.outError("AH.Service.WriteAuthority = 1 but AH.Service.Worker = 0: no worker will start; "
                      "all AH mutations will be unavailable.");
    }

    m_settings = settings;
    return true;
}
