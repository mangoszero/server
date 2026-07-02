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

#ifndef AH_IPC_PLAYER_MUTATIONS_H
#define AH_IPC_PLAYER_MUTATIONS_H

#include "Common.h"
#include "Utilities/ByteBuffer.h"

/**
 * @file PlayerMutations.h
 * @brief SP-2 wire types: player mutations + worker-initiated resolutions.
 *
 * Same idiom as AuctionIntents.h: each struct has a fixed WIRE_SIZE, an
 * Encode(ByteBuffer&) that appends fields in declared order, and a
 * Decode(ByteBuffer&) that first guards rpos()+WIRE_SIZE > size().
 *
 * The worker is the auction write-authority; it reports book facts and
 * mangosd computes ALL value math (spec 4.5). MutationFacts is the shared
 * by-value snapshot carried on every reply and resolution.
 */

enum PlayerMutationStatus : uint8
{
    MUT_OK             = 0,
    MUT_REJECTED       = 1,
    MUT_PREPARED       = 2,   ///< cancel PREPARE accepted; awaiting CONFIRM/ABORT
    MUT_REJECTED_STALE = 3    ///< confirm/abort for an unknown/expired prepare
};

enum ResolveKind : uint8
{
    RESOLVE_WON              = 0,
    RESOLVE_EXPIRED_NOBID    = 1,
    RESOLVE_CANCELLED_UNLOCK = 2,
    RESOLVE_REPAIR_RETURN    = 3
};

enum ResolveAckStatus : uint8
{
    RES_APPLIED   = 0,
    RES_FAILED    = 1,
    RES_DUPLICATE = 2
};

// ---------------------------------------------------------------------------
// MutationFacts  (wire size = 4*13 + 1 = 53 bytes)
// ---------------------------------------------------------------------------

/**
 * @brief Book-facts snapshot. The worker fills it; mangosd computes value.
 * randomPropertyId is signed (item enchant seed can be negative).
 */
struct MutationFacts
{
    static const size_t WIRE_SIZE = 53u;

    uint32 auctionId;
    uint8  houseId;
    uint32 itemGuid;
    uint32 itemTemplate;
    int32  randomPropertyId;
    uint32 itemCount;
    uint32 sellerGuid;
    uint32 deposit;
    uint32 effectiveBid;
    uint32 priorBidderGuid;
    uint32 priorBidAmount;
    uint32 curBidderGuid;
    uint32 curBid;
    uint32 buyout;

    void Encode(ByteBuffer& buf) const
    {
        buf << auctionId << houseId << itemGuid << itemTemplate
            << randomPropertyId << itemCount << sellerGuid << deposit
            << effectiveBid << priorBidderGuid << priorBidAmount
            << curBidderGuid << curBid << buyout;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> auctionId >> houseId >> itemGuid >> itemTemplate
            >> randomPropertyId >> itemCount >> sellerGuid >> deposit
            >> effectiveBid >> priorBidderGuid >> priorBidAmount
            >> curBidderGuid >> curBid >> buyout;
        return true;
    }
};

// ---------------------------------------------------------------------------
// PlayerSellIntent  (wire size = 8+4+4+1+4+4+4+4+4+4+4+4 = 49 bytes)
// ---------------------------------------------------------------------------

struct PlayerSellIntent
{
    static const size_t WIRE_SIZE = 49u;

    uint64 uuid;
    uint32 auctionId;    ///< pre-allocated by mangosd (sole allocator, spec 8)
    uint32 sellerGuid;
    uint8  house;
    uint32 itemGuid;
    uint32 itemTemplate;
    uint32 itemCount;
    int32  randomPropertyId;
    uint32 startbid;
    uint32 buyout;
    uint32 deposit;      ///< computed by mangosd at reserve time
    uint32 expireTime;

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << auctionId << sellerGuid << house << itemGuid
            << itemTemplate << itemCount << randomPropertyId << startbid
            << buyout << deposit << expireTime;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> auctionId >> sellerGuid >> house >> itemGuid
            >> itemTemplate >> itemCount >> randomPropertyId >> startbid
            >> buyout >> deposit >> expireTime;
        return true;
    }
};

// ---------------------------------------------------------------------------
// PlayerBidIntent  (wire size = 8+4+4+4 = 20 bytes)
// ---------------------------------------------------------------------------

struct PlayerBidIntent
{
    static const size_t WIRE_SIZE = 20u;

    uint64 uuid;
    uint32 auctionId;
    uint32 bidderGuid;
    uint32 bidAmount;

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << auctionId << bidderGuid << bidAmount;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> auctionId >> bidderGuid >> bidAmount;
        return true;
    }
};

// ---------------------------------------------------------------------------
// PlayerBuyoutIntent  (wire size = 8+4+4+4 = 20 bytes)
// ---------------------------------------------------------------------------

struct PlayerBuyoutIntent
{
    static const size_t WIRE_SIZE = 20u;

    uint64 uuid;
    uint32 auctionId;
    uint32 bidderGuid;
    uint32 maxPrice;

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << auctionId << bidderGuid << maxPrice;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> auctionId >> bidderGuid >> maxPrice;
        return true;
    }
};

// ---------------------------------------------------------------------------
// PlayerCancelPrepare  (wire size = 8+4+4 = 16 bytes)
// ---------------------------------------------------------------------------

struct PlayerCancelPrepare
{
    static const size_t WIRE_SIZE = 16u;

    uint64 uuid;
    uint32 auctionId;
    uint32 sellerGuid;

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << auctionId << sellerGuid;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> auctionId >> sellerGuid;
        return true;
    }
};

// ---------------------------------------------------------------------------
// PlayerCancelDecide  (wire size = 8+4 = 12 bytes) -- CONFIRM (0x1047) + ABORT (0x1048)
// ---------------------------------------------------------------------------

struct PlayerCancelDecide
{
    static const size_t WIRE_SIZE = 12u;

    uint64 uuid;
    uint32 auctionId;

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << auctionId;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> auctionId;
        return true;
    }
};

// ---------------------------------------------------------------------------
// PlayerMutationResult  (wire size = 8+1+1+1 + 53 = 64 bytes)
// ---------------------------------------------------------------------------

struct PlayerMutationResult
{
    static const size_t WIRE_SIZE = 11u + MutationFacts::WIRE_SIZE;

    uint64        uuid;
    uint8         op;       ///< originating IpcOpcode low byte (0x40..0x48)
    uint8         status;   ///< PlayerMutationStatus
    uint8         reason;   ///< AuctionError low byte on REJECTED (0 == silent)
    MutationFacts facts;

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << op << status << reason;
        facts.Encode(buf);
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> op >> status >> reason;
        return facts.Decode(buf);
    }
};

// ---------------------------------------------------------------------------
// ResolveApply  (wire size = 8+1 + 53 = 62 bytes)
// ---------------------------------------------------------------------------

struct ResolveApply
{
    static const size_t WIRE_SIZE = 9u + MutationFacts::WIRE_SIZE;

    uint64        uuid;
    uint8         kind;   ///< ResolveKind
    MutationFacts facts;

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << kind;
        facts.Encode(buf);
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> kind;
        return facts.Decode(buf);
    }
};

// ---------------------------------------------------------------------------
// ResolveAck  (wire size = 8+1 = 9 bytes)
// ---------------------------------------------------------------------------

struct ResolveAck
{
    static const size_t WIRE_SIZE = 9u;

    uint64 uuid;
    uint8  status;   ///< ResolveAckStatus

    void Encode(ByteBuffer& buf) const
    {
        buf << uuid << status;
    }

    bool Decode(ByteBuffer& buf)
    {
        if (buf.rpos() + WIRE_SIZE > buf.size())
        {
            return false;
        }
        buf >> uuid >> status;
        return true;
    }
};

#endif // AH_IPC_PLAYER_MUTATIONS_H
