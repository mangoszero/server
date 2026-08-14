/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
