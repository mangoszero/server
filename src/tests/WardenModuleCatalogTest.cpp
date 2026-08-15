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

#include "WardenModuleCatalog.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

TEST(WardenCatalog_selects_only_the_three_exact_windows_builds)
{
    warden::WardenModuleCatalog catalog;

    CHECK(catalog.Find(5875, "Win") != nullptr);
    CHECK(catalog.Find(6005, "Win") != nullptr);
    CHECK(catalog.Find(6141, "Win") != nullptr);
    CHECK(catalog.Find(5875, "OSX") == nullptr);
    CHECK(catalog.Find(6005, "OSX") == nullptr);
    CHECK(catalog.Find(6141, "OSX") == nullptr);
    CHECK(catalog.Find(9999, "Win") == nullptr);
}

TEST(WardenCatalog_exact_module_identity_is_custody_pinned)
{
    warden::WardenModuleCatalog catalog;
    warden::ModuleProfile const* profile = catalog.Find(5875, "Win");

    REQUIRE(profile != nullptr);
    CHECK_EQ(profile->module.size, 18756u);
    CHECK_HEX(profile->moduleId.data(), profile->moduleId.size(),
        "79c0768d657977d697e10bad956cced1");
    CHECK_HEX(profile->moduleSha256.data(), profile->moduleSha256.size(),
        "6c68006a2f1fd31e7208204b3f7ceb94a6ce977876e13f2f703e9cd644482289");
    CHECK(catalog.Validate(*profile) == warden::ModuleValidation::Valid);
}

TEST(WardenCatalog_exact_5875_initialization_callbacks_are_custody_pinned)
{
    warden::WardenModuleCatalog catalog;
    warden::ModuleProfile const* profile = catalog.Find(5875, "Win");

    REQUIRE(profile != nullptr);
    CHECK_HEX(profile->initialization.archive.selectors.data(),
        profile->initialization.archive.selectors.size(), "01000200");
    CHECK_EQ(profile->initialization.archive.openRva, uint32(0x002477A0));
    CHECK_EQ(profile->initialization.archive.sizeRva, uint32(0x002487F0));
    CHECK_EQ(profile->initialization.archive.readRva, uint32(0x00248460));
    CHECK_EQ(profile->initialization.archive.closeRva, uint32(0x00248730));
    CHECK_HEX(profile->initialization.lua.prefix.data(),
        profile->initialization.lua.prefix.size(), "040000");
    CHECK_EQ(profile->initialization.lua.callbackRva, uint32(0x00303BF0));
    CHECK_EQ(profile->initialization.lua.selector, uint8(0));
    CHECK_HEX(profile->initialization.timing.prefix.data(),
        profile->initialization.timing.prefix.size(), "010100");
    CHECK_EQ(profile->initialization.timing.callbackRva, uint32(0x0002C010));
    CHECK_EQ(profile->initialization.timing.install, uint8(1));

    warden::ModuleProfile invalid = *profile;
    invalid.initialization.archive.closeRva = 0;
    CHECK(catalog.Validate(invalid) ==
        warden::ModuleValidation::InvalidInitialization);
}

TEST(WardenCatalog_exact_crossbuild_initialization_callbacks_are_custody_pinned)
{
    warden::WardenModuleCatalog catalog;
    warden::ModuleProfile const* profile6005 = catalog.Find(6005, "Win");
    warden::ModuleProfile const* profile6141 = catalog.Find(6141, "Win");

    REQUIRE(profile6005 != nullptr);
    CHECK_EQ(profile6005->initialization.archive.openRva,
        uint32(0x002477A0));
    CHECK_EQ(profile6005->initialization.archive.sizeRva,
        uint32(0x002487F0));
    CHECK_EQ(profile6005->initialization.archive.readRva,
        uint32(0x00248460));
    CHECK_EQ(profile6005->initialization.archive.closeRva,
        uint32(0x00248730));
    CHECK_EQ(profile6005->initialization.lua.callbackRva,
        uint32(0x00303C20));
    CHECK_EQ(profile6005->initialization.timing.callbackRva,
        uint32(0x0002C010));

    REQUIRE(profile6141 != nullptr);
    CHECK_EQ(profile6141->initialization.archive.openRva,
        uint32(0x00249B40));
    CHECK_EQ(profile6141->initialization.archive.sizeRva,
        uint32(0x0024AB90));
    CHECK_EQ(profile6141->initialization.archive.readRva,
        uint32(0x0024A800));
    CHECK_EQ(profile6141->initialization.archive.closeRva,
        uint32(0x0024AAD0));
    CHECK_EQ(profile6141->initialization.lua.callbackRva,
        uint32(0x00305FC0));
    CHECK_EQ(profile6141->initialization.timing.callbackRva,
        uint32(0x0002C010));

    CHECK(profile6005->module.data == profile6141->module.data);
    CHECK(profile6005->module.data == catalog.Find(5875, "Win")->module.data);
    CHECK(profile6005->moduleId == profile6141->moduleId);
    CHECK(profile6005->moduleSha256 == profile6141->moduleSha256);
    CHECK(profile6005->moduleKey == profile6141->moduleKey);
    CHECK(profile6005->hashSeed == profile6141->hashSeed);
    CHECK(profile6005->clientKeySeedHash == profile6141->clientKeySeedHash);
    CHECK(profile6005->clientKeySeed == profile6141->clientKeySeed);
    CHECK(profile6005->serverKeySeed == profile6141->serverKeySeed);
}

TEST(WardenCatalog_rejects_a_corrupted_module_copy)
{
    warden::WardenModuleCatalog catalog;
    warden::ModuleProfile const* profile = catalog.Find(5875, "Win");

    REQUIRE(profile != nullptr);
    warden::ModuleProfile corrupted = *profile;
    std::vector<uint8> bytes(corrupted.module.data,
        corrupted.module.data + corrupted.module.size);
    bytes[801] ^= 0x80;
    corrupted.module = {bytes.data(), bytes.size()};

    CHECK(catalog.Validate(corrupted) ==
        warden::ModuleValidation::DigestMismatch);
}

TEST(WardenProtocol_admission_move_transfers_then_cleanses_the_source)
{
    warden::AdmissionData source;
    source.build = 5875;
    source.platform = "Win";
    source.clientLocale = "enGB";
    source.sessionKey.fill(0xA5);
    source.available = true;

    warden::AdmissionData moved(std::move(source));

    CHECK_EQ(moved.build, 5875u);
    CHECK_STR(moved.platform, "Win");
    CHECK_STR(moved.clientLocale, "enGB");
    CHECK(std::all_of(moved.sessionKey.begin(), moved.sessionKey.end(),
        [](uint8 value) { return value == 0xA5; }));
    CHECK(moved.available);
    CHECK_EQ(source.build, 0u);
    CHECK(source.platform.empty());
    CHECK(source.clientLocale.empty());
    CHECK(std::all_of(source.sessionKey.begin(), source.sessionKey.end(),
        [](uint8 value) { return value == 0; }));
    CHECK(!source.available);
}

TEST(WardenProtocol_clear_removes_pending_credentials)
{
    warden::AdmissionData admission;
    admission.build = 5875;
    admission.platform = "Win";
    admission.clientLocale = "enGB";
    admission.sessionKey.fill(0x5A);
    admission.available = true;

    admission.Clear();

    CHECK_EQ(admission.build, 0u);
    CHECK(admission.platform.empty());
    CHECK(admission.clientLocale.empty());
    CHECK(std::all_of(admission.sessionKey.begin(), admission.sessionKey.end(),
        [](uint8 value) { return value == 0; }));
    CHECK(!admission.available);
}

TEST(WardenProtocol_failure_names_are_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::WardenState::ModuleReady), "ModuleReady");
    CHECK_STR(warden::ToString(warden::WardenFailure::HashMismatch), "HashMismatch");
}
