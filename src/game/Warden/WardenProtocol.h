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

#ifndef MANGOS_WARDEN_PROTOCOL_H
#define MANGOS_WARDEN_PROTOCOL_H

#include "Platform/Define.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace warden
{
// Warden transport values use fixed widths. In particular, the authenticated
// login session key is always preserved as 40 bytes, including leading zeroes.
using Bytes = std::vector<uint8>;
using SessionKey = std::array<uint8, 40>;
using ModuleId = std::array<uint8, 16>;
using Key16 = std::array<uint8, 16>;
using Digest20 = std::array<uint8, 20>;
using Digest32 = std::array<uint8, 32>;

struct ByteView
{
    // Non-owning view used at codec boundaries; size may be zero with null data.
    uint8 const* data = nullptr;
    size_t size = 0;
};

// Command identifiers live inside the encrypted SMSG/CMSG_WARDEN_DATA body;
// they are not world opcodes.
enum class ClientCommand : uint8
{
    ModuleMissing = 0,
    ModuleOk = 1,
    CheckResult = 2,
    HashResult = 4,
    ModuleFailed = 5
};

enum class ServerCommand : uint8
{
    ModuleUse = 0,
    ModuleCache = 1,
    CheatChecksRequest = 2,
    ModuleInitialize = 3,
    HashRequest = 5
};

enum class WardenState : uint8
{
    AwaitingModuleStatus,
    AwaitingTransferResult,
    AwaitingHash,
    ModuleReady,
    AwaitingCheckResult,
    Failed
};

enum class WardenFailure : uint8
{
    None,
    UnsupportedProfile,
    MalformedPayload,
    UnexpectedCommand,
    Replay,
    ModuleDigestMismatch,
    ModuleLoadFailed,
    HashMismatch,
    DeadlineExpired,
    CryptoFailure,
    SendFailure
};

struct WardenLimits
{
    // Each waiting state receives a new cumulative deadline. Update calls
    // subtract from it; packet activity does not extend it.
    uint32 deadlineMs = 30000;
    uint16 chunkSize = 500;
    uint8 maxTransfers = 1;
};

/**
 * Carries the authenticated bootstrap inputs from the network admission path
 * to WorldSession. It is move-only so the raw session key has one owner and is
 * cleansed when consumed, replaced, or destroyed.
 */
struct AdmissionData
{
    AdmissionData() = default;
    AdmissionData(AdmissionData const&) = delete;
    AdmissionData& operator=(AdmissionData const&) = delete;
    AdmissionData(AdmissionData&& other) noexcept;
    AdmissionData& operator=(AdmissionData&& other) noexcept;
    ~AdmissionData();

    void Clear();

    uint32 build = 0;
    std::string platform;
    SessionKey sessionKey{};
    bool available = false;
};

// These fixed labels are safe for summaries; they never contain wire data or
// cryptographic values.
char const* ToString(WardenState state);
char const* ToString(WardenFailure failure);
}

#endif
