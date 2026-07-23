#include "TestSupport.hpp"

#include "IWorldGateway.h"
#include "Opcodes.h"
#include "PacketCodec.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
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
}

int main()
{
    fragmentedFrameDecodesOnce();
    combinedFramesPreserveOrder();
    splitHeadersDecryptExactlyOnce();
    malformedFramesAreRejected();
    maximumAcceptedSizeDecodes();
    serverFramesUseTheFixedClassicHeader();
    return mangos::test::failures == 0 ? 0 : 1;
}
