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

#include <algorithm>
#include <limits>

namespace
{
void AppendUint16LE(warden::Bytes& bytes, uint16 value)
{
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

    Bytes bytes;
    bytes.reserve(3 + chunk.size);
    bytes.push_back(uint8(ServerCommand::ModuleCache));
    AppendUint16LE(bytes, uint16(chunk.size));
    bytes.insert(bytes.end(), chunk.data, chunk.data + chunk.size);
    return bytes;
}

Bytes EncodeHashRequest(ModuleProfile const& profile)
{
    Bytes bytes;
    bytes.reserve(1 + profile.hashSeed.size());
    bytes.push_back(uint8(ServerCommand::HashRequest));
    Append(bytes, profile.hashSeed);
    return bytes;
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
