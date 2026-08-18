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

#include "WardenManager.h"

#include <utility>
#include <vector>

namespace warden
{
bool IsWardenEnforcementProfile(uint32 build,
    std::string const& platform, std::string const& locale)
{
    return WardenManager::Instance().FindCheckProfile(build, platform,
        locale) != nullptr;
}

WardenProfileDisposition ClassifyWardenProfile(
    WardenEnforcementMode enforcementMode, bool requireExactProfile,
    bool exactProfile)
{
    if (enforcementMode == WardenEnforcementMode::Observe)
        return WardenProfileDisposition::Observe;
    if (exactProfile)
        return WardenProfileDisposition::Enforce;
    return requireExactProfile ? WardenProfileDisposition::Reject :
        WardenProfileDisposition::Observe;
}

WardenManager& WardenManager::Instance()
{
    static WardenManager manager;
    return manager;
}

bool WardenManager::PublishCheckCatalog(
    std::shared_ptr<WardenCheckCatalog const> catalog)
{
    if (m_checkCatalog || !catalog || catalog->Profiles().empty() ||
        !catalog->TotalRows())
        return false;
    m_checkCatalog = std::move(catalog);
    return true;
}

bool WardenManager::HasPublishedCheckCatalog() const
{
    return m_checkCatalog != nullptr;
}

WardenCheckProfile const* WardenManager::FindCheckProfile(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    return m_checkCatalog ? m_checkCatalog->Find(build, platform, locale) :
        nullptr;
}

std::unique_ptr<WardenServer> WardenManager::Create(uint32 build,
    std::string const& platform, std::string const& locale,
    SessionKey const& sessionKey, SendEncrypted send,
    WardenCreationOptions options, LifecycleObserver lifecycleObserver,
    EvidenceBatchObserver evidenceObserver) const
{
    ModuleProfile const* profile = m_catalog.Find(build, platform);
    if (!profile || m_catalog.Validate(*profile) != ModuleValidation::Valid)
        return nullptr;

    // Initialization derives private stream state; WardenServer does not retain
    // the authenticated raw-40 key and Create emits no packet.
    WardenCryptoContext crypto;
    crypto.Initialize(sessionKey);

    WardenCheckProfile const* selected =
        FindCheckProfile(build, platform, locale);
    bool const enforcing = options.configuration.enforcementMode !=
        WardenEnforcementMode::Observe;
    if (enforcing && !selected)
        return nullptr;
    std::vector<WardenCheckDefinition> checks;
    if (selected)
        checks = selected->checks;

    return std::make_unique<WardenServer>(*profile, std::move(crypto),
        std::move(send), options.limits, options.configuration,
        options.initialAggressive, std::move(lifecycleObserver),
        std::move(evidenceObserver), std::move(checks));
}
}
