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

#ifndef MANGOS_WARDEN_CHECK_CATALOG_LOADER_H
#define MANGOS_WARDEN_CHECK_CATALOG_LOADER_H

#include "WardenCheckCatalog.h"
#include "WardenModuleCatalog.h"

namespace warden
{
enum class WardenCheckCatalogLoadFailure : uint8
{
    None,
    CatalogueQueryFailed,
    EmptyCatalogue,
    InvalidRow,
    ProfileWithoutModule,
    ModuleWithoutProfile,
    InvalidPlan,
    PublicationFailed
};

char const* ToString(WardenCheckCatalogLoadFailure failure);

inline WardenCheckCatalogLoadFailure ValidateWardenCatalogCoverage(
    WardenCheckCatalog const& checks, WardenModuleCatalog const& modules)
{
    for (WardenCheckProfile const& profile : checks.Profiles())
    {
        if (!modules.Find(profile.key.build, profile.key.platform))
            return WardenCheckCatalogLoadFailure::ProfileWithoutModule;
    }

    for (ModuleProfile const* module : modules.Profiles())
    {
        bool found = false;
        for (WardenCheckProfile const& profile : checks.Profiles())
        {
            if (profile.key.build == module->build &&
                profile.key.platform == module->platform)
            {
                found = true;
                break;
            }
        }
        if (!found)
            return WardenCheckCatalogLoadFailure::ModuleWithoutProfile;
    }
    return WardenCheckCatalogLoadFailure::None;
}

/** Required synchronous startup loader for the World check catalogue. */
class WardenCheckCatalogLoader
{
public:
    bool LoadAndPublish() const;
};
}

#endif
