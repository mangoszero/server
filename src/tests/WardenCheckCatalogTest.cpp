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

#include "TestHarness.h"

#include "WardenCheckCatalog.h"

#include <string>
#include <vector>

TEST(WardenCheckCatalog_selects_only_exact_5875_windows_enUS_profile)
{
    warden::WardenCheckCatalog catalog;
    warden::MpqCheckProfile const* profile =
        catalog.FindMpq(5875, "Win", "enUS");

    REQUIRE(profile != nullptr);
    CHECK_EQ(profile->checkId, uint32(1));
    CHECK_STR(profile->path.c_str(), "DBFilesClient\\AreaTable.dbc");
    CHECK_HEX(profile->expectedSha1.data(), profile->expectedSha1.size(),
        "7d88154d3411811985f5d81177c5453248133443");
    CHECK(catalog.Validate(*profile) ==
        warden::CheckCatalogValidation::Valid);

    CHECK(catalog.FindMpq(6005, "Win", "enUS") == nullptr);
    CHECK(catalog.FindMpq(6141, "Win", "enUS") == nullptr);
    CHECK(catalog.FindMpq(5875, "OSX", "enUS") == nullptr);
    CHECK(catalog.FindMpq(5875, "Win", "enGB") == nullptr);
    CHECK(catalog.FindMpq(5875, "Win", "frFR") == nullptr);
}

TEST(WardenCheckCatalog_rejects_zero_id_empty_and_embedded_nul_paths)
{
    warden::WardenCheckCatalog catalog;
    warden::MpqCheckProfile profile =
        *catalog.FindMpq(5875, "Win", "enUS");

    profile.checkId = 0;
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidId);

    profile = *catalog.FindMpq(5875, "Win", "enUS");
    profile.path.clear();
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidPath);

    profile.path.assign("DBFiles\0Client", 14);
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidPath);
}

TEST(WardenCheckCatalog_enforces_the_one_byte_path_length_boundary)
{
    warden::WardenCheckCatalog catalog;
    warden::MpqCheckProfile profile =
        *catalog.FindMpq(5875, "Win", "enUS");

    profile.path.assign(255, 'A');
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::Valid);

    profile.path.push_back('B');
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidPath);

    profile = *catalog.FindMpq(5875, "Win", "enUS");
    profile.expectedSha1.fill(0);
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::Valid);
}

TEST(WardenCheckCatalog_selects_only_exact_5875_windows_enUS_lua_profile)
{
    warden::WardenCheckCatalog catalog;
    warden::LuaCheckProfile const* profile =
        catalog.FindLua(5875, "Win", "enUS");

    REQUIRE(profile != nullptr);
    CHECK_EQ(profile->checkId, uint32(2));
    CHECK_STR(profile->query.c_str(), "OKAY");
    CHECK_STR(profile->expectedText.c_str(), "Okay");
    CHECK(catalog.Validate(*profile) ==
        warden::CheckCatalogValidation::Valid);

    CHECK(catalog.FindLua(6005, "Win", "enUS") == nullptr);
    CHECK(catalog.FindLua(6141, "Win", "enUS") == nullptr);
    CHECK(catalog.FindLua(5875, "OSX", "enUS") == nullptr);
    CHECK(catalog.FindLua(5875, "Win", "enGB") == nullptr);
    CHECK(catalog.FindLua(5875, "Win", "frFR") == nullptr);
}

TEST(WardenCheckCatalog_rejects_invalid_lua_identity_and_text)
{
    warden::WardenCheckCatalog catalog;
    warden::LuaCheckProfile profile =
        *catalog.FindLua(5875, "Win", "enUS");

    profile.checkId = 0;
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidId);

    profile = *catalog.FindLua(5875, "Win", "enUS");
    profile.query.clear();
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidQuery);

    profile.query.assign("OK\0AY", 5);
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidQuery);

    profile = *catalog.FindLua(5875, "Win", "enUS");
    profile.expectedText.assign("Ok\0ay", 5);
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidExpectedText);
}

TEST(WardenCheckCatalog_enforces_lua_query_and_result_boundaries)
{
    warden::WardenCheckCatalog catalog;
    warden::LuaCheckProfile profile =
        *catalog.FindLua(5875, "Win", "enUS");

    profile.query.assign(255, 'Q');
    profile.expectedText.assign(64, 'R');
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::Valid);

    profile.query.push_back('Q');
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidQuery);

    profile = *catalog.FindLua(5875, "Win", "enUS");
    profile.expectedText.assign(65, 'R');
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidExpectedText);

    profile.expectedText.clear();
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::Valid);
}

TEST(WardenCheckCatalog_selects_four_exact_mem_checks_per_supported_profile)
{
    warden::WardenCheckCatalog catalog;

    std::vector<warden::MemCheckProfile> const* checks5875 =
        catalog.FindMem(5875, "Win", "enUS");
    REQUIRE(checks5875 != nullptr);
    REQUIRE(checks5875->size() == 4u);
    CHECK_EQ((*checks5875)[0].checkId, uint32(1107));
    CHECK((*checks5875)[0].moduleName.empty());
    CHECK_EQ((*checks5875)[0].addressOrRva, uint32(0x00618900));
    CHECK_HEX((*checks5875)[0].expectedBytes.data(),
        (*checks5875)[0].expectedBytes.size(),
        "558bec8b51408b450c81e2ff7da075508950108b450850e824da1a005dc20800");
    CHECK_EQ((*checks5875)[1].checkId, uint32(827));
    CHECK_EQ((*checks5875)[1].addressOrRva, uint32(0x007C6206));
    CHECK_HEX((*checks5875)[1].expectedBytes.data(),
        (*checks5875)[1].expectedBytes.size(),
        "25ffffdffb0d00200000894640");
    CHECK_EQ((*checks5875)[2].checkId, uint32(1566));
    CHECK((*checks5875)[2].moduleName.empty());
    CHECK_EQ((*checks5875)[2].addressOrRva, uint32(0x00494A50));
    CHECK_HEX((*checks5875)[2].expectedBytes.data(),
        (*checks5875)[2].expectedBytes.size(), "a1c0eace00");
    CHECK_EQ((*checks5875)[3].checkId, uint32(1135));
    CHECK((*checks5875)[3].moduleName.empty());
    CHECK_EQ((*checks5875)[3].addressOrRva, uint32(0x0080DFFC));
    CHECK_HEX((*checks5875)[3].expectedBytes.data(),
        (*checks5875)[3].expectedBytes.size(), "bb8d243f");

    std::vector<warden::MemCheckProfile> const* checks6005 =
        catalog.FindMem(6005, "Win", "enGB");
    REQUIRE(checks6005 != nullptr);
    REQUIRE(checks6005->size() == 4u);
    CHECK_EQ((*checks6005)[0].addressOrRva, uint32(0x00618900));
    CHECK_HEX((*checks6005)[0].expectedBytes.data(),
        (*checks6005)[0].expectedBytes.size(),
        "558bec8b51408b450c81e2ff7da075508950108b450850e864da1a005dc20800");
    CHECK_EQ((*checks6005)[1].addressOrRva, uint32(0x007C6246));
    CHECK_EQ((*checks6005)[2].checkId, uint32(1566));
    CHECK((*checks6005)[2].moduleName.empty());
    CHECK_EQ((*checks6005)[2].addressOrRva, uint32(0x00494A50));
    CHECK_HEX((*checks6005)[2].expectedBytes.data(),
        (*checks6005)[2].expectedBytes.size(), "a1c0eace00");
    CHECK_EQ((*checks6005)[3].checkId, uint32(1135));
    CHECK((*checks6005)[3].moduleName.empty());
    CHECK_EQ((*checks6005)[3].addressOrRva, uint32(0x0080DFFC));
    CHECK_HEX((*checks6005)[3].expectedBytes.data(),
        (*checks6005)[3].expectedBytes.size(), "bb8d243f");

    std::vector<warden::MemCheckProfile> const* checks6141 =
        catalog.FindMem(6141, "Win", "zhCN");
    REQUIRE(checks6141 != nullptr);
    REQUIRE(checks6141->size() == 4u);
    CHECK_EQ((*checks6141)[0].addressOrRva, uint32(0x0061ACA0));
    CHECK_HEX((*checks6141)[0].expectedBytes.data(),
        (*checks6141)[0].expectedBytes.size(),
        "558bec8b51408b450c81e2ff7da075508950108b450850e864eb1a005dc20800");
    CHECK_EQ((*checks6141)[1].addressOrRva, uint32(0x007C96E6));
    CHECK_EQ((*checks6141)[2].checkId, uint32(1566));
    CHECK((*checks6141)[2].moduleName.empty());
    CHECK_EQ((*checks6141)[2].addressOrRva, uint32(0x00495840));
    CHECK_HEX((*checks6141)[2].expectedBytes.data(),
        (*checks6141)[2].expectedBytes.size(), "a1e031cf00");
    CHECK_EQ((*checks6141)[3].checkId, uint32(1135));
    CHECK((*checks6141)[3].moduleName.empty());
    CHECK_EQ((*checks6141)[3].addressOrRva, uint32(0x008121BC));
    CHECK_HEX((*checks6141)[3].expectedBytes.data(),
        (*checks6141)[3].expectedBytes.size(), "bb8d243f");

    CHECK(catalog.FindMem(5875, "Win", "frFR") == nullptr);
    CHECK(catalog.FindMem(6005, "Win", "enUS") == nullptr);
    CHECK(catalog.FindMem(6141, "Win", "enUS") == nullptr);
    CHECK(catalog.FindMem(6141, "OSX", "zhCN") == nullptr);
    CHECK(catalog.FindMem(9999, "Win", "enUS") == nullptr);
}

TEST(WardenCheckCatalog_rejects_invalid_mem_profiles_and_duplicate_ids)
{
    warden::WardenCheckCatalog catalog;
    std::vector<warden::MemCheckProfile> profiles =
        *catalog.FindMem(5875, "Win", "enUS");

    warden::MemCheckProfile profile = profiles[0];
    profile.checkId = 0;
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidId);

    profile = profiles[0];
    profile.addressOrRva = 0;
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidAddress);

    profile = profiles[0];
    profile.moduleName.assign("WoW\0.exe", 8);
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidModuleName);
    profile.moduleName.assign(256, 'M');
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidModuleName);

    profile = profiles[0];
    profile.expectedBytes.clear();
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidExpectedBytes);
    profile.expectedBytes.assign(256, 0);
    CHECK(catalog.Validate(profile) ==
        warden::CheckCatalogValidation::InvalidExpectedBytes);

    CHECK(catalog.Validate(profiles) ==
        warden::CheckCatalogValidation::Valid);
    profiles[1].checkId = profiles[0].checkId;
    CHECK(catalog.Validate(profiles) ==
        warden::CheckCatalogValidation::DuplicateId);
    profiles.clear();
    CHECK(catalog.Validate(profiles) ==
        warden::CheckCatalogValidation::InvalidId);
}
