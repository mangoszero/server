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

#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "AuctionIntents.h"
#include "PlayerMutations.h"
#include "BrowseMessages.h"

/**
 * @brief Small inclusive cap for the debug ECHO opcodes and free-text frames.
 *
 * ECHO is a debug round-trip and IPC_CONSOLE carries a tiny control flag plus
 * optional text. Neither has a fixed layout, so they get a bounded maximum
 * rather than an exact size. The bound is small (well under the legitimate
 * working set) so it cannot be abused to flood bytes.
 */
static const uint32 IPC_ECHO_MAX_BODY = 256u;

IpcBodySizeRule IpcExpectedBodySize(uint16 op)
{
    // Helper macros keep the table compact and unambiguous.
    // EXACT(n)   - body length must equal n exactly.
    // MAXLEN(n)  - body length must be <= n.
    #define IPC_RULE_EXACT(n)  { true,  true,  static_cast<uint32>(n) }
    #define IPC_RULE_MAXLEN(n) { true,  false, static_cast<uint32>(n) }
    #define IPC_RULE_UNKNOWN   { false, false, 0u }

    switch (op)
    {
        // --- protocol / handshake frames (fixed tiny bodies) ---
        // proto + pid + secret
        case IPC_HELLO:         return IPC_RULE_MAXLEN(IPC_ECHO_MAX_BODY);
        case IPC_HELLO_ACK:     return IPC_RULE_EXACT(5);   // uint32 run-id + uint8 write-authority
        case IPC_READY:         return IPC_RULE_EXACT(0);
        case IPC_HEARTBEAT:     return IPC_RULE_EXACT(0);
        case IPC_HEARTBEAT_ACK: return IPC_RULE_EXACT(1);   // uint8 health flag
        case IPC_GAMETIME:      return IPC_RULE_EXACT(4);   // uint32 gametime
        case IPC_CONSOLE:       return IPC_RULE_MAXLEN(IPC_ECHO_MAX_BODY);
        case IPC_SHUTDOWN:      return IPC_RULE_EXACT(0);
        case IPC_SHUTDOWN_ACK:  return IPC_RULE_EXACT(0);
        case IPC_ECHO:          return IPC_RULE_MAXLEN(IPC_ECHO_MAX_BODY);
        case IPC_ECHO_REPLY:    return IPC_RULE_MAXLEN(IPC_ECHO_MAX_BODY);

        // --- AH consumer frames (fixed wire layouts) ---
        case IPC_INTENT_SELL:   // 33
            return IPC_RULE_EXACT(SellIntent::WIRE_SIZE);
        case IPC_INTENT_BID:    // 20
            return IPC_RULE_EXACT(BidIntent::WIRE_SIZE);
        case IPC_INTENT_BUYOUT: // 16
            return IPC_RULE_EXACT(BuyoutIntent::WIRE_SIZE);
        case IPC_INTENT_RESULT: // 10
            return IPC_RULE_EXACT(IntentResult::WIRE_SIZE);
        case IPC_QUEUE_FULL:    return IPC_RULE_EXACT(0);
        case IPC_GMCMD:         // 1
            return IPC_RULE_EXACT(GmCmd::WIRE_SIZE);
        case IPC_GMCMD_RESULT:  // 2
            return IPC_RULE_EXACT(GmCmdResult::WIRE_SIZE);

        // --- SP-2 player-mutation frames (fixed wire layouts) ---
        case IPC_PLAYER_SELL:           return IPC_RULE_EXACT(PlayerSellIntent::WIRE_SIZE);
        case IPC_PLAYER_BID:            return IPC_RULE_EXACT(PlayerBidIntent::WIRE_SIZE);
        case IPC_PLAYER_BUYOUT:         return IPC_RULE_EXACT(PlayerBuyoutIntent::WIRE_SIZE);
        case IPC_PLAYER_CANCEL:         return IPC_RULE_EXACT(PlayerCancelPrepare::WIRE_SIZE);
        case IPC_PLAYER_RESULT:         return IPC_RULE_EXACT(PlayerMutationResult::WIRE_SIZE);
        case IPC_RESOLVE_APPLY:         return IPC_RULE_EXACT(ResolveApply::WIRE_SIZE);
        case IPC_RESOLVE_ACK:           return IPC_RULE_EXACT(ResolveAck::WIRE_SIZE);
        case IPC_PLAYER_CANCEL_CONFIRM: return IPC_RULE_EXACT(PlayerCancelDecide::WIRE_SIZE);
        case IPC_PLAYER_CANCEL_ABORT:   return IPC_RULE_EXACT(PlayerCancelDecide::WIRE_SIZE);

        case IPC_BROWSE_QUERY:
            return IPC_RULE_MAXLEN(BrowseQuery::MAX_WIRE);
        case IPC_BROWSE_RESULT:
            return IPC_RULE_MAXLEN(BrowseResult::MAX_WIRE);

        default:                return IPC_RULE_UNKNOWN;
    }

    #undef IPC_RULE_EXACT
    #undef IPC_RULE_MAXLEN
    #undef IPC_RULE_UNKNOWN
}

void IpcMessage::Encode(ByteBuffer& w) const
{
    w << IPC_PROTOCOL_VERSION << uint16(op) << uint32(body.size());
    w.append(body.contents(), body.size());
}

bool IpcMessage::Decode(ByteBuffer& w, IpcMessage& out, std::string& err)
{
    if (w.size() - w.rpos() < 8)
    {
        err = "short header";
        return false;
    }

    uint16 ver;
    uint16 op;
    uint32 len;
    size_t start = w.rpos();

    w >> ver >> op >> len;

    if (ver != IPC_PROTOCOL_VERSION)
    {
        w.rpos(start);
        err = "version mismatch";
        return false;
    }

    if (len > IPC_MAX_FRAME)
    {
        w.rpos(start);
        err = "oversize frame";
        return false;
    }

    if (w.size() - w.rpos() < len)
    {
        w.rpos(start);
        err = "incomplete";
        return false;
    }

    out.op = IpcOpcode(op);
    out.body.clear();

    if (len)
    {
        out.body.append(w.contents() + w.rpos(), len);
        w.rpos(w.rpos() + len);
    }

    return true;
}
