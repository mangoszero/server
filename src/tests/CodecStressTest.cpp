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

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

/**
 * @file
 * @brief Stress coverage for the framing layer, happy path and hostile path.
 *
 * The codec sits directly on the socket, so its input is whatever an
 * unauthenticated peer decides to send. Two obligations follow, and they are
 * tested separately:
 *
 *   Happy path -- every well-formed stream must decode to exactly the packets
 *   that were encoded, whatever the fragmentation. TCP gives no guarantee about
 *   where a read boundary falls, so the tests below deliberately put boundaries
 *   everywhere, including inside the six-byte header.
 *
 *   Bad path -- every malformed stream must be rejected, and rejection must be
 *   the *worst* outcome. No crash, no hang, no allocation driven by an attacker's
 *   size field, no read past the end of the buffer. The size field is 16 bits and
 *   counts the opcode, so it is trivially possible to send a length the decoder
 *   would underflow on; that case gets its own test.
 */

namespace
{
    std::vector<uint8> Frame(uint16 opcode, const std::vector<uint8>& payload)
    {
        const uint32 size = uint32(payload.size()) + 4;

        std::vector<uint8> out;
        out.push_back(uint8((size >> 8) & 0xFF));
        out.push_back(uint8(size & 0xFF));
        out.push_back(uint8(opcode & 0xFF));
        out.push_back(uint8((opcode >> 8) & 0xFF));
        out.push_back(0);
        out.push_back(0);
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

    std::vector<uint8> RawHeader(uint16 size, uint32 cmd)
    {
        std::vector<uint8> out;
        out.push_back(uint8((size >> 8) & 0xFF));
        out.push_back(uint8(size & 0xFF));
        out.push_back(uint8(cmd & 0xFF));
        out.push_back(uint8((cmd >> 8) & 0xFF));
        out.push_back(uint8((cmd >> 16) & 0xFF));
        out.push_back(uint8((cmd >> 24) & 0xFF));
        return out;
    }
}

// ---------------------------------------------------------------------------
// Happy path
// ---------------------------------------------------------------------------

TEST(CodecStress_thousands_of_packets_random_fragmentation)
{
    // Five thousand packets of random size and opcode, concatenated into one
    // stream and then delivered in random chunks. Every packet must come back
    // with its opcode and payload intact and in order.
    const int COUNT = 5000;

    std::mt19937 rng(0xC0DEu);
    std::uniform_int_distribution<int>    byteDist(0, 255);
    std::uniform_int_distribution<uint32> sizeDist(0, 600);
    std::uniform_int_distribution<uint32> opDist(0, 0x3FF);
    std::uniform_int_distribution<size_t> chunkDist(1, 1500);

    std::vector<uint16>             opcodes;
    std::vector<std::vector<uint8>> payloads;
    std::vector<uint8>              stream;

    for (int i = 0; i < COUNT; ++i)
    {
        const uint16 opcode = uint16(opDist(rng));
        std::vector<uint8> payload(sizeDist(rng));
        for (uint8& b : payload)
        {
            b = uint8(byteDist(rng));
        }

        opcodes.push_back(opcode);
        payloads.push_back(payload);

        const std::vector<uint8> framed = Frame(opcode, payload);
        stream.insert(stream.end(), framed.begin(), framed.end());
    }

    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    size_t offset = 0;
    bool   rejected = false;
    while (offset < stream.size())
    {
        const size_t chunk = std::min(chunkDist(rng), stream.size() - offset);
        if (codec.Feed(&stream[offset], chunk, out) == proto::DecodeStatus::Malformed)
        {
            rejected = true;
            break;
        }
        offset += chunk;
    }

    CHECK(!rejected);
    CHECK_EQ(int(out.size()), COUNT);

    int mismatches = 0;
    for (int i = 0; i < COUNT && i < int(out.size()); ++i)
    {
        if (uint16(out[i].GetOpcode()) != opcodes[i]) { ++mismatches; continue; }
        if (out[i].size() != payloads[i].size())      { ++mismatches; continue; }

        if (!payloads[i].empty() &&
            std::memcmp(out[i].contents(), payloads[i].data(), payloads[i].size()) != 0)
        {
            ++mismatches;
        }
    }

    CHECK_EQ(mismatches, 0);
}

TEST(CodecStress_every_possible_split_point)
{
    // Exhaustive rather than random: for a three-packet stream, cut it at every
    // single offset and feed the two halves. One of those cuts lands inside each
    // header, which is the case that has to hold the partial header across calls.
    std::vector<uint8> stream;
    for (const auto& f : { Frame(0x0101, { 1, 2, 3 }),
                           Frame(0x0202, {}),
                           Frame(0x0303, { 9, 8, 7, 6, 5 }) })
    {
        stream.insert(stream.end(), f.begin(), f.end());
    }

    int failures = 0;

    for (size_t cut = 0; cut <= stream.size(); ++cut)
    {
        proto::PacketCodec codec;
        std::vector<WorldPacket> out;

        bool bad = false;
        if (cut > 0)
        {
            bad |= codec.Feed(stream.data(), cut, out) == proto::DecodeStatus::Malformed;
        }
        if (cut < stream.size())
        {
            bad |= codec.Feed(&stream[cut], stream.size() - cut, out)
                   == proto::DecodeStatus::Malformed;
        }

        if (bad || out.size() != 3)
        {
            ++failures;
            continue;
        }
        if (out[0].GetOpcode() != 0x0101 || out[1].GetOpcode() != 0x0202 ||
            out[2].GetOpcode() != 0x0303)
        {
            ++failures;
        }
        if (out[1].size() != 0 || out[2].size() != 5)
        {
            ++failures;
        }
    }

    CHECK_EQ(failures, 0);
}

TEST(CodecStress_boundary_payload_sizes)
{
    // Zero, one, and the largest payload the size field allows once the four
    // opcode bytes are accounted for.
    const uint32 maxPayload = proto::MAX_CLIENT_PACKET_SIZE - 4;

    for (uint32 size : { 0u, 1u, 2u, maxPayload - 1, maxPayload })
    {
        proto::PacketCodec codec;
        std::vector<WorldPacket> out;

        std::vector<uint8> payload(size, 0xAB);
        const std::vector<uint8> framed = Frame(0x00FF, payload);

        const bool ok =
            codec.Feed(framed.data(), framed.size(), out) == proto::DecodeStatus::Ok;

        CHECK(ok);
        CHECK_EQ(int(out.size()), 1);
        if (!out.empty())
        {
            CHECK_EQ(int(out[0].size()), int(size));
        }
    }
}

TEST(CodecStress_every_undersized_size_field_is_rejected)
{
    // The size field counts the four opcode bytes, so 0..3 are impossible. If
    // any were accepted, `size - 4` would underflow into a payload length near
    // 4 billion -- an allocation an unauthenticated peer gets to ask for.
    for (uint16 size = 0; size < 4; ++size)
    {
        proto::PacketCodec codec;
        std::vector<WorldPacket> out;

        const std::vector<uint8> header = RawHeader(size, 0x0001);

        CHECK(codec.Feed(header.data(), header.size(), out)
              == proto::DecodeStatus::Malformed);
        CHECK_EQ(int(out.size()), 0);
    }
}

TEST(CodecStress_oversized_size_field_is_rejected)
{
    // One byte past the limit must be refused, and the limit itself accepted --
    // an off-by-one in either direction is a bug.
    {
        proto::PacketCodec codec;
        std::vector<WorldPacket> out;
        const std::vector<uint8> header =
            RawHeader(uint16(proto::MAX_CLIENT_PACKET_SIZE + 1), 0x0001);

        CHECK(codec.Feed(header.data(), header.size(), out)
              == proto::DecodeStatus::Malformed);
    }
    {
        proto::PacketCodec codec;
        std::vector<WorldPacket> out;
        const std::vector<uint8> header =
            RawHeader(uint16(proto::MAX_CLIENT_PACKET_SIZE), 0x0001);

        // Accepted as a header; the payload simply never arrives here.
        CHECK(codec.Feed(header.data(), header.size(), out)
              == proto::DecodeStatus::Ok);
        CHECK_EQ(int(out.size()), 0);
    }
}

TEST(CodecStress_absurd_opcode_is_rejected)
{
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    const std::vector<uint8> header = RawHeader(8, 0xFFFFFFFFu);

    CHECK(codec.Feed(header.data(), header.size(), out)
          == proto::DecodeStatus::Malformed);
}

TEST(CodecStress_truncated_stream_emits_nothing)
{
    // A peer that announces a large payload and then stops. The decoder must
    // wait forever without emitting a short packet and without reading past what
    // it was given -- a partial packet delivered upward would be parsed as if it
    // were complete.
    std::vector<uint8> payload(4000, 0x5A);
    std::vector<uint8> framed = Frame(0x0042, payload);

    framed.resize(framed.size() / 2);       // cut it in half

    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    CHECK(codec.Feed(framed.data(), framed.size(), out) == proto::DecodeStatus::Ok);
    CHECK_EQ(int(out.size()), 0);
}

TEST(CodecStress_good_packets_before_a_bad_one_are_kept)
{
    // The contract says the packets decoded before the malformed one are still
    // in the output. The caller drops the connection, but what already arrived
    // was legitimate and must not be silently discarded.
    std::vector<uint8> stream;

    for (const auto& f : { Frame(0x0011, { 1 }), Frame(0x0022, { 2, 2 }) })
    {
        stream.insert(stream.end(), f.begin(), f.end());
    }
    const std::vector<uint8> bad = RawHeader(1, 0x0001);   // size < 4
    stream.insert(stream.end(), bad.begin(), bad.end());

    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    CHECK(codec.Feed(stream.data(), stream.size(), out)
          == proto::DecodeStatus::Malformed);
    CHECK_EQ(int(out.size()), 2);
    if (out.size() == 2)
    {
        CHECK_EQ(int(out[0].GetOpcode()), 0x0011);
        CHECK_EQ(int(out[1].GetOpcode()), 0x0022);
    }
}

TEST(CodecStress_null_and_zero_length_feeds_are_harmless)
{
    proto::PacketCodec codec;
    std::vector<WorldPacket> out;

    CHECK(codec.Feed(nullptr, 0, out) == proto::DecodeStatus::Ok);
    CHECK(codec.Feed(nullptr, 100, out) == proto::DecodeStatus::Ok);

    const uint8 byte = 0;
    CHECK(codec.Feed(&byte, 0, out) == proto::DecodeStatus::Ok);
    CHECK_EQ(int(out.size()), 0);
}

TEST(CodecStress_random_garbage_never_faults)
{
    // Pure fuzz. The only assertions are the invariants: the status is one of
    // the two defined values, and nothing emitted ever exceeds the declared
    // maximum. Everything else being tested here is the absence of a crash.
    std::mt19937 rng(0xBAADu);
    std::uniform_int_distribution<int>    byteDist(0, 255);
    std::uniform_int_distribution<size_t> chunkDist(1, 97);

    for (int iteration = 0; iteration < 5000; ++iteration)
    {
        proto::PacketCodec codec;
        std::vector<WorldPacket> out;

        std::vector<uint8> noise(256);
        for (uint8& b : noise)
        {
            b = uint8(byteDist(rng));
        }

        size_t offset = 0;
        while (offset < noise.size())
        {
            const size_t chunk = std::min(chunkDist(rng), noise.size() - offset);
            const proto::DecodeStatus status = codec.Feed(&noise[offset], chunk, out);

            if (status == proto::DecodeStatus::Malformed)
            {
                break;
            }
            offset += chunk;
        }

        for (const WorldPacket& p : out)
        {
            CHECK(p.size() <= proto::MAX_CLIENT_PACKET_SIZE);
        }
    }
}

TEST(CodecStress_structured_fuzz_around_valid_frames)
{
    // Harder than pure noise: mostly-valid frames with one field corrupted, so
    // the decoder is pushed down its real code paths rather than being rejected
    // on the first header. This is what a buggy or hostile client produces.
    std::mt19937 rng(0x5EED2u);
    std::uniform_int_distribution<int> pick(0, 3);
    std::uniform_int_distribution<int> byteDist(0, 255);

    for (int iteration = 0; iteration < 5000; ++iteration)
    {
        std::vector<uint8> payload(iteration % 64, 0x7E);
        std::vector<uint8> framed = Frame(uint16(iteration % 512), payload);

        // Corrupt exactly one byte of the header.
        const size_t at = size_t(pick(rng));
        framed[at] = uint8(byteDist(rng));

        proto::PacketCodec codec;
        std::vector<WorldPacket> out;

        const proto::DecodeStatus status =
            codec.Feed(framed.data(), framed.size(), out);

        CHECK(status == proto::DecodeStatus::Ok ||
              status == proto::DecodeStatus::Malformed);

        for (const WorldPacket& p : out)
        {
            CHECK(p.size() <= proto::MAX_CLIENT_PACKET_SIZE);
        }
    }
}
