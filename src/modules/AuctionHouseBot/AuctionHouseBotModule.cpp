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
 * @file AuctionHouseBotModule.cpp
 * @brief The in-process auction house bot as an auction module.
 */

#include "AuctionHouseBot.h"
#include "AuctionHouseBotCommands.h"
#include "AuctionHouseModule.h"
#include "Log.h"
#include "Timer.h"

namespace
{
    uint32 const BOT_UPDATE_INTERVAL = 20 * IN_MILLISECONDS;

    class AuctionHouseBotModule : public AuctionHouseModule
    {
        public:

            AuctionHouseBotModule() : m_standingDown(false)
            {
                m_updateTimer.SetInterval(BOT_UPDATE_INTERVAL);
            }

            char const* Name() const override { return "AuctionHouseBot"; }

            void Start() override
            {
                if (AuctionHouseModules::OwnsAuctionRows())
                {
                    sLog.outString("AuctionHouseBot: another auction module owns the auction table; the bot stays idle");
                    return;
                }

                sLog.outString("Initialize AuctionHouseBot...");
                sAuctionBot.Initialize();
            }

            void Update(uint32 diff) override
            {
                m_updateTimer.Update(diff);
                if (!m_updateTimer.Passed())
                {
                    return;
                }
                m_updateTimer.Reset();

                bool const standDown = AuctionHouseModules::TakesOverListings();
                if (standDown != m_standingDown)
                {
                    sLog.outString(standDown ? "AuctionHouseBot: standing down, another module drives the listings"
                                             : "AuctionHouseBot: resuming");
                    m_standingDown = standDown;
                }

                if (!standDown)
                {
                    sAuctionBot.Update();
                }
                sAuctionBot.PurgeMailedItemsTick();
            }

            ChatCommand* Commands() override { return AuctionHouseBotCommands(); }

        private:

            IntervalTimer m_updateTimer;
            bool m_standingDown;
    };

    AuctionHouseModule* CreateAuctionHouseBotModule()
    {
        if (!AuctionHouseModules::ConfigFile().empty())
        {
            sAuctionBotConfig.SetConfigFileName(AuctionHouseModules::ConfigFile().c_str());
        }
        return sAuctionBotConfig.IsModuleEnabled() ? new AuctionHouseBotModule : NULL;
    }

    struct Registrar
    {
        Registrar() { AuctionHouseModules::Provide(&CreateAuctionHouseBotModule); }
    } const registrar;
}
