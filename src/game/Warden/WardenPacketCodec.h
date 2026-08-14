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

#ifndef MANGOS_WARDEN_PACKET_CODEC_H
#define MANGOS_WARDEN_PACKET_CODEC_H

#include "WardenModuleCatalog.h"

namespace warden
{
enum class DecodeStatus : uint8
{
    Ok,
    Empty,
    WrongSize,
    UnsupportedCommand,
    ChecksumMismatch,
    InvalidValue,
    CryptoFailure
};

struct TimingResult
{
    bool stable = false;
    uint32 clientTick = 0;
};

struct ClientMessage
{
    ClientCommand command = ClientCommand::ModuleMissing;
    // Populated only for the exact 1 + 20-byte HASH_RESULT shape.
    Digest20 hash{};
};

// These functions encode/decode the plaintext inner Warden command. Transport
// encryption and the outer SMSG/CMSG_WARDEN_DATA packet belong to other layers.
Bytes EncodeModuleUse(ModuleProfile const& profile);
Bytes EncodeModuleCache(ByteView chunk);
Bytes EncodeHashRequest(ModuleProfile const& profile);
Bytes EncodeTimingCheck(ModuleProfile const& profile);

// Accepts only the delivered module's exact command-2 timing result frame.
// The encoded length and checksum cover the five-byte result body alone.
DecodeStatus DecodeTimingResult(ByteView body, TimingResult& result);

// DecodeClient accepts only the four bootstrap commands and their exact sizes;
// trailing bytes are malformed rather than silently ignored.
DecodeStatus DecodeClient(ByteView body, ClientMessage& message);
}

#endif
