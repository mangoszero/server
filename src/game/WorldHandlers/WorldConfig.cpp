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
#include <sstream>
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

    LoadScheduledExitConfig();

    std::string forceLoadGridOnMaps = sConfig.GetStringDefault("LoadAllGridsOnMaps", "");
    if (!forceLoadGridOnMaps.empty())
    {
        unsigned int pos = 0;
        unsigned int id;
        VMAP::VMapFactory::chompAndTrim(forceLoadGridOnMaps);
        while (VMAP::VMapFactory::getNextId(forceLoadGridOnMaps, pos, id))
        {
            m_configForceLoadMapIds.insert(id);
        }
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

    setConfig(CONFIG_UINT32_LIVINGWORLD_ANCHOR_MASK, "LivingWorld.AnchorPolicyMask", 0);
    setConfig(CONFIG_BOOL_LIVINGWORLD_CELL_ENVELOPE_LOAD, "LivingWorld.CellEnvelopeLoad", false);
    setConfig(CONFIG_UINT32_LIVINGWORLD_CELL_ENVELOPE_DOWNGRADE_DELAY, "LivingWorld.CellEnvelopeDowngradeDelay", 300000);

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

    // AH Service custody escrow ledger
    setConfig(CONFIG_BOOL_AH_CUSTODY, "AH.Service.Custody", false);

    m_relocation_ai_notify_delay = sConfig.GetIntDefault("Visibility.AIRelocationNotifyDelay", 1000u);
    m_relocation_lower_limit_sq  = pow(sConfig.GetFloatDefault("Visibility.RelocationLowerLimit", 10), 2);

    m_visibility_observer_sweep_enabled  = sConfig.GetBoolDefault("Visibility.ObserverSweep.Enable", true);
    m_visibility_observer_sweep_interval = sConfig.GetIntDefault("Visibility.ObserverSweep.Interval", 2000);

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

/**
 * @brief Loads the current world database version string.
 */
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

/**
 * @brief Loads an unsigned integer config value and clamps it against negativity.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 */
void World::setConfig(eConfigUInt32Values index, char const* fieldname, uint32 defvalue)
{
    setConfig(index, sConfig.GetIntDefault(fieldname, defvalue));
    if (int32(getConfig(index)) < 0)
    {
        sLog.outError("%s (%i) can't be negative. Using %u instead.", fieldname, int32(getConfig(index)), defvalue);
        setConfig(index, defvalue);
    }
}

/**
 * @brief Loads a signed integer config value.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 */
void World::setConfig(eConfigInt32Values index, char const* fieldname, int32 defvalue)
{
    setConfig(index, sConfig.GetIntDefault(fieldname, defvalue));
}

/**
 * @brief Loads a floating-point config value.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 */
void World::setConfig(eConfigFloatValues index, char const* fieldname, float defvalue)
{
    setConfig(index, sConfig.GetFloatDefault(fieldname, defvalue));
}

/**
 * @brief Loads a boolean config value.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 */
void World::setConfig(eConfigBoolValues index, char const* fieldname, bool defvalue)
{
    setConfig(index, sConfig.GetBoolDefault(fieldname, defvalue));
}

/**
 * @brief Loads a positive floating-point config value.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 */
void World::setConfigPos(eConfigFloatValues index, char const* fieldname, float defvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < 0.0f)
    {
        sLog.outError("%s (%f) can't be negative. Using %f instead.", fieldname, getConfig(index), defvalue);
        setConfig(index, defvalue);
    }
}

/**
 * @brief Loads an unsigned integer config value with a minimum bound.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @param minvalue The minimum allowed value.
 */
void World::setConfigMin(eConfigUInt32Values index, char const* fieldname, uint32 defvalue, uint32 minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%u) must be >= %u. Using %u instead.", fieldname, getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

/**
 * @brief Loads a signed integer config value with a minimum bound.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @param minvalue The minimum allowed value.
 */
void World::setConfigMin(eConfigInt32Values index, char const* fieldname, int32 defvalue, int32 minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%i) must be >= %i. Using %i instead.", fieldname, getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

/**
 * @brief Loads a floating-point config value with a minimum bound.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @param minvalue The minimum allowed value.
 */
void World::setConfigMin(eConfigFloatValues index, char const* fieldname, float defvalue, float minvalue)
{
    setConfig(index, fieldname, defvalue);
    if (getConfig(index) < minvalue)
    {
        sLog.outError("%s (%f) must be >= %f. Using %f instead.", fieldname, getConfig(index), minvalue, minvalue);
        setConfig(index, minvalue);
    }
}

/**
 * @brief Loads an unsigned integer config value with minimum and maximum bounds.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @param minvalue The minimum allowed value.
 * @param maxvalue The maximum allowed value.
 */
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

/**
 * @brief Loads a signed integer config value with minimum and maximum bounds.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @param minvalue The minimum allowed value.
 * @param maxvalue The maximum allowed value.
 */
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

/**
 * @brief Loads a floating-point config value with minimum and maximum bounds.
 *
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @param minvalue The minimum allowed value.
 * @param maxvalue The maximum allowed value.
 */
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

/**
 * @brief Rejects reloading a uint32 config value when live reload is not supported.
 *
 * @param reload Whether this is a config reload operation.
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @return true if normal loading may continue; otherwise false.
 */
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

/**
 * @brief Rejects reloading an int32 config value when live reload is not supported.
 *
 * @param reload Whether this is a config reload operation.
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @return true if normal loading may continue; otherwise false.
 */
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

/**
 * @brief Rejects reloading a float config value when live reload is not supported.
 *
 * @param reload Whether this is a config reload operation.
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @return true if normal loading may continue; otherwise false.
 */
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

/**
 * @brief Rejects reloading a bool config value when live reload is not supported.
 *
 * @param reload Whether this is a config reload operation.
 * @param index The config slot.
 * @param fieldname The configuration key.
 * @param defvalue The default value.
 * @return true if normal loading may continue; otherwise false.
 */
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
