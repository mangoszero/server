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

#include "WardenCheckCatalog.h"
#include "WardenManager.h"
#include "WardenServer.h"

#include <openssl/crypto.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
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

warden::MpqCheckProfile TestMpqProfile()
{
    warden::MpqCheckProfile profile;
    profile.checkId = 1;
    profile.path = "DBFilesClient\\AreaTable.dbc";
    profile.expectedSha1 = FixedBytes<20>(
        "7D88154D3411811985F5D81177C5453248133443");
    return profile;
}

warden::LuaCheckProfile TestLuaProfile()
{
    return {2, "OKAY", "Okay"};
}

std::vector<warden::MemCheckProfile> TestMemProfiles()
{
    warden::WardenCheckCatalog const catalog;
    std::vector<warden::MemCheckProfile> const* profiles =
        catalog.FindMem(5875, "Win", "enUS");
    return profiles ? *profiles : std::vector<warden::MemCheckProfile>{};
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

struct ManagerLocale
{
    std::string value;
};

struct Harness
{
    explicit Harness(bool allowSend = true,
        std::optional<warden::MpqCheckProfile> mpqCheck = std::nullopt,
        std::optional<warden::LuaCheckProfile> luaCheck = std::nullopt,
        std::vector<warden::MemCheckProfile> memChecks = {})
        : peer(TestSessionKey()), sendSucceeds(allowSend)
    {
        warden::WardenModuleCatalog catalog;
        warden::ModuleProfile const* profile = catalog.Find(5875, "Win");
        warden::WardenCryptoContext crypto;
        if (!profile || !crypto.Initialize(TestSessionKey()))
            return;

        server = std::make_unique<warden::WardenServer>(*profile,
            std::move(crypto), MakeSend(), warden::WardenLimits{},
            MakeLifecycleObserver(), MakeEvidenceObserver(), mpqCheck,
            luaCheck, std::move(memChecks));
    }

    explicit Harness(ManagerLocale locale, bool allowSend = true)
        : peer(TestSessionKey()), sendSucceeds(allowSend)
    {
        server = warden::WardenManager::Instance().Create(5875, "Win",
            locale.value, TestSessionKey(), MakeSend(), {},
            MakeLifecycleObserver(), MakeEvidenceObserver());
    }

    void SendClient(warden::Bytes plain)
    {
        warden::Bytes encrypted = peer.EncryptClient(std::move(plain));
        server->HandleEncrypted({encrypted.data(), encrypted.size()});
    }

    warden::SendEncrypted MakeSend()
    {
        return [this](warden::Bytes const& bytes)
        {
            ++sendCalls;
            if (!sendSucceeds)
                return false;
            sent.push_back(bytes);
            return true;
        };
    }

    warden::LifecycleObserver MakeLifecycleObserver()
    {
        return [this](warden::WardenLifecycleEvent const& event)
        {
            events.push_back(event);
        };
    }

    warden::EvidenceObserver MakeEvidenceObserver()
    {
        return [this](warden::WardenEvidence const& evidence)
        {
            evidenceEvents.push_back(evidence);
        };
    }

    BootstrapPeer peer;
    bool sendSucceeds = true;
    size_t sendCalls = 0;
    std::vector<warden::Bytes> sent;
    std::vector<warden::WardenLifecycleEvent> events;
    std::vector<warden::WardenEvidence> evidenceEvents;
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

warden::Bytes ExactModuleInitialization()
{
    return FromHex(
        "031400693D8DD001000200A0772400F0872400"
        "6084240030872400"
        "030800F72DF4F0040000F03B300000"
        "030800672F4D0A01010010C0020001");
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
    if (harness.server->GetState() != warden::WardenState::ModuleReady ||
        harness.server->GetFailure() != warden::WardenFailure::None ||
        harness.sent.size() != 3)
        return false;

    return harness.peer.DecryptServer(harness.sent[2]) ==
        ExactModuleInitialization();
}

bool StartTimingCheck(Harness& harness)
{
    if (!ReachModuleReady(harness))
        return false;

    harness.server->Update(true, 1000);
    if (harness.server->GetState() !=
            warden::WardenState::AwaitingCheckResult ||
        harness.sent.size() != 4)
        return false;

    return harness.peer.DecryptServer(harness.sent.back()) ==
        FromHex("0200287F");
}

bool StartTimingMpqCheck(Harness& harness)
{
    if (!ReachModuleReady(harness))
        return false;

    harness.server->Update(true, 1000);
    if (harness.server->GetState() !=
            warden::WardenState::AwaitingCheckResult ||
        harness.sent.size() != 4)
        return false;

    return harness.peer.DecryptServer(harness.sent.back()) == FromHex(
        "021B444246696C6573436C69656E745C41"
        "7265615461626C652E6462630028E7017F");
}

bool StartTimingMpqLuaCheck(Harness& harness)
{
    if (!ReachModuleReady(harness))
        return false;

    harness.server->Update(true, 1000);
    if (harness.server->GetState() !=
            warden::WardenState::AwaitingCheckResult ||
        harness.sent.size() != 4)
        return false;

    return harness.peer.DecryptServer(harness.sent.back()) == FromHex(
        "021B444246696C6573436C69656E745C41"
        "7265615461626C652E646263044F4B4159"
        "0028E701F4027F");
}

bool StartTimingMpqLuaMemCheck(Harness& harness)
{
    if (!ReachModuleReady(harness))
        return false;

    harness.server->Update(true, 1000);
    if (harness.server->GetState() !=
            warden::WardenState::AwaitingCheckResult ||
        harness.sent.size() != 4)
        return false;

    return harness.peer.DecryptServer(harness.sent.back()) == FromHex(
        "021B444246696C6573436C69656E745C41"
        "7265615461626C652E646263044F4B4159"
        "0028E701F4028C0000896100208C000662"
        "7C000D7F");
}

bool StartTimingMemCheck(Harness& harness)
{
    if (!ReachModuleReady(harness))
        return false;

    harness.server->Update(true, 1000);
    if (harness.server->GetState() !=
            warden::WardenState::AwaitingCheckResult ||
        harness.sent.size() != 4)
        return false;

    return harness.peer.DecryptServer(harness.sent.back()) == FromHex(
        "0200288C0000896100208C0006627C000D7F");
}
}

TEST(WardenServer_cache_hit_reaches_module_ready)
{
    Harness harness;
    REQUIRE(ReachModuleReady(harness));
    CHECK_EQ(harness.sendCalls, 3u);
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
    REQUIRE(harness.sent.size() == 41u);
    CHECK(harness.peer.DecryptServer(harness.sent.back()) ==
        ExactModuleInitialization());
    CHECK_EQ(harness.server->GetTransferCount(), uint8(1));
    REQUIRE(harness.events.size() == 1u);
    CHECK(harness.events[0].state == warden::WardenState::ModuleReady);
    CHECK(harness.events[0].failure == warden::WardenFailure::None);
    CHECK_EQ(harness.events[0].transferCount, uint8(1));
}

TEST(WardenServer_initialization_send_failure_is_terminal_without_retry)
{
    Harness harness;
    REQUIRE(ReachAwaitingHash(harness));

    warden::Bytes encrypted = harness.peer.EncryptClient(CorrectHash());
    harness.peer.InstallModuleKeys();
    harness.sendSucceeds = false;
    harness.server->HandleEncrypted({encrypted.data(), encrypted.size()});

    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::SendFailure);
    CHECK_EQ(harness.sendCalls, 3u);
    CHECK_EQ(harness.sent.size(), 2u);
    REQUIRE(harness.events.size() == 1u);
    CHECK(harness.events[0].state == warden::WardenState::Failed);
    CHECK(harness.events[0].failure == warden::WardenFailure::SendFailure);

    size_t const calls = harness.sendCalls;
    harness.server->HandleEncrypted({encrypted.data(), encrypted.size()});
    harness.server->Update(true, 30000);
    CHECK_EQ(harness.sendCalls, calls);
    CHECK_EQ(harness.events.size(), 1u);
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
        harness.server->Update(false, 30000);
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
    ready.server->Update(false, 30000);
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
    ready.server->Update(false, 30000);
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
    failedSend.server->Update(false, 30000);
    CHECK_EQ(failedSend.sendCalls, 1u);
    CHECK_EQ(failedSend.events.size(), 1u);
}

TEST(WardenServer_deadlines_are_cumulative_in_each_waiting_state)
{
    auto expire = [](Harness& harness)
    {
        harness.server->Update(false, 12000);
        harness.server->Update(false, 17999);
        CHECK(harness.server->GetState() != warden::WardenState::Failed);
        harness.server->Update(false, 1);
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

TEST(WardenManager_creation_is_inert_for_exact_builds_and_rejects_unknown_ones)
{
    size_t calls = 0;
    auto send = [&calls](warden::Bytes const&)
    {
        ++calls;
        return true;
    };

    std::unique_ptr<warden::WardenServer> supported =
        warden::WardenManager::Instance().Create(5875, "Win", "enUS",
            TestSessionKey(), send);
    REQUIRE(supported != nullptr);
    CHECK_EQ(calls, 0u);
    CHECK(supported->Start());
    CHECK_EQ(calls, 1u);
    CHECK(supported->Start());
    CHECK_EQ(calls, 1u);

    std::unique_ptr<warden::WardenServer> supported6005 =
        warden::WardenManager::Instance().Create(6005, "Win", "enGB",
            TestSessionKey(), send);
    REQUIRE(supported6005 != nullptr);
    CHECK_EQ(calls, 1u);
    CHECK(supported6005->Start());
    CHECK_EQ(calls, 2u);

    std::unique_ptr<warden::WardenServer> supported6141 =
        warden::WardenManager::Instance().Create(6141, "Win", "zhCN",
            TestSessionKey(), send);
    REQUIRE(supported6141 != nullptr);
    CHECK_EQ(calls, 2u);
    CHECK(supported6141->Start());
    CHECK_EQ(calls, 3u);

    std::unique_ptr<warden::WardenServer> unsupported =
        warden::WardenManager::Instance().Create(9999, "Win", "enUS",
            TestSessionKey(), send);
    CHECK(unsupported == nullptr);
    CHECK_EQ(calls, 3u);
}

TEST(WardenManager_selects_content_checks_only_for_the_exact_locale)
{
    Harness enUS(ManagerLocale{"enUS"});
    REQUIRE(StartTimingMpqLuaMemCheck(enUS));

    Harness frFR(ManagerLocale{"frFR"});
    REQUIRE(StartTimingCheck(frFR));
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
        warden::WardenManager::Instance().Create(5875, "Win", "enUS",
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

TEST(WardenServer_sends_one_timing_check_after_eligibility_and_reports_stable)
{
    Harness harness;
    REQUIRE(ReachModuleReady(harness));

    harness.server->Update(false, 60000);
    harness.server->Update(true, 999);
    CHECK(harness.server->GetState() == warden::WardenState::ModuleReady);
    CHECK_EQ(harness.sent.size(), 3u);

    harness.server->Update(true, 1);
    REQUIRE(harness.server->GetState() ==
        warden::WardenState::AwaitingCheckResult);
    REQUIRE(harness.sent.size() == 4u);
    warden::Bytes const request = harness.peer.DecryptServer(harness.sent.back());
    CHECK_HEX(request.data(), request.size(), "0200287f");

    harness.SendClient(FromHex("020500A7D43E250178563412"));
    CHECK(harness.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(harness.evidenceEvents.size() == 1u);
    REQUIRE(std::holds_alternative<warden::TimingEvidence>(
        harness.evidenceEvents[0]));
    warden::TimingEvidence const& evidence =
        std::get<warden::TimingEvidence>(harness.evidenceEvents[0]);
    CHECK_EQ(evidence.requestId, uint32(1));
    CHECK(evidence.outcome == warden::TimingOutcome::Stable);
    CHECK_EQ(evidence.clientTick, uint32(0x12345678));

    harness.server->Update(true, 60000);
    CHECK_EQ(harness.sent.size(), 4u);
    CHECK_EQ(harness.evidenceEvents.size(), 1u);
}

TEST(WardenServer_reports_unstable_timing_without_protocol_failure)
{
    Harness harness;
    REQUIRE(StartTimingCheck(harness));

    harness.SendClient(FromHex("020500A490E0960078563412"));
    CHECK(harness.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(harness.evidenceEvents.size() == 1u);
    warden::TimingEvidence const& evidence =
        std::get<warden::TimingEvidence>(harness.evidenceEvents[0]);
    CHECK(evidence.outcome == warden::TimingOutcome::Unstable);
    CHECK_EQ(evidence.clientTick, uint32(0x12345678));
}

TEST(WardenServer_combined_check_preserves_stream_and_reports_ordered_match)
{
    Harness harness(true, TestMpqProfile());
    REQUIRE(StartTimingMpqCheck(harness));

    harness.SendClient(FromHex(
        "021A0088BDFAEB0104030201007D88154D"
        "3411811985F5D81177C5453248133443"));
    CHECK(harness.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(harness.evidenceEvents.size() == 2u);

    REQUIRE(std::holds_alternative<warden::TimingEvidence>(
        harness.evidenceEvents[0]));
    warden::TimingEvidence const& timing =
        std::get<warden::TimingEvidence>(harness.evidenceEvents[0]);
    CHECK_EQ(timing.requestId, uint32(1));
    CHECK(timing.outcome == warden::TimingOutcome::Stable);
    CHECK_EQ(timing.clientTick, uint32(0x01020304));

    REQUIRE(std::holds_alternative<warden::MpqEvidence>(
        harness.evidenceEvents[1]));
    warden::MpqEvidence const& mpq =
        std::get<warden::MpqEvidence>(harness.evidenceEvents[1]);
    CHECK_EQ(mpq.requestId, uint32(1));
    CHECK_EQ(mpq.checkId, uint32(1));
    CHECK(mpq.outcome == warden::MpqOutcome::Match);

    harness.SendClient(ModuleOk());
    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::Replay);
    CHECK_EQ(harness.evidenceEvents.size(), 2u);
}

TEST(WardenServer_valid_mpq_negatives_are_observation_only)
{
    Harness unavailable(true, TestMpqProfile());
    REQUIRE(StartTimingMpqCheck(unavailable));
    unavailable.SendClient(FromHex("020600C06DA567010403020101"));
    CHECK(unavailable.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(unavailable.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(unavailable.evidenceEvents.size() == 2u);
    CHECK(std::get<warden::MpqEvidence>(unavailable.evidenceEvents[1]).outcome ==
        warden::MpqOutcome::Unavailable);

    Harness mismatch(true, TestMpqProfile());
    REQUIRE(StartTimingMpqCheck(mismatch));
    mismatch.SendClient(FromHex(
        "021A000F45480201040302010000000000"
        "00000000000000000000000000000000"));
    CHECK(mismatch.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(mismatch.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(mismatch.evidenceEvents.size() == 2u);
    CHECK(std::get<warden::MpqEvidence>(mismatch.evidenceEvents[1]).outcome ==
        warden::MpqOutcome::DigestMismatch);

    Harness bothNegative(true, TestMpqProfile());
    REQUIRE(StartTimingMpqCheck(bothNegative));
    bothNegative.SendClient(FromHex("02060071EF43C6000403020101"));
    CHECK(bothNegative.server->GetState() ==
        warden::WardenState::ModuleReady);
    REQUIRE(bothNegative.evidenceEvents.size() == 2u);
    CHECK(std::get<warden::TimingEvidence>(
        bothNegative.evidenceEvents[0]).outcome ==
        warden::TimingOutcome::Unstable);
    CHECK(std::get<warden::MpqEvidence>(
        bothNegative.evidenceEvents[1]).outcome ==
        warden::MpqOutcome::Unavailable);
}

TEST(WardenServer_combined_lua_match_is_classified_without_text_evidence)
{
    Harness harness(true, TestMpqProfile(), TestLuaProfile());
    REQUIRE(StartTimingMpqLuaCheck(harness));

    harness.SendClient(FromHex(
        "022000376F4E370104030201007D88154D"
        "3411811985F5D81177C5453248133443"
        "00044F6B6179"));
    CHECK(harness.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(harness.evidenceEvents.size() == 3u);
    CHECK(std::holds_alternative<warden::TimingEvidence>(
        harness.evidenceEvents[0]));
    CHECK(std::holds_alternative<warden::MpqEvidence>(
        harness.evidenceEvents[1]));
    REQUIRE(std::holds_alternative<warden::LuaEvidence>(
        harness.evidenceEvents[2]));
    warden::LuaEvidence const& lua =
        std::get<warden::LuaEvidence>(harness.evidenceEvents[2]);
    CHECK_EQ(lua.requestId, uint32(1));
    CHECK_EQ(lua.checkId, uint32(2));
    CHECK(lua.outcome == warden::LuaOutcome::Match);

    harness.SendClient(ModuleOk());
    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::Replay);
    CHECK_EQ(harness.evidenceEvents.size(), 3u);
}

TEST(WardenServer_valid_lua_negatives_are_observation_only)
{
    Harness unavailable(true, TestMpqProfile(), TestLuaProfile());
    REQUIRE(StartTimingMpqLuaCheck(unavailable));
    unavailable.SendClient(FromHex(
        "021B007362FCCC0104030201007D88154D"
        "3411811985F5D81177C545324813344301"));
    CHECK(unavailable.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(unavailable.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(unavailable.evidenceEvents.size() == 3u);
    REQUIRE(std::holds_alternative<warden::LuaEvidence>(
        unavailable.evidenceEvents[2]));
    CHECK(std::get<warden::LuaEvidence>(
        unavailable.evidenceEvents[2]).outcome ==
        warden::LuaOutcome::Unavailable);

    Harness mismatch(true, TestMpqProfile(), TestLuaProfile());
    REQUIRE(StartTimingMpqLuaCheck(mismatch));
    mismatch.SendClient(FromHex(
        "021F00D71749220104030201007D88154D"
        "3411811985F5D81177C5453248133443"
        "0003426164"));
    CHECK(mismatch.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(mismatch.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(mismatch.evidenceEvents.size() == 3u);
    REQUIRE(std::holds_alternative<warden::LuaEvidence>(
        mismatch.evidenceEvents[2]));
    CHECK(std::get<warden::LuaEvidence>(mismatch.evidenceEvents[2]).outcome ==
        warden::LuaOutcome::TextMismatch);
}

TEST(WardenServer_malformed_lua_result_publishes_no_partial_evidence)
{
    Harness harness(true, TestMpqProfile(), TestLuaProfile());
    REQUIRE(StartTimingMpqLuaCheck(harness));

    harness.SendClient(FromHex(
        "021B00D41AA2F80104030201007D88154D"
        "3411811985F5D81177C545324813344302"));
    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() ==
        warden::WardenFailure::MalformedPayload);
    CHECK(harness.evidenceEvents.empty());
}

TEST(WardenServer_mem_results_are_identifier_only_and_ordered)
{
    Harness harness(true, std::nullopt, std::nullopt, TestMemProfiles());
    REQUIRE(StartTimingMemCheck(harness));

    harness.SendClient(FromHex(
        "023400E0F696B3010403020100558BEC8B51408B450C81E2FF7DA07550"
        "8950108B450850E824DA1A005DC208000025FFFFDFFB0D00200000894640"));
    CHECK(harness.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(harness.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(harness.evidenceEvents.size() == 3u);
    CHECK(std::holds_alternative<warden::TimingEvidence>(
        harness.evidenceEvents[0]));
    REQUIRE(std::holds_alternative<warden::MemEvidence>(
        harness.evidenceEvents[1]));
    REQUIRE(std::holds_alternative<warden::MemEvidence>(
        harness.evidenceEvents[2]));

    warden::MemEvidence const& first =
        std::get<warden::MemEvidence>(harness.evidenceEvents[1]);
    warden::MemEvidence const& second =
        std::get<warden::MemEvidence>(harness.evidenceEvents[2]);
    CHECK_EQ(first.requestId, uint32(1));
    CHECK_EQ(first.checkId, uint32(1107));
    CHECK(first.outcome == warden::MemOutcome::Match);
    CHECK_EQ(second.requestId, uint32(1));
    CHECK_EQ(second.checkId, uint32(827));
    CHECK(second.outcome == warden::MemOutcome::Match);
}

TEST(WardenServer_valid_mem_negatives_are_observation_only)
{
    Harness unavailable(true, std::nullopt, std::nullopt,
        TestMemProfiles());
    REQUIRE(StartTimingMemCheck(unavailable));
    unavailable.SendClient(FromHex("020700B7F6E18801040302010101"));
    CHECK(unavailable.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(unavailable.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(unavailable.evidenceEvents.size() == 3u);
    CHECK(std::get<warden::MemEvidence>(
        unavailable.evidenceEvents[1]).outcome ==
        warden::MemOutcome::Unavailable);
    CHECK(std::get<warden::MemEvidence>(
        unavailable.evidenceEvents[2]).outcome ==
        warden::MemOutcome::Unavailable);

    Harness mismatch(true, std::nullopt, std::nullopt, TestMemProfiles());
    REQUIRE(StartTimingMemCheck(mismatch));
    mismatch.SendClient(FromHex(
        "0234009A448C650104030201000000000000000000000000000000000000"
        "0000000000000000000000000000000025FFFFDFFB0D00200000894640"));
    CHECK(mismatch.server->GetState() == warden::WardenState::ModuleReady);
    CHECK(mismatch.server->GetFailure() == warden::WardenFailure::None);
    REQUIRE(mismatch.evidenceEvents.size() == 3u);
    CHECK(std::get<warden::MemEvidence>(
        mismatch.evidenceEvents[1]).outcome ==
        warden::MemOutcome::ByteMismatch);
    CHECK(std::get<warden::MemEvidence>(
        mismatch.evidenceEvents[2]).outcome == warden::MemOutcome::Match);
}

TEST(WardenServer_malformed_mem_result_publishes_no_partial_evidence)
{
    Harness harness(true, std::nullopt, std::nullopt, TestMemProfiles());
    REQUIRE(StartTimingMemCheck(harness));

    harness.SendClient(FromHex("0206008AFC74C1010403020102"));
    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() ==
        warden::WardenFailure::MalformedPayload);
    CHECK(harness.evidenceEvents.empty());
}

TEST(WardenServer_combined_malformed_result_publishes_no_partial_evidence)
{
    Harness harness(true, TestMpqProfile());
    REQUIRE(StartTimingMpqCheck(harness));

    harness.SendClient(FromHex("0206008AFC74C1010403020102"));
    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() ==
        warden::WardenFailure::MalformedPayload);
    CHECK(harness.evidenceEvents.empty());
}

TEST(WardenServer_invalid_internal_mpq_plan_fails_before_sending)
{
    warden::MpqCheckProfile invalid = TestMpqProfile();
    invalid.checkId = 0;
    Harness harness(true, invalid);
    REQUIRE(ReachModuleReady(harness));

    harness.server->Update(true, 1000);
    CHECK(harness.server->GetState() == warden::WardenState::Failed);
    CHECK(harness.server->GetFailure() ==
        warden::WardenFailure::UnexpectedCommand);
    CHECK_EQ(harness.sent.size(), 3u);
    CHECK(harness.evidenceEvents.empty());
}

TEST(WardenServer_timing_timeout_malformed_unexpected_and_send_fail_are_terminal)
{
    Harness timeout;
    REQUIRE(StartTimingCheck(timeout));
    timeout.server->Update(true, 29999);
    CHECK(timeout.server->GetState() ==
        warden::WardenState::AwaitingCheckResult);
    timeout.server->Update(true, 1);
    CHECK(timeout.server->GetState() == warden::WardenState::Failed);
    CHECK(timeout.server->GetFailure() ==
        warden::WardenFailure::DeadlineExpired);

    Harness malformed;
    REQUIRE(StartTimingCheck(malformed));
    malformed.SendClient(FromHex("020500A6D43E250178563412"));
    CHECK(malformed.server->GetState() == warden::WardenState::Failed);
    CHECK(malformed.server->GetFailure() ==
        warden::WardenFailure::MalformedPayload);
    CHECK(malformed.evidenceEvents.empty());

    Harness unexpected;
    REQUIRE(StartTimingCheck(unexpected));
    unexpected.SendClient(ModuleOk());
    CHECK(unexpected.server->GetState() == warden::WardenState::Failed);
    CHECK(unexpected.server->GetFailure() ==
        warden::WardenFailure::UnexpectedCommand);
    CHECK(unexpected.evidenceEvents.empty());

    Harness sendFailure(true, TestMpqProfile());
    REQUIRE(ReachModuleReady(sendFailure));
    sendFailure.sendSucceeds = false;
    sendFailure.server->Update(true, 1000);
    CHECK(sendFailure.server->GetState() == warden::WardenState::Failed);
    CHECK(sendFailure.server->GetFailure() ==
        warden::WardenFailure::SendFailure);
    CHECK(sendFailure.evidenceEvents.empty());
}
