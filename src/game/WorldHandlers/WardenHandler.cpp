/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "WorldPacket.h"
#include "WorldSession.h"

void WorldSession::HandleWardenDataOpcode(WorldPacket& recvData)
{
    recvData.rfinish();
}
