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
    CUSTOM COMMANDS HANDLERS
    Code your custom command handlers here !
 */

#include "AccountMgr.h"
#include "AuctionHouseBot/AuctionHouseBot.h"
#include "BattleGround/BattleGroundMgr.h"
#include "CellImpl.h"
#include "Chat.h"
#include "Common.h"
#include "Config/Config.h"
#include "CreatureEventAIMgr.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"
#include "DisableMgr.h"
#include "GameObject.h"
#include "GridNotifiersImpl.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "InstanceData.h"
#include "ItemEnchantmentMgr.h"
#include "Language.h"
#include "Log.h"
#include "Mail.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "MassMailMgr.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "PathFinder.h"
#include "Player.h"
#include "PlayerDump.h"
#include "PointMovementGenerator.h"
#include "SQLStorages.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "SystemConfig.h"
#include "TargetedMovementGenerator.h"
#include "Util.h"
#include "Weather.h"
#include "World.h"
#include "WorldSession.h"