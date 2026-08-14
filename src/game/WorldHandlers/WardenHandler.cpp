/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "WorldPacket.h"
#include "WorldSession.h"
#include "WardenServer.h"

void WorldSession::HandleWardenDataOpcode(WorldPacket& recvData)
{
    size_t const unread = recvData.wpos() - recvData.rpos();
    if (m_warden)
    {
        warden::ByteView const body = {
            unread ? recvData.contents() + recvData.rpos() : nullptr,
            unread};
        m_warden->HandleEncrypted(body);
    }
    recvData.rfinish();
}
