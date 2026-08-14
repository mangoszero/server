/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "TestHarness.h"

#include "WardenModuleCatalog.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

TEST(WardenCatalog_selects_only_windows_build_5875)
{
    warden::WardenModuleCatalog catalog;

    CHECK(catalog.Find(5875, "Win") != nullptr);
    CHECK(catalog.Find(5875, "OSX") == nullptr);
    CHECK(catalog.Find(6005, "Win") == nullptr);
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
    source.sessionKey.fill(0xA5);
    source.available = true;

    warden::AdmissionData moved(std::move(source));

    CHECK_EQ(moved.build, 5875u);
    CHECK_STR(moved.platform, "Win");
    CHECK(std::all_of(moved.sessionKey.begin(), moved.sessionKey.end(),
        [](uint8 value) { return value == 0xA5; }));
    CHECK(moved.available);
    CHECK_EQ(source.build, 0u);
    CHECK(source.platform.empty());
    CHECK(std::all_of(source.sessionKey.begin(), source.sessionKey.end(),
        [](uint8 value) { return value == 0; }));
    CHECK(!source.available);
}

TEST(WardenProtocol_clear_removes_pending_credentials)
{
    warden::AdmissionData admission;
    admission.build = 5875;
    admission.platform = "Win";
    admission.sessionKey.fill(0x5A);
    admission.available = true;

    admission.Clear();

    CHECK_EQ(admission.build, 0u);
    CHECK(admission.platform.empty());
    CHECK(std::all_of(admission.sessionKey.begin(), admission.sessionKey.end(),
        [](uint8 value) { return value == 0; }));
    CHECK(!admission.available);
}

TEST(WardenProtocol_failure_names_are_secret_free_fixed_labels)
{
    CHECK_STR(warden::ToString(warden::WardenState::ModuleReady), "ModuleReady");
    CHECK_STR(warden::ToString(warden::WardenFailure::HashMismatch), "HashMismatch");
}
