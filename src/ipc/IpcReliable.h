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

#ifndef AH_IPC_RELIABLE_H
#define AH_IPC_RELIABLE_H

#include "Common.h"
#include "IpcOpcodes.h"

/// [SP-2] True for mutation-class frames that must never be dropped under
/// inbound pressure (they carry money/item value or their outcome). These
/// ride an unbounded lane both directions (decision 10). Browse/heartbeat/
/// gametime stay on the bounded drop-newest queue.
inline bool IpcIsReliableOpcode(uint16 op)
{
    switch (op)
    {
        case IPC_PLAYER_SELL:
        case IPC_PLAYER_BID:
        case IPC_PLAYER_BUYOUT:
        case IPC_PLAYER_CANCEL:
        case IPC_PLAYER_RESULT:
        case IPC_RESOLVE_APPLY:
        case IPC_RESOLVE_ACK:
        case IPC_PLAYER_CANCEL_CONFIRM:
        case IPC_PLAYER_CANCEL_ABORT:
        case IPC_INTENT_SELL:
        case IPC_INTENT_RESULT:
            return true;
        default:
            return false;
    }
}

#endif // AH_IPC_RELIABLE_H
