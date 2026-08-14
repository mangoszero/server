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
#include "WardenModuleCatalog.h"
#include "WardenPacketCodec.h"

#include <algorithm>
#include <limits>
#include <variant>

namespace
{
warden::ByteView View(warden::Bytes const& bytes)
{
    return {bytes.empty() ? nullptr : bytes.data(), bytes.size()};
}

warden::ModuleProfile const* Windows5875Profile()
{
    static warden::WardenModuleCatalog const catalog;
    return catalog.Find(5875, "Win");
}

warden::MpqCheckProfile TestMpqProfile()
{
    warden::MpqCheckProfile profile;
    profile.checkId = 1;
    profile.path = "DBFilesClient\\AreaTable.dbc";
    profile.expectedSha1 =
    {
        0x7D, 0x88, 0x15, 0x4D, 0x34, 0x11, 0x81, 0x19,
        0x85, 0xF5, 0xD8, 0x11, 0x77, 0xC5, 0x45, 0x32,
        0x48, 0x13, 0x34, 0x43
    };
    return profile;
}

warden::LuaCheckProfile TestLuaProfile()
{
    return {2, "OKAY", "Okay"};
}

warden::CheckPlan TimingPlan()
{
    warden::CheckPlan plan;
    plan.requestId = 1;
    plan.checks.emplace_back(warden::TimingCheck{});
    return plan;
}

warden::CheckPlan TimingMpqPlan()
{
    warden::CheckPlan plan = TimingPlan();
    plan.checks.emplace_back(TestMpqProfile());
    return plan;
}

warden::CheckPlan TimingLuaPlan()
{
    warden::CheckPlan plan = TimingPlan();
    plan.checks.emplace_back(TestLuaProfile());
    return plan;
}

warden::CheckPlan TimingMpqLuaPlan()
{
    warden::CheckPlan plan = TimingMpqPlan();
    plan.checks.emplace_back(TestLuaProfile());
    return plan;
}

void CheckInvalidPlanLeavesOutput(warden::ModuleProfile const& profile,
    warden::CheckPlan const& plan)
{
    warden::Bytes output{0xA5};
    CHECK(warden::EncodeCheckRequest(profile, plan, output) ==
        warden::EncodeStatus::InvalidPlan);
    REQUIRE(output.size() == 1u);
    CHECK_EQ(output[0], uint8(0xA5));
}

void CheckFailedDecodeLeavesOutput(warden::ByteView body,
    warden::CheckPlan const& plan, warden::DecodeStatus expected)
{
    warden::CheckBatchResult output;
    output.checks.emplace_back(warden::TimingResult{false, 0xA5A5A5A5});
    CHECK(warden::DecodeCheckResult(body, plan, output) == expected);
    REQUIRE(output.checks.size() == 1u);
    REQUIRE(std::holds_alternative<warden::TimingResult>(output.checks[0]));
    CHECK_EQ(std::get<warden::TimingResult>(output.checks[0]).clientTick,
        uint32(0xA5A5A5A5));
}
}

TEST(WardenPacket_encodes_exact_module_use_and_hash_request)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);

    warden::Bytes const moduleUse = warden::EncodeModuleUse(*profile);
    CHECK_HEX(moduleUse.data(), moduleUse.size(),
        "0079c0768d657977d697e10bad956cced1"
        "ae25bc51063b77bd363c3efe0fc173f9"
        "44490000");

    warden::Bytes const hashRequest = warden::EncodeHashRequest(*profile);
    CHECK_HEX(hashRequest.data(), hashRequest.size(),
        "054d808d2c77d905c41a6380ec08586afe");
}

TEST(WardenPacket_encodes_exact_5875_module_initialization)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);

    warden::Bytes initialization{0xA5};
    REQUIRE(warden::EncodeModuleInitialize(*profile, initialization) ==
        warden::EncodeStatus::Ok);
    REQUIRE(initialization.size() == 57u);
    CHECK_HEX(initialization.data(), initialization.size(),
        "031400693d8dd001000200a0772400f0872400"
        "6084240030872400"
        "030800f72df4f0040000f03b300000"
        "030800672f4d0a01010010c0020001");

    CHECK_EQ(initialization[0], uint8(warden::ServerCommand::ModuleInitialize));
    CHECK_HEX(initialization.data() + 1, 2, "1400");
    CHECK_HEX(initialization.data() + 3, 4, "693d8dd0");
    CHECK_EQ(initialization[27], uint8(warden::ServerCommand::ModuleInitialize));
    CHECK_HEX(initialization.data() + 28, 2, "0800");
    CHECK_HEX(initialization.data() + 30, 4, "f72df4f0");
    CHECK_EQ(initialization[42], uint8(warden::ServerCommand::ModuleInitialize));
    CHECK_HEX(initialization.data() + 43, 2, "0800");
    CHECK_HEX(initialization.data() + 45, 4, "672f4d0a");
}

TEST(WardenPacket_invalid_initialization_does_not_replace_output)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);

    warden::ModuleProfile invalid = *profile;
    invalid.initialization.lua.callbackRva = 0;
    warden::Bytes output{0xA5};

    CHECK(warden::EncodeModuleInitialize(invalid, output) ==
        warden::EncodeStatus::InvalidProfile);
    REQUIRE(output.size() == 1u);
    CHECK_EQ(output[0], uint8(0xA5));
}

TEST(WardenPacket_encodes_exact_timing_only_and_timing_mpq_requests)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);

    warden::Bytes request{0xA5};
    REQUIRE(warden::EncodeCheckRequest(*profile, TimingPlan(), request) ==
        warden::EncodeStatus::Ok);
    CHECK_HEX(request.data(), request.size(), "0200287f");

    REQUIRE(warden::EncodeCheckRequest(*profile, TimingMpqPlan(), request) ==
        warden::EncodeStatus::Ok);
    REQUIRE(request.size() == 34u);
    CHECK_HEX(request.data(), request.size(),
        "021b444246696c6573436c69656e745c41"
        "7265615461626c652e6462630028e7017f");
}

TEST(WardenPacket_encodes_exact_timing_lua_and_combined_requests)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);

    warden::Bytes request{0xA5};
    REQUIRE(warden::EncodeCheckRequest(*profile, TimingLuaPlan(), request) ==
        warden::EncodeStatus::Ok);
    REQUIRE(request.size() == 11u);
    CHECK_HEX(request.data(), request.size(),
        "02044f4b41590028f4017f");

    REQUIRE(warden::EncodeCheckRequest(*profile, TimingMpqLuaPlan(), request) ==
        warden::EncodeStatus::Ok);
    REQUIRE(request.size() == 41u);
    CHECK_HEX(request.data(), request.size(),
        "021b444246696c6573436c69656e745c41"
        "7265615461626c652e646263044f4b4159"
        "0028e701f4027f");
}

TEST(WardenPacket_rejects_invalid_check_plans_without_replacing_output)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);

    warden::CheckPlan plan = TimingPlan();
    plan.requestId = 0;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = {};
    plan.requestId = 1;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan.checks.emplace_back(TestMpqProfile());
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingPlan();
    plan.checks.emplace_back(warden::TimingCheck{});
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    plan.checks.emplace_back(TestMpqProfile());
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1]).checkId = 0;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1]).path.clear();
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1]).path.assign(
        "DBFiles\0Client", 14);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1]).path.assign(256, 'A');
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    plan.checks.emplace_back(TestLuaProfile());
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1]).checkId = 0;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1]).query.clear();
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1]).query.assign(
        "OK\0AY", 5);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1]).query.assign(256, 'Q');
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1]).expectedText.assign(
        "Ok\0ay", 5);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1]).expectedText.assign(
        65, 'R');
    CheckInvalidPlanLeavesOutput(*profile, plan);
}

TEST(WardenPacket_decodes_exact_timing_mpq_result_vectors_in_plan_order)
{
    warden::Bytes const success =
    {
        0x02, 0x1A, 0x00, 0x88, 0xBD, 0xFA, 0xEB,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x7D, 0x88, 0x15,
        0x4D, 0x34, 0x11, 0x81, 0x19, 0x85, 0xF5, 0xD8, 0x11,
        0x77, 0xC5, 0x45, 0x32, 0x48, 0x13, 0x34, 0x43
    };
    warden::CheckBatchResult result;
    REQUIRE(warden::DecodeCheckResult(View(success), TimingMpqPlan(), result) ==
        warden::DecodeStatus::Ok);
    REQUIRE(result.checks.size() == 2u);
    REQUIRE(std::holds_alternative<warden::TimingResult>(result.checks[0]));
    warden::TimingResult const& timing =
        std::get<warden::TimingResult>(result.checks[0]);
    CHECK(timing.stable);
    CHECK_EQ(timing.clientTick, uint32(0x01020304));
    REQUIRE(std::holds_alternative<warden::MpqResult>(result.checks[1]));
    warden::MpqResult const& mpq =
        std::get<warden::MpqResult>(result.checks[1]);
    CHECK(mpq.status == warden::MpqResultStatus::Success);
    CHECK_HEX(mpq.digest.data(), mpq.digest.size(),
        "7d88154d3411811985f5d81177c5453248133443");

    warden::Bytes const unavailable =
    {
        0x02, 0x06, 0x00, 0xC0, 0x6D, 0xA5, 0x67,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x01
    };
    REQUIRE(warden::DecodeCheckResult(View(unavailable), TimingMpqPlan(),
        result) == warden::DecodeStatus::Ok);
    REQUIRE(result.checks.size() == 2u);
    REQUIRE(std::holds_alternative<warden::MpqResult>(result.checks[1]));
    CHECK(std::get<warden::MpqResult>(result.checks[1]).status ==
        warden::MpqResultStatus::Unavailable);

    warden::Bytes const mismatch =
    {
        0x02, 0x1A, 0x00, 0x0F, 0x45, 0x48, 0x02,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    REQUIRE(warden::DecodeCheckResult(View(mismatch), TimingMpqPlan(),
        result) == warden::DecodeStatus::Ok);
    warden::MpqResult const& mismatchResult =
        std::get<warden::MpqResult>(result.checks[1]);
    CHECK(mismatchResult.status == warden::MpqResultStatus::Success);
    CHECK(std::all_of(mismatchResult.digest.begin(), mismatchResult.digest.end(),
        [](uint8 value) { return value == 0; }));
}

TEST(WardenPacket_decodes_the_existing_timing_only_result_vector)
{
    warden::Bytes const response =
    {
        0x02, 0x05, 0x00, 0xA7, 0xD4, 0x3E,
        0x25, 0x01, 0x78, 0x56, 0x34, 0x12
    };
    warden::CheckBatchResult result;
    REQUIRE(warden::DecodeCheckResult(View(response), TimingPlan(), result) ==
        warden::DecodeStatus::Ok);
    REQUIRE(result.checks.size() == 1u);
    warden::TimingResult const& timing =
        std::get<warden::TimingResult>(result.checks[0]);
    CHECK(timing.stable);
    CHECK_EQ(timing.clientTick, uint32(0x12345678));
}

TEST(WardenPacket_decodes_exact_lua_result_vectors_in_plan_order)
{
    warden::Bytes const success =
    {
        0x02, 0x0B, 0x00, 0x8E, 0xF7, 0x55, 0x15,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x04,
        0x4F, 0x6B, 0x61, 0x79
    };
    warden::CheckBatchResult result;
    REQUIRE(warden::DecodeCheckResult(View(success), TimingLuaPlan(), result) ==
        warden::DecodeStatus::Ok);
    REQUIRE(result.checks.size() == 2u);
    REQUIRE(std::holds_alternative<warden::LuaResult>(result.checks[1]));
    warden::LuaResult const& lua =
        std::get<warden::LuaResult>(result.checks[1]);
    CHECK(lua.status == warden::LuaResultStatus::Success);
    CHECK_STR(lua.text.c_str(), "Okay");

    warden::Bytes const unavailable =
    {
        0x02, 0x06, 0x00, 0xC0, 0x6D, 0xA5, 0x67,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x01
    };
    REQUIRE(warden::DecodeCheckResult(View(unavailable), TimingLuaPlan(),
        result) == warden::DecodeStatus::Ok);
    REQUIRE(result.checks.size() == 2u);
    REQUIRE(std::holds_alternative<warden::LuaResult>(result.checks[1]));
    CHECK(std::get<warden::LuaResult>(result.checks[1]).status ==
        warden::LuaResultStatus::Unavailable);
    CHECK(std::get<warden::LuaResult>(result.checks[1]).text.empty());

    warden::Bytes const mismatch =
    {
        0x02, 0x0A, 0x00, 0xE1, 0x54, 0x1A, 0xB3,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x03,
        0x42, 0x61, 0x64
    };
    REQUIRE(warden::DecodeCheckResult(View(mismatch), TimingLuaPlan(),
        result) == warden::DecodeStatus::Ok);
    CHECK_STR(std::get<warden::LuaResult>(result.checks[1]).text.c_str(),
        "Bad");

    warden::Bytes maximum =
    {
        0x02, 0x47, 0x00, 0xC3, 0xC7, 0x1E, 0xBE,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x40
    };
    maximum.insert(maximum.end(), 64, uint8('A'));
    REQUIRE(warden::DecodeCheckResult(View(maximum), TimingLuaPlan(), result) ==
        warden::DecodeStatus::Ok);
    CHECK_EQ(std::get<warden::LuaResult>(result.checks[1]).text.size(),
        size_t(64));
}

TEST(WardenPacket_decodes_exact_combined_timing_mpq_lua_result)
{
    warden::Bytes const success =
    {
        0x02, 0x20, 0x00, 0x37, 0x6F, 0x4E, 0x37,
        0x01, 0x04, 0x03, 0x02, 0x01,
        0x00, 0x7D, 0x88, 0x15, 0x4D, 0x34, 0x11, 0x81,
        0x19, 0x85, 0xF5, 0xD8, 0x11, 0x77, 0xC5, 0x45,
        0x32, 0x48, 0x13, 0x34, 0x43,
        0x00, 0x04, 0x4F, 0x6B, 0x61, 0x79
    };
    warden::CheckBatchResult result;
    REQUIRE(warden::DecodeCheckResult(View(success), TimingMpqLuaPlan(),
        result) == warden::DecodeStatus::Ok);
    REQUIRE(result.checks.size() == 3u);
    CHECK(std::holds_alternative<warden::TimingResult>(result.checks[0]));
    CHECK(std::holds_alternative<warden::MpqResult>(result.checks[1]));
    REQUIRE(std::holds_alternative<warden::LuaResult>(result.checks[2]));
    CHECK_STR(std::get<warden::LuaResult>(result.checks[2]).text.c_str(),
        "Okay");
}

TEST(WardenPacket_rejects_malformed_lua_results_without_partial_output)
{
    warden::CheckPlan const plan = TimingLuaPlan();

    warden::Bytes const invalidStatus =
    {
        0x02, 0x06, 0x00, 0x8A, 0xFC, 0x74, 0xC1,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x02
    };
    CheckFailedDecodeLeavesOutput(View(invalidStatus), plan,
        warden::DecodeStatus::InvalidValue);

    warden::Bytes tooLong =
    {
        0x02, 0x48, 0x00, 0x26, 0x87, 0xBE, 0x12,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x41
    };
    tooLong.insert(tooLong.end(), 65, uint8('A'));
    CheckFailedDecodeLeavesOutput(View(tooLong), plan,
        warden::DecodeStatus::InvalidValue);

    warden::Bytes const truncatedText =
    {
        0x02, 0x0A, 0x00, 0x58, 0x4B, 0x67, 0xA2,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x04,
        0x4F, 0x6B, 0x61
    };
    CheckFailedDecodeLeavesOutput(View(truncatedText), plan,
        warden::DecodeStatus::WrongSize);

    warden::Bytes const unavailableWithTrailingByte =
    {
        0x02, 0x07, 0x00, 0xCB, 0xFD, 0x42, 0x40,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x01, 0x00
    };
    CheckFailedDecodeLeavesOutput(View(unavailableWithTrailingByte), plan,
        warden::DecodeStatus::WrongSize);

    warden::Bytes withOuterTrailingByte =
    {
        0x02, 0x0B, 0x00, 0x8E, 0xF7, 0x55, 0x15,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x04,
        0x4F, 0x6B, 0x61, 0x79, 0x00
    };
    CheckFailedDecodeLeavesOutput(View(withOuterTrailingByte), plan,
        warden::DecodeStatus::WrongSize);

    warden::Bytes const mpqSuccess =
    {
        0x02, 0x1A, 0x00, 0x88, 0xBD, 0xFA, 0xEB,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x7D, 0x88, 0x15,
        0x4D, 0x34, 0x11, 0x81, 0x19, 0x85, 0xF5, 0xD8, 0x11,
        0x77, 0xC5, 0x45, 0x32, 0x48, 0x13, 0x34, 0x43
    };
    CheckFailedDecodeLeavesOutput(View(mpqSuccess), plan,
        warden::DecodeStatus::InvalidValue);
}

TEST(WardenPacket_rejects_malformed_check_results_without_partial_output)
{
    warden::Bytes const valid =
    {
        0x02, 0x1A, 0x00, 0x88, 0xBD, 0xFA, 0xEB,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x00, 0x7D, 0x88, 0x15,
        0x4D, 0x34, 0x11, 0x81, 0x19, 0x85, 0xF5, 0xD8, 0x11,
        0x77, 0xC5, 0x45, 0x32, 0x48, 0x13, 0x34, 0x43
    };
    warden::CheckPlan const plan = TimingMpqPlan();

    CheckFailedDecodeLeavesOutput({}, plan, warden::DecodeStatus::Empty);
    CheckFailedDecodeLeavesOutput({nullptr, valid.size()}, plan,
        warden::DecodeStatus::WrongSize);

    warden::Bytes malformed = valid;
    malformed[0] = uint8(warden::ClientCommand::HashResult);
    CheckFailedDecodeLeavesOutput(View(malformed), plan,
        warden::DecodeStatus::UnsupportedCommand);

    malformed = valid;
    malformed[1] = 0x19;
    CheckFailedDecodeLeavesOutput(View(malformed), plan,
        warden::DecodeStatus::WrongSize);

    malformed = valid;
    malformed[3] ^= 1;
    CheckFailedDecodeLeavesOutput(View(malformed), plan,
        warden::DecodeStatus::ChecksumMismatch);

    malformed = valid;
    malformed.push_back(0);
    CheckFailedDecodeLeavesOutput(View(malformed), plan,
        warden::DecodeStatus::WrongSize);

    for (size_t size = 1; size < valid.size(); ++size)
    {
        warden::Bytes truncated(valid.begin(), valid.begin() + size);
        warden::CheckBatchResult output;
        output.checks.emplace_back(warden::TimingResult{false, 0xA5A5A5A5});
        CHECK(warden::DecodeCheckResult(View(truncated), plan, output) !=
            warden::DecodeStatus::Ok);
        REQUIRE(output.checks.size() == 1u);
        CHECK_EQ(std::get<warden::TimingResult>(output.checks[0]).clientTick,
            uint32(0xA5A5A5A5));
    }

    // Checksums are independently derived so validation reaches each status.
    warden::Bytes const invalidTimingStatus =
    {
        0x02, 0x06, 0x00, 0x53, 0x4D, 0x72, 0x53,
        0x02, 0x04, 0x03, 0x02, 0x01, 0x01
    };
    CheckFailedDecodeLeavesOutput(View(invalidTimingStatus), plan,
        warden::DecodeStatus::InvalidValue);

    warden::Bytes const invalidMpqStatus =
    {
        0x02, 0x06, 0x00, 0x8A, 0xFC, 0x74, 0xC1,
        0x01, 0x04, 0x03, 0x02, 0x01, 0x02
    };
    CheckFailedDecodeLeavesOutput(View(invalidMpqStatus), plan,
        warden::DecodeStatus::InvalidValue);
}

TEST(WardenPacket_encodes_cache_lengths_explicitly_little_endian)
{
    warden::Bytes chunk500(500);
    for (size_t i = 0; i < chunk500.size(); ++i)
        chunk500[i] = uint8(i);

    warden::Bytes const encoded500 = warden::EncodeModuleCache(View(chunk500));
    REQUIRE(encoded500.size() == chunk500.size() + 3);
    CHECK_HEX(encoded500.data(), 3, "01f401");
    CHECK(std::equal(chunk500.begin(), chunk500.end(), encoded500.begin() + 3));

    warden::Bytes chunk256(256, 0xA5);
    warden::Bytes const encoded256 = warden::EncodeModuleCache(View(chunk256));
    REQUIRE(encoded256.size() == chunk256.size() + 3);
    CHECK_HEX(encoded256.data(), 3, "010001");
    CHECK(std::equal(chunk256.begin(), chunk256.end(), encoded256.begin() + 3));
}

TEST(WardenPacket_rejects_invalid_cache_chunk_sizes)
{
    warden::Bytes empty;
    CHECK(warden::EncodeModuleCache(View(empty)).empty());

    warden::Bytes oversized(size_t(std::numeric_limits<uint16>::max()) + 1, 0);
    CHECK(warden::EncodeModuleCache(View(oversized)).empty());
}

TEST(WardenPacket_decodes_only_exact_supported_client_shapes)
{
    for (uint8 command : {uint8(warden::ClientCommand::ModuleMissing),
             uint8(warden::ClientCommand::ModuleOk),
             uint8(warden::ClientCommand::ModuleFailed)})
    {
        warden::Bytes body = {command};
        warden::ClientMessage message;
        CHECK(warden::DecodeClient(View(body), message) ==
            warden::DecodeStatus::Ok);
        CHECK(uint8(message.command) == command);
    }

    warden::Bytes hashBody(21);
    hashBody[0] = uint8(warden::ClientCommand::HashResult);
    for (size_t i = 1; i < hashBody.size(); ++i)
        hashBody[i] = uint8(i);

    warden::ClientMessage hashMessage;
    CHECK(warden::DecodeClient(View(hashBody), hashMessage) ==
        warden::DecodeStatus::Ok);
    CHECK(hashMessage.command == warden::ClientCommand::HashResult);
    CHECK(std::equal(hashBody.begin() + 1, hashBody.end(),
        hashMessage.hash.begin()));
}

TEST(WardenPacket_rejects_empty_wrong_sized_and_unsupported_client_shapes)
{
    warden::ClientMessage message;
    CHECK(warden::DecodeClient({}, message) == warden::DecodeStatus::Empty);

    warden::Bytes shortHash = {uint8(warden::ClientCommand::HashResult)};
    CHECK(warden::DecodeClient(View(shortHash), message) ==
        warden::DecodeStatus::WrongSize);
    shortHash.resize(20, 0);
    CHECK(warden::DecodeClient(View(shortHash), message) ==
        warden::DecodeStatus::WrongSize);
    shortHash.resize(22, 0);
    CHECK(warden::DecodeClient(View(shortHash), message) ==
        warden::DecodeStatus::WrongSize);

    warden::Bytes trailing = {uint8(warden::ClientCommand::ModuleOk), 0};
    CHECK(warden::DecodeClient(View(trailing), message) ==
        warden::DecodeStatus::WrongSize);

    for (uint8 command : {uint8(2), uint8(3), uint8(0xFF)})
    {
        warden::Bytes body = {command};
        CHECK(warden::DecodeClient(View(body), message) ==
            warden::DecodeStatus::UnsupportedCommand);
    }
}
