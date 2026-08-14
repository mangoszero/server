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

#include "WardenModuleCatalog.h"

#include "WardenModuleWin5875Data.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <array>

namespace
{
template<size_t N>
bool Digest(warden::ByteView bytes, EVP_MD const* algorithm,
    std::array<uint8, N>& output)
{
    unsigned int length = 0;
    return algorithm && bytes.data &&
        EVP_Digest(bytes.data, bytes.size, output.data(), &length,
            algorithm, nullptr) == 1 && length == N;
}

warden::ModuleProfile const ModuleWin5875 =
{
    5875,
    "Win",
    {warden::WardenModuleWin5875Data, warden::WardenModuleWin5875Size},
    {0x79, 0xC0, 0x76, 0x8D, 0x65, 0x79, 0x77, 0xD6,
     0x97, 0xE1, 0x0B, 0xAD, 0x95, 0x6C, 0xCE, 0xD1},
    {0x6C, 0x68, 0x00, 0x6A, 0x2F, 0x1F, 0xD3, 0x1E,
     0x72, 0x08, 0x20, 0x4B, 0x3F, 0x7C, 0xEB, 0x94,
     0xA6, 0xCE, 0x97, 0x78, 0x76, 0xE1, 0x3F, 0x2F,
     0x70, 0x3E, 0x9C, 0xD6, 0x44, 0x48, 0x22, 0x89},
    {0xAE, 0x25, 0xBC, 0x51, 0x06, 0x3B, 0x77, 0xBD,
     0x36, 0x3C, 0x3E, 0xFE, 0x0F, 0xC1, 0x73, 0xF9},
    {0x4D, 0x80, 0x8D, 0x2C, 0x77, 0xD9, 0x05, 0xC4,
     0x1A, 0x63, 0x80, 0xEC, 0x08, 0x58, 0x6A, 0xFE},
    {0x56, 0x8C, 0x05, 0x4C, 0x78, 0x1A, 0x97, 0x2A,
     0x60, 0x37, 0xA2, 0x29, 0x0C, 0x22, 0xB5, 0x25,
     0x71, 0xA0, 0x6F, 0x4E},
    {0x7F, 0x96, 0xEE, 0xFD, 0xA5, 0xB6, 0x3D, 0x20,
     0xA4, 0xDF, 0x8E, 0x00, 0xCB, 0xF4, 0x83, 0x04},
    {0xC2, 0xB7, 0xAD, 0xED, 0xFC, 0xCC, 0xA9, 0xC2,
     0xBF, 0xB3, 0xF8, 0x56, 0x02, 0xBA, 0x80, 0x9B}
};
}

namespace warden
{
ModuleProfile const* WardenModuleCatalog::Find(uint32 build,
    std::string const& platform) const
{
    if (build != ModuleWin5875.build || platform != ModuleWin5875.platform)
        return nullptr;

    return Validate(ModuleWin5875) == ModuleValidation::Valid
        ? &ModuleWin5875 : nullptr;
}

ModuleValidation WardenModuleCatalog::Validate(ModuleProfile const& profile) const
{
    if (profile.module.size != WardenModuleWin5875Size)
        return ModuleValidation::WrongLength;

    ModuleId md5{};
    Digest32 sha256{};
    if (!Digest(profile.module, EVP_md5(), md5) ||
        !Digest(profile.module, EVP_sha256(), sha256))
        return ModuleValidation::DigestMismatch;

    if (CRYPTO_memcmp(md5.data(), profile.moduleId.data(), md5.size()) != 0 ||
        CRYPTO_memcmp(sha256.data(), profile.moduleSha256.data(), sha256.size()) != 0)
        return ModuleValidation::DigestMismatch;

    return ModuleValidation::Valid;
}
}
