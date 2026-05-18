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
 * @file LFGHandler.cpp
 * @brief Looking For Group (Meeting Stone) opcode handlers
 *
 * This file handles player interactions with meeting stones (LFG system).
 * Meeting stones allow players/groups to queue for dungeons and be matched
 * with other players automatically.
 *
 * Opcodes handled:
 * - CMSG_MEETINGSTONE_JOIN: Join LFG queue at a meeting stone
 * - CMSG_MEETINGSTONE_LEAVE: Leave LFG queue
 * - CMSG_MEETINGSTONE_INFO: Request current queue status
 *
 * @see LFGMgr for the queue management implementation
 * @see LFGQueue for matching algorithm
 */

#include "Common.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "Object.h"
#include "Chat.h"
#include "Language.h"
#include "ScriptMgr.h"
#include "World.h"
#include "Group.h"
#include "LFGHandler.h"
#include "LFGMgr.h"

/**
 * @brief Handle meeting stone join request (CMSG_MEETINGSTONE_JOIN)
 * @param recv_data World packet containing meeting stone GameObject GUID
 *
 * Player attempts to join the LFG queue at a meeting stone.
 * Requirements:
 * - Player must not be in remote control state
 * - Target must be a valid meeting stone GameObject
 * - If in group, player must be leader
 * - Cannot be in a raid group
 * - Group must not be full
 *
 * On success, adds player/group to LFGMgr queue for the stone's area.
 */
void WorldSession::HandleMeetingStoneJoinOpcode(WorldPacket& recv_data)
{
    ObjectGuid guid;

    recv_data >> guid;

    DEBUG_LOG("WORLD: Recvd CMSG_MEETINGSTONE_JOIN Message guid: %s", guid.GetString().c_str());

    // ignore for remote control state
    if (!_player->IsSelfMover())
    {
        return;
    }

    GameObject *obj = GetPlayer()->GetMap()->GetGameObject(guid);

    if (!obj)
    {
        return;
    }

    // Never expect this opcode for some type GO's
    if (obj->GetGoType() != GAMEOBJECT_TYPE_MEETINGSTONE)
    {
        sLog.outError("HandleMeetingStoneJoinOpcode: CMSG_MEETINGSTONE_JOIN for not allowed GameObject type %u (Entry %u), didn't expect this to happen.", obj->GetGoType(), obj->GetEntry());
        return;
    }

    if (Group* grp = _player->GetGroup())
    {
        if (!grp->IsLeader(_player->GetObjectGuid()))
        {
            SendMeetingstoneFailed(MEETINGSTONE_FAIL_PARTYLEADER);

            return;
        }

        if (grp->isRaidGroup())
        {
            SendMeetingstoneFailed(MEETINGSTONE_FAIL_RAID_GROUP);
            return;
        }

        if (grp->IsFull())
        {
            SendMeetingstoneFailed(MEETINGSTONE_FAIL_FULL_GROUP);
            return;
        }
    }


   GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(obj->GetEntry());

   sLFGMgr.AddToQueue(_player, gInfo->meetingstone.areaID);
}

/**
 * @brief Handle meeting stone leave request (CMSG_MEETINGSTONE_LEAVE)
 * @param recv_data World packet (empty)
 *
 * Player leaves the LFG queue. Behavior depends on group status:
 * - In group as leader: Removes entire group from queue
 * - In group as member: Personal leave notification only
 * - Solo player: Removes from queue
 */
void WorldSession::HandleMeetingStoneLeaveOpcode(WorldPacket& /*recv_data*/)
{
    DEBUG_LOG("WORLD: Recvd CMSG_MEETINGSTONE_LEAVE");
    if (Group *grp = _player->GetGroup())
    {
        if (grp->IsLeader(_player->GetObjectGuid()) && grp->isInLFG())
        {
            sLFGMgr.RemoveGroupFromQueue(grp->GetId());
        }
        else
        {
            SendMeetingstoneSetqueue(0, MEETINGSTONE_STATUS_NONE);
        }
    }
    else
    {
        sLFGMgr.RemovePlayerFromQueue(_player->GetObjectGuid());
    }
}

/**
 * @brief Handle meeting stone info request (CMSG_MEETING_STONE_INFO)
 * @param recv_data World packet (empty)
 *
 * Player requests current LFG queue status. Used after login or
 * when reopening the meeting stone UI.
 *
 * Responses:
 * - In group in LFG: Sends area ID and JOINED_QUEUE status
 * - In group not in LFG: Sends empty queue status
 * - Solo player: Attempts to restore offline queue status
 */
void WorldSession::HandleMeetingStoneInfoOpcode(WorldPacket & /*recv_data*/)
{
    DEBUG_LOG("WORLD: Received CMSG_MEETING_STONE_INFO");

    if (Group *grp = _player->GetGroup())
    {
        if (grp->isInLFG())
        {
            SendMeetingstoneSetqueue(grp->GetLFGAreaId(), MEETINGSTONE_STATUS_JOINED_QUEUE);
        }
        else
        {
            SendMeetingstoneSetqueue(0, MEETINGSTONE_STATUS_NONE);
        }
    }
    else
    {
        sLFGMgr.RestoreOfflinePlayer(_player->GetObjectGuid());
    }
}

/**
 * @brief Send meeting stone failure response
 * @param status Failure reason code (MEETINGSTONE_FAIL_*)
 *
 * Sends SMSG_MEETINGSTONE_JOINFAILED to indicate why a join attempt failed.
 * Common reasons: not leader, raid group, group full.
 */
void WorldSession::SendMeetingstoneFailed(uint8 status)
{
    WorldPacket data(SMSG_MEETINGSTONE_JOINFAILED, 1);
    data << uint8(status);
    SendPacket(&data);
}

/**
 * @brief Send meeting stone queue status
 * @param areaid Area/dungeon ID in queue (0 if not in queue)
 * @param status Queue status (MEETINGSTONE_STATUS_*)
 *
 * Sends SMSG_MEETINGSTONE_SETQUEUE to update client's queue status UI.
 * Called when joining, leaving, or restoring queue status.
 */
void WorldSession::SendMeetingstoneSetqueue(uint32 areaid, uint8 status)
{
    WorldPacket data(SMSG_MEETINGSTONE_SETQUEUE, 5);
    data << uint32(areaid);
    data << uint8(status);
    SendPacket(&data);
}
