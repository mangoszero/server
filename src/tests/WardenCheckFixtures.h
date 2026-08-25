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

#ifndef MANGOS_TEST_WARDEN_CHECK_FIXTURES_H
#define MANGOS_TEST_WARDEN_CHECK_FIXTURES_H

#include "WardenCheckCatalog.h"

#include <string>
#include <vector>

namespace warden
{
namespace test
{
inline WardenCheckRowInput MakeRow(uint32 build,
    std::string const& localeHex, uint32 checkId, WardenCheckType type,
    uint32 sortOrder, WardenEvidenceClass evidenceClass)
{
    WardenCheckRowInput row;
    row.build = build;
    row.platformHex = "57696E";
    row.localeHex = localeHex;
    row.checkId = checkId;
    row.type = static_cast<uint32>(type);
    row.enabled = 1;
    row.sortOrder = sortOrder;
    row.evidenceClass = static_cast<uint32>(evidenceClass);
    return row;
}

inline void AppendInitialProfile(std::vector<WardenCheckRowInput>& rows,
    uint32 build, std::string const& localeHex,
    std::string const& mpqExpectedHex, std::string const& luaExpectedHex,
    uint32 functionAddress, std::string const& functionExpectedHex,
    uint32 flagsAddress, uint32 luaProtectionAddress,
    std::string const& luaProtectionExpectedHex, uint32 wallClimbAddress,
    uint32 frameDispatchAddress, std::string const& frameDispatchExpectedHex)
{
    rows.push_back(MakeRow(build, localeHex, 65536,
        WardenCheckType::Timing, 10, WardenEvidenceClass::ProtocolHealth));

    WardenCheckRowInput mpq = MakeRow(build, localeHex, 1,
        WardenCheckType::Mpq, 20, WardenEvidenceClass::Corroboration);
    mpq.requestHex =
        "444246696C6573436C69656E745C417265615461626C652E646263";
    mpq.expectedHex = mpqExpectedHex;
    rows.push_back(mpq);

    WardenCheckRowInput lua = MakeRow(build, localeHex, 2,
        WardenCheckType::Lua, 30, WardenEvidenceClass::Corroboration);
    lua.requestHex = "4F4B4159";
    lua.expectedHex = luaExpectedHex;
    rows.push_back(lua);

    WardenCheckRowInput function = MakeRow(build, localeHex, 1107,
        WardenCheckType::Mem, 40, WardenEvidenceClass::IntegrityInvariant);
    function.address = functionAddress;
    function.length = 32;
    function.expectedHex = functionExpectedHex;
    rows.push_back(function);

    WardenCheckRowInput flags = MakeRow(build, localeHex, 827,
        WardenCheckType::Mem, 50, WardenEvidenceClass::IntegrityInvariant);
    flags.address = flagsAddress;
    flags.length = 13;
    flags.expectedHex = "25FFFFDFFB0D00200000894640";
    rows.push_back(flags);

    WardenCheckRowInput luaProtection = MakeRow(build, localeHex, 1566,
        WardenCheckType::Mem, 60, WardenEvidenceClass::ThreatSignature);
    luaProtection.address = luaProtectionAddress;
    luaProtection.length = 5;
    luaProtection.expectedHex = luaProtectionExpectedHex;
    rows.push_back(luaProtection);

    WardenCheckRowInput wallClimb = MakeRow(build, localeHex, 1135,
        WardenCheckType::Mem, 70, WardenEvidenceClass::Corroboration);
    wallClimb.address = wallClimbAddress;
    wallClimb.length = 4;
    wallClimb.expectedHex = "BB8D243F";
    rows.push_back(wallClimb);

    // Both tables dispatch the client interface-signature result. A patched
    // table can turn all outcomes into success without changing Warden code.
    WardenCheckRowInput glueDispatch = MakeRow(build, localeHex, 65537,
        WardenCheckType::Mem, 80, WardenEvidenceClass::IntegrityInvariant);
    glueDispatch.address = 4631212;
    glueDispatch.length = 16;
    glueDispatch.expectedHex = "1CA9460029A9460036A946009EA94600";
    rows.push_back(glueDispatch);

    WardenCheckRowInput frameDispatch = MakeRow(build, localeHex, 65538,
        WardenCheckType::Mem, 90, WardenEvidenceClass::IntegrityInvariant);
    frameDispatch.address = frameDispatchAddress;
    frameDispatch.length = 16;
    frameDispatch.expectedHex = frameDispatchExpectedHex;
    rows.push_back(frameDispatch);
}

/** Exact database rows intended for all supported Classic Windows profiles. */
inline std::vector<WardenCheckRowInput> InitialWardenRows()
{
    std::vector<WardenCheckRowInput> rows;
    rows.reserve(72);
    AppendInitialProfile(rows, 5875, "656E5553",
        "7D88154D3411811985F5D81177C5453248133443", "4F6B6179",
        6392064,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E824DA1A005DC20800",
        8151558, 4803152, "A1C0EACE00", 8445948, 4784584,
        "5EFF48006BFF480078FF480095FF4800");
    AppendInitialProfile(rows, 5875, "6B6F4B52",
        "755D6D7F49BB34114433386D559261ED3AA23F00", "ED9995EC9DB8",
        6392064,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E824DA1A005DC20800",
        8151558, 4803152, "A1C0EACE00", 8445948, 4784584,
        "5EFF48006BFF480078FF480095FF4800");
    AppendInitialProfile(rows, 5875, "7A685457",
        "2A70E6402A40A4F9E9960CED419DBA5E6DEB8536", "E7A2BAE5AE9A",
        6392064,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E824DA1A005DC20800",
        8151558, 4803152, "A1C0EACE00", 8445948, 4784584,
        "5EFF48006BFF480078FF480095FF4800");
    AppendInitialProfile(rows, 5875, "66724652",
        "AF2D81AF013A9BA6BB92CE171E43FA903C9E8C09", "4F4B",
        6392064,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E824DA1A005DC20800",
        8151558, 4803152, "A1C0EACE00", 8445948, 4784584,
        "5EFF48006BFF480078FF480095FF4800");
    AppendInitialProfile(rows, 5875, "65734553",
        "1ECEC2C6596B8411FA5FE153EDFB9A6EE43360E9", "41636570746172",
        6392064,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E824DA1A005DC20800",
        8151558, 4803152, "A1C0EACE00", 8445948, 4784584,
        "5EFF48006BFF480078FF480095FF4800");
    AppendInitialProfile(rows, 6005, "656E4742",
        "7D88154D3411811985F5D81177C5453248133443", "4F6B6179",
        6392064,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E864DA1A005DC20800",
        8151622, 4803152, "A1C0EACE00", 8445948, 4784584,
        "5EFF48006BFF480078FF480095FF4800");
    AppendInitialProfile(rows, 6005, "64654445",
        "A0B3DC2D78AD892F2436BCD937BE51B4989D64C1", "4F4B",
        6392064,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E864DA1A005DC20800",
        8151622, 4803152, "A1C0EACE00", 8445948, 4784584,
        "5EFF48006BFF480078FF480095FF4800");
    AppendInitialProfile(rows, 6141, "7A68434E",
        "C5A1DE4C1CD412EB4D2E02AFAB6131B737EFCAF0", "E7A1AEE5AE9A",
        6401184,
        "558BEC8B51408B450C81E2FF7DA075508950108B450850E864EB1A005DC20800",
        8165094, 4806720, "A1E031CF00", 8462780, 4788152,
        "4E0D49005B0D4900680D4900850D4900");
    return rows;
}

inline WardenCheckCatalog BuildInitialWardenCatalog()
{
    WardenCheckCatalogBuilder builder;
    WardenCheckDiagnostic diagnostic;
    for (WardenCheckRowInput const& row : InitialWardenRows())
    {
        if (builder.Add(row, diagnostic) != CheckCatalogValidation::Valid)
            return WardenCheckCatalog();
    }

    WardenCheckCatalog catalog;
    if (builder.Build(catalog, diagnostic) != CheckCatalogValidation::Valid)
        return WardenCheckCatalog();
    return catalog;
}
}
}

#endif
