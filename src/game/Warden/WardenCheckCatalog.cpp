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

namespace
{
struct MpqCatalogRecord
{
    uint32 build;
    char const* platform;
    char const* locale;
    warden::MpqCheckProfile profile;
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
}
