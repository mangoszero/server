/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 1.12.xa and 5.4.8
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

#ifndef MANGOS_PROTO_PACKETCODEC_H
#define MANGOS_PROTO_PACKETCODEC_H

#include <utility>
#include "Platform/Define.h"
#include "WorldPacket.h"

#include <cstddef>
#include <functional>
#include <vector>

namespace proto
{
    /// Fixed size of the client -> server header: uint16 size + uint32 opcode.
    /// Source of truth: WorldSocket.cpp handle_input_header() (ClientPktHeader).
    static const size_t CLIENT_HEADER_SIZE = 6;

    /// Largest packet the 1.12.x client is ever allowed to send, payload
    /// included. Source: WorldSocket.cpp:654 (`header.size > 10240`).
    static const uint32 MAX_CLIENT_PACKET_SIZE = 10240;

    /**
     * @brief Result of handing a run of received bytes to the codec.
     */
    enum class DecodeStatus
    {
        Ok,         ///< Bytes consumed; zero or more complete packets produced.
        Malformed   ///< Protocol violation. The caller must drop the connection.
    };

    /**
     * @brief Turns the raw TCP byte stream into whole WorldPackets, and whole
     *        WorldPackets back into wire bytes.
     *
     * This is the entire 1.12.x framing layer and nothing else: it owns no socket,
     * knows no world, and performs no I/O, so it can be driven byte-by-byte from a
     * unit test. Header confidentiality is the caller's business, injected as the
     * two hooks below -- the codec only says when a header is in front of it.
     *
     * TCP is a stream: a single feed() may carry half a header, nine packets, or
     * one packet split across five calls. The reassembly state lives here, in one
     * place, rather than in flags spread across a socket class.
     */
    class PacketCodec
    {
        public:

            /// Decrypts CLIENT_HEADER_SIZE header bytes in place. Called exactly
            /// once per header, before the size/opcode are read out of it.
            typedef std::function<void(uint8* header, size_t len)> HeaderDecryptor;

            /// Encrypts an outgoing header in place, whose length varies (4 or 5).
            typedef std::function<void(uint8* header, size_t len)> HeaderEncryptor;

            /**
             * @param decryptor Header decryption hook. May be empty, in which case
             *                  headers are read as plain text -- which is the state
             *                  of the connection until the session key is known.
             */
            explicit PacketCodec(HeaderDecryptor decryptor = HeaderDecryptor());

            /**
             * @brief Feed received bytes; append every packet completed by them.
             *
             * @param data Received bytes. Need only stay valid for the call.
             * @param len  Number of bytes at @p data.
             * @param out  Completed packets are appended here, in arrival order.
             * @return DecodeStatus::Malformed if the peer violated the framing, in
             *         which case @p out still holds the packets decoded before the
             *         bad one and the connection must be closed.
             */
            DecodeStatus Feed(const uint8* data, size_t len,
                              std::vector<WorldPacket>& out);

            /**
             * @brief Serialise a packet for the wire: header followed by payload.
             *
             * The size field counts the opcode, and packets over 0x7FFF carry a
             * three-byte size with the top bit of the first byte set -- which is why
             * the outgoing header is 4 or 5 bytes and not a fixed struct. Mirrors
             * WorldSocket.cpp's ServerPktHeader verbatim.
             *
             * @param packet    Packet to serialise.
             * @param encryptor Header encryption hook; may be empty before the
             *                  session key is known.
             * @return The complete wire representation.
             */
            static std::vector<uint8> Encode(const WorldPacket& packet,
                                             const HeaderEncryptor& encryptor);

            /// Install the header decryptor, once the session key has been agreed.
            void SetHeaderDecryptor(HeaderDecryptor decryptor)
            {
                m_decryptor = std::move(decryptor);
            }

        private:

            HeaderDecryptor m_decryptor;

            uint8  m_header[CLIENT_HEADER_SIZE]; ///< partially received header
            size_t m_headerFill;                 ///< bytes of m_header filled so far

            bool   m_haveHeader;    ///< header complete and already decrypted
            uint16 m_opcode;        ///< opcode of the packet being received
            uint32 m_payloadNeeded; ///< payload bytes still outstanding

            std::vector<uint8> m_payload; ///< payload accumulated so far
    };
}

#endif
