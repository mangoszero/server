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

#include "WardenPacketCodec.h"

#include <openssl/evp.h>

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
void AppendUint16LE(warden::Bytes& bytes, uint16 value)
{
    // Warden inner structures are explicitly little-endian and must not depend
    // on the host compiler's struct layout or byte order.
    bytes.push_back(uint8(value));
    bytes.push_back(uint8(value >> 8));
}

void AppendUint32LE(warden::Bytes& bytes, uint32 value)
{
    bytes.push_back(uint8(value));
    bytes.push_back(uint8(value >> 8));
    bytes.push_back(uint8(value >> 16));
    bytes.push_back(uint8(value >> 24));
}

uint16 ReadUint16LE(uint8 const* bytes)
{
    return uint16(bytes[0]) | (uint16(bytes[1]) << 8);
}

uint32 ReadUint32LE(uint8 const* bytes)
{
    return uint32(bytes[0]) | (uint32(bytes[1]) << 8) |
        (uint32(bytes[2]) << 16) | (uint32(bytes[3]) << 24);
}

bool BuildChecksum(warden::ByteView body, uint32& checksum)
{
    warden::Digest20 digest{};
    unsigned int length = 0;
    if (!body.data || EVP_Digest(body.data, body.size, digest.data(), &length,
            EVP_sha1(), nullptr) != 1 || length != digest.size())
        return false;

    checksum = 0;
    for (size_t offset = 0; offset < digest.size(); offset += 4)
        checksum ^= ReadUint32LE(digest.data() + offset);
    return true;
}

template <typename Range>
void Append(warden::Bytes& bytes, Range const& range)
{
    bytes.insert(bytes.end(), range.begin(), range.end());
}

bool AppendInitializationRecord(warden::Bytes& encoded,
    warden::ByteView payload)
{
    if (!payload.data || !payload.size ||
        payload.size > std::numeric_limits<uint16>::max())
        return false;

    uint32 checksum = 0;
    if (!BuildChecksum(payload, checksum))
        return false;

    encoded.push_back(uint8(warden::ServerCommand::ModuleInitialize));
    AppendUint16LE(encoded, uint16(payload.size));
    AppendUint32LE(encoded, checksum);
    encoded.insert(encoded.end(), payload.data, payload.data + payload.size);
    return true;
}

bool ValidateCheckPlan(warden::CheckPlan const& plan,
    std::vector<std::string>& strings)
{
    if (!plan.requestId || plan.checks.empty() ||
        !std::holds_alternative<warden::TimingCheck>(plan.checks.front()))
        return false;

    size_t timingCount = 0;
    size_t mpqCount = 0;
    size_t luaCount = 0;
    size_t stringTableSize = 1; // Final zero-length string terminator.

    auto addString = [&](std::string const& value)
    {
        auto const existing = std::find(strings.begin(), strings.end(), value);
        if (existing != strings.end())
            return true;

        stringTableSize += 1 + value.size();
        if (stringTableSize > 512 ||
            strings.size() >= std::numeric_limits<uint8>::max())
            return false;
        strings.push_back(value);
        return true;
    };

    for (warden::PlannedCheck const& check : plan.checks)
    {
        if (std::holds_alternative<warden::TimingCheck>(check))
        {
            if (++timingCount > 1)
                return false;
            continue;
        }

        if (warden::MpqCheckProfile const* mpq =
                std::get_if<warden::MpqCheckProfile>(&check))
        {
            if (++mpqCount > 1 || !mpq->checkId || mpq->path.empty() ||
                mpq->path.size() > std::numeric_limits<uint8>::max() ||
                mpq->path.find('\0') != std::string::npos ||
                !addString(mpq->path))
                return false;
            continue;
        }

        warden::LuaCheckProfile const* lua =
            std::get_if<warden::LuaCheckProfile>(&check);
        if (!lua || ++luaCount > 1 || !lua->checkId || lua->query.empty() ||
            lua->query.size() > std::numeric_limits<uint8>::max() ||
            lua->query.find('\0') != std::string::npos ||
            lua->expectedText.size() > 64 ||
            lua->expectedText.find('\0') != std::string::npos ||
            !addString(lua->query))
            return false;
    }

    return timingCount == 1;
}
}

namespace warden
{
Bytes EncodeModuleUse(ModuleProfile const& profile)
{
    if (profile.module.size > std::numeric_limits<uint32>::max())
        return {};

    // command(1) || module MD5(16) || module key(16) || compressed size(4).
    Bytes bytes;
    bytes.reserve(1 + profile.moduleId.size() + profile.moduleKey.size() + 4);
    bytes.push_back(uint8(ServerCommand::ModuleUse));
    Append(bytes, profile.moduleId);
    Append(bytes, profile.moduleKey);
    AppendUint32LE(bytes, uint32(profile.module.size));
    return bytes;
}

Bytes EncodeModuleCache(ByteView chunk)
{
    if (!chunk.data || !chunk.size ||
        chunk.size > std::numeric_limits<uint16>::max())
        return {};

    // Each transfer frame carries its own uint16 payload length; the final
    // chunk may be shorter than the negotiated server-side chunk limit.
    Bytes bytes;
    bytes.reserve(3 + chunk.size);
    bytes.push_back(uint8(ServerCommand::ModuleCache));
    AppendUint16LE(bytes, uint16(chunk.size));
    bytes.insert(bytes.end(), chunk.data, chunk.data + chunk.size);
    return bytes;
}

Bytes EncodeHashRequest(ModuleProfile const& profile)
{
    // The delivered module consumes exactly a one-byte command and 16-byte seed.
    Bytes bytes;
    bytes.reserve(1 + profile.hashSeed.size());
    bytes.push_back(uint8(ServerCommand::HashRequest));
    Append(bytes, profile.hashSeed);
    return bytes;
}

EncodeStatus EncodeModuleInitialize(ModuleProfile const& profile, Bytes& output)
{
    ModuleInitializationProfile const& initialization = profile.initialization;
    if (!initialization.archive.openRva || !initialization.archive.sizeRva ||
        !initialization.archive.readRva || !initialization.archive.closeRva ||
        !initialization.lua.callbackRva || !initialization.timing.callbackRva)
        return EncodeStatus::InvalidProfile;

    // Construct every payload before hashing it. This avoids the legacy bug,
    // where checksums covered uninitialized bytes and the wrong record offset.
    Bytes archivePayload;
    archivePayload.reserve(20);
    Append(archivePayload, initialization.archive.selectors);
    AppendUint32LE(archivePayload, initialization.archive.openRva);
    AppendUint32LE(archivePayload, initialization.archive.sizeRva);
    AppendUint32LE(archivePayload, initialization.archive.readRva);
    AppendUint32LE(archivePayload, initialization.archive.closeRva);

    Bytes luaPayload;
    luaPayload.reserve(8);
    Append(luaPayload, initialization.lua.prefix);
    AppendUint32LE(luaPayload, initialization.lua.callbackRva);
    luaPayload.push_back(initialization.lua.selector);

    Bytes timingPayload;
    timingPayload.reserve(8);
    Append(timingPayload, initialization.timing.prefix);
    AppendUint32LE(timingPayload, initialization.timing.callbackRva);
    timingPayload.push_back(initialization.timing.install);

    // Build the complete body privately. Caller output changes only after all
    // three folded SHA-1 checksums and record frames have succeeded.
    Bytes encoded;
    encoded.reserve(57);
    if (!AppendInitializationRecord(encoded,
            {archivePayload.data(), archivePayload.size()}) ||
        !AppendInitializationRecord(encoded,
            {luaPayload.data(), luaPayload.size()}) ||
        !AppendInitializationRecord(encoded,
            {timingPayload.data(), timingPayload.size()}))
        return EncodeStatus::CryptoFailure;

    output = std::move(encoded);
    return EncodeStatus::Ok;
}

EncodeStatus EncodeCheckRequest(ModuleProfile const& profile,
    CheckPlan const& plan, Bytes& output)
{
    std::vector<std::string> strings;
    if (!ValidateCheckPlan(plan, strings))
        return EncodeStatus::InvalidPlan;

    uint8 const xorByte = profile.clientKeySeed[0];
    Bytes encoded;
    encoded.push_back(uint8(ServerCommand::CheatChecksRequest));
    for (std::string const& value : strings)
    {
        encoded.push_back(uint8(value.size()));
        encoded.insert(encoded.end(), value.begin(), value.end());
    }
    encoded.push_back(0);

    for (PlannedCheck const& check : plan.checks)
    {
        if (std::holds_alternative<TimingCheck>(check))
        {
            encoded.push_back(uint8(0x57 ^ xorByte));
            continue;
        }

        std::string const* value = nullptr;
        uint8 decodedType = 0;
        if (MpqCheckProfile const* mpq =
                std::get_if<MpqCheckProfile>(&check))
        {
            value = &mpq->path;
            decodedType = 0x98;
        }
        else if (LuaCheckProfile const* lua =
                     std::get_if<LuaCheckProfile>(&check))
        {
            value = &lua->query;
            decodedType = 0x8B;
        }
        else
            return EncodeStatus::InvalidPlan;

        auto const found = std::find(strings.begin(), strings.end(), *value);
        if (found == strings.end())
            return EncodeStatus::InvalidPlan;
        size_t const index = size_t(found - strings.begin()) + 1;
        if (index > std::numeric_limits<uint8>::max())
            return EncodeStatus::InvalidPlan;

        encoded.push_back(uint8(decodedType ^ xorByte));
        encoded.push_back(uint8(index));
    }
    encoded.push_back(xorByte);

    output = std::move(encoded);
    return EncodeStatus::Ok;
}

DecodeStatus DecodeCheckResult(ByteView body, CheckPlan const& plan,
    CheckBatchResult& result)
{
    if (!body.size)
        return DecodeStatus::Empty;
    if (!body.data)
        return DecodeStatus::WrongSize;
    if (body.data[0] != uint8(ClientCommand::CheckResult))
        return DecodeStatus::UnsupportedCommand;
    if (body.size < 7)
        return DecodeStatus::WrongSize;

    uint16 const resultLength = ReadUint16LE(body.data + 1);
    if (body.size != size_t(7) + resultLength)
        return DecodeStatus::WrongSize;

    ByteView const resultBody{body.data + 7, resultLength};
    uint32 calculatedChecksum = 0;
    if (!BuildChecksum(resultBody, calculatedChecksum))
        return DecodeStatus::CryptoFailure;
    if (ReadUint32LE(body.data + 3) != calculatedChecksum)
        return DecodeStatus::ChecksumMismatch;

    std::vector<std::string> strings;
    if (!ValidateCheckPlan(plan, strings))
        return DecodeStatus::InvalidValue;

    CheckBatchResult decoded;
    decoded.checks.reserve(plan.checks.size());
    size_t offset = 0;
    for (PlannedCheck const& check : plan.checks)
    {
        if (std::holds_alternative<TimingCheck>(check))
        {
            if (resultLength - offset < 5)
                return DecodeStatus::WrongSize;
            uint8 const status = resultBody.data[offset];
            if (status > 1)
                return DecodeStatus::InvalidValue;

            decoded.checks.emplace_back(TimingResult
            {
                status != 0,
                ReadUint32LE(resultBody.data + offset + 1)
            });
            offset += 5;
            continue;
        }

        if (std::holds_alternative<MpqCheckProfile>(check))
        {
            if (resultLength - offset < 1)
                return DecodeStatus::WrongSize;
            uint8 const status = resultBody.data[offset++];
            if (status > uint8(MpqResultStatus::Unavailable))
                return DecodeStatus::InvalidValue;

            MpqResult mpq;
            mpq.status = MpqResultStatus(status);
            if (mpq.status == MpqResultStatus::Success)
            {
                if (resultLength - offset < mpq.digest.size())
                    return DecodeStatus::WrongSize;
                std::copy(resultBody.data + offset,
                    resultBody.data + offset + mpq.digest.size(),
                    mpq.digest.begin());
                offset += mpq.digest.size();
            }
            decoded.checks.emplace_back(mpq);
            continue;
        }

        if (!std::holds_alternative<LuaCheckProfile>(check) ||
            resultLength - offset < 1)
            return DecodeStatus::WrongSize;
        uint8 const status = resultBody.data[offset++];
        if (status > uint8(LuaResultStatus::Unavailable))
            return DecodeStatus::InvalidValue;

        LuaResult lua;
        lua.status = LuaResultStatus(status);
        if (lua.status == LuaResultStatus::Success)
        {
            if (resultLength - offset < 1)
                return DecodeStatus::WrongSize;
            uint8 const textLength = resultBody.data[offset++];
            if (textLength > 64)
                return DecodeStatus::InvalidValue;
            if (resultLength - offset < textLength)
                return DecodeStatus::WrongSize;
            lua.text.assign(
                reinterpret_cast<char const*>(resultBody.data + offset),
                textLength);
            offset += textLength;
        }
        decoded.checks.emplace_back(std::move(lua));
    }

    if (offset != resultLength)
        return DecodeStatus::WrongSize;

    result = std::move(decoded);
    return DecodeStatus::Ok;
}

DecodeStatus DecodeClient(ByteView body, ClientMessage& message)
{
    if (!body.size)
        return DecodeStatus::Empty;
    if (!body.data)
        return DecodeStatus::WrongSize;

    ClientCommand command;
    switch (body.data[0])
    {
        case uint8(ClientCommand::ModuleMissing):
            command = ClientCommand::ModuleMissing;
            break;
        case uint8(ClientCommand::ModuleOk):
            command = ClientCommand::ModuleOk;
            break;
        case uint8(ClientCommand::HashResult):
            command = ClientCommand::HashResult;
            break;
        case uint8(ClientCommand::ModuleFailed):
            command = ClientCommand::ModuleFailed;
            break;
        default:
            return DecodeStatus::UnsupportedCommand;
    }

    // Status commands are one byte. HASH_RESULT is command + SHA-1-sized body.
    size_t const expectedSize = command == ClientCommand::HashResult ? 21 : 1;
    if (body.size != expectedSize)
        return DecodeStatus::WrongSize;

    ClientMessage decoded;
    decoded.command = command;
    if (command == ClientCommand::HashResult)
        std::copy(body.data + 1, body.data + body.size, decoded.hash.begin());
    message = decoded;
    return DecodeStatus::Ok;
}
}
