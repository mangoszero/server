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
 * @file AhServiceModule.cpp
 * @brief The out-of-process auction house service as an auction module.
 */

#include "AhService.h"
#include "AhServiceBook.h"
#include "AhServiceCommands.h"
#include "AhServiceHandlers.h"
#include "AhServiceWorld.h"
#include "AuctionHouseBot.h"
#include "AuctionHouseModule.h"
#include "BrowsePending.h"
#include "CustodyLedger.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Timer.h"
#include "WorkerSupervisor.h"

namespace
{
    uint32 const MAINTENANCE_INTERVAL = 20 * IN_MILLISECONDS;

    class AhServiceModule : public AuctionHouseModule
    {
        public:

            AhServiceModule()
            {
                m_maintenanceTimer.SetInterval(MAINTENANCE_INTERVAL);
            }

            char const* Name() const override { return "AH service"; }

            void LoadConfig(bool reload) override
            {
                if (reload)
                {
                    sAhService.LoadConfig(true);
                }
            }

            void Start() override
            {
                CustodyLedger::InitializeRouting();
                AhEnsureRecipeCastMap();
                AhAuditCustodyReconcile("boot");

                if (sAhService.IsWorkerConfigured())
                {
                    StartWorker();
                }
            }

            void Update(uint32 diff) override
            {
                m_maintenanceTimer.Update(diff);
                if (m_maintenanceTimer.Passed())
                {
                    m_maintenanceTimer.Reset();
                    AhServiceMaintenance();
                }
                AhServiceTick();
            }

            void Stop() override
            {
                WorkerSupervisor* supervisor = sAhService.GetSupervisor();
                if (!supervisor)
                {
                    return;
                }

                sAhService.SetSupervisor(NULL);
                supervisor->Shutdown();
                delete supervisor;
            }

            ChatCommand* Commands() override { return AhServiceCommands(); }

            uint32 HighestAuctionId() const override
            {
                // Terminal custody rows outlive their auction until pruned; a
                // reused id would collide their "item:<id>"/"dep:<id>" keys.
                uint32 highest = 0;
                if (QueryResult* result = CharacterDatabase.Query("SELECT MAX(`auction_id`) FROM `custody_ledger`"))
                {
                    highest = (*result)[0].GetUInt32();
                    delete result;
                }
                return highest;
            }

            bool OwnsAuctionRows() const override { return sAhService.IsWriteAuthority(); }
            bool TakesOverListings() const override { return sAhService.IsWorkerActive(); }

            bool BlocksHello(WorldSession& session) override { return AhBlocksHello(session); }
            bool ForwardSell(WorldSession& session, AuctionSale const& sale) override { return AhForwardSell(session, sale); }
            bool SettleSell(WorldSession& session, AuctionSale const& sale, AuctionEntry*& listed) override { return AhSettleSell(session, sale, listed); }
            bool ForwardBid(WorldSession& session, uint32 auctionId, uint32 price) override { return AhForwardBid(session, auctionId, price); }
            bool SettleBid(WorldSession& session, AuctionHouseObject& /*house*/, AuctionEntry& auction, uint32 price, uint32 newOutbid) override
            {
                return AhSettleBid(session, auction, price, newOutbid);
            }
            bool ForwardCancel(WorldSession& session, uint32 auctionId) override { return AhForwardCancel(session, auctionId); }
            bool SettleCancel(WorldSession& session, AuctionHouseObject& house, AuctionEntry& auction) override
            {
                return AhSettleCancel(session, house, auction);
            }
            bool ForwardList(WorldSession& session, AuctionListRequest const& request) override { return AhForwardList(session, request); }

            AuctionExpiry Expire(AuctionHouseObject& /*house*/, AuctionEntry& auction) override { return AhExpireAuction(auction); }

            bool SettleGeneratedBid(AuctionEntry& auction, uint32 price, bool& stillActive) override
            {
                bool applied = false;
                return AhSettleGeneratedBid(auction, price, stillActive, applied);
            }

        private:

            void StartWorker()
            {
                // The worker places its listings as the bot's character.
                if (!sAuctionBotConfig.GetAHBotId())
                {
                    sAuctionBotConfig.Initialize();
                }

                AhServiceSettings const& settings = sAhService.Settings();
                WorkerSupervisor* supervisor = new WorkerSupervisor("ah-service", settings.path, settings.port,
                    settings.secret, sAuctionBotConfig.GetAHBotId(), sAhService.ConfigPath());

                // IPC_HELLO_ACK carries the write authority to the worker on every respawn.
                supervisor->SetWriteAuthority(sAhService.IsWriteAuthority());

                if (!supervisor->Start())
                {
                    sLog.outError("AH service failed to start; the auction house reports itself unavailable");
                    delete supervisor;
                    return;
                }

                sAhService.SetSupervisor(supervisor);
            }

            IntervalTimer m_maintenanceTimer;
    };

    AuctionHouseModule* CreateAhServiceModule()
    {
        if (!sAhService.LoadConfig(false) || !sAhService.Settings().enable)
        {
            return NULL;
        }
        return new AhServiceModule;
    }

    struct Registrar
    {
        Registrar() { AuctionHouseModules::Provide(&CreateAhServiceModule); }
    } const registrar;
}
