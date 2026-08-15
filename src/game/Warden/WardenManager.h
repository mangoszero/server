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
 */

#ifndef MANGOS_WARDEN_MANAGER_H
#define MANGOS_WARDEN_MANAGER_H

#include "WardenCheckCatalog.h"
#include "WardenServer.h"

#include <memory>

namespace warden
{
/** Immutable per-session inputs selected by the server admission policy. */
struct WardenCreationOptions
{
    WardenLimits limits{};
    WardenConfiguration configuration{};
    bool initialAggressive = false;
    bool requireMemCatalog = false;
};

/**
 * Stateless factory boundary between authenticated session inputs and the
 * exact delivered-module profile. Creating a server is inert; Start owns the
 * first wire write after WorldSession admission completes.
 */
class WardenManager
{
public:
    static WardenManager& Instance();

    // Returns null for unsupported or custody-invalid profiles. The observer
    // receives typed terminal facts only and may be omitted by tests/tools.
    std::unique_ptr<WardenServer> Create(uint32 build,
        std::string const& platform, std::string const& locale,
        SessionKey const& sessionKey, SendEncrypted send,
        WardenCreationOptions options = {},
        LifecycleObserver lifecycleObserver = {},
        EvidenceBatchObserver evidenceObserver = {}) const;

private:
    WardenModuleCatalog m_catalog;
    WardenCheckCatalog m_checkCatalog;
};
}

#endif
