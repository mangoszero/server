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

#ifndef AH_SERVICE_SERVICE_CONFIG_H
#define AH_SERVICE_SERVICE_CONFIG_H

#include "Common.h"
#include "Config/Config.h"
#include "AhBotDefines.h"

#include <string>

/**
 * @file ServiceConfig.h
 * @brief Child-owned port of the game-lib @c AuctionBotConfig value table.
 *
 * This is a faithful copy of the bot's tuning config (src/game/AuctionHouseBot/
 * AuctionHouseBot.{h,cpp}). The full uint32/bool value table is ported now
 * because Task 8c reuses it; only the GUID-resolution step is deferred (the
 * child cannot reach @c sObjectMgr, so the bot character name is surfaced
 * verbatim and the resolved GUID arrives later from mangosd over IPC).
 *
 * The value table is loaded from a child-owned local @c Config instance pointed
 * at @c ahbot.conf, exactly mirroring the source's @c m_AhBotCfg member.
 */

/**
 * @brief uint32 configuration values (mirror of AuctionBotConfigUInt32Values).
 *
 * CLASSIC build: gem/generic/mount entries are present in the enum (so the
 * table layout matches upstream) but their corresponding loader lines stay
 * inside @c \#if !defined(CLASSIC) and are not read on Zero.
 */
enum AhBotConfigUInt32Values
{
    AHBOT_CONFIG_UINT32_MAXTIME,
    AHBOT_CONFIG_UINT32_MINTIME,
    AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_BOOST,
    AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_NORMAL,
    AHBOT_CONFIG_UINT32_ALLIANCE_ITEM_AMOUNT_RATIO,
    AHBOT_CONFIG_UINT32_HORDE_ITEM_AMOUNT_RATIO,
    AHBOT_CONFIG_UINT32_NEUTRAL_ITEM_AMOUNT_RATIO,
    AHBOT_CONFIG_UINT32_ITEM_MIN_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_ITEM_MAX_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_ITEM_MIN_REQ_LEVEL,
    AHBOT_CONFIG_UINT32_ITEM_MAX_REQ_LEVEL,
    AHBOT_CONFIG_UINT32_ITEM_MIN_SKILL_RANK,
    AHBOT_CONFIG_UINT32_ITEM_MAX_SKILL_RANK,
    AHBOT_CONFIG_UINT32_ITEM_GREY_AMOUNT,
    AHBOT_CONFIG_UINT32_ITEM_WHITE_AMOUNT,
    AHBOT_CONFIG_UINT32_ITEM_GREEN_AMOUNT,
    AHBOT_CONFIG_UINT32_ITEM_BLUE_AMOUNT,
    AHBOT_CONFIG_UINT32_ITEM_PURPLE_AMOUNT,
    AHBOT_CONFIG_UINT32_ITEM_ORANGE_AMOUNT,
    AHBOT_CONFIG_UINT32_ITEM_YELLOW_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_CONSUMABLE_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_CONTAINER_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_WEAPON_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_GEM_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_ARMOR_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_REAGENT_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_PROJECTILE_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_GENERIC_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_RECIPE_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_QUIVER_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_QUEST_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_KEY_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_MISC_AMOUNT,
    AHBOT_CONFIG_UINT32_CLASS_GLYPH_AMOUNT,
    AHBOT_CONFIG_UINT32_ALLIANCE_PRICE_RATIO,
    AHBOT_CONFIG_UINT32_HORDE_PRICE_RATIO,
    AHBOT_CONFIG_UINT32_NEUTRAL_PRICE_RATIO,
    AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_ALLIANCE,
    AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_HORDE,
    AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_NEUTRAL,
    AHBOT_CONFIG_UINT32_BUYER_RECHECK_INTERVAL,
    AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MIN_REQ_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MAX_REQ_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MIN_SKILL_RANK,
    AHBOT_CONFIG_UINT32_CLASS_MISC_MOUNT_MAX_SKILL_RANK,
    AHBOT_CONFIG_UINT32_CLASS_GLYPH_MIN_REQ_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_GLYPH_MAX_REQ_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_GLYPH_MIN_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_GLYPH_MAX_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_MIN_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_MAX_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_CONTAINER_MIN_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_CLASS_CONTAINER_MAX_ITEM_LEVEL,
    AHBOT_CONFIG_UINT32_COUNT
};

/**
 * @brief bool configuration values (mirror of AuctionBotConfigBoolValues).
 */
enum AhBotConfigBoolValues
{
    AHBOT_CONFIG_BOOL_BUYER_ALLIANCE_ENABLED,
    AHBOT_CONFIG_BOOL_BUYER_HORDE_ENABLED,
    AHBOT_CONFIG_BOOL_BUYER_NEUTRAL_ENABLED,
    AHBOT_CONFIG_BOOL_ITEMS_VENDOR,
    AHBOT_CONFIG_BOOL_ITEMS_LOOT,
    AHBOT_CONFIG_BOOL_ITEMS_MISC,
    AHBOT_CONFIG_BOOL_BIND_NO,
    AHBOT_CONFIG_BOOL_BIND_PICKUP,
    AHBOT_CONFIG_BOOL_BIND_EQUIP,
    AHBOT_CONFIG_BOOL_BIND_USE,
    AHBOT_CONFIG_BOOL_BIND_QUEST,
    AHBOT_CONFIG_BOOL_BUYPRICE_SELLER,
    AHBOT_CONFIG_BOOL_BUYPRICE_BUYER,
    AHBOT_CONFIG_BOOL_DEBUG_SELLER,
    AHBOT_CONFIG_BOOL_DEBUG_BUYER,
    AHBOT_CONFIG_BOOL_SELLER_ENABLED,
    AHBOT_CONFIG_BOOL_BUYER_ENABLED,
    AHBOT_CONFIG_BOOL_LOCKBOX_ENABLED,
    AHBOT_CONFIG_BOOL_COUNT
};

/**
 * @brief Child-owned port of the game-lib AuctionBotConfig value table.
 *
 * Loads @c ahbot.conf into a private @c Config and exposes the same getConfig()
 * accessor pair the in-process bot uses, so Task 8c can drive identical
 * decisions from inside the child process.
 */
class ServiceConfig
{
    public:
        /**
         * @brief Construct with all values zero-initialized.
         */
        ServiceConfig();

        /**
         * @brief Load the bot config from @c ahbot.conf.
         *
         * The @c ahbot.conf path is read from @c sConfig (the already-loaded
         * @c ah-service.conf) via the @c AhBot.ConfigPath key.
         *
         * @return true on success, false if the config file could not be
         *         opened.
         */
        bool Initialize();

        /**
         * @brief Get a uint32 configuration value.
         * @param index Configuration index
         * @return Configuration value
         */
        uint32 getConfig(AhBotConfigUInt32Values index) const
        {
            return m_configUint32Values[index];
        }

        /**
         * @brief Get a bool configuration value.
         * @param index Configuration index
         * @return Configuration value
         */
        bool getConfig(AhBotConfigBoolValues index) const
        {
            return m_configBoolValues[index];
        }

        /**
         * @brief Set a bool configuration value.
         * @param index Configuration index
         * @param value Value to set
         */
        void setConfig(AhBotConfigBoolValues index, bool value)
        {
            m_configBoolValues[index] = value;
        }

        /**
         * @brief Set a uint32 configuration value.
         * @param index Configuration index
         * @param value Value to set
         */
        void setConfig(AhBotConfigUInt32Values index, uint32 value)
        {
            m_configUint32Values[index] = value;
        }

        /**
         * @brief Get the forced-include item list (comma-separated item IDs).
         */
        const char* GetAHBotIncludes() const
        {
            return m_AHBotIncludes.c_str();
        }

        /**
         * @brief Get the forced-exclude item list (comma-separated item IDs).
         */
        const char* GetAHBotExcludes() const
        {
            return m_AHBotExcludes.c_str();
        }

        /**
         * @brief Get the configured bot character name (verbatim).
         *
         * GUID resolution is deferred: the child cannot call @c sObjectMgr, so
         * Task 8b/8c receive the resolved low GUID from mangosd. For now this
         * just surfaces the configured name.
         */
        const std::string& GetBotCharacterName() const
        {
            return m_botCharacterName;
        }

    private:
        Config m_botCfg;                ///< Backing store for ahbot.conf
        std::string m_AHBotIncludes;    ///< Comma-separated forced-include IDs
        std::string m_AHBotExcludes;    ///< Comma-separated forced-exclude IDs
        std::string m_botCharacterName; ///< Configured bot character name
        uint32 m_configUint32Values[AHBOT_CONFIG_UINT32_COUNT]; ///< uint32 vals
        bool m_configBoolValues[AHBOT_CONFIG_BOOL_COUNT];       ///< bool vals

        /**
         * @brief Load all values from the open @c m_botCfg.
         */
        void GetConfigFromFile();

        /// @brief Set a uint32 value with a default (negative clamped).
        void setConfig(AhBotConfigUInt32Values index, char const* fieldname,
                       uint32 defvalue);
        /// @brief Set a uint32 value with a default and a maximum cap.
        void setConfigMax(AhBotConfigUInt32Values index, char const* fieldname,
                          uint32 defvalue, uint32 maxvalue);
        /// @brief Set a uint32 value with a default, minimum and maximum.
        void setConfigMinMax(AhBotConfigUInt32Values index,
                             char const* fieldname, uint32 defvalue,
                             uint32 minvalue, uint32 maxvalue);
        /// @brief Set a bool value with a default.
        void setConfig(AhBotConfigBoolValues index, char const* fieldname,
                       bool defvalue);
};

#endif // AH_SERVICE_SERVICE_CONFIG_H
