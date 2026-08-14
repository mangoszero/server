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

#include "TestHarness.h"

#include "WardenManager.h"
#include "WardenServer.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace
{
warden::SessionKey TestSessionKey()
{
    warden::SessionKey key{};
    for (uint8 i = 0; i < 39; ++i)
        key[i] = uint8(i + 1);
    return key;
}

warden::Bytes FromHex(char const* text)
{
    auto nibble = [](char value) -> uint8
    {
        if (value >= '0' && value <= '9')
            return uint8(value - '0');
        if (value >= 'a' && value <= 'f')
            return uint8(value - 'a' + 10);
        return uint8(value - 'A' + 10);
    };

    size_t const length = std::strlen(text);
    warden::Bytes bytes;
    bytes.reserve(length / 2);
    for (size_t i = 0; i < length; i += 2)
        bytes.push_back(uint8((nibble(text[i]) << 4) | nibble(text[i + 1])));
    return bytes;
}

template <size_t Size>
std::array<uint8, Size> FixedBytes(char const* text)
{
    warden::Bytes const bytes = FromHex(text);
    std::array<uint8, Size> result{};
    if (bytes.size() == result.size())
        std::copy(bytes.begin(), bytes.end(), result.begin());
    return result;
}

bool Digest(EVP_MD const* algorithm, uint8 const* data, size_t size,
    uint8* output, size_t expectedSize)
{
    unsigned int length = 0;
    return EVP_Digest(data, size, output, &length, algorithm, nullptr) == 1 &&
        length == expectedSize;
}

class PeerRc4
{
public:
    ~PeerRc4()
    {
        OPENSSL_cleanse(m_permutation.data(), m_permutation.size());
    }

    void Initialize(std::array<uint8, 16> const& key)
    {
        for (size_t i = 0; i < m_permutation.size(); ++i)
            m_permutation[i] = uint8(i);
        uint32 j = 0;
        for (size_t i = 0; i < m_permutation.size(); ++i)
        {
            j = (j + m_permutation[i] + key[i % key.size()]) & 0xFF;
            std::swap(m_permutation[i], m_permutation[j]);
        }
        m_i = 0;
        m_j = 0;
    }

    void Transform(warden::Bytes& bytes)
    {
        for (uint8& byte : bytes)
        {
            m_i = uint8(m_i + 1);
            m_j = uint8(m_j + m_permutation[m_i]);
            std::swap(m_permutation[m_i], m_permutation[m_j]);
            byte ^= m_permutation[uint8(m_permutation[m_i] +
                m_permutation[m_j])];
        }
    }

private:
    std::array<uint8, 256> m_permutation{};
    uint8 m_i = 0;
    uint8 m_j = 0;
};

class BootstrapPeer
{
public:
    explicit BootstrapPeer(warden::SessionKey const& sessionKey)
    {
        std::array<uint8, 20> left{};
        std::array<uint8, 20> right{};
        std::array<uint8, 20> current{};
        std::array<uint8, 60> input{};
        std::array<uint8, 40> generated{};

        bool success = Digest(EVP_sha1(), sessionKey.data(), 20,
                left.data(), left.size()) &&
            Digest(EVP_sha1(), sessionKey.data() + 20, 20,
                right.data(), right.size());
        size_t offset = 0;
        while (success && offset < 32)
        {
            std::copy(left.begin(), left.end(), input.begin());
            std::copy(current.begin(), current.end(), input.begin() + 20);
            std::copy(right.begin(), right.end(), input.begin() + 40);
            success = Digest(EVP_sha1(), input.data(), input.size(),
                current.data(), current.size());
            size_t const count = std::min(current.size(), size_t(32) - offset);
            if (success)
            {
                std::copy(current.begin(), current.begin() + count,
                    generated.begin() + offset);
                offset += count;
            }
        }

        std::array<uint8, 16> clientKey{};
        std::array<uint8, 16> serverKey{};
        if (success)
        {
            std::copy(generated.begin(), generated.begin() + 16,
                clientKey.begin());
            std::copy(generated.begin() + 16, generated.begin() + 32,
                serverKey.begin());
        }
        m_clientToServer.Initialize(clientKey);
        m_serverToClient.Initialize(serverKey);

        OPENSSL_cleanse(left.data(), left.size());
        OPENSSL_cleanse(right.data(), right.size());
        OPENSSL_cleanse(current.data(), current.size());
        OPENSSL_cleanse(input.data(), input.size());
        OPENSSL_cleanse(generated.data(), generated.size());
        OPENSSL_cleanse(clientKey.data(), clientKey.size());
        OPENSSL_cleanse(serverKey.data(), serverKey.size());
    }

    warden::Bytes DecryptServer(warden::Bytes const& encrypted)
    {
        warden::Bytes plain = encrypted;
        m_serverToClient.Transform(plain);
        return plain;
    }

    warden::Bytes EncryptClient(warden::Bytes plain)
    {
        m_clientToServer.Transform(plain);
        return plain;
    }

    void InstallModuleKeys()
    {
        std::array<uint8, 16> const clientKey = FixedBytes<16>(
            "7F96EEFDA5B63D20A4DF8E00CBF48304");
        std::array<uint8, 16> const serverKey = FixedBytes<16>(
            "C2B7ADEDFCCCA9C2BFB3F85602BA809B");
        m_clientToServer.Initialize(clientKey);
        m_serverToClient.Initialize(serverKey);
    }

private:
    PeerRc4 m_clientToServer;
    PeerRc4 m_serverToClient;
};

struct Harness
{
    explicit Harness(bool allowSend = true)
        : peer(TestSessionKey()), sendSucceeds(allowSend)
    {
        server = warden::WardenManager::Instance().Create(5875, "Win",
            TestSessionKey(), [this](warden::Bytes const& bytes)
            {
                ++sendCalls;
                if (!sendSucceeds)
                    return false;
                sent.push_back(bytes);
                return true;
            }, {}, [this](warden::WardenLifecycleEvent const& event)
            {
                events.push_back(event);
            });
    }

    void SendClient(warden::Bytes plain)
    {
        warden::Bytes encrypted = peer.EncryptClient(std::move(plain));
        server->HandleEncrypted({encrypted.data(), encrypted.size()});
    }

    BootstrapPeer peer;
    bool sendSucceeds = true;
    size_t sendCalls = 0;
    std::vector<warden::Bytes> sent;
    std::vector<warden::WardenLifecycleEvent> events;
    std::unique_ptr<warden::WardenServer> server;
};

warden::Bytes ModuleOk()
{
    return {uint8(warden::ClientCommand::ModuleOk)};
}

warden::Bytes ModuleMissing()
{
    return {uint8(warden::ClientCommand::ModuleMissing)};
}

warden::Bytes CorrectHash()
{
    return FromHex("04568C054C781A972A6037A2290C22B52571A06F4E");
}

bool StartAndReadModuleUse(Harness& harness)
{
    if (!harness.server || !harness.server->Start() || harness.sent.size() != 1)
        return false;
    warden::Bytes const plain = harness.peer.DecryptServer(harness.sent[0]);
    return plain == FromHex(
        "0079C0768D657977D697E10BAD956CCED1"
        "AE25BC51063B77BD363C3EFE0FC173F9"
        "44490000");
}

bool ReachAwaitingHash(Harness& harness)
{
    if (!StartAndReadModuleUse(harness))
        return false;
    harness.SendClient(ModuleOk());
    if (harness.server->GetState() != warden::WardenState::AwaitingHash ||
        harness.sent.size() != 2)
        return false;
    return harness.peer.DecryptServer(harness.sent[1]) ==
        FromHex("054D808D2C77D905C41A6380EC08586AFE");
}

bool ReachModuleReady(Harness& harness)
{
    if (!ReachAwaitingHash(harness))
        return false;
    warden::Bytes encrypted = harness.peer.EncryptClient(CorrectHash());
    harness.peer.InstallModuleKeys();
    harness.server->HandleEncrypted({encrypted.data(), encrypted.size()});
    return harness.server->GetState() == warden::WardenState::ModuleReady &&
        harness.server->GetFailure() == warden::WardenFailure::None;
}
}

TEST(WardenServer_cache_hit_reaches_module_ready)
{
    Harness harness;
    REQUIRE(ReachModuleReady(harness));
    CHECK_EQ(harness.sendCalls, 2u);
    CHECK_EQ(harness.server->GetTransferCount(), uint8(0));
    REQUIRE(harness.events.size() == 1u);
    CHECK(harness.events[0].state == warden::WardenState::ModuleReady);
    CHECK(harness.events[0].failure == warden::WardenFailure::None);
    CHECK_EQ(harness.events[0].transferCount, uint8(0));
}

TEST(WardenServer_cache_miss_transfers_exact_custody_pinned_module_once)
{
    Harness harness;
    REQUIRE(StartAndReadModuleUse(harness));
    harness.SendClient(ModuleMissing());

    REQUIRE(harness.server->GetState() ==
        warden::WardenState::AwaitingTransferResult);
    REQUIRE(harness.sent.size() == 39u);

    warden::Bytes module;
    for (size_t i = 1; i < harness.sent.size(); ++i)
    {
        warden::Bytes const plain = harness.peer.DecryptServer(harness.sent[i]);
        REQUIRE(plain.size() >= 3);
        REQUIRE(plain[0] == uint8(warden::ServerCommand::ModuleCache));
        size_t const chunkSize = size_t(plain[1]) | (size_t(plain[2]) << 8);
        REQUIRE(chunkSize == plain.size() - 3);
        if (i < 38)
            CHECK_EQ(chunkSize, 500u);
        else
            CHECK_EQ(chunkSize, 256u);
        module.insert(module.end(), plain.begin() + 3, plain.end());
    }

    REQUIRE(module.size() == 18756u);
    std::array<uint8, 16> md5{};
    std::array<uint8, 32> sha256{};
    REQUIRE(Digest(EVP_md5(), module.data(), module.size(), md5.data(), md5.size()));
    REQUIRE(Digest(EVP_sha256(), module.data(), module.size(),
        sha256.data(), sha256.size()));
    CHECK_HEX(md5.data(), md5.size(), "79c0768d657977d697e10bad956cced1");
    CHECK_HEX(sha256.data(), sha256.size(),
        "6c68006a2f1fd31e7208204b3f7ceb94a6ce977876e13f2f703e9cd644482289");

    harness.SendClient(ModuleOk());
    REQUIRE(harness.server->GetState() == warden::WardenState::AwaitingHash);
    REQUIRE(harness.sent.size() == 40u);
    warden::Bytes const hashRequest =
        harness.peer.DecryptServer(harness.sent.back());
    CHECK_HEX(hashRequest.data(), hashRequest.size(),
        "054d808d2c77d905c41a6380ec08586afe");

    warden::Bytes encrypted = harness.peer.EncryptClient(CorrectHash());
    harness.peer.InstallModuleKeys();
    harness.server->HandleEncrypted({encrypted.data(), encrypted.size()});
    CHECK(harness.server->GetState() == warden::WardenState::ModuleReady);
    CHECK_EQ(harness.server->GetTransferCount(), uint8(1));
    REQUIRE(harness.events.size() == 1u);
    CHECK(harness.events[0].state == warden::WardenState::ModuleReady);
    CHECK(harness.events[0].failure == warden::WardenFailure::None);
    CHECK_EQ(harness.events[0].transferCount, uint8(1));
}

TEST(WardenServer_second_module_missing_is_terminal_without_retransfer)
{
    Harness harness;
    REQUIRE(StartAndReadModuleUse(harness));
    harness.SendClient(ModuleMissing());
    REQUIRE(harness.sent.size() == 39u);
    for (size_t i = 1; i < harness.sent.size(); ++i)
        harness.peer.DecryptServer(harness.sent[i]);

    harness.SendClient(ModuleMissing());
    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() ==
        warden::WardenFailure::ModuleDigestMismatch);
    CHECK_EQ(harness.sendCalls, 39u);
    CHECK_EQ(harness.server->GetTransferCount(), uint8(1));
}

TEST(WardenServer_classifies_terminal_protocol_failures)
{
    auto expectInitialFailure = [](warden::Bytes plain,
        warden::WardenFailure expected)
    {
        Harness harness;
        REQUIRE(StartAndReadModuleUse(harness));
        harness.SendClient(std::move(plain));
        CHECK(harness.server->GetState() == warden::WardenState::Failed);
        CHECK(harness.server->GetFailure() == expected);
        size_t const calls = harness.sendCalls;
        harness.server->HandleEncrypted({});
        harness.server->Update(30000);
        CHECK_EQ(harness.sendCalls, calls);
    };

    expectInitialFailure({uint8(warden::ClientCommand::ModuleFailed)},
        warden::WardenFailure::ModuleLoadFailed);
    expectInitialFailure({uint8(warden::ClientCommand::ModuleOk), 0},
        warden::WardenFailure::MalformedPayload);
    expectInitialFailure({2}, warden::WardenFailure::UnexpectedCommand);
    expectInitialFailure(CorrectHash(), warden::WardenFailure::UnexpectedCommand);

    Harness wrongHash;
    REQUIRE(ReachAwaitingHash(wrongHash));
    warden::Bytes hash(21, 0);
    hash[0] = uint8(warden::ClientCommand::HashResult);
    wrongHash.SendClient(hash);
    CHECK(wrongHash.server->GetState() == warden::WardenState::Failed);
    CHECK(wrongHash.server->GetFailure() == warden::WardenFailure::HashMismatch);
}

TEST(WardenServer_replay_and_send_failure_are_terminal)
{
    Harness ready;
    REQUIRE(ReachModuleReady(ready));
    REQUIRE(ready.events.size() == 1u);
    ready.server->Update(30000);
    CHECK_EQ(ready.events.size(), 1u);
    size_t const readyCalls = ready.sendCalls;
    ready.SendClient(ModuleOk());
    CHECK(ready.server->GetFailure() == warden::WardenFailure::Replay);
    CHECK_EQ(ready.sendCalls, readyCalls);
    REQUIRE(ready.events.size() == 2u);
    CHECK(ready.events[1].state == warden::WardenState::Failed);
    CHECK(ready.events[1].failure == warden::WardenFailure::Replay);
    CHECK_EQ(ready.events[1].transferCount, uint8(0));
    ready.SendClient(ModuleOk());
    ready.server->Update(30000);
    CHECK_EQ(ready.events.size(), 2u);

    Harness failedSend(false);
    REQUIRE(failedSend.server != nullptr);
    CHECK(!failedSend.server->Start());
    CHECK(failedSend.server->GetFailure() == warden::WardenFailure::SendFailure);
    CHECK_EQ(failedSend.sendCalls, 1u);
    REQUIRE(failedSend.events.size() == 1u);
    CHECK(failedSend.events[0].state == warden::WardenState::Failed);
    CHECK(failedSend.events[0].failure == warden::WardenFailure::SendFailure);
    CHECK(!failedSend.server->Start());
    failedSend.server->HandleEncrypted({});
    failedSend.server->Update(30000);
    CHECK_EQ(failedSend.sendCalls, 1u);
    CHECK_EQ(failedSend.events.size(), 1u);
}

TEST(WardenServer_deadlines_are_cumulative_in_each_waiting_state)
{
    auto expire = [](Harness& harness)
    {
        harness.server->Update(12000);
        harness.server->Update(17999);
        CHECK(harness.server->GetState() != warden::WardenState::Failed);
        harness.server->Update(1);
        CHECK(harness.server->GetState() == warden::WardenState::Failed);
        CHECK(harness.server->GetFailure() ==
            warden::WardenFailure::DeadlineExpired);
    };

    Harness status;
    REQUIRE(StartAndReadModuleUse(status));
    expire(status);

    Harness transfer;
    REQUIRE(StartAndReadModuleUse(transfer));
    transfer.SendClient(ModuleMissing());
    REQUIRE(transfer.server->GetState() ==
        warden::WardenState::AwaitingTransferResult);
    expire(transfer);

    Harness hash;
    REQUIRE(ReachAwaitingHash(hash));
    expire(hash);
}

TEST(WardenManager_creation_is_inert_and_rejects_unsupported_profiles)
{
    size_t calls = 0;
    auto send = [&calls](warden::Bytes const&)
    {
        ++calls;
        return true;
    };

    std::unique_ptr<warden::WardenServer> supported =
        warden::WardenManager::Instance().Create(5875, "Win",
            TestSessionKey(), send);
    REQUIRE(supported != nullptr);
    CHECK_EQ(calls, 0u);
    CHECK(supported->Start());
    CHECK_EQ(calls, 1u);
    CHECK(supported->Start());
    CHECK_EQ(calls, 1u);

    std::unique_ptr<warden::WardenServer> unsupported =
        warden::WardenManager::Instance().Create(6005, "Win",
            TestSessionKey(), send);
    CHECK(unsupported == nullptr);
    CHECK_EQ(calls, 1u);
}

TEST(WardenServer_uninitialized_crypto_fails_before_sending)
{
    warden::WardenModuleCatalog catalog;
    warden::ModuleProfile const* profile = catalog.Find(5875, "Win");
    REQUIRE(profile != nullptr);

    size_t calls = 0;
    warden::WardenCryptoContext crypto;
    warden::WardenServer server(*profile, std::move(crypto),
        [&calls](warden::Bytes const&)
        {
            ++calls;
            return true;
        });
    CHECK(!server.Start());
    CHECK(server.GetFailure() == warden::WardenFailure::CryptoFailure);
    CHECK_EQ(calls, 0u);
}

TEST(WardenServer_ignores_prestart_data_and_can_start)
{
    size_t calls = 0;
    std::unique_ptr<warden::WardenServer> server =
        warden::WardenManager::Instance().Create(5875, "Win",
            TestSessionKey(), [&calls](warden::Bytes const&)
            {
                ++calls;
                return true;
            });
    REQUIRE(server != nullptr);

    uint8 const unsolicited = 0xA5;
    server->HandleEncrypted({&unsolicited, 1});
    CHECK(server->GetState() == warden::WardenState::AwaitingModuleStatus);
    CHECK(server->GetFailure() == warden::WardenFailure::None);
    CHECK_EQ(calls, 0u);

    CHECK(server->Start());
    CHECK(server->GetState() == warden::WardenState::AwaitingModuleStatus);
    CHECK(server->GetFailure() == warden::WardenFailure::None);
    CHECK_EQ(calls, 1u);
}
