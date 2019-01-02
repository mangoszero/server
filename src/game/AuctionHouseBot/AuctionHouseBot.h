/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef AUCTION_HOUSE_BOT_H
#define AUCTION_HOUSE_BOT_H

#include "Config/Config.h"
#include "AuctionHouseMgr.h"
#include "SharedDefines.h"
#include "Item.h"

/**
 * This is the AuctionHouseBot and it is used to make less populated servers
 * appear more populated than they actually are by having auctions created by
 * the mangos daemon itself instead of by players.
 *
 * This bot can both create auctions and buyout/bid on auctions to create a feel
 * of a very active AH. The key classes for creating and buying auctions are
 * \ref AuctionBotBuyer for buying and \ref AuctionBotSeller for selling.
 *
 * \todo Describe more of how it all works, ie algorithms etc.
 */

/** \addtogroup auctionbot
 * @{
 * \file
 */

/**
 * @brief shadow of ItemQualities with skipped ITEM_QUALITY_HEIRLOOM, anything after ITEM_QUALITY_ARTIFACT(6) in fact
 *
 */
enum AuctionQuality
{
    AUCTION_QUALITY_GREY   = ITEM_QUALITY_POOR,
    AUCTION_QUALITY_WHITE  = ITEM_QUALITY_NORMAL,
    AUCTION_QUALITY_GREEN  = ITEM_QUALITY_UNCOMMON,
    AUCTION_QUALITY_BLUE   = ITEM_QUALITY_RARE,
    AUCTION_QUALITY_PURPLE = ITEM_QUALITY_EPIC,
    AUCTION_QUALITY_ORANGE = ITEM_QUALITY_LEGENDARY,
    AUCTION_QUALITY_YELLOW = ITEM_QUALITY_ARTIFACT
};

#define MAX_AUCTION_QUALITY 7

/**
 * @brief
 *
 */
enum AuctionBotConfigUInt32Values
{
    CONFIG_UINT32_AHBOT_MAXTIME,
    CONFIG_UINT32_AHBOT_MINTIME,
    CONFIG_UINT32_AHBOT_ITEMS_PER_CYCLE_BOOST,
    CONFIG_UINT32_AHBOT_ITEMS_PER_CYCLE_NORMAL,
    CONFIG_UINT32_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO,
    CONFIG_UINT32_AHBOT_HORDE_ITEM_AMOUNT_RATIO,
    CONFIG_UINT32_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO,
    CONFIG_UINT32_AHBOT_ITEM_MIN_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_ITEM_MAX_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_ITEM_MIN_REQ_LEVEL,
    CONFIG_UINT32_AHBOT_ITEM_MAX_REQ_LEVEL,
    CONFIG_UINT32_AHBOT_ITEM_MIN_SKILL_RANK,
    CONFIG_UINT32_AHBOT_ITEM_MAX_SKILL_RANK,
    CONFIG_UINT32_AHBOT_ITEM_GREY_AMOUNT,
    CONFIG_UINT32_AHBOT_ITEM_WHITE_AMOUNT,
    CONFIG_UINT32_AHBOT_ITEM_GREEN_AMOUNT,
    CONFIG_UINT32_AHBOT_ITEM_BLUE_AMOUNT,
    CONFIG_UINT32_AHBOT_ITEM_PURPLE_AMOUNT,
    CONFIG_UINT32_AHBOT_ITEM_ORANGE_AMOUNT,
    CONFIG_UINT32_AHBOT_ITEM_YELLOW_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_CONSUMABLE_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_CONTAINER_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_WEAPON_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_GEM_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_ARMOR_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_REAGENT_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_PROJECTILE_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_TRADEGOOD_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_GENERIC_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_RECIPE_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_QUIVER_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_QUEST_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_KEY_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_MISC_AMOUNT,
    CONFIG_UINT32_AHBOT_CLASS_GLYPH_AMOUNT,
    CONFIG_UINT32_AHBOT_ALLIANCE_PRICE_RATIO,
    CONFIG_UINT32_AHBOT_HORDE_PRICE_RATIO,
    CONFIG_UINT32_AHBOT_NEUTRAL_PRICE_RATIO,
    CONFIG_UINT32_AHBOT_BUYER_CHANCE_RATIO_ALLIANCE,
    CONFIG_UINT32_AHBOT_BUYER_CHANCE_RATIO_HORDE,
    CONFIG_UINT32_AHBOT_BUYER_CHANCE_RATIO_NEUTRAL,
    CONFIG_UINT32_AHBOT_BUYER_RECHECK_INTERVAL,
    CONFIG_UINT32_AHBOT_CLASS_MISC_MOUNT_MIN_REQ_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_MISC_MOUNT_MAX_REQ_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_MISC_MOUNT_MIN_SKILL_RANK,
    CONFIG_UINT32_AHBOT_CLASS_MISC_MOUNT_MAX_SKILL_RANK,
    CONFIG_UINT32_AHBOT_CLASS_GLYPH_MIN_REQ_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_GLYPH_MAX_REQ_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_GLYPH_MIN_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_GLYPH_MAX_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_TRADEGOOD_MIN_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_TRADEGOOD_MAX_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_CONTAINER_MIN_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_CLASS_CONTAINER_MAX_ITEM_LEVEL,
    CONFIG_UINT32_AHBOT_UINT32_COUNT
};

/**
 * @brief
 *
 */
enum AuctionBotConfigBoolValues
{
    CONFIG_BOOL_AHBOT_BUYER_ALLIANCE_ENABLED,
    CONFIG_BOOL_AHBOT_BUYER_HORDE_ENABLED,
    CONFIG_BOOL_AHBOT_BUYER_NEUTRAL_ENABLED,
    CONFIG_BOOL_AHBOT_ITEMS_VENDOR,
    CONFIG_BOOL_AHBOT_ITEMS_LOOT,
    CONFIG_BOOL_AHBOT_ITEMS_MISC,
    CONFIG_BOOL_AHBOT_BIND_NO,
    CONFIG_BOOL_AHBOT_BIND_PICKUP,
    CONFIG_BOOL_AHBOT_BIND_EQUIP,
    CONFIG_BOOL_AHBOT_BIND_USE,
    CONFIG_BOOL_AHBOT_BIND_QUEST,
    CONFIG_BOOL_AHBOT_BUYPRICE_SELLER,
    CONFIG_BOOL_AHBOT_BUYPRICE_BUYER,
    CONFIG_BOOL_AHBOT_DEBUG_SELLER,
    CONFIG_BOOL_AHBOT_DEBUG_BUYER,
    CONFIG_BOOL_AHBOT_SELLER_ENABLED,
    CONFIG_BOOL_AHBOT_BUYER_ENABLED,
    CONFIG_BOOL_AHBOT_LOCKBOX_ENABLED,
    CONFIG_UINT32_AHBOT_BOOL_COUNT
};

/**
 * @brief All basic config data used by other AHBot classes for self-configure.
 *
 */
class AuctionBotConfig
{
    public:
        /**
         * @brief
         *
         */
        AuctionBotConfig();

        /**
         * @brief
         *
         * @param filename
         */
        void        SetConfigFileName(char const* filename) { m_configFileName = filename; }
        /**
         * @brief
         *
         * @return bool
         */
        bool        Initialize();
        /**
         * @brief
         *
         * @return const char
         */
        const char* GetAHBotIncludes() const { return m_AHBotIncludes.c_str(); }
        /**
         * @brief
         *
         * @return const char
         */
        const char* GetAHBotExcludes() const { return m_AHBotExcludes.c_str(); }
        /**
         * @brief
         *
         * @return uint32 - AH Bot ID
         */
        uint32      GetAHBotId() const { return m_BotId; }

        /**
         * @brief
         *
         * @param index
         * @return uint32
         */
        uint32      getConfig(AuctionBotConfigUInt32Values index) const { return m_configUint32Values[index]; }
        /**
         * @brief
         *
         * @param index
         * @return bool
         */
        bool        getConfig(AuctionBotConfigBoolValues index) const { return m_configBoolValues[index]; }
        /**
         * @brief
         *
         * @param index
         * @param value
         */
        void        setConfig(AuctionBotConfigBoolValues index, bool value) { m_configBoolValues[index] = value; }
        /**
         * @brief
         *
         * @param index
         * @param value
         */
        void        setConfig(AuctionBotConfigUInt32Values index, uint32 value) { m_configUint32Values[index] = value; }


        /**
         * @brief Gets the ratio of items to sell for a given auctionhouse type
         *
         * @param houseType Type of the house.
         * @return uint32 a value between 0 and 10000 probably representing 0%-100%
         */
        uint32 getConfigItemAmountRatio(AuctionHouseType houseType) const;
        /**
         * @brief Gets if a buyer is enabled for the given auctionhouse type
         *
         * @param houseType Type of the house, ie: alliance/horde/neutral
         * @return bool true if a buyer is enabled, false otherwise
         */
        bool getConfigBuyerEnabled(AuctionHouseType houseType) const;
        /**
         * @brief Gets the ratio for the amount of items of a certain quality to be sold
         *
         * @param quality quality of the item you want to know the ratio for
         * @return uint32 probably a value between 0 and 10000 representing 0%-100% as the config values seem to be capped at this
         */
        uint32 getConfigItemQualityAmount(AuctionQuality quality) const;

        /**
         * @brief
         *
         * @return uint32
         */
        uint32      GetItemPerCycleBoost() const { return m_ItemsPerCycleBoost; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32      GetItemPerCycleNormal() const { return m_ItemsPerCycleNormal; }
        /**
         * @brief Reloads the AhBot config.
         *
         * @return bool true if the config was successfully reloaded, false otherwise
         */
        bool        Reload();

        /**
         * @brief Gets the name of the item class.
         *
         * @param itemclass class of the item you want to lookup name for
         * @return const char a string describing the name of the item class
         * \see ItemClass
         */
        static char const* GetItemClassName(ItemClass itemclass);

        /**
         * @brief Does the same thing as \ref AuctionBotConfig::GetItemClassName converts a enum
         * value to a readable string
         *
         * @param houseType the housetype you would like to "translate"
         * @return const char the string representation of the num value
         */
        static char const* GetHouseTypeName(AuctionHouseType houseType);

    private:
        std::string m_configFileName; /**< TODO */
        std::string m_AHBotIncludes; /**< TODO */
        std::string m_AHBotExcludes; /**< TODO */
        Config      m_AhBotCfg; /**< TODO */
        uint32      m_ItemsPerCycleBoost; /**< TODO */
        uint32      m_ItemsPerCycleNormal; /**< TODO */
        uint32      m_BotId;
        uint32 m_configUint32Values[CONFIG_UINT32_AHBOT_UINT32_COUNT]; /**< TODO */
        bool   m_configBoolValues[CONFIG_UINT32_AHBOT_BOOL_COUNT]; /**< TODO */

        /**
         * @brief
         *
         * @param AHBotIncludes
         */
        void SetAHBotIncludes(const std::string& AHBotIncludes) { m_AHBotIncludes = AHBotIncludes; }
        /**
         * @brief
         *
         * @param AHBotExcludes
         */
        void SetAHBotExcludes(const std::string& AHBotExcludes) { m_AHBotExcludes = AHBotExcludes; }
        /**
         * @brief
         *
         * @param AHBot Character Name
         */
        void SetAHBotId(const std::string& BotCharName);

        /**
         * @brief Sets a certain config value to the given default value
         *
         * @param index index to set
         * @param fieldname name of the field to set, ie: how it is represented in the config file
         * @param defvalue the default value for the field
         */
        void setConfig(AuctionBotConfigUInt32Values index, char const* fieldname, uint32 defvalue);
        /**
         * @brief Sets a certain config value to given default value and a max it can have that it will cap at
         *
         * @param index index to set
         * @param fieldname name of the field to set, ie: how it is represented in the config file
         * @param defvalue the default value for the field
         * @param maxvalue the maximum value this config can have
         */
        void setConfigMax(AuctionBotConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 maxvalue);
        /**
         * @brief Sets a certain config value to given default value and a max it can have that it will cap at
         *
         * @param index index to set
         * @param fieldname name of the field to set, ie: how it is represented in the config file
         * @param defvalue the default value for the field
         * @param minvalue the minimal value this config can have
         * @param maxvalue the maximum value this config can have
         */
        void setConfigMinMax(AuctionBotConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue, uint32 maxvalue);
        /**
         * @brief Sets a certain config value to the given default value
         *
         * @param index index to set
         * @param fieldname name of the field to set, ie: how it is represented in the config file
         * @param defvalue the default value for this field
         */
        void setConfig(AuctionBotConfigBoolValues index, char const* fieldname, bool defvalue);
        /**
         * @brief Retrieves the configuration for the \ref AuctionHouseBot from a configuration file.
         *
         */
        void GetConfigFromFile();
};

#define sAuctionBotConfig MaNGOS::Singleton<AuctionBotConfig>::Instance()

/**
 * @brief This is the base interface for the \ref AuctionBotSeller and \ref AuctionBotBuyer classes
 * which in itself only provides the possibility to use dynamic_cast in some of the
 * \ref AuctionHouseBot methods, ie: \ref AuctionHouseBot::SetItemsRatio uses it to cast it's
 * member AuctionHouseBot::m_Seller to a \ref AuctionBotSeller.
 *
 */
class AuctionBotAgent
{
    public:
        /**
         * @brief
         *
         */
        AuctionBotAgent() {}
        /**
         * @brief
         *
         */
        virtual ~AuctionBotAgent() {}
    public:
        /**
         * @brief Initializes this agent/bot and makes sure that there's anything to actually do for it.
         * If there's not it will return false and there's really no interest in keeping it for
         * the moment, otherwise returns true and has atleast one active house where it will do
         * business.
         *
         * @return bool true if we intialized with at least one auction house to do business in, false otherwise
         */
        virtual bool Initialize() = 0;

        /**
         * @brief This method updates what's going on on the AH for the bots, ie: if this is called for the
         * \ref AuctionBotBuyer it will place bids on items etc if there's a config file for it. If
         * the \ref AuctionBotSeller is called instead it would put up some new items if there's a
         * config file for it.
         *
         * @param houseType the house type we should work with while updating
         * @return bool true if any update was actually done, ie: we put some items up/bought some, false otherwise
         */
        virtual bool Update(AuctionHouseType houseType) = 0;
};

/**
 * @brief Structure used in \ref AuctionHouseBot::PrepareStatusInfos to show how many items there
 * are in each auction house and how many of each quality there are that were created by
 * one of the 2 agents \ref AuctionBotBuyer and \ref AuctionBotSeller
 * \see AuctionQuality
 *
 */
struct AuctionHouseBotStatusInfoPerType
{
    uint32 ItemsCount;                          /**< How many items there are totally in this AH */
    uint32 QualityInfo[MAX_AUCTION_QUALITY];    /**< How many items of each quality there are */
};

/**
 * @brief Used to get an array with all possible Action Houses, ie: neutral,ally,horde.
 *
 */
typedef AuctionHouseBotStatusInfoPerType AuctionHouseBotStatusInfo[MAX_AUCTION_HOUSE_TYPE];

/**
 * @brief This class handle both Selling and Buying method
 * (holder of AuctionBotBuyer and AuctionBotSeller objects)
 * (Taken from comments in source)
 *
 * \todo Better description here perhaps
 */
class AuctionHouseBot
{
    public:
        /**
         * @brief Initializes a new instance of the \ref AuctionHouseBot class.
         *
         */
        AuctionHouseBot();
        /**
         * @brief Finalizes an instance of the \ref AuctionHouseBot class.
         *
         */
        ~AuctionHouseBot();

        /**
         * @brief Updates the \ref AuctionHouseBot by checking if either the \ref AuctionBotSeller or
         * \ref AuctionBotBuyer wants to sell/buy anything and in that case lets one of them do
         * that and the other one will have to wait until the next call to \ref AuctionHouseBot::Update
         *
         */
        void Update();
        /**
         * @brief Initializes this instance.
         *
         */
        void Initialize();

        // Followed method is mainly used by level3.cpp for ingame/console command
        /**
         * @brief Sets the items ratio which probably decides how many items should
         * appear in each of the auction houses
         *
         * @param al The alliance house ratio
         * @param ho The horde house ratio
         * @param ne The neutral house ratio
         */
        void SetItemsRatio(uint32 al, uint32 ho, uint32 ne);
        /**
         * @brief Sets the items ratio for a specific house, like \ref AuctionHouseBot::SetItemsRatio but
         * only for one house.
         *
         * @param house The house
         * @param val The new ratio
         */
        void SetItemsRatioForHouse(AuctionHouseType house, uint32 val);
        /**
         * @brief Sets the items amount.
         *
         * @param (vals)[] The vals.
         */
        void SetItemsAmount(uint32(&vals) [MAX_AUCTION_QUALITY]);
        /**
         * @brief Changes the ratio for how often a certain quality of items should show up at the
         * AH. A specialised version of \ref AuctionHouseBot::SetItemsAmount
         *
         * @param quality quality of the items you want to change the ratio for
         * @param val the new ratio you want as a value between 0-10000 probably representing 0%-100%
         * \see AuctionQuality
         */
        void SetItemsAmountForQuality(AuctionQuality quality, uint32 val);
        /**
         * @brief Reloads all the configurations, for the AH bot and for both \ref AuctionBotBuyer and
         * \ref AuctionBotSeller and ourselves
         *
         * @return bool true if it went well, false otherwise
         */
        bool ReloadAllConfig();
        /**
         * @brief Expires all the items currently created by the AH bot and they'll be replaced later on
         * again. If parameter all is false only auctions without a bid are removed.
         *
         * @param all Whether to expire all auctions or only those without a bid
         */
        void Rebuild(bool all);

        /**
         * @brief Fills a status info structure with data about how many items of each there
         * currently are in the auction house that the auction bot has created
         *
         * @param statusInfo the structure to fill with data
         */
        void PrepareStatusInfos(AuctionHouseBotStatusInfo& statusInfo);
    private:
        /**
         * @brief Initializes the agents, ie: the \ref AuctionBotBuyer and \ref AuctionBotSeller
         *
         */
        void InitializeAgents();
        AuctionBotAgent* m_Buyer; /**< The buyer (\ref AuctionBotBuyer) for this \ref AuctionHouseBot */
        AuctionBotAgent* m_Seller; /**< The seller (\ref AuctionBotSeller) for this \ref AuctionHouseBot */

        uint32 m_OperationSelector; /**< 0..2*MAX_AUCTION_HOUSE_TYPE-1 */
};


///Convenience to easily access the singleton for the \ref AuctionHouseBot
#define sAuctionBot MaNGOS::Singleton<AuctionHouseBot>::Instance()

/** @} */

#endif
