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

#ifndef MANGOS_WARDEN_CHECK_CATALOG_H
#define MANGOS_WARDEN_CHECK_CATALOG_H

#include "WardenProtocol.h"

#include <string>
#include <vector>

namespace warden
{
enum class CheckCatalogValidation : uint8
{
    Valid,
    InvalidId,
    InvalidPath,
    InvalidQuery,
    InvalidExpectedText,
    InvalidAddress,
    InvalidModuleName,
    InvalidExpectedBytes,
    DuplicateId
};

/** Immutable input needed to request and evaluate one client MPQ digest. */
struct MpqCheckProfile
{
    uint32 checkId = 0;
    std::string path;
    Digest20 expectedSha1{};
};

/** Immutable input needed to request and evaluate one client Lua lookup. */
struct LuaCheckProfile
{
    uint32 checkId = 0;
    std::string query;
    std::string expectedText;
};

/** Immutable input needed to request and classify one process-memory read. */
struct MemCheckProfile
{
    uint32 checkId = 0;
    // Empty selects an absolute process address; nonempty selects module+RVA.
    std::string moduleName;
    uint32 addressOrRva = 0;
    Bytes expectedBytes;
};

/**
 * Selects active checks independently of the delivered module catalogue.
 * Archive contents are locale/build scoped even when module bytes are shared.
 */
class WardenCheckCatalog
{
public:
    MpqCheckProfile const* FindMpq(uint32 build,
        std::string const& platform, std::string const& locale) const;
    LuaCheckProfile const* FindLua(uint32 build,
        std::string const& platform, std::string const& locale) const;
    std::vector<MemCheckProfile> const* FindMem(uint32 build,
        std::string const& platform, std::string const& locale) const;
    CheckCatalogValidation Validate(MpqCheckProfile const& profile) const;
    CheckCatalogValidation Validate(LuaCheckProfile const& profile) const;
    CheckCatalogValidation Validate(MemCheckProfile const& profile) const;
    CheckCatalogValidation Validate(
        std::vector<MemCheckProfile> const& profiles) const;
};
}

#endif
