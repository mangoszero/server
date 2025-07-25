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

/** \file
    \ingroup world
*/

#include "World.h"
#include "Database/DatabaseEnv.h"
#include "Config/Config.h"
#include "Platform/Define.h"
#include "SystemConfig.h"
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

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#include "ElunaConfig.h"
#include "ElunaLoader.h"
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_PLAYERBOTS

#include "PlayerbotAIConfig.h"
#include "RandomPlayerbotMgr.h"
#endif

// WARDEN
#include "WardenCheckMgr.h"

#include <iostream>
#include <sstream>

INSTANTIATE_SINGLETON_1(World);

extern void LoadGameObjectModelList();

volatile bool World::m_stopEvent = false;
uint8 World::m_ExitCode = SHUTDOWN_EXIT_CODE;

ACE_Atomic_Op<ACE_Thread_Mutex, uint32> World::m_worldLoopCounter = 0;

float World::m_MaxVisibleDistanceOnContinents = DEFAULT_VISIBILITY_DISTANCE;
float World::m_MaxVisibleDistanceInInstances  = DEFAULT_VISIBILITY_INSTANCE;
float World::m_MaxVisibleDistanceInBGArenas   = DEFAULT_VISIBILITY_BGARENAS;

float World::m_MaxVisibleDistanceInFlight     = DEFAULT_VISIBILITY_DISTANCE;
float World::m_VisibleUnitGreyDistance        = 0;
float World::m_VisibleObjectGreyDistance      = 0;

float  World::m_relocation_lower_limit_sq     = 10.f * 10.f;
uint32 World::m_relocation_ai_notify_delay    = 1000u;

/// World constructor
World::World()
{
    m_playerLimit = 0;
    m_allowMovement = true;
    m_ShutdownMask = 0;
    m_ShutdownTimer = 0;
    m_gameTime = time(NULL);
    m_startTime = m_gameTime;
    m_maxActiveSessionCount = 0;
    m_maxQueuedSessionCount = 0;
    m_MaintenanceTimeChecker = 0;
    m_broadcastEnable = false;
    m_broadcastList.clear();
    m_broadcastWeight = 0;

    m_defaultDbcLocale = LOCALE_enUS;
    m_availableDbcLocaleMask = 0;

    for (int i = 0; i < CONFIG_UINT32_VALUE_COUNT; ++i)
    {
        m_configUint32Values[i] = 0;
    }

    for (int i = 0; i < CONFIG_INT32_VALUE_COUNT; ++i)
    {
        m_configInt32Values[i] = 0;
    }

    for (int i = 0; i < CONFIG_FLOAT_VALUE_COUNT; ++i)
    {
        m_configFloatValues[i] = 0.0f;
    }

    for (int i = 0; i < CONFIG_BOOL_VALUE_COUNT; ++i)
    {
        m_configBoolValues[i] = false;
    }
}

/// World destructor
World::~World()
{
#ifdef ENABLE_ELUNA
    // Delete world Eluna state
    delete eluna;
    eluna = nullptr;
#endif /* ENABLE_ELUNA */

    ///- Empty the kicked session set
    while (!m_sessions.empty())
    {
        // not remove from queue, prevent loading new sessions
        delete m_sessions.begin()->second;
        m_sessions.erase(m_sessions.begin());
    }

    CliCommandHolder* command = NULL;
    while (cliCmdQueue.next(command))
    {
        delete command;
    }

    WorldSession* session = NULL;
    while (addSessQueue.next(session))
    {
        delete session;
    }

    VMAP::VMapFactory::clear();
    MMAP::MMapFactory::clear();
}

/// Cleanups before world stop
void World::CleanupsBeforeStop()
{
    KickAll();                                       // save and kick all players
    UpdateSessions(1);                               // real players unload required UpdateSessions call
    sBattleGroundMgr.DeleteAllBattleGrounds();       // unload battleground templates before different singletons destroyed
}

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

void World::AddSession(WorldSession* s)
{
    addSessQueue.add(s);
}

void
World::AddSession_(WorldSession* s)
{
    MANGOS_ASSERT(s);

    // NOTE - Still there is race condition in WorldSession* being used in the Sockets

    ///- kick already loaded player with same account (if any) and remove session
    ///- if player is in loading and want to load again, return
    if (!RemoveSession(s->GetAccountId()))
    {
        s->KickPlayer();
        delete s;                                           // session not added yet in session list, so not listed in queue
        return;
    }

    // decrease session counts only at not reconnection case
    bool decrease_session = true;

    // if session already exist, prepare to it deleting at next world update
    // NOTE - KickPlayer() should be called on "old" in RemoveSession()
    {
        SessionMap::const_iterator old = m_sessions.find(s->GetAccountId());

        if (old != m_sessions.end())
        {
            // prevent decrease sessions count if session queued
            if (RemoveQueuedSession(old->second))
            {
                decrease_session = false;
            }
            // not remove replaced session form queue if listed
            delete old->second;
        }
    }

    m_sessions[s->GetAccountId()] = s;

    uint32 Sessions = GetActiveAndQueuedSessionCount();
    uint32 pLimit = GetPlayerAmountLimit();
    uint32 QueueSize = GetQueuedSessionCount();             // number of players in the queue

    // so we don't count the user trying to
    // login as a session and queue the socket that we are using
    if (decrease_session)
    {
        --Sessions;
    }

    if (pLimit > 0 && Sessions >= pLimit && s->GetSecurity() == SEC_PLAYER)
    {
        AddQueuedSession(s);
        UpdateMaxSessionCounters();
        DETAIL_LOG("PlayerQueue: Account id %u is in Queue Position (%u).", s->GetAccountId(), ++QueueSize);
        return;
    }

    // Checked for 1.12.2
    WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4 + 1 + 4);
    packet << uint8(AUTH_OK);
    packet << uint32(0);                                    // BillingTimeRemaining
    packet << uint8(0);                                     // BillingPlanFlags
    packet << uint32(0);                                    // BillingTimeRested
    s->SendPacket(&packet);

    UpdateMaxSessionCounters();

    // Updates the population
    if (pLimit > 0)
    {
        float popu = float(GetActiveSessionCount());        // updated number of users on the server
        popu /= pLimit;
        popu *= 2;

        static SqlStatementID id;

        SqlStatement stmt = LoginDatabase.CreateStatement(id, "UPDATE `realmlist` SET `population` = ? WHERE `id` = ?");
        stmt.PExecute(popu, realmID);

        DETAIL_LOG("Server Population (%f).", popu);
    }
}

int32 World::GetQueuedSessionPos(WorldSession* sess)
{
    uint32 position = 1;

    for (Queue::const_iterator iter = m_QueuedSessions.begin(); iter != m_QueuedSessions.end(); ++iter, ++position)
        if ((*iter) == sess)
        {
            return position;
        }

    return 0;
}

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

/// Initialize config values
void World::LoadConfigSettings(bool reload)
{
    if (reload)
    {
        if (!sConfig.Reload())
        {
            sLog.outError("World settings reload fail: can't read settings from %s.", sConfig.GetFilename().c_str());
            return;
        }
    }

    ///- Read the version of the configuration file and warn the user in case of emptiness or mismatch
    uint32 confVersion = sConfig.GetIntDefault("ConfVersion", 0);
    if (!confVersion)
    {
        sLog.outError("*****************************************************************************");
        sLog.outError(" WARNING: mangosd.conf does not include a ConfVersion variable.");
        sLog.outError("          Your configuration file may be out of date!");
        sLog.outError("*****************************************************************************");
        Log::WaitBeforeContinueIfNeed();
    }
    else
    {
        if (confVersion < MANGOSD_CONFIG_VERSION)
        {
            sLog.outError("*****************************************************************************");
            sLog.outError(" WARNING: Your mangosd.conf version indicates your conf file is out of date!");
            sLog.outError("          Please check for updates, as your current default values may cause");
            sLog.outError("          unexpected behavior.");
            sLog.outError("*****************************************************************************");
            Log::WaitBeforeContinueIfNeed();
        }
    }

    ///- Read the player limit and the Message of the day from the config file
    SetPlayerLimit(sConfig.GetIntDefault("PlayerLimit", DEFAULT_PLAYER_LIMIT), true);
    SetMotd(sConfig.GetStringDefault("Motd", "Welcome to the Massive Network Game Object Server."));

    ///- Read all rates from the config file
    setConfigPos(CONFIG_FLOAT_RATE_HEALTH, "Rate.Health", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_POWER_MANA, "Rate.Mana", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_POWER_RAGE_INCOME, "Rate.Rage.Income", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_POWER_RAGE_LOSS, "Rate.Rage.Loss", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_POWER_FOCUS,             "Rate.Focus", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_LOYALTY,              "Rate.Loyalty", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_POWER_ENERGY,            "Rate.Energy", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_SKILL_DISCOVERY,      "Rate.Skill.Discovery",      1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_POOR,       "Rate.Drop.Item.Poor",       1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_NORMAL,     "Rate.Drop.Item.Normal",     1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_UNCOMMON,   "Rate.Drop.Item.Uncommon",   1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_RARE,       "Rate.Drop.Item.Rare",       1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_EPIC,       "Rate.Drop.Item.Epic",       1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_LEGENDARY,  "Rate.Drop.Item.Legendary",  1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_ARTIFACT,   "Rate.Drop.Item.Artifact",   1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_ITEM_REFERENCED, "Rate.Drop.Item.Referenced", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DROP_MONEY,           "Rate.Drop.Money", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_XP_KILL,    "Rate.XP.Kill",    1.0f);
    setConfig(CONFIG_FLOAT_RATE_XP_PETKILL, "Rate.XP.PetKill", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_XP_QUEST,   "Rate.XP.Quest",   1.0f);
    setConfig(CONFIG_FLOAT_RATE_XP_EXPLORE, "Rate.XP.Explore", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REPUTATION_GAIN,           "Rate.Reputation.Gain", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_KILL,  "Rate.Reputation.LowLevel.Kill", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REPUTATION_LOWLEVEL_QUEST, "Rate.Reputation.LowLevel.Quest", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_DAMAGE,          "Rate.Creature.Normal.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_DAMAGE,     "Rate.Creature.Elite.Elite.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_DAMAGE, "Rate.Creature.Elite.RAREELITE.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE, "Rate.Creature.Elite.WORLDBOSS.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_DAMAGE,      "Rate.Creature.Elite.RARE.Damage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_HP,          "Rate.Creature.Normal.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_HP,     "Rate.Creature.Elite.Elite.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_HP, "Rate.Creature.Elite.RAREELITE.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_HP, "Rate.Creature.Elite.WORLDBOSS.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_HP,      "Rate.Creature.Elite.RARE.HP", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_NORMAL_SPELLDAMAGE,          "Rate.Creature.Normal.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE,     "Rate.Creature.Elite.Elite.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE, "Rate.Creature.Elite.RAREELITE.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE, "Rate.Creature.Elite.WORLDBOSS.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_ELITE_RARE_SPELLDAMAGE,      "Rate.Creature.Elite.RARE.SpellDamage", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CREATURE_AGGRO, "Rate.Creature.Aggro", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REST_INGAME,                    "Rate.Rest.InGame", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY, "Rate.Rest.Offline.InTavernOrCity", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS,     "Rate.Rest.Offline.InWilderness", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_DAMAGE_FALL,  "Rate.Damage.Fall", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_AUCTION_TIME, "Rate.Auction.Time", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_AUCTION_DEPOSIT, "Rate.Auction.Deposit", 1.0f);
    setConfig(CONFIG_FLOAT_RATE_AUCTION_CUT,     "Rate.Auction.Cut", 1.0f);
    setConfig(CONFIG_UINT32_AUCTION_DEPOSIT_MIN, "Auction.Deposit.Min", 0);
    setConfig(CONFIG_FLOAT_RATE_HONOR, "Rate.Honor", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_MINING_AMOUNT, "Rate.Mining.Amount", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_MINING_NEXT,   "Rate.Mining.Next", 1.0f);
    setConfig(CONFIG_UINT32_RATE_MINING_LOWER,         "Rate.Mining.Lower", 50);
    setConfig(CONFIG_UINT32_RATE_MINING_RARE,          "Rate.Mining.Rare", 20);
    setConfig(CONFIG_UINT32_RATE_MINING_DARKIRON,      "Rate.Mining.Darkiron", 10);
    setConfig(CONFIG_UINT32_RATE_MINING_AUTOPOOLING,   "Rate.Mining.Autopooling", 90);
    setConfigPos(CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME, "Rate.InstanceResetTime", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_TALENT, "Rate.Talent", 1.0f);
    setConfigPos(CONFIG_FLOAT_RATE_CORPSE_DECAY_LOOTED, "Rate.Corpse.Decay.Looted", 0.5f);

    setConfigMinMax(CONFIG_FLOAT_RATE_TARGET_POS_RECALCULATION_RANGE, "TargetPosRecalculateRange", 1.5f, CONTACT_DISTANCE, ATTACK_DISTANCE);

    setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_DAMAGE, "DurabilityLossChance.Damage", 0.5f);
    setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_ABSORB, "DurabilityLossChance.Absorb", 0.5f);
    setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_PARRY,  "DurabilityLossChance.Parry",  0.05f);
    setConfigPos(CONFIG_FLOAT_RATE_DURABILITY_LOSS_BLOCK,  "DurabilityLossChance.Block",  0.05f);

    setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_SAY,       "ListenRange.Say",       40.0f);
    setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_YELL,      "ListenRange.Yell",     300.0f);
    setConfigPos(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE, "ListenRange.TextEmote", 40.0f);

    setConfigPos(CONFIG_FLOAT_GROUP_XP_DISTANCE, "MaxGroupXPDistance", 74.0f);
    setConfigPos(CONFIG_FLOAT_SIGHT_GUARDER,     "GuarderSight",       50.0f);
    setConfigPos(CONFIG_FLOAT_SIGHT_MONSTER,     "MonsterSight",       50.0f);

    setConfigPos(CONFIG_FLOAT_CREATURE_FAMILY_ASSISTANCE_RADIUS,      "CreatureFamilyAssistanceRadius",     10.0f);
    setConfigPos(CONFIG_FLOAT_CREATURE_FAMILY_FLEE_ASSISTANCE_RADIUS, "CreatureFamilyFleeAssistanceRadius", 30.0f);

    ///- Read other configuration items from the config file
    setConfigMinMax(CONFIG_UINT32_COMPRESSION, "Compression", 1, 1, 9);
    setConfig(CONFIG_BOOL_ADDON_CHANNEL, "AddonChannel", true);
    setConfig(CONFIG_BOOL_CLEAN_CHARACTER_DB, "CleanCharacterDB", true);
    setConfig(CONFIG_BOOL_GRID_UNLOAD, "GridUnload", true);
    setConfig(CONFIG_UINT32_MAX_WHOLIST_RETURNS, "MaxWhoListReturns", 49);
    setConfig(CONFIG_UINT32_AUTOBROADCAST_INTERVAL, "AutoBroadcast", 600);

    if (getConfig(CONFIG_UINT32_AUTOBROADCAST_INTERVAL) > 0)
    {
        m_broadcastEnable = true;
    }
    else
    {
        m_broadcastEnable = false;
    }

    if (reload && m_broadcastEnable)
    {
        m_broadcastTimer.SetInterval(getConfig(CONFIG_UINT32_AUTOBROADCAST_INTERVAL) * IN_MILLISECONDS);
    }


    std::string forceLoadGridOnMaps = sConfig.GetStringDefault("LoadAllGridsOnMaps", "");
    if (!forceLoadGridOnMaps.empty())
    {
        unsigned int pos = 0;
        unsigned int id;
        VMAP::VMapFactory::chompAndTrim(forceLoadGridOnMaps);
        while (VMAP::VMapFactory::getNextId(forceLoadGridOnMaps, pos, id))
            m_configForceLoadMapIds.insert(id);
    }

    setConfig(CONFIG_UINT32_INTERVAL_SAVE, "PlayerSave.Interval", 15 * MINUTE * IN_MILLISECONDS);
    setConfigMinMax(CONFIG_UINT32_MIN_LEVEL_STAT_SAVE, "PlayerSave.Stats.MinLevel", 0, 0, MAX_LEVEL);
    setConfig(CONFIG_BOOL_STATS_SAVE_ONLY_ON_LOGOUT, "PlayerSave.Stats.SaveOnlyOnLogout", true);

    setConfigMin(CONFIG_UINT32_INTERVAL_GRIDCLEAN, "GridCleanUpDelay", 5 * MINUTE * IN_MILLISECONDS, MIN_GRID_DELAY);
    if (reload)
    {
        sMapMgr.SetGridCleanUpDelay(getConfig(CONFIG_UINT32_INTERVAL_GRIDCLEAN));
    }

    setConfig(CONFIG_UINT32_NUMTHREADS, "MapUpdateThreads", 2);

    setConfigMin(CONFIG_UINT32_INTERVAL_MAPUPDATE, "MapUpdateInterval", 100, MIN_MAP_UPDATE_DELAY);
    if (reload)
    {
        sMapMgr.SetMapUpdateInterval(getConfig(CONFIG_UINT32_INTERVAL_MAPUPDATE));
    }

    setConfig(CONFIG_UINT32_INTERVAL_CHANGEWEATHER, "ChangeWeatherInterval", 10 * MINUTE * IN_MILLISECONDS);

    if (configNoReload(reload, CONFIG_UINT32_PORT_WORLD, "WorldServerPort", DEFAULT_WORLDSERVER_PORT))
    {
        setConfig(CONFIG_UINT32_PORT_WORLD, "WorldServerPort", DEFAULT_WORLDSERVER_PORT);
    }

    if (configNoReload(reload, CONFIG_UINT32_GAME_TYPE, "GameType", 0))
    {
        setConfig(CONFIG_UINT32_GAME_TYPE, "GameType", 0);
    }

    if (configNoReload(reload, CONFIG_UINT32_REALM_ZONE, "RealmZone", REALM_ZONE_DEVELOPMENT))
    {
        setConfig(CONFIG_UINT32_REALM_ZONE, "RealmZone", REALM_ZONE_DEVELOPMENT);
    }

    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ACCOUNTS,            "AllowTwoSide.Accounts", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT,    "AllowTwoSide.Interaction.Chat", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL, "AllowTwoSide.Interaction.Channel", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP,   "AllowTwoSide.Interaction.Group", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD,   "AllowTwoSide.Interaction.Guild", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_TRADE,   "AllowTwoSide.Interaction.Trade", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION, "AllowTwoSide.Interaction.Auction", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL,    "AllowTwoSide.Interaction.Mail", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_WHO_LIST,            "AllowTwoSide.WhoList", false);
    setConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND,          "AllowTwoSide.AddFriend", false);

    setConfig(CONFIG_UINT32_STRICT_PLAYER_NAMES,  "StrictPlayerNames",  0);
    setConfig(CONFIG_UINT32_STRICT_CHARTER_NAMES, "StrictCharterNames", 0);
    setConfig(CONFIG_UINT32_STRICT_PET_NAMES,     "StrictPetNames",     0);

    setConfigMinMax(CONFIG_UINT32_MIN_PLAYER_NAME,  "MinPlayerName",  2, 1, MAX_PLAYER_NAME);
    setConfigMinMax(CONFIG_UINT32_MIN_CHARTER_NAME, "MinCharterName", 2, 1, MAX_CHARTER_NAME);
    setConfigMinMax(CONFIG_UINT32_MIN_PET_NAME,     "MinPetName",     2, 1, MAX_PET_NAME);

    setConfig(CONFIG_UINT32_CHARACTERS_CREATING_DISABLED, "CharactersCreatingDisabled", 0);

    setConfigMinMax(CONFIG_UINT32_CHARACTERS_PER_REALM, "CharactersPerRealm", 10, 1, 10);

    setConfig(CONFIG_BOOL_AUTOPOOLING_MINING_ENABLE, "Autopooling.Mining.Enable", false);

    // must be after CONFIG_UINT32_CHARACTERS_PER_REALM
    setConfigMin(CONFIG_UINT32_CHARACTERS_PER_ACCOUNT, "CharactersPerAccount", 50, getConfig(CONFIG_UINT32_CHARACTERS_PER_REALM));

    setConfigMinMax(CONFIG_UINT32_SKIP_CINEMATICS, "SkipCinematics", 0, 0, 2);

    if (configNoReload(reload, CONFIG_UINT32_MAX_PLAYER_LEVEL, "MaxPlayerLevel", DEFAULT_MAX_LEVEL))
    {
        setConfigMinMax(CONFIG_UINT32_MAX_PLAYER_LEVEL, "MaxPlayerLevel", DEFAULT_MAX_LEVEL, 1, DEFAULT_MAX_LEVEL);
    }

    setConfigMinMax(CONFIG_UINT32_START_PLAYER_LEVEL, "StartPlayerLevel", 1, 1, getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));

    setConfigMinMax(CONFIG_UINT32_START_PLAYER_MONEY, "StartPlayerMoney", 0, 0, MAX_MONEY_AMOUNT);

    setConfig(CONFIG_UINT32_MAX_HONOR_POINTS, "MaxHonorPoints", 75000);

    setConfigMinMax(CONFIG_UINT32_START_HONOR_POINTS, "StartHonorPoints", 0, 0, getConfig(CONFIG_UINT32_MAX_HONOR_POINTS));

    setConfigMin(CONFIG_UINT32_MIN_HONOR_KILLS, "MinHonorKills", HONOR_STANDING_MIN_KILL, 1);

    setConfigMinMax(CONFIG_UINT32_MAINTENANCE_DAY, "MaintenanceDay", 4, 0, 6);

    setConfig(CONFIG_BOOL_ALL_TAXI_PATHS, "AllFlightPaths", false);
    setConfig(CONFIG_BOOL_INSTANT_TAXI, "InstantFlightPaths", false);

    setConfig(CONFIG_UINT32_MOUNT_COST, "MountCost", 100000);
    setConfigMin(CONFIG_UINT32_MIN_TRAIN_MOUNT_LEVEL, "MinTrainMountLevel", 40, 1);
    setConfig(CONFIG_UINT32_TRAIN_MOUNT_COST, "TrainMountCost", 900000);
    setConfig(CONFIG_UINT32_EPIC_MOUNT_COST, "EpicMountCost", 1000000);
    setConfigMin(CONFIG_UINT32_MIN_TRAIN_EPIC_MOUNT_LEVEL, "MinTrainEpicMountLevel", 60, 1);
    setConfig(CONFIG_UINT32_TRAIN_EPIC_MOUNT_COST, "TrainEpicMountCost", 9000000);

    setConfig(CONFIG_BOOL_INSTANCE_IGNORE_LEVEL, "Instance.IgnoreLevel", false);
    setConfig(CONFIG_BOOL_INSTANCE_IGNORE_RAID,  "Instance.IgnoreRaid", false);

    setConfig(CONFIG_BOOL_CAST_UNSTUCK, "CastUnstuck", true);
    setConfig(CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN, "MaxSpellCastsInChain", 20);
    setConfig(CONFIG_UINT32_RABBIT_DAY, "RabbitDay", 0);

    setConfig(CONFIG_UINT32_INSTANCE_RESET_TIME_HOUR, "Instance.ResetTimeHour", 4);
    setConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY,    "Instance.UnloadDelay", 30 * MINUTE * IN_MILLISECONDS);

    setConfigMinMax(CONFIG_UINT32_MAX_PRIMARY_TRADE_SKILL, "MaxPrimaryTradeSkill", 2, 0, 10);

    setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_MAX_PRIMARY_COUNT, "TradeSkill.GMIgnore.MaxPrimarySkillsCount", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);
    setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_LEVEL, "TradeSkill.GMIgnore.Level", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);
    setConfigMinMax(CONFIG_UINT32_TRADE_SKILL_GMIGNORE_SKILL, "TradeSkill.GMIgnore.Skill", SEC_CONSOLE, SEC_PLAYER, SEC_CONSOLE);

    setConfigMinMax(CONFIG_UINT32_MIN_PETITION_SIGNS, "MinPetitionSigns", 9, 0, 9);

    setConfig(CONFIG_UINT32_GM_LOGIN_STATE,          "GM.LoginState",      2);
    setConfig(CONFIG_UINT32_GM_VISIBLE_STATE,        "GM.Visible",         2);
    setConfig(CONFIG_UINT32_GM_ACCEPT_TICKETS,       "GM.AcceptTickets",   2);
    setConfig(CONFIG_UINT32_GM_TICKET_LIST_SIZE,     "GM.TicketListSize", 30);
    setConfig(CONFIG_BOOL_GM_TICKET_OFFLINE_CLOSING, "GM.TicketOfflineClosing", false);
    setConfig(CONFIG_UINT32_GM_CHAT,                 "GM.Chat",            2);
    setConfig(CONFIG_UINT32_GM_WISPERING_TO,         "GM.WhisperingTo",    2);

    setConfig(CONFIG_UINT32_GM_LEVEL_IN_GM_LIST,  "GM.InGMList.Level",  SEC_ADMINISTRATOR);
    setConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST, "GM.InWhoList.Level", SEC_ADMINISTRATOR);
    setConfig(CONFIG_BOOL_GM_LOG_TRADE,           "GM.LogTrade", false);

    setConfigMinMax(CONFIG_UINT32_START_GM_LEVEL, "GM.StartLevel", 1, getConfig(CONFIG_UINT32_START_PLAYER_LEVEL), MAX_LEVEL);
    setConfig(CONFIG_BOOL_GM_LOWER_SECURITY, "GM.LowerSecurity", false);
    setConfig(CONFIG_UINT32_GM_INVISIBLE_AURA, "GM.InvisibleAura", 31748);
    setConfig(CONFIG_UINT32_GM_MAX_SPEED_FACTOR, "GM.MaxSpeedFactor", 10);

    setConfig(CONFIG_UINT32_GROUP_VISIBILITY, "Visibility.GroupMode", 0);

    setConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY, "MailDeliveryDelay", HOUR);

    setConfigMin(CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK, "MassMailer.SendPerTick", 10, 1);

    setConfig(CONFIG_UINT32_UPTIME_UPDATE, "UpdateUptimeInterval", 10);
    if (reload)
    {
        m_timers[WUPDATE_UPTIME].SetInterval(getConfig(CONFIG_UINT32_UPTIME_UPDATE)*MINUTE * IN_MILLISECONDS);
        m_timers[WUPDATE_UPTIME].Reset();
    }

    setConfig(CONFIG_UINT32_SKILL_CHANCE_ORANGE, "SkillChance.Orange", 100);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_YELLOW, "SkillChance.Yellow", 75);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_GREEN,  "SkillChance.Green",  25);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_GREY,   "SkillChance.Grey",   0);

    setConfig(CONFIG_UINT32_SKILL_CHANCE_MINING_STEPS,   "SkillChance.MiningSteps",   75);
    setConfig(CONFIG_UINT32_SKILL_CHANCE_SKINNING_STEPS, "SkillChance.SkinningSteps", 75);

    setConfig(CONFIG_UINT32_SKILL_GAIN_CRAFTING,  "SkillGain.Crafting",  1);
    setConfig(CONFIG_UINT32_SKILL_GAIN_DEFENSE,   "SkillGain.Defense",   1);
    setConfig(CONFIG_UINT32_SKILL_GAIN_GATHERING, "SkillGain.Gathering", 1);
    setConfig(CONFIG_UINT32_SKILL_GAIN_WEAPON,       "SkillGain.Weapon",    1);

    setConfig(CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING,         "SkillFail.Loot.Fishing", false);
    setConfig(CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING,         "SkillFail.Gain.Fishing", false);
    setConfig(CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL, "SkillFail.Possible.FishingPool", true);

    setConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS, "MaxOverspeedPings", 2);
    if (getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS) != 0 && getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS) < 2)
    {
        sLog.outError("MaxOverspeedPings (%i) must be in range 2..infinity (or 0 to disable check). Set to 2.", getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS));
        setConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS, 2);
    }

    setConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY, "SaveRespawnTimeImmediately", true);
    setConfig(CONFIG_BOOL_WEATHER, "ActivateWeather", true);

    setConfig(CONFIG_BOOL_ALWAYS_MAX_SKILL_FOR_LEVEL, "AlwaysMaxSkillForLevel", false);

    setConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_COUNT, "ChatFlood.MessageCount", 10);
    setConfig(CONFIG_UINT32_CHATFLOOD_MESSAGE_DELAY, "ChatFlood.MessageDelay", 1);
    setConfig(CONFIG_UINT32_CHATFLOOD_MUTE_TIME,     "ChatFlood.MuteTime", 10);

    setConfig(CONFIG_BOOL_EVENT_ANNOUNCE, "Event.Announce", false);

    setConfig(CONFIG_UINT32_CREATURE_FAMILY_ASSISTANCE_DELAY, "CreatureFamilyAssistanceDelay", 1500);
    setConfig(CONFIG_UINT32_CREATURE_FAMILY_FLEE_DELAY,       "CreatureFamilyFleeDelay",       7000);

    setConfig(CONFIG_UINT32_WORLD_BOSS_LEVEL_DIFF, "WorldBossLevelDiff", 3);

    setConfigMinMax(CONFIG_INT32_QUEST_LOW_LEVEL_HIDE_DIFF, "Quests.LowLevelHideDiff", 4, -1, MAX_LEVEL);
    setConfigMinMax(CONFIG_INT32_QUEST_HIGH_LEVEL_HIDE_DIFF, "Quests.HighLevelHideDiff", 7, -1, MAX_LEVEL);

    setConfig(CONFIG_BOOL_QUEST_IGNORE_RAID, "Quests.IgnoreRaid", false);

    setConfig(CONFIG_BOOL_DETECT_POS_COLLISION, "DetectPosCollision", true);

    setConfig(CONFIG_BOOL_RESTRICTED_LFG_CHANNEL,      "Channel.RestrictedLfg", true);
    setConfig(CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL, "Channel.SilentlyGMJoin", false);

    setConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING, "ChatFakeMessagePreventing", false);

    setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY, "ChatStrictLinkChecking.Severity", 0);
    setConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK,     "ChatStrictLinkChecking.Kick", 0);

    setConfig(CONFIG_BOOL_CORPSE_EMPTY_LOOT_SHOW,      "Corpse.EmptyLootShow", true);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_NORMAL,    "Corpse.Decay.NORMAL",    300);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_RARE,      "Corpse.Decay.RARE",      900);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_ELITE,     "Corpse.Decay.ELITE",     600);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_RAREELITE, "Corpse.Decay.RAREELITE", 1200);
    setConfig(CONFIG_UINT32_CORPSE_DECAY_WORLDBOSS, "Corpse.Decay.WORLDBOSS", 3600);

    setConfig(CONFIG_INT32_DEATH_SICKNESS_LEVEL, "Death.SicknessLevel", 11);

    setConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVP, "Death.CorpseReclaimDelay.PvP", true);
    setConfig(CONFIG_BOOL_DEATH_CORPSE_RECLAIM_DELAY_PVE, "Death.CorpseReclaimDelay.PvE", true);
    setConfig(CONFIG_BOOL_DEATH_BONES_WORLD,              "Death.Bones.World", true);
    setConfig(CONFIG_BOOL_DEATH_BONES_BG,                 "Death.Bones.Battleground", true);
    setConfigMinMax(CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD,   "Death.Ghost.RunSpeed.World", 1.0f, 0.1f, 10.0f);
    setConfigMinMax(CONFIG_FLOAT_GHOST_RUN_SPEED_BG,      "Death.Ghost.RunSpeed.Battleground", 1.0f, 0.1f, 10.0f);

    setConfig(CONFIG_FLOAT_THREAT_RADIUS, "ThreatRadius", 100.0f);
    setConfigMin(CONFIG_UINT32_CREATURE_RESPAWN_AGGRO_DELAY, "CreatureRespawnAggroDelay", 5000, 0);

    setConfig(CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER,                  "Battleground.CastDeserter", true);
    setConfigMinMax(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN,   "Battleground.QueueAnnouncer.Join", 0, 0, 2);
    setConfig(CONFIG_BOOL_BATTLEGROUND_QUEUE_ANNOUNCER_START,          "Battleground.QueueAnnouncer.Start", false);
    setConfig(CONFIG_BOOL_BATTLEGROUND_SCORE_STATISTICS,               "Battleground.ScoreStatistics", false);
    setConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE,              "Battleground.InvitationType", 0);
    setConfig(CONFIG_UINT32_BATTLEGROUND_PREMATURE_FINISH_TIMER,       "BattleGround.PrematureFinishTimer", 5 * MINUTE * IN_MILLISECONDS);
    setConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH, "BattleGround.PremadeGroupWaitForMatch", 0);
    setConfig(CONFIG_BOOL_OUTDOORPVP_SI_ENABLED,                       "OutdoorPvp.SIEnabled", true);
    setConfig(CONFIG_BOOL_OUTDOORPVP_EP_ENABLED,                       "OutdoorPvp.EPEnabled", true);

    setConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET, "Network.KickOnBadPacket", false);

    setConfig(CONFIG_BOOL_PLAYER_COMMANDS, "PlayerCommands", false);

    setConfig(CONFIG_UINT32_INSTANT_LOGOUT, "InstantLogout", SEC_MODERATOR);

    setConfig(CONFIG_UNIT32_GUILD_PETITION_COST, "Guild.PetitionCost", 1000);
    setConfigMin(CONFIG_UINT32_GUILD_EVENT_LOG_COUNT, "Guild.EventLogRecordsCount", GUILD_EVENTLOG_MAX_RECORDS, GUILD_EVENTLOG_MAX_RECORDS);

    setConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL, "TimerBar.Fatigue.GMLevel", SEC_CONSOLE);
    setConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_MAX,     "TimerBar.Fatigue.Max", 60);
    setConfig(CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL,  "TimerBar.Breath.GMLevel", SEC_CONSOLE);
    setConfig(CONFIG_UINT32_TIMERBAR_BREATH_MAX,      "TimerBar.Breath.Max", 60);
    setConfig(CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL,    "TimerBar.Fire.GMLevel", SEC_CONSOLE);
    setConfig(CONFIG_UINT32_TIMERBAR_FIRE_MAX,        "TimerBar.Fire.Max", 1);

    setConfig(CONFIG_UINT32_LOG_WHISPERS,             "LogWhispers", 1);

    setConfig(CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT,      "PetUnsummonAtMount", false);

    setConfig(CONFIG_BOOL_ENABLE_QUEST_TRACKER,        "QuestTracker.Enable", 0);

#ifdef ENABLE_PLAYERBOTS
    setConfig(CONFIG_BOOL_PLAYERBOT_DISABLE, "PlayerbotAI.DisableBots", true);
    setConfig(CONFIG_BOOL_PLAYERBOT_DEBUGWHISPER, "PlayerbotAI.DebugWhisper", false);
    setConfigMinMax(CONFIG_UINT32_PLAYERBOT_MAXBOTS, "PlayerbotAI.MaxNumBots", 3, 1, 9);
    setConfigMinMax(CONFIG_UINT32_PLAYERBOT_RESTRICTLEVEL, "PlayerbotAI.RestrictBotLevel", getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL), 1, getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
    setConfigMinMax(CONFIG_UINT32_PLAYERBOT_MINBOTLEVEL, "PlayerbotAI.MinBotLevel", 1, 1, getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
    setConfig(CONFIG_FLOAT_PLAYERBOT_MINDISTANCE, "PlayerbotAI.FollowDistanceMin", 0.5f);
    setConfig(CONFIG_FLOAT_PLAYERBOT_MAXDISTANCE, "PlayerbotAI.FollowDistanceMax", 1.0f);

    setConfig(CONFIG_BOOL_PLAYERBOT_ALLOW_SUMMON_OPPOSITE_FACTION, "PlayerbotAI.AllowSummonOppositeFaction", false);
    setConfig(CONFIG_BOOL_PLAYERBOT_COLLECT_COMBAT, "PlayerbotAI.Collect.Combat", true);
    setConfig(CONFIG_BOOL_PLAYERBOT_COLLECT_QUESTS, "PlayerbotAI.Collect.Quest", true);
    setConfig(CONFIG_BOOL_PLAYERBOT_COLLECT_PROFESSION, "PlayerbotAI.Collect.Profession", true);
    setConfig(CONFIG_BOOL_PLAYERBOT_COLLECT_LOOT, "PlayerbotAI.Collect.Loot", true);
    setConfig(CONFIG_BOOL_PLAYERBOT_COLLECT_SKIN, "PlayerbotAI.Collect.Skin", true);
    setConfig(CONFIG_BOOL_PLAYERBOT_COLLECT_OBJECTS, "PlayerbotAI.Collect.Objects", true);
    setConfig(CONFIG_BOOL_PLAYERBOT_SELL_TRASH, "PlayerbotAI.SellGarbage", true);

    setConfig(CONFIG_BOOL_PLAYERBOT_SHAREDBOTS, "PlayerbotAI.SharedBots", true);
#endif

    // WARDEN

    setConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED, "Warden.WinEnabled", true);
    setConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED, "Warden.OSXEnabled", false);
    setConfig(CONFIG_UINT32_WARDEN_NUM_MEM_CHECKS, "Warden.NumMemChecks", 3);
    setConfig(CONFIG_UINT32_WARDEN_NUM_OTHER_CHECKS, "Warden.NumOtherChecks", 7);
    setConfig(CONFIG_UINT32_WARDEN_CLIENT_BAN_DURATION, "Warden.BanDuration", 86400);
    setConfig(CONFIG_UINT32_WARDEN_CLIENT_CHECK_HOLDOFF, "Warden.ClientCheckHoldOff", 30);
    setConfig(CONFIG_UINT32_WARDEN_CLIENT_FAIL_ACTION, "Warden.ClientCheckFailAction", 0);
    setConfig(CONFIG_UINT32_WARDEN_CLIENT_RESPONSE_DELAY, "Warden.ClientResponseDelay", 600);
    setConfig(CONFIG_UINT32_WARDEN_DB_LOGLEVEL, "Warden.DBLogLevel", 0);

    // Recommended Or New Flag
    setConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW_ENABLED, "Realm.RecommendedOrNew.Enabled", false);
    setConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW, "Realm.RecommendedOrNew", false);

    m_relocation_ai_notify_delay = sConfig.GetIntDefault("Visibility.AIRelocationNotifyDelay", 1000u);
    m_relocation_lower_limit_sq  = pow(sConfig.GetFloatDefault("Visibility.RelocationLowerLimit", 10), 2);

    m_VisibleUnitGreyDistance = sConfig.GetFloatDefault("Visibility.Distance.Grey.Unit", 1);
    if (m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
    {
        sLog.outError("Visibility.Distance.Grey.Unit can't be greater %f", MAX_VISIBILITY_DISTANCE);
        m_VisibleUnitGreyDistance = MAX_VISIBILITY_DISTANCE;
    }
    m_VisibleObjectGreyDistance = sConfig.GetFloatDefault("Visibility.Distance.Grey.Object", 10);
    if (m_VisibleObjectGreyDistance >  MAX_VISIBILITY_DISTANCE)
    {
        sLog.outError("Visibility.Distance.Grey.Object can't be greater %f", MAX_VISIBILITY_DISTANCE);
        m_VisibleObjectGreyDistance = MAX_VISIBILITY_DISTANCE;
    }

    // visibility on continents
    m_MaxVisibleDistanceOnContinents      = sConfig.GetFloatDefault("Visibility.Distance.Continents",     DEFAULT_VISIBILITY_DISTANCE);
    if (m_MaxVisibleDistanceOnContinents < 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO))
    {
        sLog.outError("Visibility.Distance.Continents can't be less max aggro radius %f", 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO));
        m_MaxVisibleDistanceOnContinents = 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
    }
    else if (m_MaxVisibleDistanceOnContinents + m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
    {
        sLog.outError("Visibility.Distance.Continents can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
        m_MaxVisibleDistanceOnContinents = MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
    }

    // visibility in instances
    m_MaxVisibleDistanceInInstances        = sConfig.GetFloatDefault("Visibility.Distance.Instances",       DEFAULT_VISIBILITY_INSTANCE);
    if (m_MaxVisibleDistanceInInstances < 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO))
    {
        sLog.outError("Visibility.Distance.Instances can't be less max aggro radius %f", 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO));
        m_MaxVisibleDistanceInInstances = 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
    }
    else if (m_MaxVisibleDistanceInInstances + m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
    {
        sLog.outError("Visibility.Distance.Instances can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
        m_MaxVisibleDistanceInInstances = MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
    }

    // visibility in BG/Arenas
    m_MaxVisibleDistanceInBGArenas        = sConfig.GetFloatDefault("Visibility.Distance.BGArenas",       DEFAULT_VISIBILITY_BGARENAS);
    if (m_MaxVisibleDistanceInBGArenas < 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO))
    {
        sLog.outError("Visibility.Distance.BGArenas can't be less max aggro radius %f", 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO));
        m_MaxVisibleDistanceInBGArenas = 45 * getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
    }
    else if (m_MaxVisibleDistanceInBGArenas + m_VisibleUnitGreyDistance >  MAX_VISIBILITY_DISTANCE)
    {
        sLog.outError("Visibility.Distance.BGArenas can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance);
        m_MaxVisibleDistanceInBGArenas = MAX_VISIBILITY_DISTANCE - m_VisibleUnitGreyDistance;
    }

    m_MaxVisibleDistanceInFlight    = sConfig.GetFloatDefault("Visibility.Distance.InFlight",      DEFAULT_VISIBILITY_DISTANCE);
    if (m_MaxVisibleDistanceInFlight + m_VisibleObjectGreyDistance > MAX_VISIBILITY_DISTANCE)
    {
        sLog.outError("Visibility.Distance.InFlight can't be greater %f", MAX_VISIBILITY_DISTANCE - m_VisibleObjectGreyDistance);
        m_MaxVisibleDistanceInFlight = MAX_VISIBILITY_DISTANCE - m_VisibleObjectGreyDistance;
    }

    ///- Load the CharDelete related config options
    setConfigMinMax(CONFIG_UINT32_CHARDELETE_METHOD, "CharDelete.Method", 0, 0, 1);
    setConfigMinMax(CONFIG_UINT32_CHARDELETE_MIN_LEVEL, "CharDelete.MinLevel", 0, 0, getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
    setConfig(CONFIG_UINT32_CHARDELETE_KEEP_DAYS, "CharDelete.KeepDays", 30);

    if (configNoReload(reload, CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE, "GuidReserveSize.Creature", 100))
    {
        setConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE, "GuidReserveSize.Creature", 100);
    }
    if (configNoReload(reload, CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT, "GuidReserveSize.GameObject", 100))
    {
        setConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT, "GuidReserveSize.GameObject", 100);
    }

    ///- Read the "Data" directory from the config file
    std::string dataPath = sConfig.GetStringDefault("DataDir", "./");

    // for empty string use current dir as for absent case
    if (dataPath.empty())
    {
        dataPath = "./";
    }
    // normalize dir path to path/ or path\ form
    else if (dataPath.at(dataPath.length() - 1) != '/' && dataPath.at(dataPath.length() - 1) != '\\')
    {
        dataPath.append("/");
    }

    if (reload)
    {
        if (dataPath != m_dataPath)
        {
            sLog.outError("DataDir option can't be changed at mangosd.conf reload, using current value (%s).", m_dataPath.c_str());
        }
    }
    else
    {
        m_dataPath = dataPath;
        sLog.outString("Using DataDir %s", m_dataPath.c_str());
    }

    setConfig(CONFIG_BOOL_VMAP_INDOOR_CHECK, "vmap.enableIndoorCheck", true);
    bool enableLOS = sConfig.GetBoolDefault("vmap.enableLOS", false);
    bool enableHeight = sConfig.GetBoolDefault("vmap.enableHeight", false);
    std::string ignoreSpellIds = sConfig.GetStringDefault("vmap.ignoreSpellIds", "");

    if (!enableHeight)
    {
        sLog.outError("VMAP height use disabled! Creatures movements and other things will be in broken state.");
    }

    VMAP::VMapFactory::createOrGetVMapManager()->setEnableLineOfSightCalc(enableLOS);
    VMAP::VMapFactory::createOrGetVMapManager()->setEnableHeightCalc(enableHeight);
    VMAP::VMapFactory::preventSpellsFromBeingTestedForLoS(ignoreSpellIds.c_str());
    sLog.outString("WORLD: VMap support included. LineOfSight:%i, getHeight:%i, indoorCheck:%i",
                   enableLOS, enableHeight, getConfig(CONFIG_BOOL_VMAP_INDOOR_CHECK) ? 1 : 0);
    sLog.outString("WORLD: VMap data directory is: %svmaps", m_dataPath.c_str());

    setConfig(CONFIG_BOOL_MMAP_ENABLED, "mmap.enabled", true);
    std::string ignoreMapIds = sConfig.GetStringDefault("mmap.ignoreMapIds", "");
    MMAP::MMapFactory::preventPathfindingOnMaps(ignoreMapIds.c_str());
    sLog.outString("WORLD: MMap pathfinding %sabled", getConfig(CONFIG_BOOL_MMAP_ENABLED) ? "en" : "dis");

#ifdef ENABLE_ELUNA
    if (reload)
    {
        if (Eluna* e = GetEluna())
        {
            e->OnConfigLoad(reload);
        }
    }
#endif /* ENABLE_ELUNA */
    sLog.outString();
}

/// Initialize the World
void World::SetInitialWorldSettings()
{
    ///- Initialize the random number generator
    srand((unsigned int)time(NULL));

    ///- Time server startup
    uint32 startupBegin = GameTime::GetGameTimeMS();

    ///- Initialize detour memory management
    dtAllocSetCustom(dtCustomAlloc, dtCustomFree);

    ///- Initialize config settings
    LoadConfigSettings();

    ///- Initialize VMapManager function pointers (to untangle game/collision circular deps)
    if (VMAP::VMapManager2* vmmgr2 = dynamic_cast<VMAP::VMapManager2*>(VMAP::VMapFactory::createOrGetVMapManager()))
    {
        //vmmgr2->GetLiquidFlagsPtr = &GetLiquidFlags;
        vmmgr2->IsVMAPDisabledForPtr = &DisableMgr::IsVMAPDisabledFor;
    }

    ///- Check the existence of the map files for all races start areas.
    if (!MapManager::ExistMapAndVMap(0, -6240.32f, 331.033f) ||                     // Dwarf/ Gnome
        !MapManager::ExistMapAndVMap(0, -8949.95f, -132.493f) ||                // Human
        !MapManager::ExistMapAndVMap(1, -618.518f, -4251.67f) ||                // Orc
        !MapManager::ExistMapAndVMap(0, 1676.35f, 1677.45f) ||                  // Scourge
        !MapManager::ExistMapAndVMap(1, 10311.3f, 832.463f) ||                  // NightElf
        !MapManager::ExistMapAndVMap(1, -2917.58f, -257.98f))                   // Tauren
    {
        sLog.outError("Correct *.map files not found in path '%smaps' or *.vmtree/*.vmtile files in '%svmaps'. Please place *.map and vmap files in appropriate directories or correct the DataDir value in the mangosd.conf file.", m_dataPath.c_str(), m_dataPath.c_str());
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    ///- Loading strings. Getting no records means core load has to be canceled because no error message can be output.
    sLog.outString("Loading MaNGOS strings...");
    if (!sObjectMgr.LoadMangosStrings())
    {
        Log::WaitBeforeContinueIfNeed();
        exit(1);                                            // Error message displayed in function already
    }

    ///- Update the realm entry in the database with the realm type from the config file
    // No SQL injection as values are treated as integers

    // not send custom type REALM_FFA_PVP to realm list
    uint32 server_type = IsFFAPvPRealm() ? uint32(REALM_TYPE_PVP) : getConfig(CONFIG_UINT32_GAME_TYPE);
    uint32 realm_zone = getConfig(CONFIG_UINT32_REALM_ZONE);
    LoginDatabase.PExecute("UPDATE `realmlist` SET `icon` = %u, `timezone` = %u WHERE `id` = '%u'", server_type, realm_zone, realmID);

    ///- Remove the bones (they should not exist in DB though) and old corpses after a restart
    CharacterDatabase.PExecute("DELETE FROM `corpse` WHERE `corpse_type` = '0' OR `time` < (UNIX_TIMESTAMP()-'%u')", 3 * DAY);

    ///- Load the DBC files
    sLog.outString("Initialize DBC data stores...");
    LoadDBCStores(m_dataPath);
    DetectDBCLang();
    sObjectMgr.SetDBCLocaleIndex(GetDefaultDbcLocale());    // Get once for all the locale index of DBC language (console/broadcasts)

    sLog.outString("Loading Script Names...");
    sScriptMgr.LoadScriptNames();

    sLog.outString("Loading InstanceTemplate...");
    sObjectMgr.LoadInstanceTemplate();

    sLog.outString("Loading SkillLineAbilityMultiMap Data...");
    sSpellMgr.LoadSkillLineAbilityMap();

    sLog.outString("Loading SkillRaceClassInfoMultiMap Data...");
    sSpellMgr.LoadSkillRaceClassInfoMap();

    ///- Clean up and pack instances
    sLog.outString("Cleaning up instances...");
    sMapPersistentStateMgr.CleanupInstances();              // must be called before `creature_respawn`/`gameobject_respawn` tables

    sLog.outString("Packing instances...");
    sMapPersistentStateMgr.PackInstances();

    sLog.outString("Packing groups...");
    sObjectMgr.PackGroupIds();                              // must be after CleanupInstances

    ///- Init highest guids before any guid using table loading to prevent using not initialized guids in some code.
    sObjectMgr.SetHighestGuids();                           // must be after packing instances
    sLog.outString();

#ifdef ENABLE_ELUNA
    ///- Initialize Lua Engine

    // lua state begins uninitialized
    eluna = nullptr;

    sLog.outString("Loading Eluna config...");
    sElunaConfig->Initialize();

    if (sElunaConfig->IsElunaEnabled())
    {
        ///- Initialize Lua Engine
        sLog.outString("Loading Lua scripts...");
        sElunaLoader->LoadScripts();
    }
#endif /* ENABLE_ELUNA */

    sLog.outString("Loading Page Texts...");
    sObjectMgr.LoadPageTexts();

    sLog.outString("Loading Game Object Templates...");     // must be after LoadPageTexts
    sObjectMgr.LoadGameobjectInfo();

    sLog.outString("Loading GameObject models...");
    LoadGameObjectModelList();
    sLog.outString();

    sLog.outString("Loading Spell Chain Data...");
    sSpellMgr.LoadSpellChains();

    sLog.outString("Loading Spell Elixir types...");
    sSpellMgr.LoadSpellElixirs();

    sLog.outString("Loading Spell Facing Flags...");
    sSpellMgr.LoadFacingCasterFlags();

    sLog.outString("Loading Spell Learn Skills...");
    sSpellMgr.LoadSpellLearnSkills();                       // must be after LoadSpellChains

    sLog.outString("Loading Spell Learn Spells...");
    sSpellMgr.LoadSpellLearnSpells();

    sLog.outString("Loading Spell Proc Event conditions...");
    sSpellMgr.LoadSpellProcEvents();

    sLog.outString("Loading Spell Bonus Data...");
    sSpellMgr.LoadSpellBonuses();

    sLog.outString("Loading Spell Proc Item Enchant...");
    sSpellMgr.LoadSpellProcItemEnchant();                   // must be after LoadSpellChains

    sLog.outString("Loading Spell Linked definitions...");
    sSpellMgr.LoadSpellLinked();                            // must be after LoadSpellChains

    sLog.outString("Loading Aggro Spells Definitions...");
    sSpellMgr.LoadSpellThreats();

    sLog.outString("Loading NPC Texts...");
    sObjectMgr.LoadGossipText();

    sLog.outString("Loading Item Random Enchantments Table...");
    LoadRandomEnchantmentsTable();

    sLog.outString("Loading Disables...");                  // must be before loading quests and items
    DisableMgr::LoadDisables();

    sLog.outString("Loading Item Templates...");            // must be after LoadRandomEnchantmentsTable and LoadPageTexts
    sObjectMgr.LoadItemPrototypes();

    sLog.outString("Loading Creature Model Based Info Data...");
    sObjectMgr.LoadCreatureModelInfo();

    sLog.outString("Loading Creature Items...");
    sObjectMgr.LoadCreatureItemTemplates();

    sLog.outString("Loading Equipment templates...");
    sObjectMgr.LoadEquipmentTemplates();

    sLog.outString("Loading Creature Stats...");
    sObjectMgr.LoadCreatureClassLvlStats();

    sLog.outString("Loading Creature templates...");
    sObjectMgr.LoadCreatureTemplates();

    sLog.outString("Loading Creature template spells...");
    sObjectMgr.LoadCreatureTemplateSpells();

    sLog.outString("Loading Creature spells...");
    sObjectMgr.LoadCreatureSpells();

    sLog.outString("Loading SpellsScriptTarget...");
    sSpellMgr.LoadSpellScriptTarget();                      // must be after LoadCreatureTemplates and LoadGameobjectInfo

    sLog.outString("Loading ItemRequiredTarget...");
    sObjectMgr.LoadItemRequiredTarget();

    sLog.outString("Loading Reputation Reward Rates...");
    sObjectMgr.LoadReputationRewardRate();

    sLog.outString("Loading Creature Reputation OnKill Data...");
    sObjectMgr.LoadReputationOnKill();

    sLog.outString("Loading Reputation Spillover Data...");
    sObjectMgr.LoadReputationSpilloverTemplate();

    sLog.outString("Loading Points Of Interest Data...");
    sObjectMgr.LoadPointsOfInterest();

    sLog.outString("Loading Pet Create Spells...");
    sObjectMgr.LoadPetCreateSpells();

    sLog.outString("Loading Creature Data...");
    sObjectMgr.LoadCreatures();

    sLog.outString("Loading Creature Addon Data...");
    sObjectMgr.LoadCreatureAddons();                        // must be after LoadCreatureTemplates() and LoadCreatures()
    sLog.outString(">>> Creature Addon Data loaded");
    sLog.outString();

    sLog.outString("Loading Gameobject Data...");
    sObjectMgr.LoadGameObjects();

    sLog.outString("Loading CreatureLinking Data...");      // must be after Creatures
    sCreatureLinkingMgr.LoadFromDB();

    sLog.outString("Loading Objects Pooling Data...");
    sPoolMgr.LoadFromDB();

    sLog.outString("Loading Weather Data...");
    sWeatherMgr.LoadWeatherZoneChances();

    sLog.outString("Loading Quests...");
    sObjectMgr.LoadQuests();                                // must be loaded after DBCs, creature_template, item_template, gameobject tables

    sLog.outString("Loading Quests Relations...");
    sObjectMgr.LoadQuestRelations();                        // must be after quest load
    sLog.outString(">>> Quests Relations loaded");
    sLog.outString();

    sLog.outString("Checking Quest Disables...");
    DisableMgr::CheckQuestDisables();                       // must be after loading quests

    sLog.outString("Loading Game Event Data...");           // must be after sPoolMgr.LoadFromDB and quests to properly load pool events and quests for events
    sGameEventMgr.LoadFromDB();
    sLog.outString(">>> Game Event Data loaded");
    sLog.outString();

    // Load Conditions
    sLog.outString("Loading Conditions...");
    sObjectMgr.LoadConditions();

    sLog.outString("Creating map persistent states for non-instanceable maps...");     // must be after PackInstances(), LoadCreatures(), sPoolMgr.LoadFromDB(), sGameEventMgr.LoadFromDB();
    sMapPersistentStateMgr.InitWorldMaps();
    sLog.outString();

    sLog.outString("Loading Creature Respawn Data...");     // must be after LoadCreatures(), and sMapPersistentStateMgr.InitWorldMaps()
    sMapPersistentStateMgr.LoadCreatureRespawnTimes();

    sLog.outString("Loading Gameobject Respawn Data...");   // must be after LoadGameObjects(), and sMapPersistentStateMgr.InitWorldMaps()
    sMapPersistentStateMgr.LoadGameobjectRespawnTimes();

    sLog.outString("Loading SpellArea Data...");            // must be after quest load
    sSpellMgr.LoadSpellAreas();

    sLog.outString("Loading AreaTrigger definitions...");
    sObjectMgr.LoadAreaTriggerTeleports();                  // must be after item template load

    sLog.outString("Loading Quest Area Triggers...");
    sObjectMgr.LoadQuestAreaTriggers();                     // must be after LoadQuests

    sLog.outString("Loading Tavern Area Triggers...");
    sObjectMgr.LoadTavernAreaTriggers();

    //sLog.outString("Loading AreaTrigger script names...");
    //sScriptMgr.LoadAreaTriggerScripts();

    //sLog.outString("Loading event id script names...");
    //sScriptMgr.LoadEventIdScripts();

    //sLog.outString("Loading spell script names...");
    //sScriptMgr.LoadSpellIdScripts();

#ifdef ENABLE_SD3
    sLog.outString("Loading all script bindings...");
    sScriptMgr.LoadScriptBinding();
#endif /* ENABLE_SD3 */

    sLog.outString("Loading Graveyard-zone links...");
    sObjectMgr.LoadGraveyardZones();

    sLog.outString("Loading spell target destination coordinates...");
    sSpellMgr.LoadSpellTargetPositions();

    sLog.outString("Loading SpellAffect definitions...");
    sSpellMgr.LoadSpellAffects();

    sLog.outString("Loading spell pet auras...");
    sSpellMgr.LoadSpellPetAuras();

    sLog.outString("Loading Player Create Info & Level Stats...");
    sObjectMgr.LoadPlayerInfo();
    sLog.outString(">>> Player Create Info & Level Stats loaded");
    sLog.outString();

    sLog.outString("Loading Exploration BaseXP Data...");
    sObjectMgr.LoadExplorationBaseXP();

    sLog.outString("Loading Pet Name Parts...");
    sObjectMgr.LoadPetNames();

    CharacterDatabaseCleaner::CleanDatabase();
    sLog.outString();

    sLog.outString("Loading the max pet number...");
    sObjectMgr.LoadPetNumber();

    sLog.outString("Loading pet level stats...");
    sObjectMgr.LoadPetLevelInfo();

    sLog.outString("Loading Player Corpses...");
    sObjectMgr.LoadCorpses();

    sLog.outString("Loading Loot Tables...");
    LoadLootTables();
    sLog.outString(">>> Loot Tables loaded");
    sLog.outString();

    sLog.outString("Loading Skill Fishing base level requirements...");
    sObjectMgr.LoadFishingBaseSkillLevel();

    sLog.outString("Loading Gossip scripts...");
    sScriptMgr.LoadDbScripts(DBS_ON_GOSSIP);                 // must be before gossip menu options

    sObjectMgr.LoadGossipMenus();

    sLog.outString("Loading Vendors...");
    sObjectMgr.LoadVendorTemplates();                       // must be after load ItemTemplate
    sObjectMgr.LoadVendors();                               // must be after load CreatureTemplate, VendorTemplate, and ItemTemplate

    sLog.outString("Loading Trainers...");
    sObjectMgr.LoadTrainerTemplates();                      // must be after load CreatureTemplate
    sObjectMgr.LoadTrainers();                              // must be after load CreatureTemplate, TrainerTemplate

    sLog.outString("Loading Waypoint scripts...");          // before loading from creature_movement
    sScriptMgr.LoadDbScripts(DBS_ON_CREATURE_MOVEMENT);

    sLog.outString("Loading Waypoints...");
    sWaypointMgr.Load();

    sLog.outString("Modifying in-memory dbc spell attributes...");
    sSpellMgr.ModDBCSpellAttributes();

    sLog.outString("Loading ReservedNames...");
    sObjectMgr.LoadReservedPlayersNames();

    sLog.outString("Loading GameObjects for quests...");
    sObjectMgr.LoadGameObjectForQuests();

    sLog.outString("Loading BattleMasters...");
    sBattleGroundMgr.LoadBattleMastersEntry();

    sLog.outString("Loading BattleGround event indexes...");
    sBattleGroundMgr.LoadBattleEventIndexes();

    sLog.outString("Loading GameTeleports...");
    sObjectMgr.LoadGameTele();

    ///- Loading localization data
    sLog.outString("Loading Localization strings...");
    sObjectMgr.LoadCreatureLocales();                       // must be after CreatureInfo loading
    sObjectMgr.LoadGameObjectLocales();                     // must be after GameobjectInfo loading
    sObjectMgr.LoadItemLocales();                           // must be after ItemPrototypes loading
    sObjectMgr.LoadQuestLocales();                          // must be after QuestTemplates loading
    sObjectMgr.LoadGossipTextLocales();                     // must be after LoadGossipText
    sObjectMgr.LoadPageTextLocales();                       // must be after PageText loading
    sObjectMgr.LoadGossipMenuItemsLocales();                // must be after gossip menu items loading
    sObjectMgr.LoadPointOfInterestLocales();                // must be after POI loading
    sCommandMgr.LoadCommandHelpLocale();
    sLog.outString(">>> Localization strings loaded");
    sLog.outString();

    ///- Load dynamic data tables from the database
    sLog.outString("Loading Auctions...");
    sAuctionMgr.LoadAuctionItems();
    sAuctionMgr.LoadAuctions();
    sLog.outString(">>> Auctions loaded");
    sLog.outString();

    sLog.outString("Loading Guilds...");
    sGuildMgr.LoadGuilds();

    sLog.outString("Loading Groups...");
    sObjectMgr.LoadGroups();

    sLog.outString("Returning old mails...");
    sObjectMgr.ReturnOrDeleteOldMails(false);

    sLog.outString("Loading GM tickets...");
    sTicketMgr.LoadGMTickets();

#ifdef ENABLE_ELUNA
    if (sElunaConfig->IsElunaEnabled())
    {
        ///- Run eluna scripts.
        sLog.outString("Starting Eluna world state...");
        // use map id -1 for the global Eluna state
        eluna = new Eluna(nullptr, sElunaConfig->IsElunaCompatibilityMode());
        sLog.outString();
    }
#endif /*ENABLE_ELUNA*/

    ///- Load and initialize DBScripts Engine
    sLog.outString("Loading DB-Scripts Engine...");
    sScriptMgr.LoadDbScripts(DBS_ON_QUEST_START);           // must be after load Creature/Gameobject(Template/Data) and QuestTemplate
    sScriptMgr.LoadDbScripts(DBS_ON_QUEST_END);             // must be after load Creature/Gameobject(Template/Data) and QuestTemplate
    sScriptMgr.LoadDbScripts(DBS_ON_SPELL);                 // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_GO_USE);                // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_GOT_USE);               // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_EVENT);                 // must be after load Creature/Gameobject(Template/Data)
    sScriptMgr.LoadDbScripts(DBS_ON_CREATURE_DEATH);        // must be after load Creature/Gameobject(Template/Data)
    sLog.outString(">>> DB Scripts loaded");
    sLog.outString();

    sLog.outString("Loading Scripts text locales...");      // must be after Load*Scripts calls
    sScriptMgr.LoadDbScriptStrings();

    ///- Load and initialize EventAI Scripts
    sLog.outString("Loading CreatureEventAI Texts...");
    sEventAIMgr.LoadCreatureEventAI_Texts(false);           // false, will checked in LoadCreatureEventAI_Scripts

    sLog.outString("Loading CreatureEventAI Summons...");
    sEventAIMgr.LoadCreatureEventAI_Summons(false);         // false, will checked in LoadCreatureEventAI_Scripts

    sLog.outString("Loading CreatureEventAI Scripts...");
    sEventAIMgr.LoadCreatureEventAI_Scripts();

    sLog.outString("Initializing Scripts...");
#ifdef ENABLE_SD3
    switch (sScriptMgr.LoadScriptLibrary("mangosscript"))
    {
        case SCRIPT_LOAD_OK:
            sLog.outString("Scripting library loaded.");
            break;
        case SCRIPT_LOAD_ERR_NOT_FOUND:
            sLog.outError("Scripting library not found or not accessible.");
            break;
        case SCRIPT_LOAD_ERR_WRONG_API:
            sLog.outError("Scripting library has wrong list functions (outdated?).");
            break;
        case SCRIPT_LOAD_ERR_OUTDATED:
            sLog.outError("Scripting library build for old mangosd revision. You need rebuild it.");
            break;
    }
#else /* ENABLE_SD3 */
    sLog.outError("SD3 was not included in compilation, not using it.");
#endif /* ENABLE_SD3 */
    sLog.outString();

    ///- Initialize game time and timers
    sLog.outString("Initialize game time and timers");
    m_gameTime = time(NULL);
    m_startTime = m_gameTime;

    tm local;
    time_t curr;
    time(&curr);
    local = *(localtime(&curr));                            // dereference and assign
    char isoDate[128];
    sprintf(isoDate, "%04d-%02d-%02d %02d:%02d:%02d",
            local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec);

    LoginDatabase.PExecute("INSERT INTO `uptime` (`realmid`, `starttime`, `startstring`, `uptime`) VALUES('%u', " UI64FMTD ", '%s', 0)",
                           realmID, uint64(m_startTime), isoDate);

    m_timers[WUPDATE_AUCTIONS].SetInterval(MINUTE * IN_MILLISECONDS);
    m_timers[WUPDATE_UPTIME].SetInterval(getConfig(CONFIG_UINT32_UPTIME_UPDATE) * MINUTE * IN_MILLISECONDS);
    // Update "uptime" table based on configuration entry in minutes.
    m_timers[WUPDATE_CORPSES].SetInterval(20 * MINUTE * IN_MILLISECONDS);
    m_timers[WUPDATE_DELETECHARS].SetInterval(DAY * IN_MILLISECONDS); // check for chars to delete every day

    // for AhBot
    m_timers[WUPDATE_AHBOT].SetInterval(20 * IN_MILLISECONDS); // every 20 sec

    // for AutoBroadcast
    sLog.outString("Starting AutoBroadcast System");
    if (m_broadcastEnable)
    {
        LoadBroadcastStrings();
    }
    else
    {
        sLog.outString("AutoBroadcast is disabled");
    }
    sLog.outString();

    if (m_broadcastEnable)
    {
        m_broadcastTimer.SetInterval(getConfig(CONFIG_UINT32_AUTOBROADCAST_INTERVAL) * IN_MILLISECONDS);
    }

    // to set mailtimer to return mails every day between 4 and 5 am
    // mailtimer is increased when updating auctions
    // one second is 1000 -(tested on win system)
    mail_timer = uint32((((localtime(&m_gameTime)->tm_hour + 20) % 24) * HOUR * IN_MILLISECONDS) / m_timers[WUPDATE_AUCTIONS].GetInterval());
    // 1440
    mail_timer_expires = uint32((DAY * IN_MILLISECONDS) / (m_timers[WUPDATE_AUCTIONS].GetInterval()));
    DEBUG_LOG("Mail timer set to: %u, mail return is called every %u minutes", mail_timer, mail_timer_expires);

    ///- Initialize static helper structures
    AIRegistry::Initialize();
    Player::InitVisibleBits();

    ///- Initialize MapManager
    sLog.outString("Starting Map System");
    sMapMgr.Initialize();
    sLog.outString();

    ///- Initialize Battlegrounds
    sLog.outString("Starting BattleGround System");
    sBattleGroundMgr.CreateInitialBattleGrounds();

    ///- Initialize Outdoor PvP
    sLog.outString("Starting Outdoor PvP System");
    sOutdoorPvPMgr.InitOutdoorPvP();


    // Initialize Warden
    sLog.outString("Loading Warden Checks...");
    sWardenCheckMgr->LoadWardenChecks();
    sLog.outString();

    sLog.outString("Loading Warden Action Overrides...");
    sWardenCheckMgr->LoadWardenOverrides();
    sLog.outString();

    sLog.outString("Deleting expired bans...");
    LoginDatabase.Execute("DELETE FROM `ip_banned` WHERE `unbandate`<=UNIX_TIMESTAMP() AND `unbandate`<>`bandate`");
    sLog.outString();

    sLog.outString("Starting server Maintenance system...");
    InitServerMaintenanceCheck();

    sLog.outString("Loading Honor Standing list...");
    sObjectMgr.LoadStandingList();

    sLog.outString("Starting Game Event system...");
    uint32 nextGameEvent = sGameEventMgr.Initialize();
    m_timers[WUPDATE_EVENTS].SetInterval(nextGameEvent);    // depend on next event
    sLog.outString();

    sLog.outString("Loading grids for active creatures and local transports...");
    sMapMgr.LoadContinents();
    sLog.outString();

    sLog.outString("Loading global transports...");
    sMapMgr.LoadTransports();
    sLog.outString();

    // Delete all characters which have been deleted X days before
    Player::DeleteOldCharacters();

    sLog.outString("Initialize AuctionHouseBot...");
    sAuctionBot.Initialize();
    sLog.outString();

#ifdef ENABLE_ELUNA
    ///- Run eluna scripts.
    // in multithread foreach: run scripts
    if (Eluna* e = GetEluna())
    {
        e->OnConfigLoad(false); // Must be done after Eluna is initialized and scripts have run.
    }
#endif

#ifdef ENABLE_PLAYERBOTS
    sPlayerbotAIConfig.Initialize();
#endif

    showFooter();

    uint32 startupDuration = GetMSTimeDiffToNow(startupBegin);
    sLog.outString("SERVER STARTUP TIME: %i minutes %i seconds", (startupDuration / 60000), ((startupDuration % 60000) / 1000));
    sLog.outString();
}

void World::showFooter()
{
    std::set<std::string> modules_;

    // ELUNA is either included or disabled
#ifdef ENABLE_ELUNA
    modules_.insert("                 Eluna : Enabled");
#endif

    // SD3 is either included or disabled
#ifdef ENABLE_SD3
    modules_.insert("      ScriptDev3 (SD3) : Enabled");
#endif

    // PLAYERBOTS can be included or excluded but also disabled via mangos.conf
#ifdef ENABLE_PLAYERBOTS
    bool playerBotActive = sConfig.GetBoolDefault("PlayerbotAI.DisableBots", true);
    if (playerBotActive)
    {
        modules_.insert("            PlayerBots : Disabled");
    }
    else
    {
        modules_.insert("            PlayerBots : Enabled");
    }
#endif

    // Remote Access can be activated / deactivated via mangos.conf
    bool raActive = sConfig.GetBoolDefault("Ra.Enable", false);
    if (raActive)
    {
        modules_.insert("    Remote Access (RA) : Enabled");
    }
    else
    {
        modules_.insert("    Remote Access (RA) : Disabled");
    }

    // SOAP can be included or excluded but also disabled via mangos.conf
#ifdef ENABLE_SOAP
    bool soapActive = sConfig.GetBoolDefault("SOAP.Enabled", false);
    if (soapActive)
    {
        modules_.insert("                  SOAP : Enabled");
    }
    else
    {
        modules_.insert("                  SOAP : Disabled");
    }
#endif

    // Warden is always included, set active or disabled via mangos.conf
    bool wardenActive = (sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED) || sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED));
    if (wardenActive)
    {
        modules_.insert("                Warden : Enabled");
    }
    else
    {
        modules_.insert("                Warden : Disabled");
    }

    std::string thisClientVersion (EXPECTED_MANGOSD_CLIENT_VERSION);
    std::string thisClientBuilds = AcceptableClientBuildsListStr();

    std::string sModules;
    for (std::set<std::string>::const_iterator it = modules_.begin(); it != modules_.end(); ++it)
    {
        sModules = sModules + " \n" + *it;
    }

    sLog.outString("\n"
        "_______________________________________________________\n"
        "\n"
        " MaNGOS Server: World Initialization Complete\n"
        "_______________________________________________________\n"
        "\n"
        "        Server Version : %s\n"
        "         Eluna Version : %s\n"
        "           SD3 Version : %s\n"
        "      Database Version : Rel%s.%s.%s\n"
        "\n"
        "    Supporting Clients : %s\n"
        "                Builds : %s\n"
        "\n"
        "         Module Status -\n%s\n"
        "_______________________________________________________\n"
        , GitRevision::GetProductVersionStr(), GitRevision::GetDepElunaFullRevision(), GitRevision::GetDepSD3FullRevision(), GitRevision::GetWorldDBVersion(), GitRevision::GetWorldDBStructure(), GitRevision::GetWorldDBContent(),
            thisClientVersion.c_str(), thisClientBuilds.c_str(), sModules.c_str());
}

void World::DetectDBCLang()
{
    uint32 m_lang_confid = sConfig.GetIntDefault("DBC.Locale", 255);

    if (m_lang_confid != 255 && m_lang_confid >= MAX_LOCALE)
    {
        sLog.outError("Incorrect DBC.Locale! Must be >= 0 and < %d (set to 0)", MAX_LOCALE);
        m_lang_confid = LOCALE_enUS;
    }

    ChrRacesEntry const* race = sChrRacesStore.LookupEntry(RACE_HUMAN);
    MANGOS_ASSERT(race);

    std::string availableLocalsStr;

    uint32 default_locale = MAX_LOCALE;
    for (int i = MAX_LOCALE - 1; i >= 0; --i)
    {
        if (strlen(race->name[i]) > 0)                      // check by race names
        {
            default_locale = i;
            m_availableDbcLocaleMask |= (1 << i);
            availableLocalsStr += localeNames[i];
            availableLocalsStr += " ";
        }
    }

    if (default_locale != m_lang_confid && m_lang_confid < MAX_LOCALE &&
        (m_availableDbcLocaleMask & (1 << m_lang_confid)))
    {
        default_locale = m_lang_confid;
    }

    if (default_locale >= MAX_LOCALE)
    {
        sLog.outError("Unable to determine your DBC Locale! (corrupt DBC?)");
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    m_defaultDbcLocale = LocaleConstant(default_locale);

    sLog.outString("Using %s DBC Locale as default. All available DBC locales: %s", localeNames[m_defaultDbcLocale], availableLocalsStr.empty() ? "<none>" : availableLocalsStr.c_str());
    sLog.outString();
}

/// Update the World !
void World::Update(uint32 diff)
{
    ///- Update the different timers
    for (int i = 0; i < WUPDATE_COUNT; ++i)
    {
        if (m_timers[i].GetCurrent() >= 0)
        {
            m_timers[i].Update(diff);
        }
        else
        {
            m_timers[i].SetCurrent(0);
        }
    }

    if (m_broadcastEnable)
    {
        if (m_broadcastTimer.GetCurrent() >= 0)
        {
            m_broadcastTimer.Update(diff);
        }
        else
        {
            m_broadcastTimer.SetCurrent(0);
        }

        if (m_broadcastTimer.Passed())
        {
            m_broadcastTimer.Reset();
            AutoBroadcast();
        }
    }

    ///- Update the game time and check for shutdown time
    _UpdateGameTime();
    GameTime::UpdateGameTimers();
    sWorldUpdateTime.UpdateWithDiff(diff);

    ///-Update mass mailer tasks if any
    sMassMailMgr.Update();

    /// <ul><li> Handle auctions when the timer has passed
    if (m_timers[WUPDATE_AUCTIONS].Passed())
    {
        m_timers[WUPDATE_AUCTIONS].Reset();

        ///- Update mails (return old mails with item, or delete them)
        //(tested... works on win)
        if (++mail_timer > mail_timer_expires)
        {
            mail_timer = 0;
            sObjectMgr.ReturnOrDeleteOldMails(true);
        }

        ///- Handle expired auctions
        sAuctionMgr.Update();
    }

    /// <li> Handle AHBot operations
    if (m_timers[WUPDATE_AHBOT].Passed())
    {
        sAuctionBot.Update();
        m_timers[WUPDATE_AHBOT].Reset();
    }

#ifdef ENABLE_PLAYERBOTS
    sRandomPlayerbotMgr.UpdateAI(diff);
    sRandomPlayerbotMgr.UpdateSessions(diff);
#endif

    /// <li> Handle session updates
    UpdateSessions(diff);

    /// <li> Update uptime table
    if (m_timers[WUPDATE_UPTIME].Passed())
    {
        uint32 tmpDiff = uint32(m_gameTime - m_startTime);
        uint32 maxClientsNum = GetMaxActiveSessionCount();

        m_timers[WUPDATE_UPTIME].Reset();
        LoginDatabase.PExecute("UPDATE `uptime` SET `uptime` = %u, `maxplayers` = %u WHERE `realmid` = %u AND `starttime` = " UI64FMTD, tmpDiff, maxClientsNum, realmID, uint64(m_startTime));
    }

    /// <li> Handle all other objects
    ///- Update objects (maps, transport, creatures,...)
    sMapMgr.Update(diff);
    sBattleGroundMgr.Update(diff);
    sLFGMgr.Update(diff);
    sOutdoorPvPMgr.Update(diff);

    ///- Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->UpdateEluna(diff);
        e->OnWorldUpdate(diff);
    }
#endif /* ENABLE_ELUNA */

    ///- Delete all characters which have been deleted X days before
    if (m_timers[WUPDATE_DELETECHARS].Passed())
    {
        m_timers[WUPDATE_DELETECHARS].Reset();
        Player::DeleteOldCharacters();
    }

    // execute callbacks from sql queries that were queued recently
    UpdateResultQueue();

    ///- Erase corpses once every 20 minutes
    if (m_timers[WUPDATE_CORPSES].Passed())
    {
        m_timers[WUPDATE_CORPSES].Reset();

        sObjectAccessor.RemoveOldCorpses();
    }

    ///- Process Game events when necessary
    if (m_timers[WUPDATE_EVENTS].Passed())
    {
        m_timers[WUPDATE_EVENTS].Reset();                   // to give time for Update() to be processed
        uint32 nextGameEvent = sGameEventMgr.Update();
        m_timers[WUPDATE_EVENTS].SetInterval(nextGameEvent);
        m_timers[WUPDATE_EVENTS].Reset();
    }

    /// </ul>
    ///- Move all creatures with "delayed move" and remove and delete all objects with "delayed remove"
    sMapMgr.RemoveAllObjectsInRemoveList();

    // update the instance reset times
    sMapPersistentStateMgr.Update();

    if (m_MaintenanceTimeChecker < diff)
    {
        if (GetDateToday() >= m_NextMaintenanceDate)
        {
            ServerMaintenanceStart();
            sObjectMgr.LoadStandingList();
        }
        m_MaintenanceTimeChecker = 600000; // check 10 minutes
    }
    else
    {
        m_MaintenanceTimeChecker -= diff;
    }

    // And last, but not least handle the issued cli commands
    ProcessCliCommands();

    // cleanup unused GridMap objects as well as VMaps
    sTerrainMgr.Update(diff);
}

namespace MaNGOS
{
    class WorldWorldTextBuilder
    {
        public:
            typedef std::vector<WorldPacket*> WorldPacketList;
            explicit WorldWorldTextBuilder(int32 textId, va_list* args = NULL) : i_textId(textId), i_args(args) {}
            void operator()(WorldPacketList& data_list, int32 loc_idx)
            {
                char const* text = sObjectMgr.GetMangosString(i_textId, loc_idx);

                if (i_args)
                {
                    // we need copy va_list before use or original va_list will corrupted
                    va_list ap;
                    va_copy(ap, *i_args);

                    char str [2048];
                    vsnprintf(str, 2048, text, ap);
                    va_end(ap);

                    do_helper(data_list, &str[0]);
                }
                else
                {
                    do_helper(data_list, (char*)text);
                }
            }
        private:
            char* lineFromMessage(char*& pos)
            {
                char* start = strtok(pos, "\n");
                pos = NULL;
                return start;
            }
            void do_helper(WorldPacketList& data_list, char* text)
            {
                char* pos = text;

                while (char* line = lineFromMessage(pos))
                {
                    WorldPacket* data = new WorldPacket();
                    ChatHandler::BuildChatPacket(*data, CHAT_MSG_SYSTEM, line);
                    data_list.push_back(data);
                }
            }

            int32 i_textId;
            va_list* i_args;
    };
}                                                           // namespace MaNGOS

/// Sends a system message to all players
void World::SendWorldText(int32 string_id, ...)
{
    va_list ap;
    va_start(ap, string_id);

    MaNGOS::WorldWorldTextBuilder wt_builder(string_id, &ap);
    MaNGOS::LocalizedPacketListDo<MaNGOS::WorldWorldTextBuilder> wt_do(wt_builder);
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld())
            {
                wt_do(player);
            }
        }
    }

    va_end(ap);
}

/// Sends a packet to all players with optional account access level restrictions
void World::SendGlobalMessage(WorldPacket* packet, AccountTypes minSec)
{
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            if (session->GetSecurity() < minSec)
            {
                continue;
            }
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld())
            {
                session->SendPacket(packet);
            }
        }
    }
}

/// Sends a server message to the specified or all players
void World::SendServerMessage(ServerMessageType type, const char* text /*=""*/, Player* player /*= NULL*/)
{
    WorldPacket data(SMSG_SERVER_MESSAGE, 50);              // guess size
    data << uint32(type);
    data << text;

    if (player)
    {
        player->GetSession()->SendPacket(&data);
    }
    else
    {
        SendGlobalMessage(&data);
    }
}

/// Sends a zone under attack message to all players not in an instance
void World::SendZoneUnderAttackMessage(uint32 zoneId, Team team)
{
    WorldPacket data(SMSG_ZONE_UNDER_ATTACK, 4);
    data << uint32(zoneId);

    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld() && player->GetTeam() == team && !player->GetMap()->Instanceable())
            {
                itr->second->SendPacket(&data);
            }
        }
    }
}

/// Sends a world defense message to all players not in an instance
void World::SendDefenseMessage(uint32 zoneId, int32 textId)
{
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        if (WorldSession* session = itr->second)
        {
            Player* player = session->GetPlayer();
            if (player && player->IsInWorld() && !player->GetMap()->Instanceable())
            {
                char const* message = session->GetMangosString(textId);
                uint32 messageLength = strlen(message) + 1;

                WorldPacket data(SMSG_DEFENSE_MESSAGE, 4 + 4 + messageLength);
                data << uint32(zoneId);
                data << uint32(messageLength);
                data << message;
                session->SendPacket(&data);
            }
        }
    }
}

/// Kick (and save) all players
void World::KickAll()
{
    m_QueuedSessions.clear();                               // prevent send queue update packet and login queued sessions

    // session not removed at kick and will removed in next update tick
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
    {
        itr->second->KickPlayer();
    }
}

/// Kick (and save) all players with security level less `sec`
void World::KickAllLess(AccountTypes sec)
{
    // session not removed at kick and will removed in next update tick
    for (SessionMap::const_iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
        if (WorldSession* session = itr->second)
            if (session->GetSecurity() < sec)
            {
                session->KickPlayer();
            }
}

/// Ban an account or ban an IP address, duration_secs if it is positive used, otherwise permban
BanReturn World::BanAccount(BanMode mode, std::string nameOrIP, uint32 duration_secs, std::string reason, const std::string &author)
{
    LoginDatabase.escape_string(nameOrIP);
    LoginDatabase.escape_string(reason);
    std::string safe_author = author;
    LoginDatabase.escape_string(safe_author);

    QueryResult* resultAccounts;                     // used for kicking

    ///- Update the database with ban information
    switch (mode)
    {
        case BAN_IP:
            // No SQL injection as strings are escaped
            resultAccounts = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `last_ip` = '%s'", nameOrIP.c_str());
            LoginDatabase.PExecute("INSERT INTO `ip_banned` VALUES ('%s',UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+%u,'%s','%s')", nameOrIP.c_str(), duration_secs, safe_author.c_str(), reason.c_str());
            break;
        case BAN_ACCOUNT:
            // No SQL injection as string is escaped
            resultAccounts = LoginDatabase.PQuery("SELECT `id` FROM `account` WHERE `username` = '%s'", nameOrIP.c_str());
            break;
        case BAN_CHARACTER:
            // No SQL injection as string is escaped
            resultAccounts = CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `name` = '%s'", nameOrIP.c_str());
            break;
        default:
            return BAN_SYNTAX_ERROR;
    }

    if (!resultAccounts)
    {
        if (mode == BAN_IP)
        {
            return BAN_SUCCESS;
        }                             // ip correctly banned but nobody affected (yet)
        else
        {
            return BAN_NOTFOUND;
        }                            // Nobody to ban
    }

    ///- Disconnect all affected players (for IP it can be several)
    do
    {
        Field* fieldsAccount = resultAccounts->Fetch();
        uint32 account = fieldsAccount->GetUInt32();

        if (mode != BAN_IP)
        {
            // No SQL injection as strings are escaped
            LoginDatabase.PExecute("INSERT INTO `account_banned` VALUES ('%u', UNIX_TIMESTAMP(), UNIX_TIMESTAMP()+%u, '%s', '%s', '1')",
                                   account, duration_secs, safe_author.c_str(), reason.c_str());
        }

        if (WorldSession* sess = FindSession(account))
        {
            if (std::string(sess->GetPlayerName()) != author)
            {
                sess->LogoutPlayer(true);
                sess->KickPlayer();
            }
        }
    }
    while (resultAccounts->NextRow());

    delete resultAccounts;
    return BAN_SUCCESS;
}

/// Remove a ban from an account or IP address
bool World::RemoveBanAccount(BanMode mode, std::string nameOrIP)
{
    if (mode == BAN_IP)
    {
        LoginDatabase.escape_string(nameOrIP);
        LoginDatabase.PExecute("DELETE FROM `ip_banned` WHERE `ip` = '%s'", nameOrIP.c_str());
    }
    else
    {
        uint32 account = 0;
        if (mode == BAN_ACCOUNT)
        {
            account = sAccountMgr.GetId(nameOrIP);
        }
        else if (mode == BAN_CHARACTER)
        {
            account = sObjectMgr.GetPlayerAccountIdByPlayerName(nameOrIP);
        }

        if (!account)
        {
            return false;
        }

        // NO SQL injection as account is uint32
        LoginDatabase.PExecute("UPDATE `account_banned` SET `active` = '0' WHERE `id` = '%u'", account);
    }
    return true;
}

/// Update the game time
void World::_UpdateGameTime()
{
    ///- update the time
    time_t thisTime = time(NULL);
    uint32 elapsed = uint32(thisTime - m_gameTime);
    m_gameTime = thisTime;

    ///- if there is a shutdown timer
    if (!m_stopEvent && m_ShutdownTimer > 0 && elapsed > 0)
    {
        ///- ... and it is overdue, stop the world (set m_stopEvent)
        if (m_ShutdownTimer <= elapsed)
        {
            if (!(m_ShutdownMask & SHUTDOWN_MASK_IDLE) || GetActiveAndQueuedSessionCount() == 0)
            {
                m_stopEvent = true;
            }                         // exist code already set
            else
            {
                m_ShutdownTimer = 1;
            }                        // minimum timer value to wait idle state
        }
        ///- ... else decrease it and if necessary display a shutdown countdown to the users
        else
        {
            m_ShutdownTimer -= elapsed;

            ShutdownMsg();
        }
    }
}

/// Shutdown the server
void World::ShutdownServ(uint32 time, uint32 options, uint8 exitcode)
{
    // ignore if server shutdown at next tick
    if (m_stopEvent)
    {
        return;
    }

    m_ShutdownMask = options;
    m_ExitCode = exitcode;

    ///- If the shutdown time is 0, set m_stopEvent (except if shutdown is 'idle' with remaining sessions)
    if (time == 0)
    {
        if (!(options & SHUTDOWN_MASK_IDLE) || GetActiveAndQueuedSessionCount() == 0)
        {
                sObjectAccessor.SaveAllPlayers();        // save all players.
                m_stopEvent = true;                                // exist code already set
        }
        else
        {
            m_ShutdownTimer = 1;
        }                            // So that the session count is re-evaluated at next world tick
    }
    ///- Else set the shutdown timer and warn users
    else
    {
        m_ShutdownTimer = time;
        ShutdownMsg(true);
    }

#ifdef ENABLE_PLAYERBOTS
    sRandomPlayerbotMgr.LogoutAllBots();
#endif

    ///- Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnShutdownInitiate(ShutdownExitCode(exitcode), ShutdownMask(options));
    }
#endif /* ENABLE_ELUNA */
}

/// Display a shutdown message to the user(s)
void World::ShutdownMsg(bool show /*= false*/, Player* player /*= NULL*/)
{
    // not show messages for idle shutdown mode
    if (m_ShutdownMask & SHUTDOWN_MASK_IDLE)
    {
        return;
    }

    ///- Display a message every 12 hours, 1 hour, 5 minutes, 1 minute and 15 seconds
    if (show ||
        (m_ShutdownTimer < 5 * MINUTE && (m_ShutdownTimer % 15) == 0) ||            // < 5 min; every 15 sec
        (m_ShutdownTimer < 15 * MINUTE && (m_ShutdownTimer % MINUTE) == 0) ||       // < 15 min; every 1 min
        (m_ShutdownTimer < 30 * MINUTE && (m_ShutdownTimer % (5 * MINUTE)) == 0) || // < 30 min; every 5 min
        (m_ShutdownTimer < 12 * HOUR && (m_ShutdownTimer % HOUR) == 0) ||           // < 12 h; every 1 h
        (m_ShutdownTimer >= 12 * HOUR && (m_ShutdownTimer % (12 * HOUR)) == 0))     // >= 12 h; every 12 h
    {
        std::string str = secsToTimeString(m_ShutdownTimer, TimeFormat::Numeric);

        ServerMessageType msgid = (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? SERVER_MSG_RESTART_TIME : SERVER_MSG_SHUTDOWN_TIME;

        SendServerMessage(msgid, str.c_str(), player);
        DEBUG_LOG("Server is %s in %s", (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? "restart" : "shutting down", str.c_str());
    }
}

/// Cancel a planned server shutdown
void World::ShutdownCancel()
{
    // nothing cancel or too later
    if (!m_ShutdownTimer || m_stopEvent)
    {
        return;
    }

    ServerMessageType msgid = (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? SERVER_MSG_RESTART_CANCELLED : SERVER_MSG_SHUTDOWN_CANCELLED;

    m_ShutdownMask = 0;
    m_ShutdownTimer = 0;
    m_ExitCode = SHUTDOWN_EXIT_CODE;                       // to default value
    SendServerMessage(msgid);

    DEBUG_LOG("Server %s cancelled.", (m_ShutdownMask & SHUTDOWN_MASK_RESTART) ? "restart" : "shutdown");

    ///- Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnShutdownCancel();
    }
#endif /* ENABLE_ELUNA */
}

void World::UpdateSessions(uint32 /*diff*/)
{
    ///- Add new sessions
    WorldSession* sess;
    while (addSessQueue.next(sess))
    {
        AddSession_(sess);
    }

    ///- Then send an update signal to remaining ones
    for (SessionMap::iterator itr = m_sessions.begin(), next; itr != m_sessions.end(); itr = next)
    {
        next = itr;
        ++next;
        ///- and remove not active sessions from the list
        WorldSession* pSession = itr->second;
        WorldSessionFilter updater(pSession);

        if (!pSession->Update(updater))
        {
            RemoveQueuedSession(pSession);
            m_sessions.erase(itr);
            delete pSession;
        }
    }
}

void World::ServerMaintenanceStart()
{
    uint32 LastWeekEnd    = GetDateLastMaintenanceDay();
    m_NextMaintenanceDate   = LastWeekEnd + 7; // next maintenance begin

    if (m_NextMaintenanceDate <= GetDateToday())            // avoid loop in manually case, maybe useless
    {
        m_NextMaintenanceDate += 7;
    }

    // flushing rank points list ( standing must be reloaded after server maintenance )
    sObjectMgr.FlushRankPoints(LastWeekEnd);

    // save and update all online players
    for (SessionMap::iterator itr = m_sessions.begin(); itr != m_sessions.end(); ++itr)
        if (itr->second->GetPlayer() && itr->second->GetPlayer()->IsInWorld())
        {
            itr->second->GetPlayer()->SaveToDB();
        }

    CharacterDatabase.PExecute("UPDATE `saved_variables` SET `NextMaintenanceDate` = '" UI64FMTD "'", uint64(m_NextMaintenanceDate));
}

void World::InitServerMaintenanceCheck()
{
    QueryResult* result = CharacterDatabase.Query("SELECT `NextMaintenanceDate` FROM `saved_variables`");
    if (!result)
    {
        DEBUG_LOG("Maintenance date not found in SavedVariables, reseting it now.");
        uint32 mDate = GetDateLastMaintenanceDay();
        m_NextMaintenanceDate = mDate == GetDateToday() ?  mDate : mDate + 7;
        CharacterDatabase.PExecute("INSERT INTO `saved_variables` (`NextMaintenanceDate`) VALUES ('" UI64FMTD "')", uint64(m_NextMaintenanceDate));
    }
    else
    {
        m_NextMaintenanceDate = (*result)[0].GetUInt64();
        delete result;
    }

    if (m_NextMaintenanceDate <= GetDateToday())
    {
        ServerMaintenanceStart();
    }

    DEBUG_LOG("Server maintenance check initialized.");
}

// This handles the issued and queued CLI/RA commands
void World::ProcessCliCommands()
{
    CliCommandHolder* command;
    while (cliCmdQueue.next(command))
    {
        DEBUG_LOG("CLI command under processing...");
        CliCommandHolder::Print* zprint = command->m_print;
        void* callbackArg = command->m_callbackArg;
        CliHandler handler(command->m_cliAccountId, command->m_cliAccessLevel, callbackArg, zprint);
        handler.ParseCommands(command->m_command);

        if (command->m_commandFinished)
        {
            command->m_commandFinished(callbackArg, !handler.HasSentErrorMessage());
        }

        delete command;
    }
}

void World::InitResultQueue()
{
}

void World::UpdateResultQueue()
{
    // process async result queues
    CharacterDatabase.ProcessResultQueue();
    WorldDatabase.ProcessResultQueue();
    LoginDatabase.ProcessResultQueue();
}

void World::UpdateRealmCharCount(uint32 accountId)
{
    CharacterDatabase.AsyncPQuery(this, &World::_UpdateRealmCharCount, accountId,
                                  "SELECT COUNT(`guid`) FROM `characters` WHERE `account` = '%u'", accountId);
}

void World::_UpdateRealmCharCount(QueryResult* resultCharCount, uint32 accountId)
{
    if (resultCharCount)
    {
        Field* fields = resultCharCount->Fetch();
        uint32 charCount = fields[0].GetUInt32();
        delete resultCharCount;

        LoginDatabase.BeginTransaction();
        LoginDatabase.PExecute("DELETE FROM `realmcharacters` WHERE `acctid`= '%u' AND `realmid` = '%u'", accountId, realmID);
        LoginDatabase.PExecute("INSERT INTO `realmcharacters` (`numchars`, `acctid`, `realmid`) VALUES (%u, %u, %u)", charCount, accountId, realmID);
        LoginDatabase.CommitTransaction();
    }
}

void World::SetPlayerLimit(int32 limit, bool needUpdate)
{
    if (limit < -SEC_ADMINISTRATOR)
    {
        limit = -SEC_ADMINISTRATOR;
    }

    // lock update need
    bool db_update_need = needUpdate || (limit < 0) != (m_playerLimit < 0) || (limit < 0 && m_playerLimit < 0 && limit != m_playerLimit);

    m_playerLimit = limit;

    if (db_update_need)
        LoginDatabase.PExecute("UPDATE `realmlist` SET `allowedSecurityLevel` = '%u' WHERE `id` = '%u'",
                               uint32(GetPlayerSecurityLimit()), realmID);
}

void World::UpdateMaxSessionCounters()
{
    m_maxActiveSessionCount = std::max(m_maxActiveSessionCount, uint32(m_sessions.size() - m_QueuedSessions.size()));
    m_maxQueuedSessionCount = std::max(m_maxQueuedSessionCount, uint32(m_QueuedSessions.size()));
}

void World::LoadDBVersion()
{
    QueryResult* result = WorldDatabase.Query("SELECT `version`, `structure`, `content` FROM `db_version` ORDER BY `version` DESC, `structure` DESC, `content` DESC LIMIT 1");
    if (result)
    {
        Field* fields = result->Fetch();

        uint32 version = fields[0].GetUInt32();
        uint32 structure = fields[1].GetUInt32();
        uint32 content = fields[2].GetUInt32();

        delete result;

        std::stringstream ss;
        ss << "Version: " << version << ", Structure: " << structure << ", Content: " << content;

        m_DBVersion = ss.str();
    }

    if (m_DBVersion.empty())
    {
        m_DBVersion = "Unknown world database.";
    }
}

void World::setConfig(eConfigUInt32Values index, char const* fieldname, uint32 defvalue)
{
    setConfig(index, sConfig.GetIntDefault(fieldname, defvalue));
    if (int32(getConfig(index)) < 0)
    {
        sLog.outError("%s (%i) can't be negative. Using %u instead.", fieldname, int32(getConfig(index)), defvalue);
        setConfig(index, defvalue);
    }
}

void World::setConfig(eConfigInt32Values index, char const* fieldname, int32 defvalue)
{
    setConfig(index, sConfig.GetIntDefault(fieldname, defvalue));
}

void World::setConfig(eConfigFloatValues index, char const* fieldname, float defvalue)
{
    setConfig(index, sConfig.GetFloatDefault(fieldname, defvalue));
}

void World::setConfig(eConfigBoolValues index, char const* fieldname, bool defvalue)
{
    setConfig(index, sConfig.GetBoolDefault(fieldname, defvalue));
}

void World::setConfigPos(eConfigFloatValues index, char const* fieldname, float defvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < 0.0f)
    {
        sLog.outError("%s (%f) can't be negative. Using %f instead.", fieldname, getConfig(index), defvalue);
        setConfig(index, defvalue);
    }
}

void World::setConfigMin(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%u) must be >= %u. Using %u instead.", fieldname, getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

void World::setConfigMin(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%i) must be >= %i. Using %i instead.", fieldname, getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

void World::setConfigMin(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%f) must be >= %f. Using %f instead.", fieldname, getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

void World::setConfigMinMax(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue, uint32 maxvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%u) must be in range %u...%u. Using %u instead.", fieldname, getConfig(index), minvalue, maxvalue, minvalue);
        setConfig(index, minvalue);
    }
    else if (getConfig(index) > maxvalue)
    {
        sLog.outError("%s (%u) must be in range %u...%u. Using %u instead.", fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
}

void World::setConfigMinMax(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue, int32 maxvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%i) must be in range %i...%i. Using %i instead.", fieldname, getConfig(index), minvalue, maxvalue, minvalue);
        setConfig(index, minvalue);
    }
    else if (getConfig(index) > maxvalue)
    {
        sLog.outError("%s (%i) must be in range %i...%i. Using %i instead.", fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
}

void World::setConfigMinMax(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue, float maxvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%f) must be in range %f...%f. Using %f instead.", fieldname, getConfig(index), minvalue, maxvalue, minvalue);
        setConfig(index, minvalue);
    }
    else if (getConfig(index) > maxvalue)
    {
        sLog.outError("%s (%f) must be in range %f...%f. Using %f instead.", fieldname, getConfig(index), minvalue, maxvalue, maxvalue);
        setConfig(index, maxvalue);
    }
}

bool World::configNoReload(bool reload, eConfigUInt32Values index, char const* fieldname, uint32 defvalue)
{
    if (!reload)
    {
        return true;
    }

    uint32 val = sConfig.GetIntDefault(fieldname, defvalue);
    if (val != getConfig(index))
    {
        sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%u).", fieldname, getConfig(index));
    }

    return false;
}

bool World::configNoReload(bool reload, eConfigInt32Values index, char const* fieldname, int32 defvalue)
{
    if (!reload)
    {
        return true;
    }

    int32 val = sConfig.GetIntDefault(fieldname, defvalue);
    if (val != getConfig(index))
    {
        sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%i).", fieldname, getConfig(index));
    }

    return false;
}

bool World::configNoReload(bool reload, eConfigFloatValues index, char const* fieldname, float defvalue)
{
    if (!reload)
    {
        return true;
    }

    float val = sConfig.GetFloatDefault(fieldname, defvalue);
    if (val != getConfig(index))
    {
        sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%f).", fieldname, getConfig(index));
    }

    return false;
}

bool World::configNoReload(bool reload, eConfigBoolValues index, char const* fieldname, bool defvalue)
{
    if (!reload)
    {
        return true;
    }

    bool val = sConfig.GetBoolDefault(fieldname, defvalue);
    if (val != getConfig(index))
    {
        sLog.outError("%s option can't be changed at mangosd.conf reload, using current value (%s).", fieldname, getConfig(index) ? "'true'" : "'false'");
    }

    return false;
}

void World::InvalidatePlayerDataToAllClient(ObjectGuid guid)
{
    WorldPacket data(SMSG_INVALIDATE_PLAYER, 8);
    data << guid;
    SendGlobalMessage(&data);
}

void World::LoadBroadcastStrings()
{
    if (!m_broadcastEnable)
    {
        return;
    }

    std::string queryStr = "SELECT `autobroadcast`.`id`, `autobroadcast`.`content`,`autobroadcast`.`ratio` FROM `autobroadcast`";

    QueryResult* result = WorldDatabase.Query(queryStr.c_str());

    if (!result)
    {
        m_broadcastEnable = false;
        sLog.outErrorDb("DB table `autobroadcast` is empty.");
        sLog.outString();
        return;
    }

    m_broadcastList.clear();

    BarGoLink bar(result->GetRowCount());
    m_broadcastWeight = 0;

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 ratio = fields[2].GetUInt32();
        if (ratio == 0)
        {
            continue;
        }

        m_broadcastWeight += ratio;

        BroadcastString bs;
        bs.text = fields[1].GetString();
        bs.freq = m_broadcastWeight;
        m_broadcastList.push_back(bs);
    } while (result->NextRow());

    delete result;
    if (m_broadcastWeight == 0)
    {
        sLog.outString(">> Loaded 0 broadcast strings.");
        m_broadcastEnable = false;
    }
    else
    {
        sLog.outString(">> Loaded %zu broadcast strings.", m_broadcastList.size());
    }
}

void World::AutoBroadcast()
{
    if (m_broadcastList.size() == 1)
    {
        SendWorldText(LANG_AUTOBROADCAST, m_broadcastList[0].text.c_str());
    }
    else
    {
        uint32 rn = urand(1, m_broadcastWeight);
        std::vector<BroadcastString>::const_iterator it;
        for (it = m_broadcastList.begin(); it != m_broadcastList.end(); ++it)
        {
            if (rn <= it->freq)
            {
                break;
            }
        }
        SendWorldText(LANG_AUTOBROADCAST, it->text.c_str());
    }
}
