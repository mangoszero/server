/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "TestHarness.h"

#include "PacketCodec.h"

#include <vector>

/**
 * @file
 * @brief Framing tests for the 3.3.5a client protocol.
 *
 * TCP is a stream, so the reassembly path has to survive arbitrary
 * fragmentation. That is exactly the property that is impossible to test through
 * a socket -- you cannot make the kernel split a packet where you want it -- and
 * it is why the codec was pulled out of the socket class in the first place.
 * Feeding it one byte at a time here is worth more than any amount of
 * loopback traffic, which always arrives conveniently whole.
 */

namespace
{
    /// Build one client->server frame: 6-byte header then payload.
    std::vector<uint8> Frame(uint16 opcode, const std::vector<uint8>& payload)
    {
        const uint32 size = uint32(payload.size()) + 4;   // the size field counts the opcode

        std::vector<uint8> out;
        out.push_back(uint8((size >> 8) & 0xFF));         // size is big-endian
        out.push_back(uint8(size & 0xFF));
        out.push_back(uint8(opcode & 0xFF));              // opcode is little-endian
        out.push_back(uint8((opcode >> 8) & 0xFF));
        out.push_back(0);
        out.push_back(0);
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }
}

TEST(PacketCodec_decodes_one_whole_packet)
{
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> payload = { 0xDE, 0xAD, 0xBE, 0xEF };
    const std::vector<uint8> wire = Frame(0x1234, payload);

    CHECK(codec.Feed(wire.data(), wire.size(), out) == proto::DecodeStatus::Ok);
    REQUIRE(out.size() == 1);
    CHECK_EQ(int(out[0].GetOpcode()), 0x1234);
    CHECK_EQ(int(out[0].size()), 4);
}

TEST(PacketCodec_survives_byte_at_a_time_delivery)
{
    // The case a loopback test can never produce: every possible split point,
    // including one that lands inside the header.
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> payload = { 1, 2, 3, 4, 5, 6, 7, 8 };
    const std::vector<uint8> wire = Frame(0x00AA, payload);

    for (size_t i = 0; i < wire.size(); ++i)
    {
        CHECK(codec.Feed(&wire[i], 1, out) == proto::DecodeStatus::Ok);

        // Nothing may be emitted until the very last byte arrives.
        if (i + 1 < wire.size())
        {
            CHECK_EQ(int(out.size()), 0);
        }
    }

    REQUIRE(out.size() == 1);
    CHECK_EQ(int(out[0].GetOpcode()), 0x00AA);
    CHECK_EQ(int(out[0].size()), 8);
}

TEST(PacketCodec_splits_coalesced_packets)
{
    // Nagle and the receive buffer routinely hand over several packets at once.
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    std::vector<uint8> wire = Frame(0x0001, { 0x11 });
    const std::vector<uint8> second = Frame(0x0002, { 0x22, 0x33 });
    const std::vector<uint8> third  = Frame(0x0003, {});
    wire.insert(wire.end(), second.begin(), second.end());
    wire.insert(wire.end(), third.begin(), third.end());

    CHECK(codec.Feed(wire.data(), wire.size(), out) == proto::DecodeStatus::Ok);
    REQUIRE(out.size() == 3);
    CHECK_EQ(int(out[0].GetOpcode()), 1);
    CHECK_EQ(int(out[1].GetOpcode()), 2);
    CHECK_EQ(int(out[2].GetOpcode()), 3);
    CHECK_EQ(int(out[2].size()), 0);
}

TEST(PacketCodec_handles_an_empty_payload)
{
    // A bare opcode. This is the shape that made ByteBuffer::contents() index
    // element zero of an empty vector.
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> wire = Frame(0x0100, {});

    CHECK(codec.Feed(wire.data(), wire.size(), out) == proto::DecodeStatus::Ok);
    REQUIRE(out.size() == 1);
    CHECK_EQ(int(out[0].size()), 0);
}

TEST(PacketCodec_rejects_an_undersized_size_field)
{
    // The size field counts the 4 opcode bytes, so anything below 4 is
    // impossible -- and would underflow the payload length if it were trusted.
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    const uint8 wire[6] = { 0x00, 0x03, 0x01, 0x00, 0x00, 0x00 };

    CHECK(codec.Feed(wire, sizeof(wire), out) == proto::DecodeStatus::Malformed);
}

TEST(PacketCodec_rejects_an_oversized_packet)
{
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    // 0xFFFF is far past MAX_CLIENT_PACKET_SIZE.
    const uint8 wire[6] = { 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x00 };

    CHECK(codec.Feed(wire, sizeof(wire), out) == proto::DecodeStatus::Malformed);
}

TEST(PacketCodec_header_decryptor_runs_once_per_header)
{
    // The header cipher is a stream cipher: decrypting a header in two pieces as
    // its bytes trickle in would desynchronise the keystream for the rest of the
    // connection. The codec must therefore call the hook exactly once per
    // header, after all six bytes are in hand -- so this counts the calls while
    // feeding one byte at a time.
    int calls = 0;
    proto::PacketCodec codec([&calls](uint8*, size_t len)
    {
        ++calls;
        CHECK_EQ(int(len), int(proto::CLIENT_HEADER_SIZE));
    });

    std::vector<WorldPacket> out;
    const std::vector<uint8> wire = Frame(0x0055, { 9, 9, 9 });

    for (size_t i = 0; i < wire.size(); ++i)
    {
        codec.Feed(&wire[i], 1, out);
    }

    CHECK_EQ(calls, 1);
    CHECK_EQ(int(out.size()), 1);
}

TEST(PacketCodec_encode_round_trips_through_decode)
{
    WorldPacket packet(0x02AB, 3);
    packet << uint8(7);
    packet << uint8(8);
    packet << uint8(9);

    const std::vector<uint8> wire =
        proto::PacketCodec::Encode(packet, proto::PacketCodec::HeaderEncryptor());

    // Encode writes the server->client header (4 or 5 bytes), which is a
    // different shape from the client->server header Feed() parses, so this
    // checks the wire form directly rather than round-tripping through Feed.
    const uint32 size = 3 + 2;                       // payload + opcode
    CHECK_EQ(int(wire.size()), 4 + 3);
    CHECK_EQ(int(wire[0]), int((size >> 8) & 0xFF));
    CHECK_EQ(int(wire[1]), int(size & 0xFF));
    CHECK_EQ(int(wire[2]), 0xAB);
    CHECK_EQ(int(wire[3]), 0x02);
    CHECK_EQ(int(wire[4]), 7);
    CHECK_EQ(int(wire[6]), 9);
}

// THE SERVER HEADER IS EXPANSION-SPECIFIC, so this asserts two different things
// rather than one bent to fit both. The three-byte size arrives in WotLK; before it,
// a client reads a fixed four-byte header and a five-byte one desynchronises the
// stream for good. Asserting the WotLK form everywhere is how a wire-format
// regression gets a green test to hide behind.
TEST(PacketCodec_encode_sizes_the_header_the_way_this_expansion_does)
{
    WorldPacket packet(0x0001, 0x8000);
    for (int i = 0; i < 0x8000; ++i)
    {
        packet << uint8(0);
    }

    const std::vector<uint8> wire =
        proto::PacketCodec::Encode(packet, proto::PacketCodec::HeaderEncryptor());

    // THE FIRST BYTE CANNOT TELL THE TWO FORMS APART, which is the whole trap. The
    // size here is 0x8002, so a four-byte header starts 0x80 0x02 -- that 0x80 is the
    // high byte of the SIZE, not the large-packet marker -- and the five-byte form
    // starts 0x80 0x80 0x02, where it is. Only the LENGTH distinguishes them, so that
    // is what gets asserted.
#if defined(CLASSIC) || defined(TBC)
    CHECK_EQ(int(wire.size()), 4 + 0x8000);
    CHECK_EQ(int(wire[0]), 0x80);
    CHECK_EQ(int(wire[1]), 0x02);
#else
    CHECK_EQ(int(wire.size()), 5 + 0x8000);
    CHECK_EQ(int(wire[0]), 0x80);
    CHECK_EQ(int(wire[1]), 0x80);
    CHECK_EQ(int(wire[2]), 0x02);
#endif
}
