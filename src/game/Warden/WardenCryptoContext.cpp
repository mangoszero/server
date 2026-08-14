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

#include "WardenCryptoContext.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

namespace
{
bool Sha1(uint8 const* data, size_t size, warden::Digest20& digest)
{
    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context)
        return false;

    unsigned int length = 0;
    bool const success = EVP_DigestInit_ex(context, EVP_sha1(), nullptr) == 1 &&
        EVP_DigestUpdate(context, data, size) == 1 &&
        EVP_DigestFinal_ex(context, digest.data(), &length) == 1 &&
        length == digest.size();
    EVP_MD_CTX_free(context);
    return success;
}

bool DeriveInitialKeys(warden::SessionKey const& sessionKey,
    warden::Key16& clientKey, warden::Key16& serverKey)
{
    warden::Digest20 left{};
    warden::Digest20 right{};
    warden::Digest20 current{};
    std::array<uint8, 60> input{};
    std::array<uint8, 32> generated{};

    bool success = Sha1(sessionKey.data(), 20, left) &&
        Sha1(sessionKey.data() + 20, 20, right);
    size_t offset = 0;
    while (success && offset < generated.size())
    {
        std::copy(left.begin(), left.end(), input.begin());
        std::copy(current.begin(), current.end(), input.begin() + 20);
        std::copy(right.begin(), right.end(), input.begin() + 40);
        success = Sha1(input.data(), input.size(), current);
        size_t const count = std::min(current.size(), generated.size() - offset);
        if (success)
        {
            std::copy(current.begin(), current.begin() + count,
                generated.begin() + offset);
            offset += count;
        }
    }

    if (success)
    {
        std::copy(generated.begin(), generated.begin() + clientKey.size(),
            clientKey.begin());
        std::copy(generated.begin() + clientKey.size(), generated.end(),
            serverKey.begin());
    }

    OPENSSL_cleanse(left.data(), left.size());
    OPENSSL_cleanse(right.data(), right.size());
    OPENSSL_cleanse(current.data(), current.size());
    OPENSSL_cleanse(input.data(), input.size());
    OPENSSL_cleanse(generated.data(), generated.size());
    return success;
}
}

namespace warden
{
WardenCryptoContext::WardenCryptoContext(WardenCryptoContext&& other) noexcept
    : m_clientToServer(other.m_clientToServer),
      m_serverToClient(other.m_serverToClient)
{
    other.Clear();
}

WardenCryptoContext& WardenCryptoContext::operator=(
    WardenCryptoContext&& other) noexcept
{
    if (this != &other)
    {
        Clear();
        m_clientToServer = other.m_clientToServer;
        m_serverToClient = other.m_serverToClient;
        other.Clear();
    }
    return *this;
}

WardenCryptoContext::~WardenCryptoContext()
{
    Clear();
}

bool WardenCryptoContext::Initialize(SessionKey const& sessionKey)
{
    Key16 clientKey{};
    Key16 serverKey{};
    Rc4State clientState;
    Rc4State serverState;

    bool const success = DeriveInitialKeys(sessionKey, clientKey, serverKey) &&
        clientState.Initialize(clientKey) && serverState.Initialize(serverKey);
    if (success)
    {
        Clear();
        m_clientToServer = clientState;
        m_serverToClient = serverState;
    }

    clientState.Clear();
    serverState.Clear();
    OPENSSL_cleanse(clientKey.data(), clientKey.size());
    OPENSSL_cleanse(serverKey.data(), serverKey.size());
    return success;
}

bool WardenCryptoContext::IsInitialized() const
{
    return m_clientToServer.initialized && m_serverToClient.initialized;
}

bool WardenCryptoContext::TransformClientToServer(Bytes& bytes)
{
    return m_clientToServer.Transform(bytes.empty() ? nullptr : bytes.data(),
        bytes.size());
}

bool WardenCryptoContext::TransformServerToClient(Bytes& bytes)
{
    return m_serverToClient.Transform(bytes.empty() ? nullptr : bytes.data(),
        bytes.size());
}

bool WardenCryptoContext::InstallModuleKeys(Key16 const& clientKey,
    Key16 const& serverKey)
{
    if (!IsInitialized())
        return false;

    Rc4State clientState;
    Rc4State serverState;
    if (!clientState.Initialize(clientKey) || !serverState.Initialize(serverKey))
    {
        clientState.Clear();
        serverState.Clear();
        return false;
    }

    m_clientToServer.Clear();
    m_serverToClient.Clear();
    m_clientToServer = clientState;
    m_serverToClient = serverState;
    clientState.Clear();
    serverState.Clear();
    return true;
}

bool WardenCryptoContext::Rc4State::Initialize(Key16 const& key)
{
    for (size_t index = 0; index < permutation.size(); ++index)
        permutation[index] = uint8(index);

    uint32 swapIndex = 0;
    for (size_t index = 0; index < permutation.size(); ++index)
    {
        swapIndex = (swapIndex + permutation[index] + key[index % key.size()]) & 0xFF;
        std::swap(permutation[index], permutation[swapIndex]);
    }
    i = 0;
    j = 0;
    initialized = true;
    return true;
}

bool WardenCryptoContext::Rc4State::Transform(uint8* data, size_t size)
{
    if (!initialized || (size && !data))
        return false;

    for (size_t offset = 0; offset < size; ++offset)
    {
        i = uint8(i + 1);
        j = uint8(j + permutation[i]);
        std::swap(permutation[i], permutation[j]);
        data[offset] ^= permutation[uint8(permutation[i] + permutation[j])];
    }
    return true;
}

void WardenCryptoContext::Rc4State::Clear()
{
    OPENSSL_cleanse(permutation.data(), permutation.size());
    i = 0;
    j = 0;
    initialized = false;
}

void WardenCryptoContext::Clear()
{
    m_clientToServer.Clear();
    m_serverToClient.Clear();
}
}
