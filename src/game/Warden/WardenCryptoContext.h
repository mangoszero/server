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

#ifndef MANGOS_WARDEN_CRYPTO_CONTEXT_H
#define MANGOS_WARDEN_CRYPTO_CONTEXT_H

#include "WardenProtocol.h"

#include <array>

namespace warden
{
/**
 * Owns the two independent streaming RC4 directions for one Warden session.
 * Login-key streams are derived from the exact raw-40 session key and are
 * atomically replaced by the delivered module's keys after hash validation.
 */
class WardenCryptoContext
{
public:
    WardenCryptoContext() = default;
    WardenCryptoContext(WardenCryptoContext const&) = delete;
    WardenCryptoContext& operator=(WardenCryptoContext const&) = delete;
    WardenCryptoContext(WardenCryptoContext&& other) noexcept;
    WardenCryptoContext& operator=(WardenCryptoContext&& other) noexcept;
    ~WardenCryptoContext();

    // Initializes both directions without retaining the login session key.
    bool Initialize(SessionKey const& sessionKey);
    bool IsInitialized() const;

    // Transforms advance their directional stream across packet boundaries;
    // callers must invoke the matching direction exactly once per body.
    bool TransformClientToServer(Bytes& bytes);
    bool TransformServerToClient(Bytes& bytes);

    // Builds replacement states first, then swaps both directions together.
    bool InstallModuleKeys(Key16 const& clientKey, Key16 const& serverKey);

private:
    struct Rc4State
    {
        bool Initialize(Key16 const& key);
        bool Transform(uint8* data, size_t size);
        void Clear();

        std::array<uint8, 256> permutation{};
        // PRGA indices are part of the stream state and must not reset per packet.
        uint8 i = 0;
        uint8 j = 0;
        bool initialized = false;
    };

    void Clear();

    Rc4State m_clientToServer;
    Rc4State m_serverToClient;
};
}

#endif
