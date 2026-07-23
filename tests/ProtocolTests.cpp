#include "TestSupport.hpp"

#include "Auth/Sha1.h"
#include "ClientConnection.h"
#include "IWorldGateway.h"
#include "Opcodes.h"
#include "PacketCodec.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
static_assert(uint8(proto::AuthStatus::Ok) == 0x0C);
static_assert(uint8(proto::AuthStatus::Failed) == 0x0D);
static_assert(uint8(proto::AuthStatus::Reject) == 0x0E);
static_assert(uint8(proto::AuthStatus::BadServerProof) == 0x0F);
static_assert(uint8(proto::AuthStatus::Unavailable) == 0x10);
static_assert(uint8(proto::AuthStatus::SystemError) == 0x11);
static_assert(uint8(proto::AuthStatus::BillingError) == 0x12);
static_assert(uint8(proto::AuthStatus::BillingExpired) == 0x13);
static_assert(uint8(proto::AuthStatus::VersionMismatch) == 0x14);
static_assert(uint8(proto::AuthStatus::UnknownAccount) == 0x15);
static_assert(uint8(proto::AuthStatus::IncorrectPassword) == 0x16);
static_assert(uint8(proto::AuthStatus::SessionExpired) == 0x17);
static_assert(uint8(proto::AuthStatus::ServerShuttingDown) == 0x18);
static_assert(uint8(proto::AuthStatus::AlreadyLoggingIn) == 0x19);
static_assert(uint8(proto::AuthStatus::LoginServerNotFound) == 0x1A);
static_assert(uint8(proto::AuthStatus::WaitQueue) == 0x1B);
static_assert(uint8(proto::AuthStatus::Banned) == 0x1C);
static_assert(uint8(proto::AuthStatus::AlreadyOnline) == 0x1D);
static_assert(uint8(proto::AuthStatus::NoTime) == 0x1E);
static_assert(uint8(proto::AuthStatus::DatabaseBusy) == 0x1F);
static_assert(uint8(proto::AuthStatus::Suspended) == 0x20);
static_assert(uint8(proto::AuthStatus::ParentalControl) == 0x21);

std::vector<uint8> ClientFrame(uint32 opcode, std::size_t payloadSize = 0)
{
    uint16 const size = uint16(4 + payloadSize);
    std::vector<uint8> wire = {
        uint8(size >> 8), uint8(size),
        uint8(opcode), uint8(opcode >> 8), uint8(opcode >> 16), uint8(opcode >> 24)
    };
    wire.resize(proto::CLIENT_HEADER_SIZE + payloadSize, 0x5A);
    return wire;
}

std::vector<uint8> ClientFrame(uint32 opcode, uint8 const* payload,
    std::size_t payloadSize)
{
    std::vector<uint8> wire = ClientFrame(opcode, payloadSize);
    if (payloadSize != 0)
        std::copy(payload, payload + payloadSize, wire.begin() + proto::CLIENT_HEADER_SIZE);
    return wire;
}

std::vector<uint8> ClientFrame(uint32 opcode, std::initializer_list<uint8> payload)
{
    return ClientFrame(opcode, payload.begin(), payload.size());
}

class DummyAuthContext final : public proto::AuthContext
{
};

class FakeGateway final : public proto::IWorldGateway
{
public:
    proto::AuthLookup lookup;
    proto::SessionId attachResult = 41;
    unsigned filterCalls = 0;
    unsigned lookupCalls = 0;
    unsigned attachCalls = 0;
    unsigned deliverCalls = 0;
    unsigned detachCalls = 0;
    bool filterResult = true;
    bool throwOnLookup = false;
    bool throwOnTrace = false;
    bool throwOnAttach = false;
    bool throwOnDeliver = false;
    bool throwOnDetach = false;
    bool throwOnSender = false;
    bool throwOnCloser = false;
    bool sendDuringAttach = false;
    bool monotonicRoutes = false;
    proto::SessionId nextSessionId = proto::INVALID_SESSION_ID;
    std::unordered_set<proto::SessionId> liveRoutes;
    std::function<void()> duringAttach;
    std::vector<std::pair<uint16, bool>> traced;
    std::vector<WorldPacket> delivered;
    proto::AuthRequest attachedRequest;
    std::shared_ptr<proto::IClientLink> retainedLink;

    bool FilterAuthPacket(WorldPacket&) override
    {
        ++filterCalls;
        return filterResult;
    }

    void TracePacket(WorldPacket const& packet, bool incoming) override
    {
        if (throwOnTrace)
            throw std::runtime_error("simulated trace failure");
        traced.emplace_back(packet.GetOpcode(), incoming);
    }

    proto::AuthLookup LookupAccount(proto::AuthRequest const&) override
    {
        ++lookupCalls;
        if (throwOnLookup)
            throw std::runtime_error("simulated lookup failure");
        return lookup;
    }

    proto::SessionId Attach(proto::AuthRequest const& request,
        std::shared_ptr<proto::IClientLink> const& link,
        std::shared_ptr<proto::AuthContext> const&) override
    {
        ++attachCalls;
        if (throwOnAttach)
            throw std::runtime_error("simulated attach failure");
        if (duringAttach)
            duringAttach();
        attachedRequest = request;
        retainedLink = link;
        if (sendDuringAttach)
        {
            WorldPacket addon(SMSG_ADDON_INFO, 1);
            addon << uint8(0xA5);
            link->SendPacket(addon);
        }
        proto::SessionId session = attachResult;
        if (monotonicRoutes)
        {
            do
            {
                session = ++nextSessionId;
                if (session == proto::INVALID_SESSION_ID)
                    session = ++nextSessionId;
            }
            while (liveRoutes.find(session) != liveRoutes.end());
        }
        liveRoutes.insert(session);
        return session;
    }

    void Deliver(proto::SessionId session, WorldPacket&& packet) override
    {
        if (liveRoutes.find(session) == liveRoutes.end())
            return;
        if (!monotonicRoutes)
            CHECK(session == attachResult);
        ++deliverCalls;
        if (throwOnDeliver)
            throw std::runtime_error("simulated delivery failure");
        delivered.push_back(std::move(packet));
    }

    void Detach(proto::SessionId session) override
    {
        if (!monotonicRoutes)
            CHECK(session == attachResult);
        ++detachCalls;
        liveRoutes.erase(session);
        if (throwOnDetach)
            throw std::runtime_error("simulated detach failure");
    }
};

struct ConnectionHarness
{
    FakeGateway gateway;
    std::shared_ptr<proto::ClientConnection> connection =
        std::make_shared<proto::ClientConnection>(gateway);
    std::vector<std::vector<uint8>> sent;
    unsigned closeCalls = 0;

    ConnectionHarness()
    {
        connection->setPeerAddress("127.0.0.1");
        connection->setSender([this](uint8 const* data, std::size_t len)
        {
            if (gateway.throwOnSender)
                throw std::runtime_error("simulated sender failure");
            sent.emplace_back(data, data + len);
        });
        connection->setCloser([this]()
        {
            ++closeCalls;
            if (gateway.throwOnCloser)
                throw std::runtime_error("simulated closer failure");
        });
    }
};

uint16 ServerOpcode(std::vector<uint8> const& frame)
{
    CHECK(frame.size() >= proto::SERVER_HEADER_SIZE);
    return uint16(frame[2]) | (uint16(frame[3]) << 8);
}

uint32 ServerSeed(std::vector<uint8> const& challenge)
{
    CHECK(challenge.size() == proto::SERVER_HEADER_SIZE + 4);
    return uint32(challenge[4])
        | (uint32(challenge[5]) << 8)
        | (uint32(challenge[6]) << 16)
        | (uint32(challenge[7]) << 24);
}

std::array<uint8, 20> MakeProof(std::string const& account, uint32 clientSeed,
    uint32 serverSeed, BigNumber& sessionKey)
{
    uint8 const zero[4] = {0, 0, 0, 0};
    Sha1Hash sha;
    sha.UpdateData(account);
    sha.UpdateData(zero, sizeof(zero));
    sha.UpdateData(reinterpret_cast<uint8 const*>(&clientSeed), sizeof(clientSeed));
    sha.UpdateData(reinterpret_cast<uint8 const*>(&serverSeed), sizeof(serverSeed));
    sha.UpdateBigNumbers(&sessionKey, nullptr);
    sha.Finalize();

    std::array<uint8, 20> digest{};
    std::copy(sha.GetDigest(), sha.GetDigest() + digest.size(), digest.begin());
    return digest;
}

std::vector<uint8> AuthFrame(uint32 clientSeed, std::array<uint8, 20> const& digest,
    std::initializer_list<uint8> addonData = {})
{
    WorldPacket packet(CMSG_AUTH_SESSION, 64);
    packet << uint32(5875);
    packet << uint32(0x12345678);
    packet << std::string("ACCOUNT");
    packet << clientSeed;
    packet.append(digest.data(), digest.size());
    if (addonData.size() != 0)
        packet.append(addonData.begin(), addonData.size());
    return ClientFrame(CMSG_AUTH_SESSION, packet.contents(), packet.size());
}

class ClassicHeaderCipher
{
public:
    explicit ClassicHeaderCipher(BigNumber& sessionKey)
    {
        uint8* key = sessionKey.AsByteArray(40);
        m_key.assign(key, key + 40);
    }

    void EncryptClientHeader(std::vector<uint8>& frame)
    {
        CHECK(frame.size() >= proto::CLIENT_HEADER_SIZE);
        for (std::size_t offset = 0; offset < proto::CLIENT_HEADER_SIZE; ++offset)
        {
            m_clientIndex %= m_key.size();
            uint8 const encrypted =
                uint8((frame[offset] ^ m_key[m_clientIndex]) + m_clientPrevious);
            ++m_clientIndex;
            frame[offset] = m_clientPrevious = encrypted;
        }
    }

    void DecryptServerHeader(std::vector<uint8>& frame)
    {
        CHECK(frame.size() >= proto::SERVER_HEADER_SIZE);
        for (std::size_t offset = 0; offset < proto::SERVER_HEADER_SIZE; ++offset)
        {
            m_serverIndex %= m_key.size();
            uint8 const encrypted = frame[offset];
            frame[offset] =
                uint8((encrypted - m_serverPrevious) ^ m_key[m_serverIndex]);
            ++m_serverIndex;
            m_serverPrevious = encrypted;
        }
    }

private:
    std::vector<uint8> m_key;
    std::size_t m_clientIndex = 0;
    std::size_t m_serverIndex = 0;
    uint8 m_clientPrevious = 0;
    uint8 m_serverPrevious = 0;
};

BigNumber SuccessfulLookup(FakeGateway& gateway)
{
    BigNumber sessionKey;
    sessionKey.SetHexStr(
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF");
    gateway.lookup.status = proto::AuthStatus::Ok;
    gateway.lookup.sessionKey = sessionKey;
    gateway.lookup.context = std::make_shared<DummyAuthContext>();
    return sessionKey;
}

BigNumber Authenticate(ConnectionHarness& harness, uint32 clientSeed = 0xA1B2C3D4)
{
    BigNumber sessionKey = SuccessfulLookup(harness.gateway);
    std::vector<uint8> const challenge = harness.connection->onConnect();
    std::array<uint8, 20> const proof =
        MakeProof("ACCOUNT", clientSeed, ServerSeed(challenge), sessionKey);
    std::vector<uint8> const auth = AuthFrame(clientSeed, proof, {0xCA, 0xFE});
    harness.connection->onData(auth.data(), auth.size());
    CHECK(harness.gateway.attachCalls == 1);
    CHECK(!harness.connection->closed());
    return sessionKey;
}

void fragmentedFrameDecodesOnce()
{
    proto::PacketCodec codec;
    std::vector<WorldPacket> packets;
    uint8 const frame[] = {0x00, 0x06, 0x34, 0x12, 0x00, 0x00, 0xAA, 0xBB};
    std::size_t consumed = 0;
    CHECK(codec.FeedOne(frame, 3, consumed, packets) == proto::DecodeStatus::NeedMore);
    CHECK(consumed == 3 && packets.empty());
    CHECK(codec.FeedOne(frame + 3, 5, consumed, packets) == proto::DecodeStatus::Ready);
    CHECK(consumed == 5 && packets.size() == 1);
    CHECK(packets[0].GetOpcode() == 0x1234);
    CHECK_BYTES(packets[0].contents(), packets[0].size(), {0xAA, 0xBB});
}

void combinedFramesPreserveOrder()
{
    proto::PacketCodec codec;
    std::vector<WorldPacket> packets;
    std::vector<uint8> wire = ClientFrame(CMSG_PING, 1);
    wire.back() = 0x11;
    std::vector<uint8> second = ClientFrame(CMSG_KEEP_ALIVE);
    wire.insert(wire.end(), second.begin(), second.end());

    CHECK(codec.Feed(wire.data(), wire.size(), packets) == proto::DecodeStatus::Ready);
    CHECK(packets.size() == 2);
    CHECK(packets[0].GetOpcode() == CMSG_PING);
    CHECK(packets[1].GetOpcode() == CMSG_KEEP_ALIVE);
}

void splitHeadersDecryptExactlyOnce()
{
    for (std::size_t split = 1; split < proto::CLIENT_HEADER_SIZE; ++split)
    {
        unsigned decryptCalls = 0;
        proto::PacketCodec codec([&decryptCalls](uint8*, std::size_t len)
        {
            ++decryptCalls;
            CHECK(len == proto::CLIENT_HEADER_SIZE);
        });
        std::vector<WorldPacket> packets;
        std::vector<uint8> const wire = ClientFrame(CMSG_KEEP_ALIVE, 1);
        std::size_t consumed = 0;

        CHECK(codec.FeedOne(wire.data(), split, consumed, packets)
            == proto::DecodeStatus::NeedMore);
        CHECK(consumed == split);
        CHECK(decryptCalls == 0);
        CHECK(codec.FeedOne(wire.data() + split, wire.size() - split, consumed, packets)
            == proto::DecodeStatus::Ready);
        CHECK(consumed == wire.size() - split);
        CHECK(decryptCalls == 1);
        CHECK(packets.size() == 1);
    }
}

void malformedFramesAreRejected()
{
    proto::PacketCodec codec;
    std::vector<WorldPacket> packets;
    std::size_t consumed = 0;
    uint8 const tooSmall[] = {0x00, 0x03, 0x01, 0x00, 0x00, 0x00};
    CHECK(codec.FeedOne(tooSmall, sizeof(tooSmall), consumed, packets)
        == proto::DecodeStatus::Malformed);

    proto::PacketCodec nullCodec;
    CHECK(nullCodec.FeedOne(nullptr, 1, consumed, packets)
        == proto::DecodeStatus::Malformed);

    std::vector<uint8> oversized = {0x28, 0x01, 0, 0, 0, 0};
    proto::PacketCodec oversizedCodec;
    CHECK(oversizedCodec.FeedOne(oversized.data(), oversized.size(), consumed, packets)
        == proto::DecodeStatus::Malformed);
}

void maximumAcceptedSizeDecodes()
{
    std::vector<uint8> const wire =
        ClientFrame(CMSG_KEEP_ALIVE, proto::MAX_CLIENT_PACKET_SIZE - 4);
    proto::PacketCodec codec;
    std::vector<WorldPacket> packets;
    std::size_t consumed = 0;
    CHECK(codec.FeedOne(wire.data(), wire.size(), consumed, packets)
        == proto::DecodeStatus::Ready);
    CHECK(consumed == wire.size());
    CHECK(packets.size() == 1);
    CHECK(packets[0].size() == proto::MAX_CLIENT_PACKET_SIZE - 4);
}

void serverFramesUseTheFixedClassicHeader()
{
    WorldPacket packet(0x1234, 2);
    packet << uint8(0xAA) << uint8(0xBB);
    std::vector<uint8> const frame = proto::PacketCodec::Encode(packet);
    CHECK_BYTES(frame.data(), frame.size(), {0x00, 0x04, 0x34, 0x12, 0xAA, 0xBB});
}

void connectionChallengeHasTheExpectedClassicShape()
{
    ConnectionHarness harness;
    std::vector<uint8> const challenge = harness.connection->onConnect();

    CHECK(challenge.size() == proto::SERVER_HEADER_SIZE + 4);
    CHECK(challenge[0] == 0x00);
    CHECK(challenge[1] == 0x06);
    CHECK(ServerOpcode(challenge) == SMSG_AUTH_CHALLENGE);
    CHECK(harness.gateway.traced.size() == 1);
    CHECK(harness.gateway.traced[0]
        == std::make_pair(uint16(SMSG_AUTH_CHALLENGE), false));
    CHECK(harness.connection->GetRemoteAddress() == "127.0.0.1");
}

void preAuthenticationWorldPacketsAreRejected()
{
    for (uint32 opcode : {uint32(CMSG_PING), uint32(CMSG_KEEP_ALIVE)})
    {
        ConnectionHarness harness;
        std::vector<uint8> const frame = ClientFrame(opcode);
        harness.connection->onData(frame.data(), frame.size());

        CHECK(harness.connection->closed());
        CHECK(harness.closeCalls == 1);
        CHECK(harness.gateway.lookupCalls == 0);
        CHECK(harness.gateway.attachCalls == 0);
        CHECK(harness.gateway.deliverCalls == 0);
    }
}

void authenticationFilterVetoAllowsALaterAcceptedAttempt()
{
    ConnectionHarness harness;
    BigNumber sessionKey = SuccessfulLookup(harness.gateway);
    uint32 const clientSeed = 0x55667788;
    std::vector<uint8> const challenge = harness.connection->onConnect();
    std::array<uint8, 20> const proof =
        MakeProof("ACCOUNT", clientSeed, ServerSeed(challenge), sessionKey);
    std::vector<uint8> const frame = AuthFrame(clientSeed, proof);

    harness.gateway.filterResult = false;
    harness.connection->onData(frame.data(), frame.size());
    CHECK(harness.gateway.filterCalls == 1);
    CHECK(harness.gateway.lookupCalls == 0);
    CHECK(!harness.connection->closed());

    harness.gateway.filterResult = true;
    harness.connection->onData(frame.data(), frame.size());
    CHECK(harness.gateway.filterCalls == 2);
    CHECK(harness.gateway.lookupCalls == 1);
    CHECK(harness.gateway.attachCalls == 1);
    CHECK(!harness.connection->closed());
}

void lookupRejectionSendsStatusAndCloses()
{
    ConnectionHarness harness;
    harness.gateway.lookup.status = proto::AuthStatus::UnknownAccount;
    harness.connection->onConnect();
    std::array<uint8, 20> const digest{};
    std::vector<uint8> const frame = AuthFrame(7, digest);

    harness.connection->onData(frame.data(), frame.size());

    CHECK(harness.gateway.lookupCalls == 1);
    CHECK(harness.gateway.attachCalls == 0);
    CHECK(harness.sent.size() == 1);
    CHECK(ServerOpcode(harness.sent[0]) == SMSG_AUTH_RESPONSE);
    CHECK(harness.sent[0].size() == proto::SERVER_HEADER_SIZE + 1);
    CHECK(harness.sent[0][4] == uint8(proto::AuthStatus::UnknownAccount));
    CHECK(harness.connection->closed());
    CHECK(harness.closeCalls == 1);
}

void invalidProofSendsFailureAndNeverAttaches()
{
    ConnectionHarness harness;
    SuccessfulLookup(harness.gateway);
    harness.connection->onConnect();
    std::array<uint8, 20> const digest{};
    std::vector<uint8> const frame = AuthFrame(7, digest);

    harness.connection->onData(frame.data(), frame.size());

    CHECK(harness.gateway.lookupCalls == 1);
    CHECK(harness.gateway.attachCalls == 0);
    CHECK(harness.sent.size() == 1);
    CHECK(harness.sent[0][4] == uint8(proto::AuthStatus::Failed));
    CHECK(harness.connection->closed());
}

void successfulAuthenticationInitializesClassicCryptBeforeAttach()
{
    ConnectionHarness harness;
    harness.gateway.sendDuringAttach = true;
    BigNumber sessionKey = Authenticate(harness);

    CHECK(harness.gateway.attachedRequest.build == 5875);
    CHECK(harness.gateway.attachedRequest.unknown == 0x12345678);
    CHECK(harness.gateway.attachedRequest.account == "ACCOUNT");
    CHECK(harness.gateway.attachedRequest.peerAddress == "127.0.0.1");
    CHECK(harness.gateway.attachedRequest.addonData
        == std::vector<uint8>({0xCA, 0xFE}));
    CHECK(harness.sent.size() == 1);

    ClassicHeaderCipher cipher(sessionKey);
    cipher.DecryptServerHeader(harness.sent[0]);
    CHECK(ServerOpcode(harness.sent[0]) == SMSG_ADDON_INFO);
}

void authenticationAddonTailStartsAtPositionZero()
{
    ConnectionHarness harness;
    Authenticate(harness);
    WorldPacket addon(CMSG_AUTH_SESSION,
        harness.gateway.attachedRequest.addonData.size());
    addon.append(harness.gateway.attachedRequest.addonData.data(),
        harness.gateway.attachedRequest.addonData.size());

    CHECK(addon.rpos() == 0);
    CHECK_BYTES(addon.contents(), addon.size(), {0xCA, 0xFE});
}

void attachFailureSendsEncryptedSystemErrorWithoutPublishingASession()
{
    ConnectionHarness harness;
    harness.gateway.attachResult = proto::INVALID_SESSION_ID;
    BigNumber sessionKey = SuccessfulLookup(harness.gateway);
    uint32 const clientSeed = 0x11223344;
    std::vector<uint8> const challenge = harness.connection->onConnect();
    std::array<uint8, 20> const proof =
        MakeProof("ACCOUNT", clientSeed, ServerSeed(challenge), sessionKey);
    std::vector<uint8> const auth = AuthFrame(clientSeed, proof);

    harness.connection->onData(auth.data(), auth.size());

    CHECK(harness.gateway.attachCalls == 1);
    CHECK(harness.gateway.detachCalls == 0);
    CHECK(harness.sent.size() == 1);
    ClassicHeaderCipher cipher(sessionKey);
    cipher.DecryptServerHeader(harness.sent[0]);
    CHECK(ServerOpcode(harness.sent[0]) == SMSG_AUTH_RESPONSE);
    CHECK(harness.sent[0][4] == uint8(proto::AuthStatus::SystemError));
    CHECK(harness.connection->closed());
}

void authenticatedPacketsStayOpaqueToTheConnection()
{
    ConnectionHarness harness;
    BigNumber sessionKey = Authenticate(harness);
    ClassicHeaderCipher cipher(sessionKey);

    std::vector<uint8> ping = ClientFrame(CMSG_PING, {0x01, 0x02});
    cipher.EncryptClientHeader(ping);
    harness.connection->onData(ping.data(), ping.size());
    std::vector<uint8> keepAlive = ClientFrame(CMSG_KEEP_ALIVE);
    cipher.EncryptClientHeader(keepAlive);
    harness.connection->onData(keepAlive.data(), keepAlive.size());

    CHECK(harness.gateway.deliverCalls == 2);
    CHECK(harness.gateway.delivered.size() == 2);
    CHECK(harness.gateway.delivered[0].GetOpcode() == CMSG_PING);
    CHECK(harness.gateway.delivered[1].GetOpcode() == CMSG_KEEP_ALIVE);
    CHECK(!harness.connection->closed());
}

void connectionRejectsOutOfRangeOpcodes()
{
    ConnectionHarness accepted;
    BigNumber acceptedKey = Authenticate(accepted);
    ClassicHeaderCipher acceptedCipher(acceptedKey);
    std::vector<uint8> maximum = ClientFrame(NUM_MSG_TYPES - 1);
    acceptedCipher.EncryptClientHeader(maximum);
    accepted.connection->onData(maximum.data(), maximum.size());
    CHECK(accepted.gateway.deliverCalls == 1);
    CHECK(!accepted.connection->closed());

    ConnectionHarness rejected;
    BigNumber rejectedKey = Authenticate(rejected);
    ClassicHeaderCipher rejectedCipher(rejectedKey);
    std::vector<uint8> invalid = ClientFrame(NUM_MSG_TYPES);
    rejectedCipher.EncryptClientHeader(invalid);
    rejected.connection->onData(invalid.data(), invalid.size());
    CHECK(rejected.gateway.deliverCalls == 0);
    CHECK(rejected.connection->closed());
    CHECK(rejected.closeCalls == 1);
}

void repeatedAuthenticationClosesWithoutSecondLookup()
{
    ConnectionHarness harness;
    BigNumber sessionKey = Authenticate(harness);
    ClassicHeaderCipher cipher(sessionKey);
    std::array<uint8, 20> const digest{};
    std::vector<uint8> repeated = AuthFrame(9, digest);
    cipher.EncryptClientHeader(repeated);

    harness.connection->onData(repeated.data(), repeated.size());

    CHECK(harness.gateway.lookupCalls == 1);
    CHECK(harness.connection->closed());
    CHECK(harness.closeCalls == 1);
}

void closeDuringAttachDetachesThePublishedSession()
{
    ConnectionHarness harness;
    BigNumber sessionKey = SuccessfulLookup(harness.gateway);
    uint32 const clientSeed = 0x1234ABCD;
    std::vector<uint8> const challenge = harness.connection->onConnect();
    std::array<uint8, 20> const proof =
        MakeProof("ACCOUNT", clientSeed, ServerSeed(challenge), sessionKey);
    std::vector<uint8> const auth = AuthFrame(clientSeed, proof);
    harness.gateway.duringAttach = [&harness]() { harness.connection->onClose(); };

    harness.connection->onData(auth.data(), auth.size());

    CHECK(harness.gateway.attachCalls == 1);
    CHECK(harness.gateway.detachCalls == 1);
    CHECK(harness.connection->closed());
}

void closeDetachesOnceAndLateSendsAreIgnored()
{
    ConnectionHarness harness;
    Authenticate(harness);
    CHECK(harness.gateway.retainedLink != nullptr);

    harness.connection->onClose();
    harness.connection->onClose();
    std::size_t const tracesBefore = harness.gateway.traced.size();
    std::size_t const sendsBefore = harness.sent.size();
    WorldPacket late(SMSG_PONG, 0);
    harness.gateway.retainedLink->SendPacket(late);

    CHECK(harness.gateway.detachCalls == 1);
    CHECK(harness.gateway.traced.size() == tracesBefore);
    CHECK(harness.sent.size() == sendsBefore);
}

void detachedRoutesDoNotReachMonotonicReplacements()
{
    FakeGateway gateway;
    gateway.monotonicRoutes = true;
    proto::AuthRequest request;

    proto::SessionId const oldId = gateway.Attach(request, nullptr, nullptr);
    CHECK(oldId != proto::INVALID_SESSION_ID);
    gateway.Detach(oldId);

    proto::SessionId const replacementId =
        gateway.Attach(request, nullptr, nullptr);
    CHECK(replacementId != proto::INVALID_SESSION_ID);
    CHECK(replacementId != oldId);

    WorldPacket delayed(CMSG_KEEP_ALIVE, 0);
    gateway.Deliver(oldId, std::move(delayed));
    CHECK(gateway.deliverCalls == 0);
    CHECK(gateway.delivered.empty());

    WorldPacket current(CMSG_KEEP_ALIVE, 0);
    gateway.Deliver(replacementId, std::move(current));
    CHECK(gateway.deliverCalls == 1);
    CHECK(gateway.delivered.size() == 1);
}

void callbackExceptionsDoNotEscapeTransportCallbacks()
{
    {
        ConnectionHarness harness;
        harness.gateway.throwOnTrace = true;
        bool escaped = false;
        try { harness.connection->onConnect(); }
        catch (...) { escaped = true; }
        CHECK(!escaped);
        CHECK(harness.connection->closed());
    }
    {
        ConnectionHarness harness;
        harness.gateway.throwOnLookup = true;
        harness.connection->onConnect();
        std::array<uint8, 20> const digest{};
        std::vector<uint8> const auth = AuthFrame(7, digest);
        bool escaped = false;
        try { harness.connection->onData(auth.data(), auth.size()); }
        catch (...) { escaped = true; }
        CHECK(!escaped);
        CHECK(harness.connection->closed());
    }
    {
        ConnectionHarness harness;
        BigNumber sessionKey = SuccessfulLookup(harness.gateway);
        uint32 const seed = 0x99887766;
        std::vector<uint8> const challenge = harness.connection->onConnect();
        std::array<uint8, 20> const proof =
            MakeProof("ACCOUNT", seed, ServerSeed(challenge), sessionKey);
        std::vector<uint8> const auth = AuthFrame(seed, proof);
        harness.gateway.throwOnAttach = true;
        bool escaped = false;
        try { harness.connection->onData(auth.data(), auth.size()); }
        catch (...) { escaped = true; }
        CHECK(!escaped);
        CHECK(harness.connection->closed());
    }
    {
        ConnectionHarness harness;
        BigNumber sessionKey = Authenticate(harness);
        ClassicHeaderCipher cipher(sessionKey);
        harness.gateway.throwOnDeliver = true;
        std::vector<uint8> packet = ClientFrame(CMSG_KEEP_ALIVE);
        cipher.EncryptClientHeader(packet);
        bool escaped = false;
        try { harness.connection->onData(packet.data(), packet.size()); }
        catch (...) { escaped = true; }
        CHECK(!escaped);
        CHECK(harness.connection->closed());
    }
    {
        ConnectionHarness harness;
        Authenticate(harness);
        harness.gateway.throwOnDetach = true;
        bool escaped = false;
        try { harness.connection->onClose(); }
        catch (...) { escaped = true; }
        CHECK(!escaped);
        CHECK(harness.gateway.detachCalls == 1);
    }
    {
        ConnectionHarness harness;
        harness.gateway.throwOnSender = true;
        WorldPacket packet(SMSG_PONG, 0);
        bool escaped = false;
        try { harness.connection->SendPacket(packet); }
        catch (...) { escaped = true; }
        CHECK(!escaped);
        CHECK(harness.connection->closed());
    }
    {
        ConnectionHarness harness;
        harness.gateway.throwOnCloser = true;
        bool escaped = false;
        try { harness.connection->Close(); }
        catch (...) { escaped = true; }
        CHECK(!escaped);
        CHECK(harness.connection->closed());
        CHECK(harness.closeCalls == 1);
    }
}

void coalescedAuthenticationActivatesCryptBeforeTheNextFrame()
{
    ConnectionHarness harness;
    BigNumber sessionKey = SuccessfulLookup(harness.gateway);
    uint32 const clientSeed = 0x10203040;
    std::vector<uint8> const challenge = harness.connection->onConnect();
    std::array<uint8, 20> const proof =
        MakeProof("ACCOUNT", clientSeed, ServerSeed(challenge), sessionKey);
    std::vector<uint8> input = AuthFrame(clientSeed, proof);

    ClassicHeaderCipher cipher(sessionKey);
    std::vector<uint8> keepAlive = ClientFrame(CMSG_KEEP_ALIVE);
    cipher.EncryptClientHeader(keepAlive);
    input.insert(input.end(), keepAlive.begin(), keepAlive.end());

    harness.connection->onData(input.data(), input.size());

    CHECK(harness.gateway.attachCalls == 1);
    CHECK(harness.gateway.deliverCalls == 1);
    CHECK(harness.gateway.delivered[0].GetOpcode() == CMSG_KEEP_ALIVE);
    CHECK(!harness.connection->closed());
}

void fragmentedEncryptedHeadersKeepCipherStateSynchronized()
{
    for (std::size_t split = 1; split < proto::CLIENT_HEADER_SIZE; ++split)
    {
        ConnectionHarness harness;
        BigNumber sessionKey = Authenticate(harness);
        ClassicHeaderCipher cipher(sessionKey);
        std::vector<uint8> keepAlive = ClientFrame(CMSG_KEEP_ALIVE, {0x42});
        cipher.EncryptClientHeader(keepAlive);

        harness.connection->onData(keepAlive.data(), split);
        CHECK(harness.gateway.deliverCalls == 0);
        CHECK(!harness.connection->closed());
        harness.connection->onData(
            keepAlive.data() + split, keepAlive.size() - split);

        CHECK(harness.gateway.deliverCalls == 1);
        CHECK(harness.gateway.delivered[0].GetOpcode() == CMSG_KEEP_ALIVE);
        CHECK(!harness.connection->closed());
    }
}

void concurrentSendsPreserveEncryptionAndSubmissionOrder()
{
    ConnectionHarness harness;
    BigNumber sessionKey = Authenticate(harness);

    std::mutex lock;
    std::condition_variable changed;
    bool firstEntered = false;
    bool releaseFirst = false;
    std::atomic<bool> secondStarted{false};
    std::atomic<bool> secondReturned{false};
    unsigned senderCalls = 0;
    std::vector<std::vector<uint8>> frames;
    harness.connection->setSender(
        [&](uint8 const* data, std::size_t len)
        {
            std::unique_lock<std::mutex> guard(lock);
            ++senderCalls;
            frames.emplace_back(data, data + len);
            if (senderCalls == 1)
            {
                firstEntered = true;
                changed.notify_all();
                changed.wait(guard, [&]() { return releaseFirst; });
            }
        });

    WorldPacket first(SMSG_PONG, 1);
    first << uint8(1);
    WorldPacket second(SMSG_NOTIFICATION, 1);
    second << uint8(2);
    std::thread firstThread([&]() { harness.connection->SendPacket(first); });
    {
        std::unique_lock<std::mutex> guard(lock);
        changed.wait(guard, [&]() { return firstEntered; });
    }
    std::thread secondThread([&]()
    {
        secondStarted.store(true);
        harness.connection->SendPacket(second);
        secondReturned.store(true);
    });
    while (!secondStarted.load())
        std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    {
        std::lock_guard<std::mutex> guard(lock);
        CHECK(senderCalls == 1);
        releaseFirst = true;
    }
    changed.notify_all();
    firstThread.join();
    secondThread.join();

    CHECK(secondReturned.load());
    CHECK(frames.size() == 2);
    ClassicHeaderCipher cipher(sessionKey);
    cipher.DecryptServerHeader(frames[0]);
    cipher.DecryptServerHeader(frames[1]);
    CHECK(ServerOpcode(frames[0]) == SMSG_PONG);
    CHECK(ServerOpcode(frames[1]) == SMSG_NOTIFICATION);
}
}

int main()
{
    fragmentedFrameDecodesOnce();
    combinedFramesPreserveOrder();
    splitHeadersDecryptExactlyOnce();
    malformedFramesAreRejected();
    maximumAcceptedSizeDecodes();
    serverFramesUseTheFixedClassicHeader();
    connectionChallengeHasTheExpectedClassicShape();
    preAuthenticationWorldPacketsAreRejected();
    authenticationFilterVetoAllowsALaterAcceptedAttempt();
    lookupRejectionSendsStatusAndCloses();
    invalidProofSendsFailureAndNeverAttaches();
    successfulAuthenticationInitializesClassicCryptBeforeAttach();
    authenticationAddonTailStartsAtPositionZero();
    attachFailureSendsEncryptedSystemErrorWithoutPublishingASession();
    authenticatedPacketsStayOpaqueToTheConnection();
    connectionRejectsOutOfRangeOpcodes();
    repeatedAuthenticationClosesWithoutSecondLookup();
    closeDuringAttachDetachesThePublishedSession();
    closeDetachesOnceAndLateSendsAreIgnored();
    detachedRoutesDoNotReachMonotonicReplacements();
    callbackExceptionsDoNotEscapeTransportCallbacks();
    coalescedAuthenticationActivatesCryptBeforeTheNextFrame();
    fragmentedEncryptedHeadersKeepCipherStateSynchronized();
    concurrentSendsPreserveEncryptionAndSubmissionOrder();
    return mangos::test::failures == 0 ? 0 : 1;
}
