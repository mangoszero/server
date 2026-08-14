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

#include "WardenProtocol.h"

#include <openssl/crypto.h>

#include <utility>

namespace warden
{
AdmissionData::AdmissionData(AdmissionData&& other) noexcept
    : build(other.build), platform(std::move(other.platform)),
      sessionKey(other.sessionKey), available(other.available)
{
    other.Clear();
}

AdmissionData& AdmissionData::operator=(AdmissionData&& other) noexcept
{
    if (this != &other)
    {
        Clear();
        build = other.build;
        platform = std::move(other.platform);
        sessionKey = other.sessionKey;
        available = other.available;
        other.Clear();
    }
    return *this;
}

AdmissionData::~AdmissionData()
{
    Clear();
}

void AdmissionData::Clear()
{
    OPENSSL_cleanse(sessionKey.data(), sessionKey.size());
    platform.clear();
    platform.shrink_to_fit();
    build = 0;
    available = false;
}

char const* ToString(WardenState state)
{
    switch (state)
    {
        case WardenState::AwaitingModuleStatus: return "AwaitingModuleStatus";
        case WardenState::AwaitingTransferResult: return "AwaitingTransferResult";
        case WardenState::AwaitingHash: return "AwaitingHash";
        case WardenState::ModuleReady: return "ModuleReady";
        case WardenState::Failed: return "Failed";
    }
    return "Unknown";
}

char const* ToString(WardenFailure failure)
{
    switch (failure)
    {
        case WardenFailure::None: return "None";
        case WardenFailure::UnsupportedProfile: return "UnsupportedProfile";
        case WardenFailure::MalformedPayload: return "MalformedPayload";
        case WardenFailure::UnexpectedCommand: return "UnexpectedCommand";
        case WardenFailure::Replay: return "Replay";
        case WardenFailure::ModuleDigestMismatch: return "ModuleDigestMismatch";
        case WardenFailure::ModuleLoadFailed: return "ModuleLoadFailed";
        case WardenFailure::HashMismatch: return "HashMismatch";
        case WardenFailure::DeadlineExpired: return "DeadlineExpired";
        case WardenFailure::CryptoFailure: return "CryptoFailure";
        case WardenFailure::SendFailure: return "SendFailure";
    }
    return "Unknown";
}
}
