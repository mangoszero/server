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

/**
 * @file AuctionHouseBotCommands.cpp
 * @brief The .ahbot commands: rebuild, reload, status, item amounts and ratios.
 */

#include "AuctionHouseBotCommands.h"
#include "AuctionHouseBot.h"
#include "Chat.h"
#include "Language.h"

namespace
{
    uint32 const ahbotQualityIds[MAX_AUCTION_QUALITY] =
    {
        LANG_AHBOT_QUALITY_GREY, LANG_AHBOT_QUALITY_WHITE,
        LANG_AHBOT_QUALITY_GREEN, LANG_AHBOT_QUALITY_BLUE,
        LANG_AHBOT_QUALITY_PURPLE, LANG_AHBOT_QUALITY_ORANGE,
        LANG_AHBOT_QUALITY_YELLOW
    };

    bool HandleRebuild(ChatHandler& handler, char* args)
    {
        bool all = false;
        if (*args)
        {
            if (!handler.ExtractLiteralArg(&args, "all"))
            {
                return false;
            }
            all = true;
        }

        sAuctionBot.Rebuild(all);
        return true;
    }

    bool HandleReload(ChatHandler& handler, char* /*args*/)
    {
        if (sAuctionBot.ReloadAllConfig())
        {
            handler.SendSysMessage(LANG_AHBOT_RELOAD_OK);
            return true;
        }

        handler.SendSysMessage(LANG_AHBOT_RELOAD_FAIL);
        handler.SetSentErrorMessage(true);
        return false;
    }

    bool HandleStatus(ChatHandler& handler, char* args)
    {
        bool all = false;
        if (*args)
        {
            if (!handler.ExtractLiteralArg(&args, "all"))
            {
                return false;
            }
            all = true;
        }

        AuctionHouseBotStatusInfo statusInfo;
        sAuctionBot.PrepareStatusInfos(statusInfo);

        bool const console = !handler.GetSession();
        if (console)
        {
            handler.SendSysMessage(LANG_AHBOT_STATUS_BAR_CONSOLE);
            handler.SendSysMessage(LANG_AHBOT_STATUS_TITLE1_CONSOLE);
            handler.SendSysMessage(LANG_AHBOT_STATUS_MIDBAR_CONSOLE);
        }
        else
        {
            handler.SendSysMessage(LANG_AHBOT_STATUS_TITLE1_CHAT);
        }

        int32 const fmtId = console ? LANG_AHBOT_STATUS_FORMAT_CONSOLE : LANG_AHBOT_STATUS_FORMAT_CHAT;

        handler.PSendSysMessage(fmtId, handler.GetMangosString(LANG_AHBOT_STATUS_ITEM_COUNT),
            statusInfo[AUCTION_HOUSE_ALLIANCE].ItemsCount,
            statusInfo[AUCTION_HOUSE_HORDE].ItemsCount,
            statusInfo[AUCTION_HOUSE_NEUTRAL].ItemsCount,
            statusInfo[AUCTION_HOUSE_ALLIANCE].ItemsCount +
            statusInfo[AUCTION_HOUSE_HORDE].ItemsCount +
            statusInfo[AUCTION_HOUSE_NEUTRAL].ItemsCount);

        if (all)
        {
            handler.PSendSysMessage(fmtId, handler.GetMangosString(LANG_AHBOT_STATUS_ITEM_RATIO),
                sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO),
                sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_HORDE_ITEM_AMOUNT_RATIO),
                sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO),
                sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_ALLIANCE_ITEM_AMOUNT_RATIO) +
                sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_HORDE_ITEM_AMOUNT_RATIO) +
                sAuctionBotConfig.getConfig(CONFIG_UINT32_AHBOT_NEUTRAL_ITEM_AMOUNT_RATIO));

            if (console)
            {
                handler.SendSysMessage(LANG_AHBOT_STATUS_BAR_CONSOLE);
                handler.SendSysMessage(LANG_AHBOT_STATUS_TITLE2_CONSOLE);
                handler.SendSysMessage(LANG_AHBOT_STATUS_MIDBAR_CONSOLE);
            }
            else
            {
                handler.SendSysMessage(LANG_AHBOT_STATUS_TITLE2_CHAT);
            }

            for (int i = 0; i < MAX_AUCTION_QUALITY; ++i)
            {
                handler.PSendSysMessage(fmtId, handler.GetMangosString(ahbotQualityIds[i]),
                    statusInfo[AUCTION_HOUSE_ALLIANCE].QualityInfo[i],
                    statusInfo[AUCTION_HOUSE_HORDE].QualityInfo[i],
                    statusInfo[AUCTION_HOUSE_NEUTRAL].QualityInfo[i],
                    sAuctionBotConfig.getConfigItemQualityAmount(AuctionQuality(i)));
            }
        }

        if (console)
        {
            handler.SendSysMessage(LANG_AHBOT_STATUS_BAR_CONSOLE);
        }

        return true;
    }

    bool HandleItemsAmount(ChatHandler& handler, char* args)
    {
        uint32 qVals[MAX_AUCTION_QUALITY];
        for (int i = 0; i < MAX_AUCTION_QUALITY; ++i)
        {
            if (!handler.ExtractUInt32(&args, qVals[i]))
            {
                return false;
            }
        }

        sAuctionBot.SetItemsAmount(qVals);

        for (int i = 0; i < MAX_AUCTION_QUALITY; ++i)
        {
            handler.PSendSysMessage(LANG_AHBOT_ITEMS_AMOUNT, handler.GetMangosString(ahbotQualityIds[i]), sAuctionBotConfig.getConfigItemQualityAmount(AuctionQuality(i)));
        }

        return true;
    }

    template<int Q>
    bool HandleItemsAmountQuality(ChatHandler& handler, char* args)
    {
        uint32 qVal;
        if (!handler.ExtractUInt32(&args, qVal))
        {
            return false;
        }
        sAuctionBot.SetItemsAmountForQuality(AuctionQuality(Q), qVal);
        handler.PSendSysMessage(LANG_AHBOT_ITEMS_AMOUNT, handler.GetMangosString(ahbotQualityIds[Q]),
            sAuctionBotConfig.getConfigItemQualityAmount(AuctionQuality(Q)));
        return true;
    }

    bool HandleItemsRatio(ChatHandler& handler, char* args)
    {
        uint32 rVal[MAX_AUCTION_HOUSE_TYPE];
        for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
        {
            if (!handler.ExtractUInt32(&args, rVal[i]))
            {
                return false;
            }
        }

        sAuctionBot.SetItemsRatio(rVal[0], rVal[1], rVal[2]);

        for (int i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
        {
            handler.PSendSysMessage(LANG_AHBOT_ITEMS_RATIO, AuctionBotConfig::GetHouseTypeName(AuctionHouseType(i)), sAuctionBotConfig.getConfigItemAmountRatio(AuctionHouseType(i)));
        }
        return true;
    }

    template<int H>
    bool HandleItemsRatioHouse(ChatHandler& handler, char* args)
    {
        uint32 rVal;
        if (!handler.ExtractUInt32(&args, rVal))
        {
            return false;
        }
        sAuctionBot.SetItemsRatioForHouse(AuctionHouseType(H), rVal);
        handler.PSendSysMessage(LANG_AHBOT_ITEMS_RATIO, AuctionBotConfig::GetHouseTypeName(AuctionHouseType(H)), sAuctionBotConfig.getConfigItemAmountRatio(AuctionHouseType(H)));
        return true;
    }
}

ChatCommand* AuctionHouseBotCommands()
{
    static ChatCommand itemsAmountTable[] =
    {
        ChatCommand("grey",     &HandleItemsAmountQuality<AUCTION_QUALITY_GREY>,   SEC_ADMINISTRATOR, true),
        ChatCommand("white",    &HandleItemsAmountQuality<AUCTION_QUALITY_WHITE>,  SEC_ADMINISTRATOR, true),
        ChatCommand("green",    &HandleItemsAmountQuality<AUCTION_QUALITY_GREEN>,  SEC_ADMINISTRATOR, true),
        ChatCommand("blue",     &HandleItemsAmountQuality<AUCTION_QUALITY_BLUE>,   SEC_ADMINISTRATOR, true),
        ChatCommand("purple",   &HandleItemsAmountQuality<AUCTION_QUALITY_PURPLE>, SEC_ADMINISTRATOR, true),
        ChatCommand("orange",   &HandleItemsAmountQuality<AUCTION_QUALITY_ORANGE>, SEC_ADMINISTRATOR, true),
        ChatCommand("yellow",   &HandleItemsAmountQuality<AUCTION_QUALITY_YELLOW>, SEC_ADMINISTRATOR, true),
        ChatCommand("",         &HandleItemsAmount,                                SEC_ADMINISTRATOR, true),
        ChatCommand(NULL,       0,                                                 true,  NULL, "", NULL)
    };

    static ChatCommand itemsRatioTable[] =
    {
        ChatCommand("alliance", &HandleItemsRatioHouse<AUCTION_HOUSE_ALLIANCE>,    SEC_ADMINISTRATOR, true),
        ChatCommand("horde",    &HandleItemsRatioHouse<AUCTION_HOUSE_HORDE>,       SEC_ADMINISTRATOR, true),
        ChatCommand("neutral",  &HandleItemsRatioHouse<AUCTION_HOUSE_NEUTRAL>,     SEC_ADMINISTRATOR, true),
        ChatCommand("",         &HandleItemsRatio,                                 SEC_ADMINISTRATOR, true),
        ChatCommand(NULL,       0,                                                 true,  NULL, "", NULL)
    };

    static ChatCommand itemsTable[] =
    {
        ChatCommand("amount",   SEC_ADMINISTRATOR, true, NULL, "", itemsAmountTable),
        ChatCommand("ratio",    SEC_ADMINISTRATOR, true, NULL, "", itemsRatioTable),
        ChatCommand(NULL,       0,                 true, NULL, "", NULL)
    };

    static ChatCommand ahbotTable[] =
    {
        ChatCommand("items",    SEC_ADMINISTRATOR, true, NULL, "", itemsTable),
        ChatCommand("rebuild",  &HandleRebuild,    SEC_ADMINISTRATOR, true),
        ChatCommand("reload",   &HandleReload,     SEC_ADMINISTRATOR, true),
        ChatCommand("status",   &HandleStatus,     SEC_ADMINISTRATOR, true),
        ChatCommand(NULL,       0,                 true, NULL, "", NULL)
    };

    static ChatCommand rootTable[] =
    {
        ChatCommand("ahbot",    SEC_ADMINISTRATOR, true, NULL, "", ahbotTable),
        ChatCommand(NULL,       0,                 false, NULL, "", NULL)
    };

    return rootTable;
}
