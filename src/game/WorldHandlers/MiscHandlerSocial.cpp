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
 * @brief Starts an asynchronous add-friend lookup.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleAddFriendOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_FRIEND");

    std::string friendName = GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);

    recv_data >> friendName;

    if (!normalizePlayerName(friendName))
    {
        return;
    }

    CharacterDatabase.escape_string(friendName);            // prevent SQL injection - normal name don't must changed by this call

    DEBUG_LOG("WORLD: %s asked to add friend : '%s'",
        GetPlayer()->GetName(), friendName.c_str());

    CharacterDatabase.AsyncPQuery(&WorldSession::HandleAddFriendOpcodeCallBack, GetAccountId(), "SELECT `guid`, `race` FROM `characters` WHERE `name` = '%s'", friendName.c_str());
}

/**
 * @brief Completes an add-friend request after the character lookup.
 *
 * @param result The async query result.
 * @param accountId The requesting account id.
 */
void WorldSession::HandleAddFriendOpcodeCallBack(QueryResult* result, uint32 accountId)
{
    if (!result)
    {
        return;
    }

    uint32 friendLowGuid = (*result)[0].GetUInt32();
    ObjectGuid friendGuid = ObjectGuid(HIGHGUID_PLAYER, friendLowGuid);
    Team team = Player::TeamForRace((*result)[1].GetUInt8());

    delete result;

    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player)
    {
        return;
    }

    FriendsResult friendResult = FRIEND_NOT_FOUND;
    if (friendGuid)
    {
        if (friendGuid == player->GetObjectGuid())
        {
            friendResult = FRIEND_SELF;
        }
        else if (player->GetTeam() != team && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_ADD_FRIEND) && session->GetSecurity() < SEC_MODERATOR)
        {
            friendResult = FRIEND_ENEMY;
        }
        else if (player->GetSocial()->HasFriend(friendGuid))
        {
            friendResult = FRIEND_ALREADY;
        }
        else
        {
            Player* pFriend = sObjectAccessor.FindPlayer(friendGuid);
            if (pFriend && pFriend->IsInWorld() && pFriend->IsVisibleGloballyFor(player))
            {
                friendResult = FRIEND_ADDED_ONLINE;
            }
            else
            {
                friendResult = FRIEND_ADDED_OFFLINE;
            }

            if (!player->GetSocial()->AddToSocialList(friendGuid, false))
            {
                friendResult = FRIEND_LIST_FULL;
                DEBUG_LOG("WORLD: %s's friend list is full.", player->GetName());
            }
        }
    }

    sSocialMgr.SendFriendStatus(player, friendResult, friendGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Removes a friend from the player's social list.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleDelFriendOpcode(WorldPacket& recv_data)
{
    ObjectGuid friendGuid;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_FRIEND");

    recv_data >> friendGuid;

    _player->GetSocial()->RemoveFromSocialList(friendGuid, false);

    sSocialMgr.SendFriendStatus(GetPlayer(), FRIEND_REMOVED, friendGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Starts an asynchronous add-ignore lookup.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleAddIgnoreOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ADD_IGNORE");

    std::string IgnoreName = GetMangosString(LANG_FRIEND_IGNORE_UNKNOWN);

    recv_data >> IgnoreName;

    if (!normalizePlayerName(IgnoreName))
    {
        return;
    }

    CharacterDatabase.escape_string(IgnoreName);            // prevent SQL injection - normal name don't must changed by this call

    DEBUG_LOG("WORLD: %s asked to Ignore: '%s'",
        GetPlayer()->GetName(), IgnoreName.c_str());

    CharacterDatabase.AsyncPQuery(&WorldSession::HandleAddIgnoreOpcodeCallBack, GetAccountId(), "SELECT `guid` FROM `characters` WHERE `name` = '%s'", IgnoreName.c_str());
}

/**
 * @brief Completes an add-ignore request after the character lookup.
 *
 * @param result The async query result.
 * @param accountId The requesting account id.
 */
void WorldSession::HandleAddIgnoreOpcodeCallBack(QueryResult* result, uint32 accountId)
{
    if (!result)
    {
        return;
    }

    uint32 ignoreLowGuid = (*result)[0].GetUInt32();
    ObjectGuid ignoreGuid = ObjectGuid(HIGHGUID_PLAYER, ignoreLowGuid);

    delete result;

    WorldSession* session = sWorld.FindSession(accountId);
    if (!session)
    {
        return;
    }

    Player* player = session->GetPlayer();
    if (!player)
    {
        return;
    }

    FriendsResult ignoreResult = FRIEND_IGNORE_NOT_FOUND;
    if (ignoreGuid)
    {
        if (ignoreGuid == player->GetObjectGuid())
        {
            ignoreResult = FRIEND_IGNORE_SELF;
        }
        else if (player->GetSocial()->HasIgnore(ignoreGuid))
        {
            ignoreResult = FRIEND_IGNORE_ALREADY;
        }
        else
        {
            ignoreResult = FRIEND_IGNORE_ADDED;

            // ignore list full
            if (!player->GetSocial()->AddToSocialList(ignoreGuid, true))
            {
                ignoreResult = FRIEND_IGNORE_FULL;
            }
        }
    }

    sSocialMgr.SendFriendStatus(player, ignoreResult, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent (SMSG_FRIEND_STATUS)");
}

/**
 * @brief Removes an ignored player from the social list.
 *
 * @param recv_data The received opcode packet.
 */
void WorldSession::HandleDelIgnoreOpcode(WorldPacket& recv_data)
{
    ObjectGuid ignoreGuid;

    DEBUG_LOG("WORLD: Received opcode CMSG_DEL_IGNORE");

    recv_data >> ignoreGuid;

    _player->GetSocial()->RemoveFromSocialList(ignoreGuid, true);

    sSocialMgr.SendFriendStatus(GetPlayer(), FRIEND_IGNORE_REMOVED, ignoreGuid, false);

    DEBUG_LOG("WORLD: Sent motd (SMSG_FRIEND_STATUS)");
}
