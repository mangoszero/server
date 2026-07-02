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

#ifndef AH_IPC_AUCTION_INTENTS_H
#define AH_IPC_AUCTION_INTENTS_H

#include "Common.h"
#include "Utilities/ByteBuffer.h"

/**
 * @file AuctionIntents.h
 * @brief Wire types for Milestone 2 AH subprocess intent messages.
 *
 * Each struct maps 1:1 to an IPC opcode body:
 *   SellIntent    -> IPC_INTENT_SELL   (0x1001)
 *   BidIntent     -> IPC_INTENT_BID    (0x1002)
 *   BuyoutIntent  -> IPC_INTENT_BUYOUT (0x1003)
 *   IntentResult  -> IPC_INTENT_RESULT (0x1010)
 *
 * Usage (sender):
 *   SellIntent si{...};
 *   IpcMessage m;
 *   m.op = IPC_INTENT_SELL;
 *   si.Encode(m.body);
 *   channel.SendFrame(m);
 *
 * Usage (receiver):
 *   SellIntent si;
 *   if (si.Decode(msg.body)) { ... }
 *
 * NOTE: uuid generation is intentionally deferred to Task 8.
 * The intended composition is: high 32 bits = child run-id (assigned at
 * handshake), low 32 bits = per-child monotonic sequence number.
 *
 * This header is consumed only by the selftest in Task 7; Task 8 wires
 * it into the real producer path.
 */

// ---------------------------------------------------------------------------
// Status / reason enumerations (sized to the wire: uint8)
// ---------------------------------------------------------------------------

/**
 * @brief Outcome status for an IntentResult message.
 */
enum IntentStatus : uint8
{
    INTENT_OK        = 0,   ///< Intent was accepted and applied
    INTENT_REJECTED  = 1,   ///< Intent was rejected (see reason)
    INTENT_DUPLICATE = 2    ///< uuid was seen before; ignored
};

/**
 * @brief Rejection reason accompanying INTENT_REJECTED status.
 */
enum IntentReason : uint8
{
    REASON_NONE          = 0,   ///< No specific reason (OK path)
    REASON_NOT_FOUND     = 1,   ///< Auction ID not found
    REASON_EXPIRED       = 2,   ///< Auction already expired
    REASON_BOUGHT_OUT    = 3,   ///< Auction already bought out
    REASON_STALE_BID     = 4,   ///< Bid is below current bid
    REASON_GUID_MISMATCH = 5,   ///< botGuid does not own the auction
    REASON_NO_FUNDS      = 6,   ///< Insufficient funds for bid/buyout
    REASON_BAD_ITEM      = 7    ///< Item ID invalid or not listable
};

// ---------------------------------------------------------------------------
// SellIntent  (wire size = 8+4+1+4+4+4+4+4 = 33 bytes)
// ---------------------------------------------------------------------------

/**
 * @brief Request to post an item on the auction house.
 *
 * Opcode: IPC_INTENT_SELL (0x1001), direction: ah-service -> mangosd.
 *
 * Wire size: 33 bytes.
 *
 * @note uuid high 32 bits = child run-id (from handshake);
 *       low 32 bits = per-child monotonic sequence. Generated in Task 8.
 */
struct SellIntent
{
    static const size_t WIRE_SIZE = 33u;  ///< Fixed wire size in bytes.

    uint64 uuid;         ///< Unique intent ID (see file note on composition)
    uint32 botGuid;      ///< GUID of the bot placing the listing
    uint8  house;        ///< Auction house faction (0=Alliance,1=Horde,2=Neutral)
    uint32 itemId;       ///< Item entry ID to list
    uint32 stack;        ///< Stack count
    uint32 bid;          ///< Starting bid in copper
    uint32 buyout;       ///< Buyout price in copper (0 = no buyout)
    uint32 durationHrs;  ///< Listing duration in hours (12/24/48)

    /** @brief Append all fields to @p buf in wire order. */
    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << botGuid << house
            << itemId << stack << bid << buyout << durationHrs;
    }

    /**
     * @brief Read all fields from @p buf in wire order.
     *
     * Guards against truncated frames: returns false if fewer than
     * 33 bytes remain before reading any field.
     *
     * @return true on success, false if the buffer is too short.
     */
    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> botGuid >> house
            >> itemId >> stack >> bid >> buyout >> durationHrs;
        return true;
    }
};

// ---------------------------------------------------------------------------
// BidIntent  (wire size = 8+4+4+4 = 20 bytes)
// ---------------------------------------------------------------------------

/**
 * @brief Request to place a bid on an existing auction.
 *
 * Opcode: IPC_INTENT_BID (0x1002), direction: ah-service -> mangosd.
 *
 * Wire size: 20 bytes.
 */
struct BidIntent
{
    static const size_t WIRE_SIZE = 20u;  ///< Fixed wire size in bytes.

    uint64 uuid;       ///< Unique intent ID (see SellIntent note)
    uint32 botGuid;    ///< GUID of the bot placing the bid
    uint32 auctionId;  ///< Server auction entry ID to bid on
    uint32 bidAmount;  ///< Bid amount in copper

    /** @brief Append all fields to @p buf in wire order. */
    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << botGuid << auctionId << bidAmount;
    }

    /**
     * @brief Read all fields from @p buf in wire order.
     *
     * @return true on success, false if fewer than 20 bytes remain.
     */
    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> botGuid >> auctionId >> bidAmount;
        return true;
    }
};

// ---------------------------------------------------------------------------
// BuyoutIntent  (wire size = 8+4+4 = 16 bytes)
// ---------------------------------------------------------------------------

/**
 * @brief Request to instant-buy an existing auction at its buyout price.
 *
 * Opcode: IPC_INTENT_BUYOUT (0x1003), direction: ah-service -> mangosd.
 *
 * Wire size: 16 bytes.
 */
struct BuyoutIntent
{
    static const size_t WIRE_SIZE = 16u;  ///< Fixed wire size in bytes.

    uint64 uuid;       ///< Unique intent ID (see SellIntent note)
    uint32 botGuid;    ///< GUID of the bot buying out
    uint32 auctionId;  ///< Server auction entry ID to buy out

    /** @brief Append all fields to @p buf in wire order. */
    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << botGuid << auctionId;
    }

    /**
     * @brief Read all fields from @p buf in wire order.
     *
     * @return true on success, false if fewer than 16 bytes remain.
     */
    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> botGuid >> auctionId;
        return true;
    }
};

// ---------------------------------------------------------------------------
// GmCmd / GmCmdResult  (wire sizes: 1 / 2 bytes)
// ---------------------------------------------------------------------------

/**
 * @brief GM command type relayed to the ah-service child.
 */
enum GmCmdType : uint8
{
    GMCMD_RELOAD = 1   ///< Live-reload ahbot.conf tuning in the child.
};

/**
 * @brief GM command frame sent from mangosd to the ah-service child.
 *
 * Opcode: IPC_GMCMD (0x1020), direction: mangosd -> ah-service.
 *
 * Wire size: 1 byte.
 */
struct GmCmd
{
    static const size_t WIRE_SIZE = 1u;  ///< Fixed wire size in bytes.

    uint8 cmd;  ///< GmCmdType value.

    /** @brief Append all fields to @p buf in wire order. */
    void Encode(ByteBuffer& buf) const
    {
        buf << cmd;
    }

    /**
     * @brief Read all fields from @p buf in wire order.
     *
     * @return true on success, false if fewer than 1 byte remains.
     */
    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> cmd;
        return true;
    }
};

/**
 * @brief Result of a GM command execution in the ah-service child.
 *
 * Opcode: IPC_GMCMD_RESULT (0x1021), direction: ah-service -> mangosd.
 *
 * Wire size: 2 bytes.
 */
struct GmCmdResult
{
    static const size_t WIRE_SIZE = 2u;  ///< Fixed wire size in bytes.

    uint8 cmd;  ///< GmCmdType value echoed from the triggering GmCmd.
    uint8 ok;   ///< 1 = success, 0 = failure.

    /** @brief Append all fields to @p buf in wire order. */
    void Encode(ByteBuffer& buf) const
    {
        buf << cmd << ok;
    }

    /**
     * @brief Read all fields from @p buf in wire order.
     *
     * @return true on success, false if fewer than 2 bytes remain.
     */
    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> cmd >> ok;
        return true;
    }
};

// ---------------------------------------------------------------------------
// IntentResult  (wire size = 8+1+1 = 10 bytes)
// ---------------------------------------------------------------------------

/**
 * @brief Outcome of a previously submitted intent.
 *
 * Opcode: IPC_INTENT_RESULT (0x1010), direction: mangosd -> ah-service.
 *
 * Wire size: 18 bytes.
 */
struct IntentResult
{
    static const size_t WIRE_SIZE = 18u;  ///< 8+1+1+4+4; SP-2 adds itemGuid+auctionId.

    uint64 uuid;       ///< Echoes the uuid from the triggering intent
    uint8  status;     ///< IntentStatus value
    uint8  reason;     ///< IntentReason value (REASON_NONE when status == INTENT_OK)
    uint32 itemGuid;   ///< [SP-2] materialized item low GUID (0 for bid/buyout/legacy)
    uint32 auctionId;  ///< [SP-2] allocated auction id (0 for bid/buyout/legacy)

    /** @brief Append all fields to @p buf in wire order. */
    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << status << reason << itemGuid << auctionId;
    }

    /**
     * @brief Read all fields from @p buf in wire order.
     *
     * @return true on success, false if fewer than 18 bytes remain.
     */
    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> status >> reason >> itemGuid >> auctionId;
        return true;
    }
};

#endif // AH_IPC_AUCTION_INTENTS_H
