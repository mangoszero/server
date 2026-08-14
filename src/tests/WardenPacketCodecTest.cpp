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
#include "WardenPacketCodec.h"

#include <algorithm>
#include <limits>

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

TEST(WardenPacket_encodes_exact_timing_check_request)
{
    warden::ModuleProfile const* profile = Windows5875Profile();
    REQUIRE(profile != nullptr);

    warden::Bytes const request = warden::EncodeTimingCheck(*profile);
    CHECK_HEX(request.data(), request.size(), "0200287f");
}

TEST(WardenPacket_decodes_exact_timing_result_vector)
{
    warden::Bytes const response =
    {
        0x02, 0x05, 0x00, 0xA7, 0xD4, 0x3E,
        0x25, 0x01, 0x78, 0x56, 0x34, 0x12
    };

    warden::TimingResult result;
    CHECK(warden::DecodeTimingResult(View(response), result) ==
        warden::DecodeStatus::Ok);
    CHECK(result.stable);
    CHECK_EQ(result.clientTick, uint32(0x12345678));
}

TEST(WardenPacket_rejects_malformed_timing_result_frames)
{
    warden::Bytes const valid =
    {
        0x02, 0x05, 0x00, 0xA7, 0xD4, 0x3E,
        0x25, 0x01, 0x78, 0x56, 0x34, 0x12
    };
    warden::TimingResult result;

    CHECK(warden::DecodeTimingResult({}, result) ==
        warden::DecodeStatus::Empty);
    CHECK(warden::DecodeTimingResult({nullptr, valid.size()}, result) ==
        warden::DecodeStatus::WrongSize);

    warden::Bytes malformed = valid;
    malformed.pop_back();
    CHECK(warden::DecodeTimingResult(View(malformed), result) ==
        warden::DecodeStatus::WrongSize);
    malformed = valid;
    malformed.push_back(0);
    CHECK(warden::DecodeTimingResult(View(malformed), result) ==
        warden::DecodeStatus::WrongSize);

    malformed = valid;
    malformed[0] = uint8(warden::ClientCommand::HashResult);
    CHECK(warden::DecodeTimingResult(View(malformed), result) ==
        warden::DecodeStatus::UnsupportedCommand);
    malformed = {uint8(warden::ClientCommand::ModuleOk)};
    CHECK(warden::DecodeTimingResult(View(malformed), result) ==
        warden::DecodeStatus::UnsupportedCommand);
    malformed = valid;
    malformed[1] = 4;
    CHECK(warden::DecodeTimingResult(View(malformed), result) ==
        warden::DecodeStatus::WrongSize);
    malformed = valid;
    malformed[3] ^= 1;
    CHECK(warden::DecodeTimingResult(View(malformed), result) ==
        warden::DecodeStatus::ChecksumMismatch);

    // This checksum is independently derived for body 02 78 56 34 12, so the
    // decoder reaches the Boolean validation instead of failing the checksum.
    warden::Bytes const nonBoolean =
    {
        0x02, 0x05, 0x00, 0x24, 0x36, 0x22,
        0x04, 0x02, 0x78, 0x56, 0x34, 0x12
    };
    CHECK(warden::DecodeTimingResult(View(nonBoolean), result) ==
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
