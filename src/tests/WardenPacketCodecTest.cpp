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
#include "WardenModuleCatalog.h"
#include "WardenPacketCodec.h"
#include "PacketCodec.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <variant>

namespace
{
warden::ByteView View(warden::Bytes const& bytes)
{
    return {bytes.empty() ? nullptr : bytes.data(), bytes.size()};
}

warden::Bytes FromHex(char const* text)
{
    auto nibble = [](char value) -> uint8
    {
        if (value >= '0' && value <= '9')
            return uint8(value - '0');
        if (value >= 'a' && value <= 'f')
            return uint8(value - 'a' + 10);
        return uint8(value - 'A' + 10);
    };

    size_t const length = std::strlen(text);
    warden::Bytes bytes;
    bytes.reserve(length / 2);
    for (size_t index = 0; index < length; index += 2)
    {
        bytes.push_back(uint8((nibble(text[index]) << 4) |
            nibble(text[index + 1])));
    }
    return bytes;
}

warden::ModuleProfile const* Windows5875Profile()
{
    static warden::WardenModuleCatalog const catalog;
    return catalog.Find(5875, "Win");
}

warden::ModuleProfile const* WindowsProfile(uint32 build)
{
    static warden::WardenModuleCatalog const catalog;
    return catalog.Find(build, "Win");
}

warden::WardenCheckCatalog const& TestCheckCatalog()
{
    static warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    return catalog;
}

warden::WardenCheckProfile const* TestCheckProfile(uint32 build,
    std::string const& locale)
{
    return TestCheckCatalog().Find(build, "Win", locale);
}

warden::WardenCheckDefinition TestDefinition(uint32 checkId)
{
    warden::WardenCheckProfile const* profile =
        TestCheckProfile(5875, "enUS");
    if (profile)
    {
        auto const found = std::find_if(profile->checks.begin(),
            profile->checks.end(), [checkId](auto const& definition)
            {
                return warden::GetWardenCheckId(definition) == checkId;
            });
        if (found != profile->checks.end())
            return *found;
    }
    return {};
}

warden::CheckPlan TimingPlan()
{
    warden::CheckPlan plan;
    plan.requestId = 1;
    plan.checks.push_back(TestDefinition(65536));
    return plan;
}

warden::CheckPlan TimingMpqPlan()
{
    warden::CheckPlan plan = TimingPlan();
    plan.checks.push_back(TestDefinition(1));
    return plan;
}

warden::CheckPlan TimingLuaPlan()
{
    warden::CheckPlan plan = TimingPlan();
    plan.checks.push_back(TestDefinition(2));
    return plan;
}

warden::CheckPlan TimingMpqLuaPlan()
{
    warden::CheckPlan plan = TimingMpqPlan();
    plan.checks.push_back(TestDefinition(2));
    return plan;
}

warden::CheckPlan TimingMemPlan(uint32 build, std::string const& locale)
{
    warden::WardenCheckProfile const* profile =
        TestCheckProfile(build, locale);
    warden::CheckPlan plan;
    plan.requestId = 1;
    if (profile)
    {
        for (warden::WardenCheckDefinition const& check : profile->checks)
        {
            if (warden::GetWardenCheckType(check) ==
                    warden::WardenCheckType::Timing ||
                warden::GetWardenCheckType(check) ==
                    warden::WardenCheckType::Mem)
                plan.checks.push_back(check);
        }
    }
    return plan;
}

warden::CheckPlan TimingSingleMemPlan()
{
    warden::CheckPlan plan = TimingPlan();
    plan.checks.push_back(TestDefinition(827));
    return plan;
}

warden::CheckPlan ConfirmationMemPlan()
{
    warden::CheckPlan plan;
    plan.requestId = 7;
    plan.purpose = warden::CheckPlanPurpose::Confirmation;
    plan.checks.push_back(TestDefinition(1566));
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

TEST(WardenPacket_encodes_exact_6005_and_6141_module_initialization)
{
    warden::ModuleProfile const* profile6005 = WindowsProfile(6005);
    warden::ModuleProfile const* profile6141 = WindowsProfile(6141);
    REQUIRE(profile6005 != nullptr);
    REQUIRE(profile6141 != nullptr);

    warden::Bytes initialization;
    REQUIRE(warden::EncodeModuleInitialize(*profile6005, initialization) ==
        warden::EncodeStatus::Ok);
    CHECK_HEX(initialization.data(), initialization.size(),
        "031400693d8dd001000200a0772400f0872400"
        "6084240030872400"
        "0308003549fd5e040000203c300000"
        "030800672f4d0a01010010c0020001");

    REQUIRE(warden::EncodeModuleInitialize(*profile6141, initialization) ==
        warden::EncodeStatus::Ok);
    CHECK_HEX(initialization.data(), initialization.size(),
        "0314003f3d35ae01000200409b240090ab2400"
        "00a82400d0aa2400"
        "0308005d3e0413040000c05f300000"
        "030800672f4d0a01010010c0020001");
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

TEST(WardenPacket_encodes_exact_crossbuild_timing_and_mem_requests)
{
    struct Vector
    {
        uint32 build;
        char const* locale;
        char const* expected;
    };
    Vector const vectors[] =
    {
        {5875, "enUS",
            "0200288c0000896100208c0006627c000d8c00504a4900058c00fcdf8000047f"},
        {6005, "enGB",
            "0200288c0000896100208c0046627c000d8c00504a4900058c00fcdf8000047f"},
        {6141, "zhCN",
            "0200288c00a0ac6100208c00e6967c000d8c0040584900058c00bc218100047f"}
    };

    for (Vector const& vector : vectors)
    {
        warden::ModuleProfile const* profile = WindowsProfile(vector.build);
        REQUIRE(profile != nullptr);
        warden::Bytes request{0xA5};
        REQUIRE(warden::EncodeCheckRequest(*profile,
            TimingMemPlan(vector.build, vector.locale), request) ==
            warden::EncodeStatus::Ok);
        CHECK_HEX(request.data(), request.size(), vector.expected);
    }
}

TEST(WardenPacket_encodes_and_decodes_exact_mem_only_confirmation)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);
    warden::CheckPlan const plan = ConfirmationMemPlan();
    REQUIRE(plan.checks.size() == 1u);

    warden::Bytes encoded;
    REQUIRE(warden::EncodeCheckRequest(*profile, plan, encoded) ==
        warden::EncodeStatus::Ok);
    CHECK_HEX(encoded.data(), encoded.size(), "02008c00504a4900057f");

    warden::CheckBatchResult decoded;
    warden::Bytes const result =
        FromHex("0206005a6f3fca00a1c0eace00");
    REQUIRE(warden::DecodeCheckResult(View(result), plan, decoded) ==
        warden::DecodeStatus::Ok);
    REQUIRE(decoded.checks.size() == 1u);
    REQUIRE(std::holds_alternative<warden::MemResult>(decoded.checks[0]));
    warden::MemResult const& memory =
        std::get<warden::MemResult>(decoded.checks[0]);
    CHECK(memory.status == warden::MemResultStatus::Success);
    CHECK_HEX(memory.actualBytes.data(), memory.actualBytes.size(),
        "a1c0eace00");
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

    plan = TimingPlan();
    plan.checks.push_back(plan.checks[0]);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    plan.checks.push_back(TestDefinition(1));
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1].payload).checkId = 0;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1].payload).path.clear();
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1].payload).path.assign(
        "DBFiles\0Client", 14);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    std::get<warden::MpqCheckProfile>(plan.checks[1].payload).path.assign(
        256, 'A');
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    plan.checks.push_back(TestDefinition(2));
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1].payload).checkId = 0;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1].payload).query.clear();
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1].payload).query.assign(
        "OK\0AY", 5);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(plan.checks[1].payload).query.assign(
        256, 'Q');
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(
        plan.checks[1].payload).expectedText.assign(
        "Ok\0ay", 5);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingLuaPlan();
    std::get<warden::LuaCheckProfile>(
        plan.checks[1].payload).expectedText.assign(
        65, 'R');
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMemPlan(5875, "enUS");
    std::get<warden::MemCheckProfile>(plan.checks[1].payload).checkId = 0;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMemPlan(5875, "enUS");
    std::get<warden::MemCheckProfile>(
        plan.checks[1].payload).addressOrRva = 0;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMemPlan(5875, "enUS");
    std::get<warden::MemCheckProfile>(
        plan.checks[1].payload).expectedBytes.clear();
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMemPlan(5875, "enUS");
    std::get<warden::MemCheckProfile>(
        plan.checks[1].payload).expectedBytes.assign(
        256, 0);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMemPlan(5875, "enUS");
    std::get<warden::MemCheckProfile>(
        plan.checks[1].payload).moduleName.assign(
        "WoW\0.exe", 8);
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMemPlan(5875, "enUS");
    std::get<warden::MemCheckProfile>(plan.checks[2].payload).checkId =
        std::get<warden::MemCheckProfile>(
            plan.checks[1].payload).checkId;
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingMpqPlan();
    warden::WardenCheckDefinition duplicate = TestDefinition(1107);
    std::get<warden::MemCheckProfile>(duplicate.payload).checkId =
        std::get<warden::MpqCheckProfile>(plan.checks[1].payload).checkId;
    plan.checks.push_back(std::move(duplicate));
    CheckInvalidPlanLeavesOutput(*profile, plan);

    plan = TimingPlan();
    warden::WardenCheckDefinition oversized = TestDefinition(1107);
    std::get<warden::MemCheckProfile>(
        oversized.payload).expectedBytes.assign(255, 0);
    for (uint32 index = 0; index < 256; ++index)
    {
        std::get<warden::MemCheckProfile>(oversized.payload).checkId =
            1000 + index;
        plan.checks.push_back(oversized);
    }
    CheckInvalidPlanLeavesOutput(*profile, plan);
}

TEST(WardenPacket_decodes_exact_crossbuild_mem_result_vectors)
{
    struct Vector
    {
        uint32 build;
        char const* locale;
        char const* response;
    };
    Vector const vectors[] =
    {
        {5875, "enUS",
            "023f00833bdafb010403020100558bec8b51408b450c81e2ff7da07550"
            "8950108b450850e824da1a005dc208000025ffffdffb0d00200000894640"
            "00a1c0eace0000bb8d243f"},
        {6005, "enGB",
            "023f0053b1b911010403020100558bec8b51408b450c81e2ff7da07550"
            "8950108b450850e864da1a005dc208000025ffffdffb0d00200000894640"
            "00a1c0eace0000bb8d243f"},
        {6141, "zhCN",
            "023f0099e39fee010403020100558bec8b51408b450c81e2ff7da07550"
            "8950108b450850e864eb1a005dc208000025ffffdffb0d00200000894640"
            "00a1e031cf0000bb8d243f"}
    };

    for (Vector const& vector : vectors)
    {
        warden::Bytes const response = FromHex(vector.response);
        warden::CheckBatchResult result;
        REQUIRE(warden::DecodeCheckResult(View(response),
            TimingMemPlan(vector.build, vector.locale), result) ==
            warden::DecodeStatus::Ok);
        REQUIRE(result.checks.size() == 5u);
        size_t const expectedSizes[] = {32u, 13u, 5u, 4u};
        for (size_t index = 0; index < 4u; ++index)
        {
            REQUIRE(std::holds_alternative<warden::MemResult>(
                result.checks[index + 1u]));
            warden::MemResult const& mem =
                std::get<warden::MemResult>(result.checks[index + 1u]);
            CHECK(mem.status == warden::MemResultStatus::Success);
            CHECK_EQ(mem.actualBytes.size(), expectedSizes[index]);
        }
    }

    warden::Bytes const unavailable =
        FromHex("0209005848324d010403020101010101");
    warden::CheckBatchResult result;
    REQUIRE(warden::DecodeCheckResult(View(unavailable),
        TimingMemPlan(5875, "enUS"), result) == warden::DecodeStatus::Ok);
    REQUIRE(result.checks.size() == 5u);
    for (size_t index = 1; index < result.checks.size(); ++index)
    {
        warden::MemResult const& mem =
            std::get<warden::MemResult>(result.checks[index]);
        CHECK(mem.status == warden::MemResultStatus::Unavailable);
        CHECK(mem.actualBytes.empty());
    }
}

TEST(WardenPacket_rejects_malformed_mem_results_without_partial_output)
{
    warden::CheckPlan const plan = TimingSingleMemPlan();
    warden::Bytes const valid = FromHex(
        "021300c7c99e6e01040302010025ffffdffb0d00200000894640");

    for (size_t size = 1; size < valid.size(); ++size)
    {
        warden::Bytes const truncated(valid.begin(), valid.begin() + size);
        CheckFailedDecodeLeavesOutput(View(truncated), plan,
            warden::DecodeStatus::WrongSize);
    }

    CheckFailedDecodeLeavesOutput(
        View(FromHex("0206008afc74c1010403020102")), plan,
        warden::DecodeStatus::InvalidValue);
    CheckFailedDecodeLeavesOutput(
        View(FromHex("020700cbfd424001040302010100")), plan,
        warden::DecodeStatus::WrongSize);

    warden::Bytes wrongChecksum = valid;
    wrongChecksum[3] ^= 1;
    CheckFailedDecodeLeavesOutput(View(wrongChecksum), plan,
        warden::DecodeStatus::ChecksumMismatch);

    warden::Bytes trailing = valid;
    trailing.push_back(0);
    CheckFailedDecodeLeavesOutput(View(trailing), plan,
        warden::DecodeStatus::WrongSize);

    CheckFailedDecodeLeavesOutput(View(valid), TimingPlan(),
        warden::DecodeStatus::WrongSize);
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

TEST(WardenPacket_repeats_mpq_and_lua_definitions_with_shared_strings)
{
    warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    warden::WardenCheckProfile const* checks =
        catalog.Find(5875, "Win", "enUS");
    warden::ModuleProfile const* module = Windows5875Profile();
    REQUIRE(checks != nullptr);
    REQUIRE(module != nullptr);

    warden::WardenCheckDefinition mpqSecond = checks->checks[1];
    std::get<warden::MpqCheckProfile>(mpqSecond.payload).checkId = 3;
    mpqSecond.sortOrder = 25;
    warden::WardenCheckDefinition luaSecond = checks->checks[2];
    std::get<warden::LuaCheckProfile>(luaSecond.payload).checkId = 4;
    luaSecond.sortOrder = 35;

    warden::CheckPlan plan;
    plan.requestId = 1;
    plan.checks = {checks->checks[0], checks->checks[1], mpqSecond,
        checks->checks[2], luaSecond};
    warden::WardenCheckPlanBudget budget;
    REQUIRE(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::Valid);
    CHECK_EQ(budget.stringCount, size_t(2));
    CHECK_EQ(budget.stringTableBytes, size_t(34));
    CHECK_EQ(budget.requestBodyBytes, size_t(45));
    CHECK_EQ(budget.maximumResultBytes, size_t(179));

    warden::Bytes encoded;
    REQUIRE(warden::EncodeCheckRequest(*module, plan, encoded) ==
        warden::EncodeStatus::Ok);
    CHECK_HEX(encoded.data(), encoded.size(),
        "021b444246696c6573436c69656e745c417265615461626c652e646263"
        "044f4b41590028e701e701f402f4027f");

    warden::Bytes const response = FromHex(
        "0226007e858a430178563412"
        "007d88154d3411811985f5d81177c5453248133443"
        "0100044f6b61790003426164");
    warden::CheckBatchResult decoded;
    REQUIRE(warden::DecodeCheckResult(View(response), plan, decoded) ==
        warden::DecodeStatus::Ok);
    REQUIRE(decoded.checks.size() == 5u);
    CHECK(std::holds_alternative<warden::TimingResult>(decoded.checks[0]));
    CHECK(std::holds_alternative<warden::MpqResult>(decoded.checks[1]));
    CHECK(std::holds_alternative<warden::MpqResult>(decoded.checks[2]));
    CHECK(std::holds_alternative<warden::LuaResult>(decoded.checks[3]));
    CHECK(std::holds_alternative<warden::LuaResult>(decoded.checks[4]));
    CHECK(std::get<warden::MpqResult>(decoded.checks[2]).status ==
        warden::MpqResultStatus::Unavailable);
    CHECK_STR(std::get<warden::LuaResult>(decoded.checks[3]).text.c_str(),
        "Okay");
    CHECK_STR(std::get<warden::LuaResult>(decoded.checks[4]).text.c_str(),
        "Bad");
}

TEST(WardenPacket_inspects_plan_identity_and_confirmation_contracts)
{
    warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    warden::WardenCheckProfile const* profile =
        catalog.Find(5875, "Win", "enUS");
    REQUIRE(profile != nullptr);

    warden::CheckPlan plan;
    plan.requestId = 1;
    plan.checks = profile->checks;
    warden::WardenCheckPlanBudget budget;
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::Valid);

    plan.checks.push_back(plan.checks[1]);
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::DuplicateCheckId);
    plan.checks = profile->checks;
    warden::WardenCheckDefinition secondTiming = plan.checks[0];
    std::get<warden::TimingCheckProfile>(secondTiming.payload).checkId = 65537;
    secondTiming.sortOrder = 11;
    plan.checks.push_back(secondTiming);
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::DuplicateTiming);

    plan.purpose = warden::CheckPlanPurpose::Confirmation;
    plan.checks.clear();
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::InvalidConfirmation);
    plan.checks = {profile->checks[1], profile->checks[2]};
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::InvalidConfirmation);
    plan.checks = {profile->checks[0]};
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::InvalidConfirmation);
}

TEST(WardenPacket_inspects_exact_string_and_body_budget_boundaries)
{
    warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    warden::WardenCheckProfile const* profile =
        catalog.Find(5875, "Win", "enUS");
    REQUIRE(profile != nullptr);

    warden::CheckPlan plan;
    plan.requestId = 1;
    warden::WardenCheckDefinition mpq = profile->checks[1];
    for (uint32 index = 1; index <= 255; ++index)
    {
        warden::MpqCheckProfile& payload =
            std::get<warden::MpqCheckProfile>(mpq.payload);
        payload.checkId = 1000 + index;
        payload.path.assign(1, static_cast<char>(index));
        mpq.sortOrder = static_cast<uint16>(index);
        plan.checks.push_back(mpq);
    }
    warden::WardenCheckPlanBudget budget;
    REQUIRE(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::Valid);
    CHECK_EQ(budget.stringCount, size_t(255));
    CHECK_EQ(budget.stringTableBytes, size_t(511));
    std::get<warden::MpqCheckProfile>(mpq.payload).checkId = 2000;
    std::get<warden::MpqCheckProfile>(mpq.payload).path = "XX";
    plan.checks.push_back(mpq);
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::TooManyStrings);

    plan.checks.clear();
    for (uint32 index = 0; index < 2; ++index)
    {
        warden::MpqCheckProfile& payload =
            std::get<warden::MpqCheckProfile>(mpq.payload);
        payload.checkId = 3000 + index;
        payload.path.assign(255, static_cast<char>('A' + index));
        plan.checks.push_back(mpq);
    }
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::StringTableTooLarge);

    plan.checks.clear();
    warden::WardenCheckDefinition mem = profile->checks[3];
    std::get<warden::MemCheckProfile>(mem.payload).expectedBytes.assign(1,
        uint8(0x90));
    for (uint32 index = 0; index < 9362; ++index)
    {
        std::get<warden::MemCheckProfile>(mem.payload).checkId = 100000 + index;
        plan.checks.push_back(mem);
    }
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::RequestBodyTooLarge);

    plan.checks.clear();
    for (uint32 index = 0; index < 9361; ++index)
    {
        std::get<warden::MemCheckProfile>(mem.payload).checkId = 110000 + index;
        plan.checks.push_back(mem);
    }
    std::get<warden::MpqCheckProfile>(mpq.payload).checkId = 300000;
    std::get<warden::MpqCheckProfile>(mpq.payload).path.assign(255, 'P');
    plan.checks.push_back(mpq);
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::RequestBodyTooLarge);

    plan.checks.clear();
    warden::WardenCheckDefinition lua = profile->checks[2];
    std::get<warden::LuaCheckProfile>(lua.payload).query = "Q";
    std::get<warden::LuaCheckProfile>(lua.payload).expectedText.assign(64, 'R');
    for (uint32 index = 0; index < 1000; ++index)
    {
        std::get<warden::LuaCheckProfile>(lua.payload).checkId = 200000 + index;
        plan.checks.push_back(lua);
    }
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::ResultBodyTooLarge);
}

TEST(WardenPacket_enforces_client_transport_result_body_budget)
{
    warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    warden::WardenCheckProfile const* profile =
        catalog.Find(5875, "Win", "enUS");
    REQUIRE(profile != nullptr);
    REQUIRE(profile->checks.size() > 3u);

    warden::WardenCheckDefinition mem = profile->checks[3];
    REQUIRE(std::holds_alternative<warden::MemCheckProfile>(mem.payload));

    // The client packet size field counts its four-byte opcode. The encrypted
    // Warden body then adds command(1), result length(2), and checksum(4).
    size_t constexpr WardenResultEnvelopeBytes = 7;
    size_t constexpr MaximumResultBodyBytes =
        proto::MAX_CLIENT_PACKET_SIZE - sizeof(uint32) -
        WardenResultEnvelopeBytes;
    size_t constexpr OneByteMemResultBytes = 2;
    size_t constexpr TwoByteMemResultBytes = 3;
    size_t constexpr OneByteMemCount =
        (MaximumResultBodyBytes - TwoByteMemResultBytes) /
        OneByteMemResultBytes;

    warden::CheckPlan plan;
    plan.requestId = 1;
    plan.checks.reserve(OneByteMemCount + 1);
    warden::MemCheckProfile& payload =
        std::get<warden::MemCheckProfile>(mem.payload);
    payload.expectedBytes.assign(1, uint8(0x90));
    for (size_t index = 0; index < OneByteMemCount; ++index)
    {
        payload.checkId = uint32(400000 + index);
        plan.checks.push_back(mem);
    }
    payload.checkId = uint32(400000 + OneByteMemCount);
    payload.expectedBytes.push_back(uint8(0xCC));
    plan.checks.push_back(mem);

    warden::WardenCheckPlanBudget budget;
    REQUIRE(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::Valid);
    CHECK_EQ(budget.maximumResultBytes, MaximumResultBodyBytes);

    std::get<warden::MemCheckProfile>(
        plan.checks.back().payload).expectedBytes.push_back(uint8(0xCC));
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::ResultBodyTooLarge);
}

TEST(WardenPacket_failed_inspection_and_encoding_leave_outputs_unchanged)
{
    warden::WardenCheckCatalog const catalog =
        warden::test::BuildInitialWardenCatalog();
    warden::WardenCheckProfile const* profile =
        catalog.Find(5875, "Win", "enUS");
    warden::ModuleProfile const* module = Windows5875Profile();
    REQUIRE(profile != nullptr);
    REQUIRE(module != nullptr);

    warden::CheckPlan plan;
    plan.requestId = 1;
    plan.checks = {profile->checks[1], profile->checks[1]};
    warden::WardenCheckPlanBudget budget{9, 8, 7, 6};
    CHECK(warden::InspectCheckPlan(plan, budget) ==
        warden::CheckPlanValidation::DuplicateCheckId);
    CHECK_EQ(budget.stringCount, size_t(9));
    CHECK_EQ(budget.stringTableBytes, size_t(8));
    CHECK_EQ(budget.requestBodyBytes, size_t(7));
    CHECK_EQ(budget.maximumResultBytes, size_t(6));

    warden::Bytes output{0xA5};
    CHECK(warden::EncodeCheckRequest(*module, plan, output) ==
        warden::EncodeStatus::InvalidPlan);
    REQUIRE(output.size() == 1u);
    CHECK_EQ(output[0], uint8(0xA5));
}
