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

Bytes EncodeTimingCheck(ModuleProfile const& profile)
{
    // Command 2 begins with a terminated string table. Each following check
    // type, including the zero terminator, is XORed with the first byte of the
    // module's client-to-server post-hash key.
    uint8 const xorByte = profile.clientKeySeed[0];
    return
    {
        uint8(ServerCommand::CheatChecksRequest),
        0,
        uint8(0x57 ^ xorByte),
        xorByte
    };
}

DecodeStatus DecodeTimingResult(ByteView body, TimingResult& result)
{
    if (!body.size)
        return DecodeStatus::Empty;
    if (!body.data)
        return DecodeStatus::WrongSize;
    if (body.data[0] != uint8(ClientCommand::CheckResult))
        return DecodeStatus::UnsupportedCommand;
    if (body.size != 12)
        return DecodeStatus::WrongSize;

    uint16 const resultLength = ReadUint16LE(body.data + 1);
    if (resultLength != 5 || body.size != 7 + resultLength)
        return DecodeStatus::WrongSize;

    ByteView const resultBody{body.data + 7, resultLength};
    uint32 calculatedChecksum = 0;
    if (!BuildChecksum(resultBody, calculatedChecksum))
        return DecodeStatus::CryptoFailure;
    if (ReadUint32LE(body.data + 3) != calculatedChecksum)
        return DecodeStatus::ChecksumMismatch;
    if (resultBody.data[0] > 1)
        return DecodeStatus::InvalidValue;

    TimingResult decoded;
    decoded.stable = resultBody.data[0] != 0;
    decoded.clientTick = ReadUint32LE(resultBody.data + 1);
    result = decoded;
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
