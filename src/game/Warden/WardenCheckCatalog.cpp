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

#include "WardenCheckCatalog.h"

#include <array>
#include <limits>

namespace
{
struct MpqCatalogRecord
{
    uint32 build;
    char const* platform;
    char const* locale;
    warden::MpqCheckProfile profile;
};

struct LuaCatalogRecord
{
    uint32 build;
    char const* platform;
    char const* locale;
    warden::LuaCheckProfile profile;
};

struct MemCatalogRecord
{
    uint32 build;
    char const* platform;
    char const* locale;
    std::vector<warden::MemCheckProfile> profiles;
};

MpqCatalogRecord const& Windows5875EnUsMpqRecord()
{
    static MpqCatalogRecord const record
    {
        5875,
        "Win",
        "enUS",
        {
            1,
            "DBFilesClient\\AreaTable.dbc",
            {
                0x7D, 0x88, 0x15, 0x4D, 0x34, 0x11, 0x81, 0x19,
                0x85, 0xF5, 0xD8, 0x11, 0x77, 0xC5, 0x45, 0x32,
                0x48, 0x13, 0x34, 0x43
            }
        }
    };
    return record;
}

LuaCatalogRecord const& Windows5875EnUsLuaRecord()
{
    static LuaCatalogRecord const record
    {
        5875,
        "Win",
        "enUS",
        {2, "OKAY", "Okay"}
    };
    return record;
}

MemCatalogRecord const& Windows5875EnUsMemRecord()
{
    static MemCatalogRecord const record
    {
        5875,
        "Win",
        "enUS",
        {
            {
                1107, "", 0x00618900,
                {
                    0x55, 0x8B, 0xEC, 0x8B, 0x51, 0x40, 0x8B, 0x45,
                    0x0C, 0x81, 0xE2, 0xFF, 0x7D, 0xA0, 0x75, 0x50,
                    0x89, 0x50, 0x10, 0x8B, 0x45, 0x08, 0x50, 0xE8,
                    0x24, 0xDA, 0x1A, 0x00, 0x5D, 0xC2, 0x08, 0x00
                }
            },
            {
                827, "", 0x007C6206,
                {
                    0x25, 0xFF, 0xFF, 0xDF, 0xFB, 0x0D, 0x00,
                    0x20, 0x00, 0x00, 0x89, 0x46, 0x40
                }
            }
        }
    };
    return record;
}

MemCatalogRecord const& Windows6005EnGbMemRecord()
{
    static MemCatalogRecord const record
    {
        6005,
        "Win",
        "enGB",
        {
            {
                1107, "", 0x00618900,
                {
                    0x55, 0x8B, 0xEC, 0x8B, 0x51, 0x40, 0x8B, 0x45,
                    0x0C, 0x81, 0xE2, 0xFF, 0x7D, 0xA0, 0x75, 0x50,
                    0x89, 0x50, 0x10, 0x8B, 0x45, 0x08, 0x50, 0xE8,
                    0x64, 0xDA, 0x1A, 0x00, 0x5D, 0xC2, 0x08, 0x00
                }
            },
            {
                827, "", 0x007C6246,
                {
                    0x25, 0xFF, 0xFF, 0xDF, 0xFB, 0x0D, 0x00,
                    0x20, 0x00, 0x00, 0x89, 0x46, 0x40
                }
            }
        }
    };
    return record;
}

MemCatalogRecord const& Windows6141ZhCnMemRecord()
{
    static MemCatalogRecord const record
    {
        6141,
        "Win",
        "zhCN",
        {
            {
                1107, "", 0x0061ACA0,
                {
                    0x55, 0x8B, 0xEC, 0x8B, 0x51, 0x40, 0x8B, 0x45,
                    0x0C, 0x81, 0xE2, 0xFF, 0x7D, 0xA0, 0x75, 0x50,
                    0x89, 0x50, 0x10, 0x8B, 0x45, 0x08, 0x50, 0xE8,
                    0x64, 0xEB, 0x1A, 0x00, 0x5D, 0xC2, 0x08, 0x00
                }
            },
            {
                827, "", 0x007C96E6,
                {
                    0x25, 0xFF, 0xFF, 0xDF, 0xFB, 0x0D, 0x00,
                    0x20, 0x00, 0x00, 0x89, 0x46, 0x40
                }
            }
        }
    };
    return record;
}
}

namespace warden
{
MpqCheckProfile const* WardenCheckCatalog::FindMpq(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    MpqCatalogRecord const& record = Windows5875EnUsMpqRecord();
    if (record.build != build || platform != record.platform ||
        locale != record.locale || Validate(record.profile) !=
            CheckCatalogValidation::Valid)
        return nullptr;

    return &record.profile;
}

LuaCheckProfile const* WardenCheckCatalog::FindLua(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    LuaCatalogRecord const& record = Windows5875EnUsLuaRecord();
    if (record.build != build || platform != record.platform ||
        locale != record.locale || Validate(record.profile) !=
            CheckCatalogValidation::Valid)
        return nullptr;

    return &record.profile;
}

std::vector<MemCheckProfile> const* WardenCheckCatalog::FindMem(uint32 build,
    std::string const& platform, std::string const& locale) const
{
    std::array<MemCatalogRecord const*, 3> const records =
    {
        &Windows5875EnUsMemRecord(),
        &Windows6005EnGbMemRecord(),
        &Windows6141ZhCnMemRecord()
    };
    for (MemCatalogRecord const* record : records)
    {
        if (record->build == build && platform == record->platform &&
            locale == record->locale &&
            Validate(record->profiles) == CheckCatalogValidation::Valid)
            return &record->profiles;
    }
    return nullptr;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    MpqCheckProfile const& profile) const
{
    if (!profile.checkId)
        return CheckCatalogValidation::InvalidId;
    if (profile.path.empty() || profile.path.size() > 255 ||
        profile.path.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidPath;
    return CheckCatalogValidation::Valid;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    LuaCheckProfile const& profile) const
{
    if (!profile.checkId)
        return CheckCatalogValidation::InvalidId;
    if (profile.query.empty() || profile.query.size() > 255 ||
        profile.query.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidQuery;

    // The exact delivered module truncates callback output at byte 64.
    if (profile.expectedText.size() > 64 ||
        profile.expectedText.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidExpectedText;
    return CheckCatalogValidation::Valid;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    MemCheckProfile const& profile) const
{
    if (!profile.checkId)
        return CheckCatalogValidation::InvalidId;
    if (!profile.addressOrRva)
        return CheckCatalogValidation::InvalidAddress;
    if (profile.moduleName.size() > std::numeric_limits<uint8>::max() ||
        profile.moduleName.find('\0') != std::string::npos)
        return CheckCatalogValidation::InvalidModuleName;
    if (profile.expectedBytes.empty() ||
        profile.expectedBytes.size() > std::numeric_limits<uint8>::max())
        return CheckCatalogValidation::InvalidExpectedBytes;
    return CheckCatalogValidation::Valid;
}

CheckCatalogValidation WardenCheckCatalog::Validate(
    std::vector<MemCheckProfile> const& profiles) const
{
    if (profiles.empty())
        return CheckCatalogValidation::InvalidId;

    for (size_t index = 0; index < profiles.size(); ++index)
    {
        CheckCatalogValidation const validation = Validate(profiles[index]);
        if (validation != CheckCatalogValidation::Valid)
            return validation;
        for (size_t prior = 0; prior < index; ++prior)
        {
            if (profiles[prior].checkId == profiles[index].checkId)
                return CheckCatalogValidation::DuplicateId;
        }
    }
    return CheckCatalogValidation::Valid;
}
}
