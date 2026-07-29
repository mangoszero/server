/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_AHSERVICE
#define MANGOS_H_AHSERVICE

// mangos_zero only. The out-of-process auction house is this core's alone and is
// deliberately not carried to mangos_one/two/three. It lives in its own header so
// that Master.cpp stays identical to the other cores apart from one registration,
// mirroring how RaService lives in RASession.h.

#include "Service.h"
#include "AuctionHouseBot.h"
#include "Config/Config.h"
#include "Log.h"
#include "World.h"
#include "WorkerSupervisor.h"


/**
 * @brief The auction-house worker subprocess (optional, default-off).
 *
 * Not a thread: the supervisor owns a child process and its IPC link. It gets the
 * same lifecycle as everything else so its Shutdown() is ordered by the same rule —
 * joined after the world loop has stopped, so no tick can race it.
 */
class AhServiceService : public IService
{
    public:

        AhServiceService() : m_supervisor(nullptr) {}

        const char* Name() const override { return "AH service"; }

        void Start() override
        {
            // SP-1 coordinator authority: the worker is the configured AH read
            // authority from here on. Set BEFORE Start() so that even if Start()
            // fails (missing exe / port taken / bad bot GUID) and the supervisor is
            // torn down below, the read handlers still send "AH unavailable" rather
            // than silently reverting to in-process reads.
            sWorld.SetAhServiceConfigured(true);

            m_supervisor = new WorkerSupervisor(
                "ah-service",
                sConfig.GetStringDefault("AH.Service.Path", "service-workers/ah-service/ah-service"),
                uint16(sConfig.GetIntDefault("AH.Service.Port", 5760)),
                sConfig.GetStringDefault("AH.Service.Secret", "changeme"),
                sAuctionBotConfig.GetAHBotId(),
                sConfig.GetStringDefault("AH.Service.Config", "ah-service.conf"));

            // SP-2: arm the write-authority bit for the IPC handshake BEFORE Start() --
            // IPC_HELLO_ACK carries {runId, writeAuthority} to the worker (spec
            // decision 7: the worker never reads it from its own conf). Applied on
            // every child respawn.
            m_supervisor->SetWriteAuthority(sWorld.IsAhWriteAuthority());

            if (!m_supervisor->Start())
            {
                sLog.outError("AH service failed to start; falling back to in-process bot");
                delete m_supervisor;
                m_supervisor = nullptr;
            }

            // Published before the world loop starts, so the first tick already sees it.
            sWorld.SetAhSupervisor(m_supervisor);
        }

        void Join() override
        {
            if (!m_supervisor)
            {
                return;
            }

            // Unpublish before destroying: the world loop has stopped, but nothing else
            // should be able to observe a dangling supervisor.
            sWorld.SetAhSupervisor(nullptr);
            m_supervisor->Shutdown();
            delete m_supervisor;
            m_supervisor = nullptr;
        }

    private:

        WorkerSupervisor* m_supervisor;
};

#endif
