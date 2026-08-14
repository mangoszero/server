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
// Plaintext Warden bodies are short-lived but may contain challenge results or
// module bytes. Clean every owned working buffer on every return path.
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
    WardenCryptoContext&& crypto, SendEncrypted send, WardenLimits limits,
    LifecycleObserver observer, EvidenceObserver evidenceObserver)
    : m_profile(profile), m_crypto(std::move(crypto)), m_send(std::move(send)),
      m_limits(limits), m_observer(std::move(observer)),
      m_evidenceObserver(std::move(evidenceObserver))
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

    // The session schedules Start after the character list in the normal
    // retail flow, with an idempotent player-login safety net for clients that
    // skip enumeration. Queued sessions remain completely inert.
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
        // No client Warden command is solicited before MODULE_USE. Ignore the
        // body without advancing the receive cipher so it cannot latch
        // bootstrap off before the server starts negotiation.
        return;
    }
    if (encryptedBody.size && !encryptedBody.data)
    {
        Fail(WardenFailure::MalformedPayload);
        return;
    }

    // Decrypt a private copy so the WorldPacket remains an opaque transport
    // object and plaintext lifetime is bounded by this call.
    Bytes plain;
    if (encryptedBody.size)
        plain.assign(encryptedBody.data, encryptedBody.data + encryptedBody.size);
    CleanseBytes const cleanse(plain);

    if (!m_crypto.TransformClientToServer(plain))
    {
        Fail(WardenFailure::CryptoFailure);
        return;
    }

    ByteView const plainView
    {
        plain.empty() ? nullptr : plain.data(), plain.size()
    };
    if (m_state == WardenState::AwaitingCheckResult)
    {
        HandleTimingResult(plainView);
        return;
    }

    ClientMessage message;
    DecodeStatus const status = DecodeClient(plainView, message);
    if (status == DecodeStatus::Empty || status == DecodeStatus::WrongSize ||
        status == DecodeStatus::ChecksumMismatch ||
        status == DecodeStatus::InvalidValue)
    {
        Fail(WardenFailure::MalformedPayload);
        return;
    }
    if (status == DecodeStatus::UnsupportedCommand)
    {
        Fail(WardenFailure::UnexpectedCommand);
        return;
    }
    if (status == DecodeStatus::CryptoFailure)
    {
        Fail(WardenFailure::CryptoFailure);
        return;
    }

    // The same command value has different validity in each bootstrap state.
    // Decode shape first, then enforce the state-specific transition table.
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
        {
            if (message.command != ClientCommand::HashResult)
            {
                Fail(WardenFailure::UnexpectedCommand);
                return;
            }
            // Compare all 20 bytes before installing replacement stream keys.
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

            // Command 3 is the first server body under the replacement stream.
            // It installs the exact build-scoped archive, Lua, and timing host
            // callbacks. The module sends no acknowledgement, so a successful
            // transport handoff is the only transition into ModuleReady.
            Bytes initialization;
            EncodeStatus const encodeStatus =
                EncodeModuleInitialize(m_profile, initialization);
            if (encodeStatus == EncodeStatus::InvalidProfile)
            {
                Fail(WardenFailure::UnsupportedProfile);
                return;
            }
            if (encodeStatus == EncodeStatus::CryptoFailure)
            {
                Fail(WardenFailure::CryptoFailure);
                return;
            }
            if (!SendPlain(std::move(initialization)))
                return;

            m_state = WardenState::ModuleReady;
            m_remainingMs = 0;
            NotifyTerminal();
            return;
        }

        case WardenState::ModuleReady:
        case WardenState::AwaitingCheckResult:
        case WardenState::Failed:
            return;
    }
}

void WardenServer::Update(bool eligible, uint32 diffMs)
{
    if (!m_started || m_state == WardenState::Failed)
        return;

    if (m_state == WardenState::ModuleReady)
    {
        std::optional<CheckPlan> const plan = m_planner.Update(eligible, diffMs);
        if (plan)
            SendTimingCheck(*plan);
        return;
    }

    // Many small updates cannot extend the deadline: elapsed world time is
    // accumulated until the state either advances or expires.
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
    m_pendingRequestId = 0;
    NotifyTerminal();
}

void WardenServer::NotifyTerminal()
{
    // The callback receives classifications only; protocol bytes and keys never
    // cross this observability boundary.
    if (m_observer)
        m_observer({m_state, m_failure, m_transferCount});
}

bool WardenServer::SendPlain(Bytes plain)
{
    // Encrypt in place and cleanse the temporary regardless of callback result.
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
    // One bounded transfer prevents a client from requesting an unbounded
    // 18,756-byte amplification loop. A second miss is a digest/load failure.
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
    // A successful response proves the delivered module's seed transform and
    // authorizes the switch to its post-hash transport keys.
    if (!SendPlain(EncodeHashRequest(m_profile)))
        return false;
    m_state = WardenState::AwaitingHash;
    ResetDeadline();
    return true;
}

bool WardenServer::SendTimingCheck(CheckPlan const& plan)
{
    if (plan.kind != CheckKind::Timing || !plan.requestId)
    {
        Fail(WardenFailure::UnexpectedCommand);
        return false;
    }

    if (!SendPlain(EncodeTimingCheck(m_profile)))
        return false;

    m_pendingRequestId = plan.requestId;
    m_state = WardenState::AwaitingCheckResult;
    ResetDeadline();
    return true;
}

void WardenServer::HandleTimingResult(ByteView plain)
{
    TimingResult result;
    DecodeStatus const status = DecodeTimingResult(plain, result);
    if (status == DecodeStatus::UnsupportedCommand)
    {
        Fail(WardenFailure::UnexpectedCommand);
        return;
    }
    if (status == DecodeStatus::CryptoFailure)
    {
        Fail(WardenFailure::CryptoFailure);
        return;
    }
    if (status != DecodeStatus::Ok)
    {
        Fail(WardenFailure::MalformedPayload);
        return;
    }

    TimingEvidence const evidence
    {
        m_pendingRequestId,
        result.stable ? TimingOutcome::Stable : TimingOutcome::Unstable,
        result.clientTick
    };
    m_pendingRequestId = 0;
    m_state = WardenState::ModuleReady;
    m_remainingMs = 0;

    // Publish only after state is committed, so observer re-entry cannot see a
    // half-consumed request or cause duplicate evidence.
    if (m_evidenceObserver)
        m_evidenceObserver(evidence);
}
}
