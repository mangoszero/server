/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

 /*
 *   AHBot related commands
 */

#include "Chat.h"
#include "Language.h"
#include "AuctionHouseBot/AuctionHouseBot.h"


 /**********************************************************************
     Useful constants definition
 /***********************************************************************/

static uint32 ahbotQualityIds[MAX_AUCTION_QUALITY] =
{
    LANG_AHBOT_QUALITY_GREY, LANG_AHBOT_QUALITY_WHITE,
    LANG_AHBOT_QUALITY_GREEN, LANG_AHBOT_QUALITY_BLUE,
    LANG_AHBOT_QUALITY_PURPLE, LANG_AHBOT_QUALITY_ORANGE,
    LANG_AHBOT_QUALITY_YELLOW
};

 /**********************************************************************
     CommandTable : ahbotCommandTable
 /***********************************************************************/

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

/**********************************************************************
    CommandTable : ahbotItemsAmountCommandTable
/***********************************************************************/

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

template<int Q>
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

/**********************************************************************
    CommandTable : ahbotItemsRatioCommandTable
/***********************************************************************/

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

template<int H>
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