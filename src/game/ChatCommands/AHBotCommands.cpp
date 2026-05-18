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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file AHBotCommands.cpp
 * @brief Implementation of auction house bot management chat commands.
 *
 * This file contains chat command handlers for configuring and managing the
 * automated auction house bot system, including:
 * - Bot rebuild and reload operations
 * - Status information display
 * - Item amount configuration by quality
 * - Item distribution ratio settings
 */

#include "Chat.h"
#include "AuctionHouseBot/AuctionHouseBot.h"


 /**********************************************************************
     Useful constants definition
  **********************************************************************/

static uint32 ahbotQualityIds[MAX_AUCTION_QUALITY] =
{
    LANG_AHBOT_QUALITY_GREY, LANG_AHBOT_QUALITY_WHITE,
    LANG_AHBOT_QUALITY_GREEN, LANG_AHBOT_QUALITY_BLUE,
    LANG_AHBOT_QUALITY_PURPLE, LANG_AHBOT_QUALITY_ORANGE,
    LANG_AHBOT_QUALITY_YELLOW
};

 /**
 * @brief Rebuilds the auction house bot's inventory.
 *
 * @param args Command arguments: optional "all" flag to rebuild all auction houses.
 * @returns True if the rebuild was initiated successfully, false otherwise.
 */
bool ChatHandler::HandleAHBotRebuildCommand(char* args)
{
    bool all = false;
    if (*args)
    {
        if (!ExtractLiteralArg(&args, "all"))
        {
            return false;
        }
        all = true;
    }

    sAuctionBot.Rebuild(all);
    return true;
}

/**
 * @brief Reloads the auction house bot configuration.
 *
 * @param args Command arguments (not used).
 * @returns True if the reload was successful, false otherwise.
 */
bool ChatHandler::HandleAHBotReloadCommand(char* /*args*/)
{
    if (sAuctionBot.ReloadAllConfig())
    {
        SendSysMessage(LANG_AHBOT_RELOAD_OK);
        return true;
    }
    else
    {
        SendSysMessage(LANG_AHBOT_RELOAD_FAIL);
        SetSentErrorMessage(true);
        return false;
    }
}

/**
 * @brief Displays the status of the auction house bot.
 *
 * @param args Command arguments: optional "all" flag for detailed status information.
 * @returns True if the status was displayed successfully, false otherwise.
 */
bool ChatHandler::HandleAHBotStatusCommand(char* args)
{
    bool all = false;
    if (*args)
    {
        if (!ExtractLiteralArg(&args, "all"))
        {
            return false;
        }
        all = true;
    }

    AuctionHouseBotStatusInfo statusInfo;
    sAuctionBot.PrepareStatusInfos(statusInfo);

    if (!m_session)
    {
        SendSysMessage(LANG_AHBOT_STATUS_BAR_CONSOLE);
        SendSysMessage(LANG_AHBOT_STATUS_TITLE1_CONSOLE);
        SendSysMessage(LANG_AHBOT_STATUS_MIDBAR_CONSOLE);
    }
    else
    {
        SendSysMessage(LANG_AHBOT_STATUS_TITLE1_CHAT);
    }

    uint32 fmtId = m_session ? LANG_AHBOT_STATUS_FORMAT_CHAT : LANG_AHBOT_STATUS_FORMAT_CONSOLE;

    PSendSysMessage(fmtId, GetMangosString(LANG_AHBOT_STATUS_ITEM_COUNT),
                    statusInfo[AUCTION_HOUSE_ALLIANCE].ItemsCount,
                    statusInfo[AUCTION_HOUSE_HORDE].ItemsCount,
                    statusInfo[AUCTION_HOUSE_NEUTRAL].ItemsCount,
                    statusInfo[AUCTION_HOUSE_ALLIANCE].ItemsCount +
                    statusInfo[AUCTION_HOUSE_HORDE].ItemsCount +
                    statusInfo[AUCTION_HOUSE_NEUTRAL].ItemsCount);

    if (all)
    {
        PSendSysMessage(fmtId, GetMangosString(LANG_AHBOT_STATUS_ITEM_RATIO),
                        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO),
                        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_HORDE_ITEM_AMOUNT_RATIO),
                        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO),
                        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO) +
                        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_HORDE_ITEM_AMOUNT_RATIO) +
                        sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO));

        if (!m_session)
        {
            SendSysMessage(LANG_AHBOT_STATUS_BAR_CONSOLE);
            SendSysMessage(LANG_AHBOT_STATUS_TITLE2_CONSOLE);
            SendSysMessage(LANG_AHBOT_STATUS_MIDBAR_CONSOLE);
        }
        else
        {
            SendSysMessage(LANG_AHBOT_STATUS_TITLE2_CHAT);
        }

        for (int i = 0; i < MAX_AUCTION_QUALITY; ++i)
            PSendSysMessage(fmtId, GetMangosString(ahbotQualityIds[i]),
                            statusInfo[AUCTION_HOUSE_ALLIANCE].QualityInfo[i],
                            statusInfo[AUCTION_HOUSE_HORDE].QualityInfo[i],
                            statusInfo[AUCTION_HOUSE_NEUTRAL].QualityInfo[i],
                            sAuctionBotConfig.getConfigItemQualityAmount(AuctionQuality(i)));
    }

    if (!m_session)
    {
        SendSysMessage(LANG_AHBOT_STATUS_BAR_CONSOLE);
    }

    return true;
}

/**
 * @brief Sets the item amounts for all auction quality levels.
 *
 * @param args Command arguments: seven values for each quality level (grey, white, green, blue, purple, orange, yellow).
 * @returns True if the items amount was set successfully, false otherwise.
 */
bool ChatHandler::HandleAHBotItemsAmountCommand(char* args)
{
    uint32 qVals[MAX_AUCTION_QUALITY];
    for (int i = 0; i < MAX_AUCTION_QUALITY; ++i)
        if (!ExtractUInt32(&args, qVals[i]))
        {
            return false;
        }

    sAuctionBot.SetItemsAmount(qVals);

    for (int i = 0; i < MAX_AUCTION_QUALITY; ++i)
    {
        PSendSysMessage(LANG_AHBOT_ITEMS_AMOUNT, GetMangosString(ahbotQualityIds[i]), sAuctionBotConfig.getConfigItemQualityAmount(AuctionQuality(i)));
    }

    return true;
}

/**
 * @brief Template function to set item amount for a specific quality level.
 *
 * @tparam Q The auction quality level to configure.
 * @param args Command arguments: value for the specified quality level.
 * @returns True if the quality item amount was set successfully, false otherwise.
 */
template<int Q>

/**
 * @brief Handler for HandleAHBotItemsAmountQualityCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleAHBotItemsAmountQualityCommand(char* args)
{
    uint32 qVal;
    if (!ExtractUInt32(&args, qVal))
    {
        return false;
    }
    sAuctionBot.SetItemsAmountForQuality(AuctionQuality(Q), qVal);
    PSendSysMessage(LANG_AHBOT_ITEMS_AMOUNT, GetMangosString(ahbotQualityIds[Q]),
                    sAuctionBotConfig.getConfigItemQualityAmount(AuctionQuality(Q)));
    return true;
}

template bool ChatHandler::HandleAHBotItemsAmountQualityCommand<AUCTION_QUALITY_GREY>(char*);
template bool ChatHandler::HandleAHBotItemsAmountQualityCommand<AUCTION_QUALITY_WHITE>(char*);
template bool ChatHandler::HandleAHBotItemsAmountQualityCommand<AUCTION_QUALITY_GREEN>(char*);
template bool ChatHandler::HandleAHBotItemsAmountQualityCommand<AUCTION_QUALITY_BLUE>(char*);
template bool ChatHandler::HandleAHBotItemsAmountQualityCommand<AUCTION_QUALITY_PURPLE>(char*);
template bool ChatHandler::HandleAHBotItemsAmountQualityCommand<AUCTION_QUALITY_ORANGE>(char*);
template bool ChatHandler::HandleAHBotItemsAmountQualityCommand<AUCTION_QUALITY_YELLOW>(char*);

/**
 * @brief Sets the item distribution ratios for all auction houses.
 *
 * @param args Command arguments: three ratio values for alliance, horde, and neutral auction houses.
 * @returns True if the item ratios were set successfully, false otherwise.
 */
bool ChatHandler::HandleAHBotItemsRatioCommand(char* args)
{
    uint32 rVal[MAX_AUCTION_HOUSE_TYPE];
    for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
        if (!ExtractUInt32(&args, rVal[i]))
        {
            return false;
        }

    sAuctionBot.SetItemsRatio(rVal[0], rVal[1], rVal[2]);

    for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        PSendSysMessage(LANG_AHBOT_ITEMS_RATIO, AuctionBotConfig::GetHouseTypeName(AuctionHouseType(i)), sAuctionBotConfig.getConfigItemAmountRatio(AuctionHouseType(i)));
    }
    return true;
}

/**
 * @brief Template function to set item ratio for a specific auction house.
 *
 * @tparam H The auction house type to configure (alliance, horde, or neutral).
 * @param args Command arguments: ratio value for the specified auction house.
 * @returns True if the house item ratio was set successfully, false otherwise.
 */
template<int H>

/**
 * @brief Handler for HandleAHBotItemsRatioHouseCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleAHBotItemsRatioHouseCommand(char* args)
{
    uint32 rVal;
    if (!ExtractUInt32(&args, rVal))
    {
        return false;
    }
    sAuctionBot.SetItemsRatioForHouse(AuctionHouseType(H), rVal);
    PSendSysMessage(LANG_AHBOT_ITEMS_RATIO, AuctionBotConfig::GetHouseTypeName(AuctionHouseType(H)), sAuctionBotConfig.getConfigItemAmountRatio(AuctionHouseType(H)));
    return true;
}

template bool ChatHandler::HandleAHBotItemsRatioHouseCommand<AUCTION_HOUSE_ALLIANCE>(char*);
template bool ChatHandler::HandleAHBotItemsRatioHouseCommand<AUCTION_HOUSE_HORDE>(char*);
template bool ChatHandler::HandleAHBotItemsRatioHouseCommand<AUCTION_HOUSE_NEUTRAL>(char*);