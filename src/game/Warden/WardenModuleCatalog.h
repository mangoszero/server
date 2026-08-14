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

#ifndef MANGOS_WARDEN_MODULE_CATALOG_H
#define MANGOS_WARDEN_MODULE_CATALOG_H

#include "WardenProtocol.h"

namespace warden
{
enum class ModuleValidation : uint8
{
    Valid,
    WrongLength,
    DigestMismatch
};

struct ModuleProfile
{
    uint32 build;
    char const* platform;
    ByteView module;
    ModuleId moduleId;
    Digest32 moduleSha256;
    Key16 moduleKey;
    Key16 hashSeed;
    Digest20 clientKeySeedHash;
    Key16 clientKeySeed;
    Key16 serverKeySeed;
};

class WardenModuleCatalog
{
public:
    ModuleProfile const* Find(uint32 build, std::string const& platform) const;
    ModuleValidation Validate(ModuleProfile const& profile) const;
};
}

#endif
