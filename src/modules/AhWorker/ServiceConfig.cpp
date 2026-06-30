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

#include "ServiceConfig.h"
#include "Log/Log.h"
#include "SystemConfig.h"

#include <cstring>

/// One day in minutes (mirror of the game-lib DAY / MINUTE used upstream).
#define AHBOT_RECHECK_INTERVAL_MAX 1440

ServiceConfig::ServiceConfig()
{
    memset(m_configUint32Values, 0, sizeof(m_configUint32Values));
    memset(m_configBoolValues, 0, sizeof(m_configBoolValues));
}

bool ServiceConfig::Initialize()
{
    // The ahbot.conf path comes from the already-loaded ah-service.conf.
    std::string cfgPath = sConfig.GetStringDefault(
        "AhBot.ConfigPath", AUCTIONHOUSEBOT_CONFIG_NAME);

    if (!m_botCfg.SetSource(cfgPath.c_str()))
    {
        // Try a bare "ahbot.conf" in the current folder as a fallback,
        // mirroring AuctionBotConfig::Initialize().
        if (!m_botCfg.SetSource(AUCTIONHOUSEBOT_CONFIG_NAME))
        {
            sLog.outError("ah-service: unable to open AH bot configuration"
                          " file '%s'", cfgPath.c_str());
            return false;
        }
    }
    else
    {
        sLog.outString("ah-service: AH bot using configuration file %s",
                       cfgPath.c_str());
    }

    GetConfigFromFile();
    return true;
}

bool ServiceConfig::Reload()
{
    // Re-open the same path that Initialize() used.
    std::string cfgPath = sConfig.GetStringDefault(
        "AhBot.ConfigPath", AUCTIONHOUSEBOT_CONFIG_NAME);

    if (!m_botCfg.SetSource(cfgPath.c_str()))
    {
        if (!m_botCfg.SetSource(AUCTIONHOUSEBOT_CONFIG_NAME))
        {
            sLog.outError("ah-service: Reload: unable to re-open AH bot"
                          " configuration file '%s'", cfgPath.c_str());
            return false;
        }
    }

    GetConfigFromFile();
    sLog.outString("ah-service: AH bot configuration reloaded from %s",
                   cfgPath.c_str());
    return true;
}

void ServiceConfig::setConfig(AhBotConfigUInt32Values index,
                              char const* fieldname, uint32 defvalue)
{
    setConfig(index, uint32(m_botCfg.GetIntDefault(fieldname, defvalue)));
    if (int32(getConfig(index)) < 0)
    {
        sLog.outError("AHBot: %s (%i) can't be negative. Using %u instead.",
                      fieldname, int32(getConfig(index)), defvalue);
        setConfig(index, defvalue);
    }
}

void ServiceConfig::setConfigMax(AhBotConfigUInt32Values index,
                                 char const* fieldname, uint32 defvalue,
                                 uint32 maxvalue)
{
    setConfig(index, uint32(m_botCfg.GetIntDefault(fieldname, defvalue)));
    if (getConfig(index) > maxvalue)
    {
        sLog.outError("AHBot: %s (%u) must be in range 0...%u."
                      " Using %u instead.",
                      fieldname, getConfig(index), maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
}

void ServiceConfig::setConfigMinMax(AhBotConfigUInt32Values index,
                                    char const* fieldname, uint32 defvalue,
                                    uint32 minvalue, uint32 maxvalue)
{
    setConfig(index, uint32(m_botCfg.GetIntDefault(fieldname, defvalue)));
    if (getConfig(index) > maxvalue)
    {
        sLog.outError("AHBot: %s (%u) must be in range %u...%u."
                      " Using %u instead.", fieldname, getConfig(index),
                      minvalue, maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
    if (getConfig(index) < minvalue)
    {
        sLog.outError("AHBot: %s (%u) must be in range %u...%u."
                      " Using %u instead.", fieldname, getConfig(index),
                      minvalue, maxvalue, minvalue);
        setConfig(index, minvalue);
    }
}

void ServiceConfig::setConfig(AhBotConfigBoolValues index,
                              char const* fieldname, bool defvalue)
{
    setConfig(index, m_botCfg.GetBoolDefault(fieldname, defvalue));
}

void ServiceConfig::GetConfigFromFile()
{
    // Check config file version.
    if (m_botCfg.GetIntDefault("ConfVersion", 0) != AHBOT_CONFIG_VERSION)
    {
        sLog.outError("AHBot: Configuration file version doesn't match"
                      " expected version. Some config variables may be wrong"
                      " or missing.");
    }

    // The ah-service infra config (sConfig / ah-service.conf) shares the
    // AHBot conf version; warn if it is stale too.
    if (sConfig.GetIntDefault("ConfVersion", 0) != AHBOT_CONFIG_VERSION)
    {
        sLog.outError("AH-service: ah-service.conf version doesn't match the"
                      " expected version. Some config variables may be wrong"
                      " or missing.");
    }

    setConfigMax(AHBOT_CONFIG_UINT32_ALLIANCE_ITEM_AMOUNT_RATIO,
                 "AuctionHouseBot.Alliance.Items.Amount.Ratio", 100, 10000);
    setConfigMax(AHBOT_CONFIG_UINT32_HORDE_ITEM_AMOUNT_RATIO,
                 "AuctionHouseBot.Horde.Items.Amount.Ratio", 100, 10000);
    setConfigMax(AHBOT_CONFIG_UINT32_NEUTRAL_ITEM_AMOUNT_RATIO,
                 "AuctionHouseBot.Neutral.Items.Amount.Ratio", 100, 10000);

    m_AHBotIncludes =
        m_botCfg.GetStringDefault("AuctionHouseBot.forceIncludeItems", "");
    m_AHBotExcludes =
        m_botCfg.GetStringDefault("AuctionHouseBot.forceExcludeItems", "");

    // GUID resolution is deferred (no sObjectMgr in the child); surface the
    // configured name verbatim for Task 8b/8c handshake resolution.
    m_botCharacterName =
        m_botCfg.GetStringDefault("AuctionHouseBot.CharacterName", "");

    setConfig(AHBOT_CONFIG_BOOL_BUYER_ALLIANCE_ENABLED,
              "AuctionHouseBot.Buyer.Alliance.Enabled", false);
    setConfig(AHBOT_CONFIG_BOOL_BUYER_HORDE_ENABLED,
              "AuctionHouseBot.Buyer.Horde.Enabled", false);
    setConfig(AHBOT_CONFIG_BOOL_BUYER_NEUTRAL_ENABLED,
              "AuctionHouseBot.Buyer.Neutral.Enabled", false);

    setConfig(AHBOT_CONFIG_BOOL_ITEMS_VENDOR,
              "AuctionHouseBot.Items.Vendor", false);
    setConfig(AHBOT_CONFIG_BOOL_ITEMS_LOOT,
              "AuctionHouseBot.Items.Loot", true);
    setConfig(AHBOT_CONFIG_BOOL_ITEMS_MISC,
              "AuctionHouseBot.Items.Misc", false);

    setConfig(AHBOT_CONFIG_BOOL_BIND_NO,
              "AuctionHouseBot.Bind.No", true);
    setConfig(AHBOT_CONFIG_BOOL_BIND_PICKUP,
              "AuctionHouseBot.Bind.Pickup", false);
    setConfig(AHBOT_CONFIG_BOOL_BIND_EQUIP,
              "AuctionHouseBot.Bind.Equip", true);
    setConfig(AHBOT_CONFIG_BOOL_BIND_USE,
              "AuctionHouseBot.Bind.Use", true);
    setConfig(AHBOT_CONFIG_BOOL_BIND_QUEST,
              "AuctionHouseBot.Bind.Quest", false);
    setConfig(AHBOT_CONFIG_BOOL_LOCKBOX_ENABLED,
              "AuctionHouseBot.LockBox.Enabled", false);

    setConfig(AHBOT_CONFIG_BOOL_BUYPRICE_SELLER,
              "AuctionHouseBot.BuyPrice.Seller", true);

    setConfig(AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_BOOST,
              "AuctionHouseBot.ItemsPerCycle.Boost", 75);
    setConfig(AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_NORMAL,
              "AuctionHouseBot.ItemsPerCycle.Normal", 20);

    setConfig(AHBOT_CONFIG_UINT32_ITEM_MIN_ITEM_LEVEL,
              "AuctionHouseBot.Items.ItemLevel.Min", 0);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_MAX_ITEM_LEVEL,
              "AuctionHouseBot.Items.ItemLevel.Max", 0);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_MIN_REQ_LEVEL,
              "AuctionHouseBot.Items.ReqLevel.Min", 0);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_MAX_REQ_LEVEL,
              "AuctionHouseBot.Items.ReqLevel.Max", 0);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_MIN_SKILL_RANK,
              "AuctionHouseBot.Items.ReqSkill.Min", 0);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_MAX_SKILL_RANK,
              "AuctionHouseBot.Items.ReqSkill.Max", 0);

    setConfig(AHBOT_CONFIG_UINT32_ITEM_GREY_AMOUNT,
              "AuctionHouseBot.Items.Amount.Grey", 0);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_WHITE_AMOUNT,
              "AuctionHouseBot.Items.Amount.White", 2000);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_GREEN_AMOUNT,
              "AuctionHouseBot.Items.Amount.Green", 2500);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_BLUE_AMOUNT,
              "AuctionHouseBot.Items.Amount.Blue", 1500);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_PURPLE_AMOUNT,
              "AuctionHouseBot.Items.Amount.Purple", 500);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_ORANGE_AMOUNT,
              "AuctionHouseBot.Items.Amount.Orange", 0);
    setConfig(AHBOT_CONFIG_UINT32_ITEM_YELLOW_AMOUNT,
              "AuctionHouseBot.Items.Amount.Yellow", 0);

    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_CONSUMABLE_AMOUNT,
                 "AuctionHouseBot.Class.Consumable", 6, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_CONTAINER_AMOUNT,
                 "AuctionHouseBot.Class.Container", 4, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_WEAPON_AMOUNT,
                 "AuctionHouseBot.Class.Weapon", 8, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_ARMOR_AMOUNT,
                 "AuctionHouseBot.Class.Armor", 8, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_REAGENT_AMOUNT,
                 "AuctionHouseBot.Class.Reagent", 1, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_PROJECTILE_AMOUNT,
                 "AuctionHouseBot.Class.Projectile", 2, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_AMOUNT,
                 "AuctionHouseBot.Class.TradeGood", 10, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_RECIPE_AMOUNT,
                 "AuctionHouseBot.Class.Recipe", 6, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_QUIVER_AMOUNT,
                 "AuctionHouseBot.Class.Quiver", 1, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_QUEST_AMOUNT,
                 "AuctionHouseBot.Class.Quest", 1, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_KEY_AMOUNT,
                 "AuctionHouseBot.Class.Key", 1, 10);
    setConfigMax(AHBOT_CONFIG_UINT32_CLASS_MISC_AMOUNT,
                 "AuctionHouseBot.Class.Misc", 5, 10);
    // CLASSIC build (Zero): the gem/generic class amounts live behind
    // #if !defined(CLASSIC) upstream and are not read here.

    setConfig(AHBOT_CONFIG_UINT32_ALLIANCE_PRICE_RATIO,
              "AuctionHouseBot.Alliance.Price.Ratio", 200);
    setConfig(AHBOT_CONFIG_UINT32_HORDE_PRICE_RATIO,
              "AuctionHouseBot.Horde.Price.Ratio", 200);
    setConfig(AHBOT_CONFIG_UINT32_NEUTRAL_PRICE_RATIO,
              "AuctionHouseBot.Neutral.Price.Ratio", 200);

    setConfig(AHBOT_CONFIG_UINT32_MINTIME,
              "AuctionHouseBot.MinTime", 1);
    setConfig(AHBOT_CONFIG_UINT32_MAXTIME,
              "AuctionHouseBot.MaxTime", 72);

    setConfigMinMax(AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_ALLIANCE,
                    "AuctionHouseBot.Buyer.Alliance.Chance.Ratio", 3, 1, 100);
    setConfigMinMax(AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_HORDE,
                    "AuctionHouseBot.Buyer.Horde.Chance.Ratio", 3, 1, 100);
    setConfigMinMax(AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_NEUTRAL,
                    "AuctionHouseBot.Buyer.Neutral.Chance.Ratio", 3, 1, 100);
    setConfigMinMax(AHBOT_CONFIG_UINT32_BUYER_RECHECK_INTERVAL,
                    "AuctionHouseBot.Buyer.Recheck.Interval", 20, 1,
                    AHBOT_RECHECK_INTERVAL_MAX);

    setConfig(AHBOT_CONFIG_BOOL_DEBUG_SELLER,
              "AuctionHouseBot.DEBUG.Seller", false);
    setConfig(AHBOT_CONFIG_BOOL_DEBUG_BUYER,
              "AuctionHouseBot.DEBUG.Buyer", false);
    setConfig(AHBOT_CONFIG_BOOL_SELLER_ENABLED,
              "AuctionHouseBot.Seller.Enabled", false);
    setConfig(AHBOT_CONFIG_BOOL_BUYER_ENABLED,
              "AuctionHouseBot.Buyer.Enabled", false);
    setConfig(AHBOT_CONFIG_BOOL_BUYPRICE_BUYER,
              "AuctionHouseBot.Buyer.BuyPrice", false);

    setConfig(AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MIN_REQ_LEVEL,
              "AuctionHouseBot.Class.Misc.Mount.ReqLevel.Min", 0);
    setConfig(AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MAX_REQ_LEVEL,
              "AuctionHouseBot.Class.Misc.Mount.ReqLevel.Max", 0);
    setConfig(AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MIN_SKILL_RANK,
              "AuctionHouseBot.Class.Misc.Mount.ReqSkill.Min", 0);
    setConfig(AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MAX_SKILL_RANK,
              "AuctionHouseBot.Class.Misc.Mount.ReqSkill.Max", 0);
    setConfig(AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_MIN_ITEM_LEVEL,
              "AuctionHouseBot.Class.TradeGood.ItemLevel.Min", 0);
    setConfig(AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_MAX_ITEM_LEVEL,
              "AuctionHouseBot.Class.TradeGood.ItemLevel.Max", 0);
    setConfig(AHBOT_CONFIG_UINT32_CLASS_CONTAINER_MIN_ITEM_LEVEL,
              "AuctionHouseBot.Class.Container.ItemLevel.Min", 0);
    setConfig(AHBOT_CONFIG_UINT32_CLASS_CONTAINER_MAX_ITEM_LEVEL,
              "AuctionHouseBot.Class.Container.ItemLevel.Max", 0);
}
