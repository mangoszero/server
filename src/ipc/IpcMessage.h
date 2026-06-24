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
 */

#ifndef AH_IPC_MESSAGE_H
#define AH_IPC_MESSAGE_H

#include "Common.h"
#include "Utilities/ByteBuffer.h"
#include "IpcOpcodes.h"
#include "IpcVersion.h"
#include <string>

/**
 * Maximum allowed body size for a single IPC frame (1 MiB).
 * Frames advertising a larger body are rejected with "oversize frame".
 */
static constexpr uint32 IPC_MAX_FRAME = 1u << 20;

/**
 * @brief Per-opcode body-size constraint for inbound frame validation.
 *
 * Result of IpcExpectedBodySize(): describes the accepted body length for a
 * given opcode. The server side (which must NOT trust the child) rejects any
 * frame whose body length violates this constraint, and rejects unknown
 * opcodes outright. With this in force every accepted frame is tiny, so a
 * hostile child cannot queue oversize-for-op or payload-flood frames.
 */
struct IpcBodySizeRule
{
    bool   known;  ///< false => unknown opcode; reject the frame.
    bool   exact;  ///< true => body length must equal @ref maxLen exactly.
    uint32 maxLen; ///< Exact length (when @ref exact) or inclusive max length.
};

/**
 * @brief Authoritative expected-body-size rule for an inbound opcode.
 *
 * Single source of truth used by the inbound decode path to bound the body
 * length of every accepted frame. Fixed-layout opcodes (intents, results,
 * GM commands, and the tiny protocol frames) get an EXACT size; the debug
 * ECHO opcodes get a small inclusive maximum; everything else is reported
 * unknown so the caller can reject it.
 *
 * @param op Opcode taken from a decoded frame header.
 * @return   The size rule for @p op (see @ref IpcBodySizeRule).
 */
IpcBodySizeRule IpcExpectedBodySize(uint16 op);

/**
 * @brief A framed IPC message: an opcode plus a variable-length body.
 *
 * Wire layout (little-endian):
 *   uint16  version   == IPC_PROTOCOL_VERSION
 *   uint16  opcode
 *   uint32  body_len
 *   uint8[] body      (body_len bytes)
 */
class IpcMessage
{
    public:
        IpcOpcode  op;    ///< Message opcode
        ByteBuffer body;  ///< Message payload (may be empty)

        /// Per-connection run-id stamped LOCALLY on the mangosd side when an
        /// inbound frame is received (IpcServerHandler stamps its own
        /// m_runId). This is NOT part of the wire frame and is NEVER touched by
        /// Encode()/Decode(); it exists solely so the supervisor can drop a
        /// frame produced by a PRIOR child connection that slipped into the
        /// inbound queue after a purge (a stale-run frame). Default 0 = unset.
        uint32     generation;

        IpcMessage() : op(IpcOpcode(0)), body(), generation(0) {}

        /**
         * @brief Encode this message into the wire buffer @p w.
         *
         * Appends the 8-byte header followed by body bytes to @p w.
         * The caller is responsible for sending the contents of @p w
         * over the transport.
         *
         * @param w Destination ByteBuffer (data is appended).
         */
        void Encode(ByteBuffer& w) const;

        /**
         * @brief Attempt to decode one frame from the wire buffer @p w.
         *
         * On success, advances @p w's read position past the consumed frame
         * and populates @p out. On failure, restores @p w's read position
         * and sets @p err to a human-readable reason.
         *
         * Failure reasons:
         *   "short header"     - fewer than 8 bytes available
         *   "version mismatch" - header version != IPC_PROTOCOL_VERSION
         *   "oversize frame"   - body_len > IPC_MAX_FRAME
         *   "incomplete"       - header parsed but body not yet fully buffered
         *
         * @param w   Source ByteBuffer (read position advanced on success).
         * @param out Populated with the decoded message on success.
         * @param err Set to an error description on failure.
         * @return    true on success, false on failure.
         */
        static bool Decode(ByteBuffer& w, IpcMessage& out, std::string& err);
};

#endif // AH_IPC_MESSAGE_H
