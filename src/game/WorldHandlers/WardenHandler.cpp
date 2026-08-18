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

#include "WorldPacket.h"
#include "WorldSession.h"
#include "WardenServer.h"

void WorldSession::HandleWardenDataOpcode(WorldPacket& recvData)
{
    // CMSG_WARDEN_DATA is one grouped transport opcode. Internal commands stay
    // encrypted here and are decoded by the per-session state machine rather
    // than becoming separate WorldSession opcode handlers.
    size_t const unread = recvData.wpos() - recvData.rpos();
    if (m_warden && !m_wardenEnforcementClosed)
    {
        warden::ByteView const body = {
            unread ? recvData.contents() + recvData.rpos() : nullptr,
            unread};
        m_warden->HandleEncrypted(body);
    }
    // Always consume the outer body, including when no supported profile was
    // created, so it cannot be reconsidered by later session processing.
    recvData.rfinish();
}
