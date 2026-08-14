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
    // Profiles are explicit build/platform contracts. Future check addresses
    // must remain build scoped even when two builds can load the same module.
    uint32 build;
    char const* platform;
    ByteView module;
    ModuleId moduleId;             // MD5 identity sent in MODULE_USE.
    Digest32 moduleSha256;         // Server custody check; not sent on wire.
    Key16 moduleKey;               // Client decryption key sent in MODULE_USE.
    Key16 hashSeed;                // Challenge sent in HASH_REQUEST.
    Digest20 clientKeySeedHash;    // Exact accepted 20-byte client response.
    Key16 clientKeySeed;           // Post-hash client-to-server RC4 key.
    Key16 serverKeySeed;           // Post-hash server-to-client RC4 key.
};

/** Selects and validates immutable, custody-pinned delivered modules. */
class WardenModuleCatalog
{
public:
    // Returns null rather than falling back across builds or platforms.
    ModuleProfile const* Find(uint32 build, std::string const& platform) const;

    // Recomputes both the wire MD5 identity and server-only SHA-256 identity.
    ModuleValidation Validate(ModuleProfile const& profile) const;
};
}

#endif
