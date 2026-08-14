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

#ifndef MANGOS_WARDEN_SERVER_H
#define MANGOS_WARDEN_SERVER_H

#include "WardenCryptoContext.h"
#include "WardenPacketCodec.h"

#include <functional>

namespace warden
{
// SendEncrypted receives a complete encrypted inner Warden body. The session
// adapter is responsible only for wrapping it in SMSG_WARDEN_DATA.
using SendEncrypted = std::function<bool(Bytes const&)>;

/** Secret-free terminal state supplied to the session observability adapter. */
struct WardenLifecycleEvent
{
    WardenState state = WardenState::AwaitingModuleStatus;
    WardenFailure failure = WardenFailure::None;
    uint8 transferCount = 0;
};

using LifecycleObserver =
    std::function<void(WardenLifecycleEvent const&)>;

/**
 * Per-session bootstrap state machine for the delivered Warden module.
 *
 * This class alone decrypts/decodes client bodies, advances directional
 * streams, bounds module transfer, owns waiting-state deadlines, and installs
 * post-hash keys. The current phase deliberately stops at ModuleReady.
 */
class WardenServer
{
public:
    WardenServer(ModuleProfile const& profile, WardenCryptoContext&& crypto,
        SendEncrypted send, WardenLimits limits = {},
        LifecycleObserver observer = {});

    // Idempotently emits MODULE_USE and begins the module-status deadline.
    bool Start();

    // Accepts one complete encrypted CMSG_WARDEN_DATA body on the world thread.
    // Bodies received before Start are ignored without advancing crypto.
    void HandleEncrypted(ByteView encryptedBody);

    // Advances the cumulative deadline for the current waiting state.
    void Update(uint32 diffMs);

    WardenState GetState() const;
    WardenFailure GetFailure() const;
    uint8 GetTransferCount() const;

private:
    // Failed is absorbing. ModuleReady may transition once to Replay failure if
    // another bootstrap command arrives.
    void Fail(WardenFailure reason);
    void NotifyTerminal();
    bool SendPlain(Bytes plain);
    void ResetDeadline();
    bool SendModuleTransfer();
    bool SendHashRequest();

    ModuleProfile m_profile;
    WardenCryptoContext m_crypto;
    SendEncrypted m_send;
    WardenLimits m_limits;
    LifecycleObserver m_observer;
    WardenState m_state = WardenState::AwaitingModuleStatus;
    WardenFailure m_failure = WardenFailure::None;
    uint32 m_remainingMs = 0;
    uint8 m_transferCount = 0;
    bool m_started = false;
};
}

#endif
