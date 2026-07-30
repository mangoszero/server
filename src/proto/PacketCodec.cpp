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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include <utility>
#include <vector>
#include "PacketCodec.h"

#include <algorithm>
#include <cstring>

namespace proto
{
    PacketCodec::PacketCodec(HeaderDecryptor decryptor)
        : m_decryptor(std::move(decryptor)),
          m_headerFill(0),
          m_haveHeader(false),
          m_opcode(0),
          m_payloadNeeded(0)
    {
        std::memset(m_header, 0, sizeof(m_header));
    }

    DecodeStatus PacketCodec::Feed(const uint8* data, size_t len,
                                   std::vector<WorldPacket>& out)
    {
        if (data == NULL || len == 0)
        {
            return DecodeStatus::Ok;
        }

        size_t offset = 0;

        while (offset < len)
        {
            // ---- Phase 1: collect and decode the fixed-size header -------------
            if (!m_haveHeader)
            {
                const size_t want = CLIENT_HEADER_SIZE - m_headerFill;
                const size_t take = std::min(want, len - offset);

                std::memcpy(m_header + m_headerFill, data + offset, take);
                m_headerFill += take;
                offset       += take;

                if (m_headerFill < CLIENT_HEADER_SIZE)
                {
                    return DecodeStatus::Ok; // header still incomplete
                }

                // Decrypt exactly once, now that all six bytes are in hand. Doing
                // it per-fragment would corrupt the stream cipher's keystream.
                if (m_decryptor)
                {
                    m_decryptor(m_header, CLIENT_HEADER_SIZE);
                }

                // Read the fields out byte by byte rather than casting the buffer
                // to a packed struct: the size is big-endian and the opcode little-
                // endian, so a struct needs a byte-swap dance anyway, and the cast
                // itself is an aliasing violation on a char buffer.
                const uint32 size = (uint32(m_header[0]) << 8) | uint32(m_header[1]);
                const uint32 cmd  =  uint32(m_header[2])
                                  | (uint32(m_header[3]) << 8)
                                  | (uint32(m_header[4]) << 16)
                                  | (uint32(m_header[5]) << 24);

                // `size` counts the four opcode bytes, so anything below that is
                // impossible and would underflow the payload length below.
                if (size < 4 || size > MAX_CLIENT_PACKET_SIZE
                    || cmd > MAX_CLIENT_PACKET_SIZE)
                {
                    return DecodeStatus::Malformed;
                }

                m_opcode        = uint16(cmd);
                m_payloadNeeded = size - 4;
                m_haveHeader    = true;

                m_payload.clear();
                m_payload.reserve(m_payloadNeeded);
            }

            // ---- Phase 2: collect the payload ---------------------------------
            if (m_payloadNeeded > 0)
            {
                const size_t take = std::min(size_t(m_payloadNeeded), len - offset);
                if (take == 0)
                {
                    return DecodeStatus::Ok; // need more bytes
                }

                m_payload.insert(m_payload.end(), data + offset, data + offset + take);
                offset          += take;
                m_payloadNeeded -= uint32(take);

                if (m_payloadNeeded > 0)
                {
                    return DecodeStatus::Ok; // payload still incomplete
                }
            }

            // ---- Phase 3: emit and reset for the next packet ------------------
            WorldPacket packet(m_opcode, m_payload.size());
            if (!m_payload.empty())
            {
                packet.append(m_payload.data(), m_payload.size());
            }
            out.push_back(std::move(packet));

            m_haveHeader = false;
            m_headerFill = 0;
            m_payload.clear();
        }

        return DecodeStatus::Ok;
    }

    std::vector<uint8> PacketCodec::Encode(const WorldPacket& packet,
                                           const HeaderEncryptor& encryptor)
    {
        // The size field counts the two opcode bytes along with the payload.
        const uint32 size = uint32(packet.size()) + 2;

        // THE SERVER HEADER IS EXPANSION-SPECIFIC. The three-byte size, marked by
        // 0x80 in the first byte, arrives in WotLK. A 1.12 or 2.4.3 client reads a
        // fixed four-byte header, so meeting a five-byte one desynchronises the
        // stream permanently -- it is not a packet it can skip.
#if defined(CLASSIC) || defined(TBC)
        const bool large = false;
        (void)large;
#else
        const bool large = size > 0x7FFF;
#endif

        uint8  header[5];
        size_t headerLen = 0;

        if (large)
        {
            header[headerLen++] = uint8(0x80 | ((size >> 16) & 0xFF));
        }
        header[headerLen++] = uint8((size >> 8) & 0xFF);
        header[headerLen++] = uint8(size & 0xFF);

        const uint16 opcode = uint16(packet.GetOpcode());
        header[headerLen++] = uint8(opcode & 0xFF);
        header[headerLen++] = uint8((opcode >> 8) & 0xFF);

        if (encryptor)
        {
            encryptor(header, headerLen);
        }

        std::vector<uint8> wire;
        wire.reserve(headerLen + packet.size());
        wire.insert(wire.end(), header, header + headerLen);

        // contents() is only safe on a non-empty buffer; many packets are pure
        // opcodes with no payload at all.
        if (!packet.empty())
        {
            wire.insert(wire.end(), packet.contents(),
                        packet.contents() + packet.size());
        }

        return wire;
    }
}
