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

        IpcMessage() : op(IpcOpcode(0)), body() {}

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
         *   "short header"     — fewer than 8 bytes available
         *   "version mismatch" — header version != IPC_PROTOCOL_VERSION
         *   "oversize frame"   — body_len > IPC_MAX_FRAME
         *   "incomplete"       — header parsed but body not yet fully buffered
         *
         * @param w   Source ByteBuffer (read position advanced on success).
         * @param out Populated with the decoded message on success.
         * @param err Set to an error description on failure.
         * @return    true on success, false on failure.
         */
        static bool Decode(ByteBuffer& w, IpcMessage& out, std::string& err);
};

#endif // AH_IPC_MESSAGE_H
