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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_AUCTION_HOUSE_MODULE
#define MANGOS_H_AUCTION_HOUSE_MODULE

#include "Platform/Define.h"
#include "DBCStructure.h"

#include <memory>
#include <string>
#include <vector>

class AuctionHouseObject;
class ChatCommand;
class Item;
class WorldSession;
struct AuctionEntry;

/// A player's request to put an item up for auction, after the core checks.
struct AuctionSale
{
    AuctionHouseEntry const* houseEntry;
    AuctionHouseObject* house;
    Item* item;
    uint32 etime;                                           ///< seconds
    uint32 bid;
    uint32 buyout;
    uint32 deposit;
};

enum AuctionListKind
{
    AUCTION_LIST_ITEMS  = 0,
    AUCTION_LIST_OWNER  = 1,
    AUCTION_LIST_BIDDER = 2
};

/// One of the three auction list requests, as the client sent it.
struct AuctionListRequest
{
    AuctionListKind kind;
    AuctionHouseEntry const* houseEntry;
    AuctionHouseObject* house;
    uint32 listfrom;
    std::string searchedName;                               ///< raw UTF-8, AUCTION_LIST_ITEMS only
    uint8 levelmin;
    uint8 levelmax;
    uint8 usable;
    uint32 inventoryType;
    uint32 itemClass;
    uint32 itemSubClass;
    uint32 quality;
    std::vector<uint32> outbidIds;                          ///< client order, AUCTION_LIST_BIDDER only
};

enum AuctionExpiry
{
    AUCTION_EXPIRY_CORE,                                    ///< the core settles it
    AUCTION_EXPIRY_DONE,                                    ///< the module settled or skipped it
    AUCTION_EXPIRY_STOP                                     ///< leave the rest of this house for the next tick
};

/**
 * @brief Something that lives beside the auction house: a bot that trades on
 * it, or a service that takes part of it over.
 *
 * Every hook does nothing by default. A request hook returns true when the
 * module answered the request itself; the core then does nothing more.
 */
class AuctionHouseModule
{
    public:

        virtual ~AuctionHouseModule() {}

        virtual char const* Name() const = 0;

        virtual void LoadConfig(bool /*reload*/) {}
        /// After the auctions are loaded, before the world loop starts.
        virtual void Start() {}
        virtual void Update(uint32 /*diff*/) {}
        /// After the world loop has stopped.
        virtual void Stop() {}
        /// Top-level commands, terminated like every command table.
        virtual ChatCommand* Commands() { return NULL; }

        virtual uint32 HighestAuctionId() const { return 0; }
        /// Another process writes the `auction` table; the core and the bots must not.
        virtual bool OwnsAuctionRows() const { return false; }
        /// Listings are driven elsewhere for now; the bots stand down.
        virtual bool TakesOverListings() const { return false; }

        virtual bool BlocksHello(WorldSession& /*session*/) { return false; }
        virtual bool ForwardSell(WorldSession& /*session*/, AuctionSale const& /*sale*/) { return false; }
        /// The deposit is already taken; @p listed stays NULL when the sale failed and was answered.
        virtual bool SettleSell(WorldSession& /*session*/, AuctionSale const& /*sale*/, AuctionEntry*& /*listed*/) { return false; }
        virtual bool ForwardBid(WorldSession& /*session*/, uint32 /*auctionId*/, uint32 /*price*/) { return false; }
        virtual bool SettleBid(WorldSession& /*session*/, AuctionHouseObject& /*house*/, AuctionEntry& /*auction*/, uint32 /*price*/, uint32 /*newOutbid*/) { return false; }
        virtual bool ForwardCancel(WorldSession& /*session*/, uint32 /*auctionId*/) { return false; }
        virtual bool SettleCancel(WorldSession& /*session*/, AuctionHouseObject& /*house*/, AuctionEntry& /*auction*/) { return false; }
        virtual bool ForwardList(WorldSession& /*session*/, AuctionListRequest const& /*request*/) { return false; }

        virtual AuctionExpiry Expire(AuctionHouseObject& /*house*/, AuctionEntry& /*auction*/) { return AUCTION_EXPIRY_CORE; }
        /// A bid with no player behind it; @p stillActive is UpdateBid's result.
        virtual bool SettleGeneratedBid(AuctionEntry& /*auction*/, uint32 /*price*/, bool& /*stillActive*/) { return false; }
};

/**
 * @brief The auction modules linked into this server.
 *
 * A module provides a factory from a static object in its own sources; the
 * factory reads the module's configuration and returns NULL when it is off.
 */
class AuctionHouseModules
{
    public:

        typedef AuctionHouseModule* (*Factory)();

        static void Provide(Factory factory);

        static void LoadConfig(bool reload);
        static void Start();
        static void Update(uint32 diff);
        static void Stop();

        static std::vector<ChatCommand*> Commands();
        static uint32 HighestAuctionId();
        static bool OwnsAuctionRows();
        static bool TakesOverListings();
        static std::vector<std::unique_ptr<AuctionHouseModule>> const& All();

        /// Asks the modules in turn until one answers.
        template<typename Hook>
        static bool Answer(Hook hook)
        {
            for (std::unique_ptr<AuctionHouseModule> const& module : All())
            {
                if (hook(*module))
                {
                    return true;
                }
            }
            return false;
        }

        static void SetConfigFile(std::string const& path);
        /// The bots' configuration file (ahbot.conf), as given to mangosd.
        static std::string const& ConfigFile();
};

#endif
