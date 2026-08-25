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
#include "WardenCheckFixtures.h"

#include <algorithm>
#include <string>
#include <variant>
#include <vector>

namespace
{
warden::CheckCatalogValidation AddOne(
    warden::WardenCheckRowInput const& row)
{
    warden::WardenCheckCatalogBuilder builder;
    warden::WardenCheckDiagnostic diagnostic;
    return builder.Add(row, diagnostic);
}

warden::CheckCatalogValidation BuildRows(
    std::vector<warden::WardenCheckRowInput> const& rows,
    warden::WardenCheckCatalog& catalog)
{
    warden::WardenCheckCatalogBuilder builder;
    warden::WardenCheckDiagnostic diagnostic;
    for (warden::WardenCheckRowInput const& row : rows)
    {
        warden::CheckCatalogValidation const validation =
            builder.Add(row, diagnostic);
        if (validation != warden::CheckCatalogValidation::Valid)
            return validation;
    }
    return builder.Build(catalog, diagnostic);
}

warden::CheckCatalogValidation BuildRows(
    std::vector<warden::WardenCheckRowInput> const& rows)
{
    warden::WardenCheckCatalog catalog;
    return BuildRows(rows, catalog);
}

std::vector<warden::WardenCheckRowInput> FirstProfileRows()
{
    std::vector<warden::WardenCheckRowInput> rows =
        warden::test::InitialWardenRows();
    rows.resize(9);
    return rows;
}
}

TEST(WardenCheckCatalog_decodes_and_selects_eight_exact_profiles)
{
    warden::WardenCheckCatalogBuilder builder;
    warden::WardenCheckDiagnostic diagnostic;
    for (warden::WardenCheckRowInput const& row :
        warden::test::InitialWardenRows())
    {
        REQUIRE(builder.Add(row, diagnostic) ==
            warden::CheckCatalogValidation::Valid);
    }

    warden::WardenCheckCatalog catalog;
    REQUIRE(builder.Build(catalog, diagnostic) ==
        warden::CheckCatalogValidation::Valid);
    CHECK_EQ(catalog.TotalRows(), uint32(72));
    CHECK_EQ(catalog.EnabledRows(), uint32(72));
    CHECK_EQ(catalog.Profiles().size(), size_t(8));

    struct ProfileExpectation
    {
        uint32 build;
        char const* locale;
        char const* mpqSha1;
        char const* luaText;
        uint32 frameDispatchAddress;
        char const* frameDispatchBytes;
    };
    ProfileExpectation const profiles[] =
    {
        {5875, "enUS", "7d88154d3411811985f5d81177c5453248133443",
            "4f6b6179", 4784584,
            "5eff48006bff480078ff480095ff4800"},
        {5875, "koKR", "755d6d7f49bb34114433386d559261ed3aa23f00",
            "ed9995ec9db8", 4784584,
            "5eff48006bff480078ff480095ff4800"},
        {5875, "zhTW", "2a70e6402a40a4f9e9960ced419dba5e6deb8536",
            "e7a2bae5ae9a", 4784584,
            "5eff48006bff480078ff480095ff4800"},
        {5875, "frFR", "af2d81af013a9ba6bb92ce171e43fa903c9e8c09",
            "4f4b", 4784584,
            "5eff48006bff480078ff480095ff4800"},
        {5875, "esES", "1ecec2c6596b8411fa5fe153edfb9a6ee43360e9",
            "41636570746172", 4784584,
            "5eff48006bff480078ff480095ff4800"},
        {6005, "enGB", "7d88154d3411811985f5d81177c5453248133443",
            "4f6b6179", 4784584,
            "5eff48006bff480078ff480095ff4800"},
        {6005, "deDE", "a0b3dc2d78ad892f2436bcd937be51b4989d64c1",
            "4f4b", 4784584,
            "5eff48006bff480078ff480095ff4800"},
        {6141, "zhCN", "c5a1de4c1cd412eb4d2e02afab6131b737efcaf0",
            "e7a1aee5ae9a", 4788152,
            "4e0d49005b0d4900680d4900850d4900"}
    };
    uint32 const expectedIds[] =
        {65536, 1, 2, 1107, 827, 1566, 1135, 65537, 65538};

    for (ProfileExpectation const& expected : profiles)
    {
        warden::WardenCheckProfile const* profile =
            catalog.Find(expected.build, "Win", expected.locale);
        REQUIRE(profile != nullptr);
        REQUIRE(profile->checks.size() == 9u);
        CHECK(profile->hasActionableChecks);
        CHECK_EQ(profile->totalRows, uint32(9));
        for (size_t index = 0; index < 9u; ++index)
        {
            CHECK_EQ(warden::GetWardenCheckId(profile->checks[index]),
                expectedIds[index]);
        }

        CHECK(warden::GetWardenCheckType(profile->checks[0]) ==
            warden::WardenCheckType::Timing);
        CHECK(!warden::IsConfirmationEligible(profile->checks[0]));
        CHECK(warden::IsConfirmationEligible(profile->checks[1]));

        warden::MpqCheckProfile const& mpq =
            std::get<warden::MpqCheckProfile>(profile->checks[1].payload);
        CHECK_HEX(mpq.expectedSha1.data(), mpq.expectedSha1.size(),
            expected.mpqSha1);
        warden::LuaCheckProfile const& lua =
            std::get<warden::LuaCheckProfile>(profile->checks[2].payload);
        CHECK_HEX(reinterpret_cast<uint8 const*>(lua.expectedText.data()),
            lua.expectedText.size(), expected.luaText);

        warden::MemCheckProfile const& glue =
            std::get<warden::MemCheckProfile>(profile->checks[7].payload);
        CHECK_EQ(glue.addressOrRva, uint32(4631212));
        CHECK_HEX(glue.expectedBytes.data(), glue.expectedBytes.size(),
            "1ca9460029a9460036a946009ea94600");
        CHECK(profile->checks[7].evidenceClass ==
            warden::WardenEvidenceClass::IntegrityInvariant);

        warden::MemCheckProfile const& frame =
            std::get<warden::MemCheckProfile>(profile->checks[8].payload);
        CHECK_EQ(frame.addressOrRva, expected.frameDispatchAddress);
        CHECK_HEX(frame.expectedBytes.data(), frame.expectedBytes.size(),
            expected.frameDispatchBytes);
        CHECK(profile->checks[8].evidenceClass ==
            warden::WardenEvidenceClass::IntegrityInvariant);
    }

    CHECK(warden::IsActionableEvidenceClass(
        warden::WardenEvidenceClass::IntegrityInvariant));
    CHECK(warden::IsActionableEvidenceClass(
        warden::WardenEvidenceClass::ThreatSignature));
    CHECK(!warden::IsActionableEvidenceClass(
        warden::WardenEvidenceClass::ProtocolHealth));
    CHECK(!warden::IsActionableEvidenceClass(
        warden::WardenEvidenceClass::Corroboration));

    CHECK(catalog.Find(5875, "Win", "itIT") == nullptr);
    CHECK(catalog.Find(6005, "Win", "enUS") == nullptr);
    CHECK(catalog.Find(6141, "OSX", "zhCN") == nullptr);
}

TEST(WardenCheckCatalog_preserves_embedded_zero_bytes)
{
    std::vector<warden::WardenCheckRowInput> rows = FirstProfileRows();
    rows.resize(2);
    rows[1] = warden::test::MakeRow(5875, "656E5553", 9001,
        warden::WardenCheckType::Mem, 20,
        warden::WardenEvidenceClass::IntegrityInvariant);
    rows[1].address = 0x00400000;
    rows[1].length = 3;
    rows[1].expectedHex = "A100B2";

    warden::WardenCheckCatalog catalog;
    REQUIRE(BuildRows(rows, catalog) ==
        warden::CheckCatalogValidation::Valid);
    warden::WardenCheckProfile const* profile =
        catalog.Find(5875, "Win", "enUS");
    REQUIRE(profile != nullptr);
    REQUIRE(profile->checks.size() == 2u);
    warden::MemCheckProfile const& decoded =
        std::get<warden::MemCheckProfile>(profile->checks[1].payload);
    REQUIRE(decoded.expectedBytes.size() == 3u);
    CHECK_HEX(decoded.expectedBytes.data(), decoded.expectedBytes.size(),
        "a100b2");
}

TEST(WardenCheckCatalog_rejects_noncanonical_hex)
{
    warden::WardenCheckRowInput row = FirstProfileRows()[3];
    row.expectedHex = "ABC";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidHex);
    row.expectedHex = "GG";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidHex);
    row.expectedHex = "AA BB";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidHex);
}

TEST(WardenCheckCatalog_rejects_invalid_profile_identity)
{
    warden::WardenCheckRowInput row = FirstProfileRows()[3];
    row.build = 0;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidBuild);
    row.build = 65536;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidBuild);

    row = FirstProfileRows()[3];
    row.platformHex.clear();
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidPlatform);
    row.platformHex = "4142434445";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidPlatform);
    row.platformHex = "5700696E";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidPlatform);
    row.platformHex = "57696E20";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidPlatform);

    row = FirstProfileRows()[3];
    row.localeHex = "656E55";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidLocale);
    row.localeHex = "656E555300";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidLocale);
    row.localeHex = "65005553";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidLocale);
    row.localeHex = "656E2053";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidLocale);
}

TEST(WardenCheckCatalog_rejects_invalid_scalar_fields_before_narrowing)
{
    warden::WardenCheckRowInput row = FirstProfileRows()[3];
    row.checkId = 0;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidId);
    row = FirstProfileRows()[3];
    row.enabled = 2;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidEnabled);
    row.enabled = 256;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidEnabled);
    row = FirstProfileRows()[3];
    row.sortOrder = 65536;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidSortOrder);
    row = FirstProfileRows()[3];
    row.type = 1;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidType);
    row.type = 0x1F3;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidType);
    row = FirstProfileRows()[3];
    row.evidenceClass = 4;
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidEvidenceClass);
    row.evidenceClass = 259;
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidEvidenceClass);
}

TEST(WardenCheckCatalog_enforces_timing_contract_and_cardinality)
{
    warden::WardenCheckRowInput row = FirstProfileRows()[0];
    row.moduleHex = "41";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[0];
    row.requestHex = "41";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[0];
    row.expectedHex = "41";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[0];
    row.address = 1;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[0];
    row.length = 1;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[0];
    row.evidenceClass = 1;
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::IllegalTypeEvidenceClass);

    std::vector<warden::WardenCheckRowInput> rows = FirstProfileRows();
    rows[0].enabled = 0;
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::DisabledTiming);
    rows = FirstProfileRows();
    warden::WardenCheckRowInput secondTiming = rows[0];
    secondTiming.checkId = 65539;
    secondTiming.sortOrder = 11;
    rows.push_back(secondTiming);
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::MultipleTiming);
}

TEST(WardenCheckCatalog_enforces_mpq_contract)
{
    warden::WardenCheckRowInput row = FirstProfileRows()[1];
    row.requestHex.clear();
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidPath);
    row.requestHex = "410042";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidPath);
    row.requestHex.assign(512, '4');
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidPath);
    row = FirstProfileRows()[1];
    row.expectedHex.assign(38, 'A');
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidExpectedBytes);
    row = FirstProfileRows()[1];
    row.moduleHex = "41";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[1];
    row.address = 1;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[1];
    row.length = 1;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
}

TEST(WardenCheckCatalog_enforces_lua_contract)
{
    warden::WardenCheckRowInput row = FirstProfileRows()[2];
    row.requestHex.clear();
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidQuery);
    row.requestHex = "410042";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidQuery);
    row.requestHex.assign(512, '5');
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidQuery);
    row = FirstProfileRows()[2];
    row.expectedHex.clear();
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidExpectedText);
    row.expectedHex = "410042";
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidExpectedText);
    row.expectedHex.assign(130, '6');
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidExpectedText);
    row = FirstProfileRows()[2];
    row.moduleHex = "41";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[2];
    row.address = 1;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[2];
    row.length = 1;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
}

TEST(WardenCheckCatalog_enforces_mem_contract)
{
    warden::WardenCheckRowInput row = FirstProfileRows()[3];
    row.address = 0;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidAddress);
    row = FirstProfileRows()[3];
    row.moduleHex = "410042";
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidModuleName);
    row.moduleHex.assign(512, '4');
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidModuleName);
    row = FirstProfileRows()[3];
    row.length = 0;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidLength);
    row.length = 256;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidLength);
    row = FirstProfileRows()[3];
    row.requestHex = "41";
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidUnusedField);
    row = FirstProfileRows()[3];
    row.expectedHex.resize(row.expectedHex.size() - 2);
    CHECK(AddOne(row) ==
        warden::CheckCatalogValidation::InvalidExpectedBytes);
    row = FirstProfileRows()[3];
    row.address = 0xFFFFFFF0u;
    CHECK(AddOne(row) == warden::CheckCatalogValidation::InvalidAddress);
}

TEST(WardenCheckCatalog_pins_legal_type_evidence_class_pairs)
{
    warden::WardenCheckRowInput timing = FirstProfileRows()[0];
    for (uint32 value = 0; value <= 3; ++value)
    {
        timing.evidenceClass = value;
        CHECK(AddOne(timing) == (value == 0 ?
            warden::CheckCatalogValidation::Valid :
            warden::CheckCatalogValidation::IllegalTypeEvidenceClass));
    }

    warden::WardenCheckRowInput mpq = FirstProfileRows()[1];
    for (uint32 value = 0; value <= 3; ++value)
    {
        mpq.evidenceClass = value;
        bool const legal = value == 1 || value == 3;
        CHECK(AddOne(mpq) == (legal ? warden::CheckCatalogValidation::Valid :
            warden::CheckCatalogValidation::IllegalTypeEvidenceClass));
    }

    warden::WardenCheckRowInput lua = FirstProfileRows()[2];
    for (uint32 value = 0; value <= 3; ++value)
    {
        lua.evidenceClass = value;
        CHECK(AddOne(lua) == (value == 3 ?
            warden::CheckCatalogValidation::Valid :
            warden::CheckCatalogValidation::IllegalTypeEvidenceClass));
    }

    warden::WardenCheckRowInput mem = FirstProfileRows()[3];
    for (uint32 value = 0; value <= 3; ++value)
    {
        mem.evidenceClass = value;
        CHECK(AddOne(mem) == (value == 0 ?
            warden::CheckCatalogValidation::IllegalTypeEvidenceClass :
            warden::CheckCatalogValidation::Valid));
    }
}

TEST(WardenCheckCatalog_rejects_duplicate_ids_and_sort_orders_when_disabled)
{
    std::vector<warden::WardenCheckRowInput> rows = FirstProfileRows();
    rows[2].checkId = rows[1].checkId;
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::DuplicateId);
    rows = FirstProfileRows();
    rows[2].sortOrder = rows[1].sortOrder;
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::DuplicateSortOrder);
    rows = FirstProfileRows();
    rows[2].enabled = 0;
    rows[2].checkId = rows[1].checkId;
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::DuplicateId);
    rows = FirstProfileRows();
    rows[2].enabled = 0;
    rows[2].sortOrder = rows[1].sortOrder;
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::DuplicateSortOrder);
}

TEST(WardenCheckCatalog_rejects_conflicting_expectations_for_one_request)
{
    std::vector<warden::WardenCheckRowInput> rows = FirstProfileRows();
    warden::WardenCheckRowInput duplicate = rows[1];
    duplicate.checkId = 9001;
    duplicate.sortOrder = 21;
    duplicate.expectedHex[0] = duplicate.expectedHex[0] == '0' ? '1' : '0';
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::ConflictingRequestExpectation);

    rows = FirstProfileRows();
    duplicate = rows[2];
    duplicate.checkId = 9002;
    duplicate.sortOrder = 31;
    duplicate.expectedHex[0] = duplicate.expectedHex[0] == '0' ? '1' : '0';
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::ConflictingRequestExpectation);

    rows = FirstProfileRows();
    duplicate = rows[3];
    duplicate.checkId = 9003;
    duplicate.sortOrder = 41;
    duplicate.expectedHex[0] = duplicate.expectedHex[0] == '0' ? '1' : '0';
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::ConflictingRequestExpectation);

    rows = FirstProfileRows();
    duplicate = rows[3];
    duplicate.checkId = 9004;
    duplicate.sortOrder = 41;
    duplicate.length /= 2;
    duplicate.expectedHex.resize(duplicate.length * 2);
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::Valid);

    rows = FirstProfileRows();
    duplicate = rows[3];
    duplicate.checkId = 9005;
    duplicate.sortOrder = 41;
    duplicate.address += 2;
    duplicate.length = 4;
    duplicate.expectedHex = "FFFFFFFF";
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::ConflictingRequestExpectation);

    rows = FirstProfileRows();
    duplicate = rows[3];
    duplicate.checkId = 9008;
    duplicate.sortOrder = 41;
    duplicate.address -= 2;
    duplicate.length = 4;
    duplicate.expectedHex = "0000" + rows[3].expectedHex.substr(0, 4);
    duplicate.expectedHex[4] =
        duplicate.expectedHex[4] == '0' ? '1' : '0';
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::ConflictingRequestExpectation);

    rows = FirstProfileRows();
    duplicate = rows[3];
    duplicate.checkId = 9006;
    duplicate.sortOrder = 41;
    duplicate.address += 2;
    duplicate.length = 4;
    duplicate.expectedHex = rows[3].expectedHex.substr(4, 8);
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::Valid);

    rows = FirstProfileRows();
    warden::WardenCheckRowInput moduleName = rows[3];
    moduleName.checkId = 9009;
    moduleName.sortOrder = 41;
    moduleName.moduleHex = "576F772E657865";
    rows.push_back(moduleName);
    duplicate = moduleName;
    duplicate.checkId = 9010;
    duplicate.sortOrder = 42;
    duplicate.moduleHex = "776F772E657865";
    duplicate.expectedHex[0] =
        duplicate.expectedHex[0] == '0' ? '1' : '0';
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::ConflictingRequestExpectation);

    rows = FirstProfileRows();
    duplicate = rows[1];
    duplicate.checkId = 9007;
    duplicate.sortOrder = 21;
    duplicate.enabled = 0;
    duplicate.expectedHex[0] = duplicate.expectedHex[0] == '0' ? '1' : '0';
    rows.push_back(duplicate);
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::Valid);
}

TEST(WardenCheckCatalog_enforces_complete_profiles_and_atomic_build)
{
    std::vector<warden::WardenCheckRowInput> rows;
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::EmptyCatalog);
    rows = FirstProfileRows();
    rows.erase(rows.begin());
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::MissingTiming);
    rows = FirstProfileRows();
    rows[0].enabled = 0;
    CHECK(BuildRows(rows) == warden::CheckCatalogValidation::DisabledTiming);
    rows = FirstProfileRows();
    for (size_t index = 1; index < rows.size(); ++index)
        rows[index].enabled = 0;
    CHECK(BuildRows(rows) ==
        warden::CheckCatalogValidation::MissingNonHealth);

    warden::WardenCheckRowInput malformed = FirstProfileRows()[3];
    malformed.enabled = 0;
    malformed.address = 0;
    CHECK(AddOne(malformed) ==
        warden::CheckCatalogValidation::InvalidAddress);

    rows = FirstProfileRows();
    rows.resize(3);
    rows[1].enabled = 0;
    warden::WardenCheckCatalog observationOnly;
    REQUIRE(BuildRows(rows, observationOnly) ==
        warden::CheckCatalogValidation::Valid);
    warden::WardenCheckProfile const* profile =
        observationOnly.Find(5875, "Win", "enUS");
    REQUIRE(profile != nullptr);
    CHECK(!profile->hasActionableChecks);
    CHECK_EQ(profile->totalRows, uint32(3));
    CHECK_EQ(profile->checks.size(), size_t(2));
    CHECK_EQ(observationOnly.EnabledRows(), uint32(2));

    warden::WardenCheckCatalog unchanged =
        warden::test::BuildInitialWardenCatalog();
    REQUIRE(unchanged.TotalRows() == 72u);
    rows = FirstProfileRows();
    rows[2].checkId = rows[1].checkId;
    CHECK(BuildRows(rows, unchanged) ==
        warden::CheckCatalogValidation::DuplicateId);
    CHECK_EQ(unchanged.TotalRows(), uint32(72));
    CHECK(unchanged.Find(6141, "Win", "zhCN") != nullptr);
}

TEST(WardenCheckCatalog_exposes_stable_validation_names)
{
    CHECK_STR(warden::ToString(warden::CheckCatalogValidation::Valid),
        "Valid");
    CHECK_STR(warden::ToString(
        warden::CheckCatalogValidation::IllegalTypeEvidenceClass),
        "IllegalTypeEvidenceClass");
    CHECK_STR(warden::ToString(
        warden::CheckCatalogValidation::MissingNonHealth),
        "MissingNonHealth");
    CHECK_STR(warden::ToString(
        warden::CheckCatalogValidation::ConflictingRequestExpectation),
        "ConflictingRequestExpectation");
}
