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
 * @file World.cpp
 * @brief Core world server implementation
 *
 * This file implements the World class, which is the central hub of the
 * MaNGOS game server. It manages:
 * - Server configuration and settings
 * - Game time and world updates
 * - Player sessions and limits
 * - All game systems initialization (maps, spells, quests, etc.)
 * - Server shutdown and restart procedures
 * - In-game announcements and events
 * - Various utility functions for the game world
 *
 * The World class is a singleton accessed via sWorld, and runs the main
 * server loop that processes all game logic.
 *
 * @ingroup world
 */



#include "World.h"
#include "Database/DatabaseEnv.h"
#include "Config/Config.h"
#include "Platform/Define.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Player.h"
#include "AccountMgr.h"
#include "AuctionHouseMgr.h"
#include "ObjectMgr.h"
#include "CreatureEventAIMgr.h"
#include "GuildMgr.h"
#include "SpellMgr.h"
#include "Chat.h"
#include "DBCStores.h"
#include "MassMailMgr.h"
#include "LootMgr.h"
#include "ItemEnchantmentMgr.h"
#include "MapManager.h"
#include "ScriptMgr.h"
#include "CreatureAIRegistry.h"
#include "ProgressBar.h"
#include "Policies/Singleton.h"
#include "BattleGround/BattleGroundMgr.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "TemporarySummon.h"
#include "VMapFactory.h"
#include "MoveMap.h"
#include "GameEventMgr.h"
#include "PoolManager.h"
#include "Database/DatabaseImpl.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MapPersistentStateMgr.h"
#include "WaypointManager.h"
#include "GMTicketMgr.h"
#include "Util.h"
#include "AuctionHouseBot/AuctionHouseBot.h"
#include "CharacterDatabaseCleaner.h"
#include "CreatureLinkingMgr.h"
#include "Weather.h"
#include "LFGMgr.h"
#include "DisableMgr.h"
#include "Language.h"
#include "CommandMgr.h"
#include "GitRevision.h"
#include "UpdateTime.h"
#include "GameTime.h"
#include "WardenCheckMgr.h"
#include "SystemConfig.h"
#include "AuctionHouseBot/AuctionIntentExecutor.h"
#include "AuctionHouseBot/CustodyLedger.h"
#include "AuctionHouseBot/CustodyService.h"
#include "ScheduledExit.h"
#include "WorkerSupervisor.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "AuctionIntents.h"
#include "BrowseMessages.h"
#include "AuctionHouseBot/BrowsePending.h"
#include <iostream>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaLoader.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "PlayerbotAIConfig.h"
#endif /* ENABLE_PLAYERBOTS */
#ifdef ENABLE_PLAYERBOTS
#include "RandomPlayerbotMgr.h"
#endif /* ENABLE_PLAYERBOTS */

/// Find a session by its id
WorldSession* World::FindSession(uint32 id) const
{
    SessionMap::const_iterator itr = m_sessions.find(id);

    if (itr != m_sessions.end())
    {
        return itr->second;
    }                                 // also can return NULL for kicked session
    else
    {
        return NULL;
    }
}

/// Remove a given session
bool World::RemoveSession(uint32 id)
{
    ///- Find the session, kick the user, but we can't delete session at this moment to prevent iterator invalidation
    SessionMap::const_iterator itr = m_sessions.find(id);

    if (itr != m_sessions.end() && itr->second)
    {
        if (itr->second->PlayerLoading())
        {
            return false;
        }
        itr->second->KickPlayer();
    }

    return true;
}

/**
 * @brief Queues a new session for addition during the world update.
 *
 * @param s The session to add.
 */
void World::AddSession(WorldSession* s)
{
    addSessQueue.add(s);
}

/**
 * @brief Returns the current queue position of a session.
 *
 * @param sess The session to locate.
 * @return The 1-based queue position, or 0 if not queued.
 */
int32 World::GetQueuedSessionPos(WorldSession* sess)
{
    uint32 position = 1;

    for (Queue::const_iterator iter = m_QueuedSessions.begin(); iter != m_QueuedSessions.end(); ++iter, ++position)
    {
        if ((*iter) == sess)
        {
            return position;
        }
    }
    return 0;
}

/**
 * @brief Places a session into the login queue and notifies the client.
 *
 * @param sess The session being queued.
 */
void World::AddQueuedSession(WorldSession* sess)
{
    sess->SetInQueue(true);
    m_QueuedSessions.push_back(sess);

    // [-ZERO] Possible wrong
    // The 1st SMSG_AUTH_RESPONSE needs to contain other info too.
    WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1 + 4 + 4);
    packet << uint8(AUTH_WAIT_QUEUE);
    packet << uint32(0);                                    // BillingTimeRemaining
    packet << uint8(0);                                     // BillingPlanFlags
    packet << uint32(0);                                    // BillingTimeRested
    packet << uint32(GetQueuedSessionPos(sess));            // position in queue
    sess->SendPacket(&packet);
}

/**
 * @brief Removes a session from the login queue and updates later positions.
 *
 * @param sess The session to remove.
 * @return true if the session was queued and removed; otherwise false.
 */
bool World::RemoveQueuedSession(WorldSession* sess)
{
    // sessions count including queued to remove (if removed_session set)
    uint32 sessions = GetActiveSessionCount();

    uint32 position = 1;
    Queue::iterator iter = m_QueuedSessions.begin();

    // search to remove and count skipped positions
    bool found = false;

    for (; iter != m_QueuedSessions.end(); ++iter, ++position)
    {
        if (*iter == sess)
        {
            sess->SetInQueue(false);
            iter = m_QueuedSessions.erase(iter);
            found = true;                                   // removing queued session
            break;
        }
    }

    // iter point to next socked after removed or end()
    // position store position of removed socket and then new position next socket after removed

    // if session not queued then we need decrease sessions count
    if (!found && sessions)
    {
        --sessions;
    }

    // accept first in queue
    if ((!m_playerLimit || (int32)sessions < m_playerLimit) && !m_QueuedSessions.empty())
    {
        WorldSession* pop_sess = m_QueuedSessions.front();
        pop_sess->SetInQueue(false);
        pop_sess->SendAuthWaitQue(0);
        m_QueuedSessions.pop_front();

        // update iter to point first queued socket or end() if queue is empty now
        iter = m_QueuedSessions.begin();
        position = 1;
    }

    // update position from iter to end()
    // iter point to first not updated socket, position store new position
    for (; iter != m_QueuedSessions.end(); ++iter, ++position)
    {
        (*iter)->SendAuthWaitQue(position);
    }

    return found;
}
