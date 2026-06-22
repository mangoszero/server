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

#ifndef AH_SERVICE_AHBOT_DEFINES_H
#define AH_SERVICE_AHBOT_DEFINES_H

#include "Common.h"

/**
 * @file AhBotDefines.h
 * @brief Child-owned copies of the game-lib enums/constants the AH bot port
 *        needs.
 *
 * The ah-service child links only @c ah_ipc + @c shared and cannot include
 * game-lib headers. The values below are verbatim reimplementations of the
 * game-lib definitions used by the item-pool build:
 *   - @c ItemClass / @c MAX_ITEM_CLASS        (Object/ItemPrototype.h)
 *   - @c ItemBondingType                      (Object/ItemPrototype.h)
 *   - @c ItemPrototypeFlags                   (Object/ItemPrototype.h)
 *   - @c ItemQualities / @c MAX_ITEM_QUALITY  (Server/SharedDefines.h)
 *   - @c AuctionHouseType /
 *     @c MAX_AUCTION_HOUSE_TYPE               (Object/AuctionHouseMgr.h)
 *   - @c AuctionQuality / @c MAX_AUCTION_QUALITY
 *                                          (AuctionHouseBot/AuctionHouseBot.h)
 *
 * This is a CLASSIC (MaNGOS Zero) build: the gem/generic item classes and the
 * mount sub-class filter are absent here exactly as the upstream
 * @c \#if !defined(CLASSIC) blocks exclude them.
 */

/// Item classes (mirror of game-lib enum ItemClass, ItemPrototype.h).
enum AhItemClass
{
    AH_ITEM_CLASS_CONSUMABLE  = 0,
    AH_ITEM_CLASS_CONTAINER   = 1,
    AH_ITEM_CLASS_WEAPON      = 2,
    AH_ITEM_CLASS_RESERVED_1  = 3,
    AH_ITEM_CLASS_ARMOR       = 4,
    AH_ITEM_CLASS_REAGENT     = 5,
    AH_ITEM_CLASS_PROJECTILE  = 6,
    AH_ITEM_CLASS_TRADE_GOODS = 7,
    AH_ITEM_CLASS_RESERVED_2  = 8,
    AH_ITEM_CLASS_RECIPE      = 9,
    AH_ITEM_CLASS_RESERVED_3  = 10,
    AH_ITEM_CLASS_QUIVER      = 11,
    AH_ITEM_CLASS_QUEST       = 12,
    AH_ITEM_CLASS_KEY         = 13,
    AH_ITEM_CLASS_RESERVED_4  = 14,
    AH_ITEM_CLASS_MISC        = 15
};

/// Total number of item classes (mirror of MAX_ITEM_CLASS).
#define AH_MAX_ITEM_CLASS 16

/// Item bonding types (mirror of game-lib enum ItemBondingType).
enum AhItemBondingType
{
    AH_NO_BIND              = 0,
    AH_BIND_WHEN_PICKED_UP  = 1,
    AH_BIND_WHEN_EQUIPPED   = 2,
    AH_BIND_WHEN_USE        = 3,
    AH_BIND_QUEST_ITEM      = 4,
    AH_BIND_QUEST_ITEM1     = 5      ///< not used in game
};

/// ItemPrototype.Flags bit used by the pool filter (mirror ItemPrototypeFlags).
enum AhItemPrototypeFlags
{
    AH_ITEM_FLAG_LOOTABLE = 0x00000004
};

/// Item qualities (mirror of game-lib enum ItemQualities, SharedDefines.h).
enum AhItemQualities
{
    AH_ITEM_QUALITY_POOR      = 0,  ///< GREY
    AH_ITEM_QUALITY_NORMAL    = 1,  ///< WHITE
    AH_ITEM_QUALITY_UNCOMMON  = 2,  ///< GREEN
    AH_ITEM_QUALITY_RARE      = 3,  ///< BLUE
    AH_ITEM_QUALITY_EPIC      = 4,  ///< PURPLE
    AH_ITEM_QUALITY_LEGENDARY = 5,  ///< ORANGE
    AH_ITEM_QUALITY_ARTIFACT  = 6   ///< LIGHT YELLOW
};

/**
 * @brief Auction item quality count.
 *
 * Mirror of MAX_AUCTION_QUALITY: a shadow of ItemQualities that skips
 * heirloom and anything past ITEM_QUALITY_ARTIFACT(6).
 */
#define AH_MAX_AUCTION_QUALITY 7

/// Auction house types (mirror of game-lib enum AuctionHouseType).
enum AhAuctionHouseType
{
    AH_AUCTION_HOUSE_ALLIANCE = 0,
    AH_AUCTION_HOUSE_HORDE    = 1,
    AH_AUCTION_HOUSE_NEUTRAL  = 2
};

/// Total number of auction house types (mirror of MAX_AUCTION_HOUSE_TYPE).
#define AH_MAX_AUCTION_HOUSE_TYPE 3

#endif // AH_SERVICE_AHBOT_DEFINES_H
