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

#include "WardenServer.h"

#include <openssl/crypto.h>

#include <algorithm>
#include <utility>

namespace
{
class CleanseBytes
{
public:
    explicit CleanseBytes(warden::Bytes& bytes) : m_bytes(bytes) {}

    ~CleanseBytes()
    {
        if (!m_bytes.empty())
            OPENSSL_cleanse(m_bytes.data(), m_bytes.size());
    }

private:
    warden::Bytes& m_bytes;
};
}

namespace warden
{
WardenServer::WardenServer(ModuleProfile const& profile,
    WardenCryptoContext&& crypto, SendEncrypted send, WardenLimits limits)
    : m_profile(profile), m_crypto(std::move(crypto)), m_send(std::move(send)),
      m_limits(limits)
{
}

bool WardenServer::Start()
{
    if (m_state == WardenState::Failed)
        return false;
    if (m_started)
        return true;

    m_started = true;
    if (!m_crypto.IsInitialized())
    {
        Fail(WardenFailure::CryptoFailure);
        return false;
    }

    if (!SendPlain(EncodeModuleUse(m_profile)))
        return false;

    m_state = WardenState::AwaitingModuleStatus;
    ResetDeadline();
    return true;
}

void WardenServer::HandleEncrypted(ByteView encryptedBody)
{
    if (m_state == WardenState::Failed)
        return;
    if (m_state == WardenState::ModuleReady)
    {
        Fail(WardenFailure::Replay);
        return;
    }
    if (!m_started)
    {
        Fail(WardenFailure::UnexpectedCommand);
        return;
    }
    if (encryptedBody.size && !encryptedBody.data)
    {
        Fail(WardenFailure::MalformedPayload);
        return;
    }

    Bytes plain;
    if (encryptedBody.size)
        plain.assign(encryptedBody.data, encryptedBody.data + encryptedBody.size);
    CleanseBytes const cleanse(plain);

    if (!m_crypto.TransformClientToServer(plain))
    {
        Fail(WardenFailure::CryptoFailure);
        return;
    }

    ClientMessage message;
    DecodeStatus const status = DecodeClient(
        {plain.empty() ? nullptr : plain.data(), plain.size()}, message);
    if (status == DecodeStatus::Empty || status == DecodeStatus::WrongSize)
    {
        Fail(WardenFailure::MalformedPayload);
        return;
    }
    if (status == DecodeStatus::UnsupportedCommand)
    {
        Fail(WardenFailure::UnexpectedCommand);
        return;
    }

    switch (m_state)
    {
        case WardenState::AwaitingModuleStatus:
            if (message.command == ClientCommand::ModuleOk)
                SendHashRequest();
            else if (message.command == ClientCommand::ModuleMissing)
                SendModuleTransfer();
            else if (message.command == ClientCommand::ModuleFailed)
                Fail(WardenFailure::ModuleLoadFailed);
            else
                Fail(WardenFailure::UnexpectedCommand);
            return;

        case WardenState::AwaitingTransferResult:
            if (message.command == ClientCommand::ModuleOk)
                SendHashRequest();
            else if (message.command == ClientCommand::ModuleMissing)
                Fail(WardenFailure::ModuleDigestMismatch);
            else if (message.command == ClientCommand::ModuleFailed)
                Fail(WardenFailure::ModuleLoadFailed);
            else
                Fail(WardenFailure::UnexpectedCommand);
            return;

        case WardenState::AwaitingHash:
            if (message.command != ClientCommand::HashResult)
            {
                Fail(WardenFailure::UnexpectedCommand);
                return;
            }
            if (CRYPTO_memcmp(message.hash.data(),
                    m_profile.clientKeySeedHash.data(), message.hash.size()) != 0)
            {
                Fail(WardenFailure::HashMismatch);
                return;
            }
            if (!m_crypto.InstallModuleKeys(m_profile.clientKeySeed,
                    m_profile.serverKeySeed))
            {
                Fail(WardenFailure::CryptoFailure);
                return;
            }
            m_state = WardenState::ModuleReady;
            m_remainingMs = 0;
            return;

        case WardenState::ModuleReady:
        case WardenState::Failed:
            return;
    }
}

void WardenServer::Update(uint32 diffMs)
{
    if (!m_started || m_state == WardenState::ModuleReady ||
        m_state == WardenState::Failed)
        return;

    if (diffMs >= m_remainingMs)
    {
        m_remainingMs = 0;
        Fail(WardenFailure::DeadlineExpired);
        return;
    }
    m_remainingMs -= diffMs;
}

WardenState WardenServer::GetState() const
{
    return m_state;
}

WardenFailure WardenServer::GetFailure() const
{
    return m_failure;
}

uint8 WardenServer::GetTransferCount() const
{
    return m_transferCount;
}

void WardenServer::Fail(WardenFailure reason)
{
    if (m_state == WardenState::Failed)
        return;
    m_failure = reason;
    m_state = WardenState::Failed;
    m_remainingMs = 0;
}

bool WardenServer::SendPlain(Bytes plain)
{
    CleanseBytes const cleanse(plain);
    if (plain.empty())
    {
        Fail(WardenFailure::SendFailure);
        return false;
    }
    if (!m_crypto.TransformServerToClient(plain))
    {
        Fail(WardenFailure::CryptoFailure);
        return false;
    }
    if (!m_send || !m_send(plain))
    {
        Fail(WardenFailure::SendFailure);
        return false;
    }
    return true;
}

void WardenServer::ResetDeadline()
{
    m_remainingMs = m_limits.deadlineMs;
}

bool WardenServer::SendModuleTransfer()
{
    if (!m_limits.chunkSize || m_transferCount >= m_limits.maxTransfers)
    {
        Fail(WardenFailure::ModuleDigestMismatch);
        return false;
    }

    ++m_transferCount;
    for (size_t offset = 0; offset < m_profile.module.size;)
    {
        size_t const size = std::min<size_t>(m_limits.chunkSize,
            m_profile.module.size - offset);
        if (!SendPlain(EncodeModuleCache(
                {m_profile.module.data + offset, size})))
            return false;
        offset += size;
    }

    m_state = WardenState::AwaitingTransferResult;
    ResetDeadline();
    return true;
}

bool WardenServer::SendHashRequest()
{
    if (!SendPlain(EncodeHashRequest(m_profile)))
        return false;
    m_state = WardenState::AwaitingHash;
    ResetDeadline();
    return true;
}
}
