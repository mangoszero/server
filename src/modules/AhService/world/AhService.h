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

#ifndef MANGOS_H_AHSERVICE
#define MANGOS_H_AHSERVICE

#include "Platform/Define.h"
#include "Config/Config.h"
#include "BrowsePending.h"
#include "MutationPending.h"

#include <string>

class WorkerSupervisor;

/// The keys of ah-service.conf that mangosd reads; the worker reads the rest.
struct AhServiceSettings
{
    bool enable = false;
    bool worker = false;                                    ///< spawn and supervise the ah-service child
    std::string path;
    uint16 port = 5760;
    std::string secret;
    int32 intentTtlSec = 900;
    bool custody = false;
    bool writeAuthority = false;                            ///< read once at startup
    std::string custodyCrashAt;                             ///< test only
    std::string custodyFailCommitAt;                        ///< test only
};

/**
 * @brief State of the auction house service inside mangosd.
 *
 * Owned by the world thread: created at configuration load, started before
 * the world loop and stopped after it.
 */
class AhService
{
    public:

        /// Loads ah-service.conf; false when it cannot be opened.
        bool LoadConfig(bool reload);

        AhServiceSettings const& Settings() const { return m_settings; }
        std::string const& ConfigPath() const { return m_configPath; }

        bool IsCustodyEnabled() const { return m_settings.custody; }
        void SetCustody(bool on) { m_settings.custody = on; }
        void SetCustodyFailCommitAt(std::string const& phase) { m_settings.custodyFailCommitAt = phase; }
        bool IsWriteAuthority() const { return m_settings.writeAuthority; }
        /// The worker is the configured authority, whether or not it is running.
        bool IsWorkerConfigured() const { return m_settings.worker; }

        void SetSupervisor(WorkerSupervisor* supervisor) { m_supervisor = supervisor; }
        WorkerSupervisor* GetSupervisor() const { return m_supervisor; }
        bool IsWorkerActive() const;

        BrowsePendingMap& GetBrowsePending() { return m_browsePending; }
        MutationPendingMap& GetMutationPending() { return m_mutationPending; }

        static AhService& Instance();

    private:

        AhService() : m_supervisor(NULL) {}

        Config m_file;
        std::string m_configPath;
        AhServiceSettings m_settings;
        WorkerSupervisor* m_supervisor;
        BrowsePendingMap m_browsePending;
        MutationPendingMap m_mutationPending;
};

#define sAhService AhService::Instance()

#endif
