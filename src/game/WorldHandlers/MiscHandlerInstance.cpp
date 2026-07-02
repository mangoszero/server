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
 * @file MiscHandler.cpp
 * @brief Miscellaneous opcode handlers
 *
 * This file handles miscellaneous opcodes that don't fit into
 * other specific handler categories:
 *
 * - CMSG_NAME_QUERY: Query character name by GUID
 * - CMSG_PING: Client ping/pong
 * - CMSG_LOGOUT_REQUEST: Logout request
 * - CMSG_LOGOUT_CANCEL: Cancel logout
 * - CMSG_ZONE_UPDATE: Zone update
 * - CMSG_SET_ACTIONBAR_TOGGLES: Set action bar toggles
 * - CMSG_SET_ACTIONBAR_TEXT: Set action bar text
 * - CMSG_MOVE_TIME_SKIPPED: Movement time skipped
 * - CMSG_MOVE_FALL_RESET: Fall reset
 * - CMSG_WORLD_STATE_UI_TIMER: UI timer
 * - CMSG_NEXT_CINEMATIC_CAMERA: Cinematic camera
 * - CMSG_COMPLETE_CINEMATIC: Complete cinematic
 * - CMSG_SET_FACTION_AT_WAR: Set faction at war
 * - CMSG_SET_WATCHED_FACTION: Set watched faction
 * - CMSG_TOGGLE_PVP: Toggle PVP flag
 * - CMSG_SET_PLAYER_DECLARED_NAME: Set player name
 */



#include "Common.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "World.h"
#include "CinematicFlyover.h"
#include "GuildMgr.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "UpdateData.h"
#include "LootMgr.h"
#include "Chat.h"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "Object.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Pet.h"
#include "SocialMgr.h"
#include "DBCEnums.h"
#include <zlib.h>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Resets the player's or group's saved instances.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleResetInstancesOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_RESET_INSTANCES");

    if (Group* pGroup = _player->GetGroup())
    {
        if (pGroup->IsLeader(_player->GetObjectGuid()))
        {
            pGroup->ResetInstances(INSTANCE_RESET_ALL, _player);
        }
    }
    else
    {
        _player->ResetInstances(INSTANCE_RESET_ALL);
    }
}
