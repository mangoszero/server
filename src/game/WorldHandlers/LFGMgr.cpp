/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "Common.h"
#include "ProgressBar.h"
#include "SharedDefines.h"
#include "Player.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Language.h"
#include "Group.h"
#include "LFGMgr.h"
#include "LFGHandler.h"
#include "DisableMgr.h"

// Add group or player into queue. If player has group and he's a leader then whole party will be added to queue.
void LFGQueue::AddToQueue(Player* leader, uint32 queAreaID)
{
    if(!leader)
        return;

    if (AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(queAreaID))
    {
        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_MAP, areaEntry->mapid))
        {
            ChatHandler(leader->GetSession()).SendSysMessage(LANG_MAP_IS_DISABLED);
            return;
        }
    }

    Group* grp = leader->GetGroup();

    //add players from group to queue list & group to group list
    // Calculate what roles group need and add it to the GroupQueueList, (DONT'ADD PLAYERS!)
    if (grp && grp->IsLeader(leader->GetObjectGuid()))
    {
        // Add group to queued groups list
        LFGGroupQueueInfo& i_Group = m_QueuedGroups[grp->GetId()];

        grp->CalculateLFGRoles(i_Group);
       i_Group.team = leader->GetTeam();
        i_Group.areaId = queAreaID;
        i_Group.groupTimer = 5 * MINUTE * IN_MILLISECONDS; // Minute timer for SMSG_MEETINGSTONE_IN_PROGRESS

        grp->SetLFGAreaId(queAreaID);

        WorldPacket data;
        BuildSetQueuePacket(data, queAreaID, MEETINGSTONE_STATUS_JOINED_QUEUE);

        grp->BroadcastPacket(&data, true);
    }
    else if(!grp)
    {
        // Add player to queued players list
        LFGPlayerQueueInfo& i_Player = m_QueuedPlayers[leader->GetObjectGuid()];

        i_Player.roleMask = CalculateRoles((Classes)leader->getClass());
       i_Player.team = leader->GetTeam();
        i_Player.areaId = queAreaID;
        i_Player.hasQueuePriority = false;

        leader->GetSession()->SendMeetingstoneSetqueue(queAreaID, MEETINGSTONE_STATUS_JOINED_QUEUE);
    }
    else                                                    // Player is in group, but it's not leader
        MANGOS_ASSERT(false && "LFGQueue::AddToQueue called while player is not leader and in group.");
}

void LFGQueue::RestoreOfflinePlayer(ObjectGuid plrGuid)
{
    Player* plr = sObjectMgr.GetPlayer(plrGuid);

    // Should not happen, but there's always chance of quick disconnection
    if(!plr)
        return;

    QueuedPlayersMap::iterator offlinePlr = m_OfflinePlayers.find(plrGuid);

    if(offlinePlr != m_OfflinePlayers.end())
    {
        plr->GetSession()->SendMeetingstoneSetqueue(offlinePlr->second.areaId, MEETINGSTONE_STATUS_JOINED_QUEUE);
        m_QueuedPlayers[plrGuid] = offlinePlr->second;
        m_OfflinePlayers.erase(offlinePlr);
    }
    else
    {
        plr->GetSession()->SendMeetingstoneSetqueue(0, MEETINGSTONE_STATUS_NONE);
    }
}

ClassRoles LFGQueue::CalculateRoles(Classes playerClass)
{
    switch (playerClass)
    {
        case CLASS_DRUID:   return (ClassRoles)(LFG_ROLE_TANK | LFG_ROLE_DPS | LFG_ROLE_HEALER);
        case CLASS_HUNTER:  return (ClassRoles)(LFG_ROLE_DPS);
        case CLASS_MAGE:    return (ClassRoles)(LFG_ROLE_DPS);
        case CLASS_PALADIN: return (ClassRoles)(LFG_ROLE_TANK | LFG_ROLE_DPS | LFG_ROLE_HEALER);
        case CLASS_PRIEST:  return (ClassRoles)(LFG_ROLE_DPS | LFG_ROLE_HEALER);
        case CLASS_ROGUE:   return (ClassRoles)(LFG_ROLE_DPS);
        case CLASS_SHAMAN:  return (ClassRoles)(LFG_ROLE_DPS | LFG_ROLE_HEALER);
        case CLASS_WARLOCK: return (ClassRoles)(LFG_ROLE_DPS);
        case CLASS_WARRIOR: return (ClassRoles)(LFG_ROLE_TANK | LFG_ROLE_DPS);
        default:            return (ClassRoles)(LFG_ROLE_NONE);
    }
}

RolesPriority LFGQueue::getPriority(Classes playerClass, ClassRoles playerRoles)
{
    switch (playerRoles)
    {
        case LFG_ROLE_TANK:
        {
            switch (playerClass)
            {
                case CLASS_DRUID:   return LFG_PRIORITY_NORMAL;
                case CLASS_PALADIN: return LFG_PRIORITY_NORMAL;
                case CLASS_WARRIOR: return LFG_PRIORITY_HIGH;
                default:            return LFG_PRIORITY_NONE;
            }
            break;
        }

        case LFG_ROLE_HEALER:
        {
            switch (playerClass)
            {
                case CLASS_DRUID:   return LFG_PRIORITY_HIGH;
                case CLASS_PALADIN: return LFG_PRIORITY_NORMAL;
                case CLASS_PRIEST:  return LFG_PRIORITY_HIGH;
                case CLASS_SHAMAN:  return LFG_PRIORITY_NORMAL;
                default:            return LFG_PRIORITY_NONE;
            }

            break;
        }

        case LFG_ROLE_DPS:
        {
            switch (playerClass)
            {
                case CLASS_DRUID:   return LFG_PRIORITY_NORMAL;
                case CLASS_HUNTER:  return LFG_PRIORITY_HIGH;
                case CLASS_MAGE:    return LFG_PRIORITY_HIGH;
                case CLASS_PALADIN: return LFG_PRIORITY_LOW;
                case CLASS_PRIEST:  return LFG_PRIORITY_LOW;
                case CLASS_ROGUE:   return LFG_PRIORITY_HIGH;
                case CLASS_SHAMAN:  return LFG_PRIORITY_NORMAL;
                case CLASS_WARLOCK: return LFG_PRIORITY_HIGH;
                case CLASS_WARRIOR: return LFG_PRIORITY_NORMAL;
                default:            return LFG_PRIORITY_NONE;
            }

            break;
        }
        default:                    return LFG_PRIORITY_NONE;
    }
}

void LFGQueue::UpdateGroup(uint32 groupId)
{
    QueuedGroupsMap::iterator qGroup = m_QueuedGroups.find(groupId);

    if(qGroup != m_QueuedGroups.end())
    {
        Group* grp = sObjectMgr.GetGroupById(qGroup->first);

        if(grp)
        {
            grp->CalculateLFGRoles(qGroup->second);
        }

    }
}

void LFGQueue::Update(uint32 diff)
{
    if(m_QueuedGroups.empty() && m_QueuedPlayers.empty())
        return;

    // Iterate over QueuedPlayersMap to update players timers and remove offline/disconnected players.
    for(QueuedPlayersMap::iterator qPlayer = m_QueuedPlayers.begin(); qPlayer != m_QueuedPlayers.end(); ++qPlayer)
    {
        Player* plr = sObjectMgr.GetPlayer(qPlayer->first);

        // Player could have been disconnected
        if(!plr ||!plr->IsInWorld())
        {
            m_OfflinePlayers[qPlayer->first] = qPlayer->second;
            m_QueuedPlayers.erase(qPlayer);
            break;
        }

        qPlayer->second.timeInLFG += diff;

        // Update player timer and give him queue priority.
        if(qPlayer->second.timeInLFG >= (30 * MINUTE * IN_MILLISECONDS))
        {
            qPlayer->second.hasQueuePriority = true;
        }
    }

    if(!m_QueuedGroups.empty())
    {
        // Iterate over QueuedGroupsMap to fill groups with roles they're missing.
        for(QueuedGroupsMap::iterator qGroup = m_QueuedGroups.begin(); qGroup != m_QueuedGroups.end(); ++qGroup)
        {
            Group* grp = sObjectMgr.GetGroupById(qGroup->first);

            // Safe check
            if(!grp)
                return;

            // Remove group from Queue if it's full
            if(grp->IsFull())
            {
                RemoveGroupFromQueue(qGroup->first, GROUP_SYSTEM_LEAVE);
                break;
            }

            // Iterate over QueuedPlayersMap to find suitable player to join group
            for(QueuedPlayersMap::iterator qPlayer = m_QueuedPlayers.begin(); qPlayer != m_QueuedPlayers.end(); ++qPlayer)
            {
                Player* plr = sObjectMgr.GetPlayer(qPlayer->first);

                // Check here that players team and areaId they're in queue are same
                if(qPlayer->second.team == qGroup->second.team &&
                   qPlayer->second.areaId == qGroup->second.areaId)
                {
                    // Check if player can perform tank role
                    if((canPerformRole(qPlayer->second.roleMask, LFG_ROLE_TANK) & qGroup->second.availableRoles) == LFG_ROLE_TANK)
                    {
                        if(FindRoleToGroup(plr, grp, LFG_ROLE_TANK))
                        {
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    // Check if player can perform healer role
                    if((canPerformRole(qPlayer->second.roleMask, LFG_ROLE_HEALER) & qGroup->second.availableRoles) == LFG_ROLE_HEALER)
                    {
                        if(FindRoleToGroup(plr, grp, LFG_ROLE_HEALER))
                        {
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    // Check if player can perform dps role
                    if((canPerformRole(qPlayer->second.roleMask, LFG_ROLE_DPS) & qGroup->second.availableRoles) == LFG_ROLE_DPS)
                    {
                        if(FindRoleToGroup(plr, grp, LFG_ROLE_DPS))
                        {
                            break;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    // Check if group is full, no need to try to iterate same group if it's already full.
                    if(grp->IsFull())
                    {
                        RemoveGroupFromQueue(qGroup->first, GROUP_SYSTEM_LEAVE);
                        break;
                    }
                }
            }

            // Update group timer. After each 5 minutes group will be broadcasted they're still waiting more members.
            if (qGroup->second.groupTimer <= diff)
            {
                WorldPacket data;
                BuildInProgressPacket(data);

                grp->BroadcastPacket(&data, true);

                qGroup->second.groupTimer = 5 * MINUTE * IN_MILLISECONDS;
            }
            else
            {
                qGroup->second.groupTimer -= diff;
            }
        }
    }

    // Pick first 2 players and form group out of them also inserting them into queue as group.
    if(m_QueuedPlayers.size() > 5)
    {
        // Pick Leader as first target.
        QueuedPlayersMap::iterator nPlayer1 = m_QueuedPlayers.begin();

        if(findInArea(nPlayer1->second.areaId) > 5)
        {
            Group* newQueueGroup = new Group;

            // Iterate of QueuedPlayersMap and pick first member to accompany leader.
            for(QueuedPlayersMap::iterator nPlayer2 = m_QueuedPlayers.begin(); nPlayer2 != m_QueuedPlayers.end(); ++nPlayer2)
            {
                if(nPlayer1->first == nPlayer2->first)
                    continue;

                if(nPlayer1->second.team == nPlayer2->second.team &&
                    nPlayer1->second.areaId == nPlayer2->second.areaId)
                {
                    Player* leader = sObjectMgr.GetPlayer(nPlayer1->first);
                    Player* member = sObjectMgr.GetPlayer(nPlayer2->first);
                    uint32 areaId = nPlayer1->second.areaId;

                    if(!newQueueGroup->IsCreated())
                    {
                        if(newQueueGroup->Create(leader->GetObjectGuid(), leader->GetName()))
                            sObjectMgr.AddGroup(newQueueGroup);
                        else
                            return;
                    }

                    WorldPacket data;
                    BuildMemberAddedPacket(data, member->GetObjectGuid());

                    leader->GetSession()->SendPacket(&data);

                    // Add member to the group. Leader is already added upon creation of group.
                    newQueueGroup->AddMember(member->GetObjectGuid(), member->GetName(), GROUP_LFG);

                    // Add this new group to GroupQueue now and remove players from PlayerQueue
                    RemovePlayerFromQueue(nPlayer1->first, PLAYER_SYSTEM_LEAVE);
                    RemovePlayerFromQueue(nPlayer2->first, PLAYER_SYSTEM_LEAVE);
                    AddToQueue(leader, areaId);

                    break;
                }
            }
        }
    }
}

bool LFGQueue::FindRoleToGroup(Player* plr, Group* grp, ClassRoles role)
{
    // Safe check
    if(!plr || !grp)
        return false;

    QueuedGroupsMap::iterator qGroup = m_QueuedGroups.find(grp->GetId());
    QueuedPlayersMap::iterator qPlayer = m_QueuedPlayers.find(plr->GetObjectGuid());

    if(qGroup != m_QueuedGroups.end() && qPlayer != m_QueuedPlayers.end())
    {
        if (getPriority((Classes)plr->getClass(), role) >= LFG_PRIORITY_HIGH || qPlayer->second.hasQueuePriority)
        {
            bool hasBeenLongerInQueue = false;

            // Iterate over QueuedPlayersMap to find if players have been longer in Queue.
            for (QueuedPlayersMap::iterator qPlayer_loop = m_QueuedPlayers.begin(); qPlayer_loop != m_QueuedPlayers.end(); ++qPlayer_loop)
            {
                if (qPlayer->first == qPlayer_loop->first)
                    continue;

                if(qPlayer->second.timeInLFG > qPlayer_loop->second.timeInLFG)
                    hasBeenLongerInQueue = true;
            }

            if(hasBeenLongerInQueue)
            {
                switch(role)
                {
                    case LFG_ROLE_TANK:
                    {
                        // Remove tank flag if player can perform tank role.
                        qGroup->second.availableRoles &= ~LFG_ROLE_TANK;
                        break;
                    }

                    case LFG_ROLE_HEALER:
                    {
                        // Remove healer flag if player can perform healer role.
                        qGroup->second.availableRoles &= ~LFG_ROLE_HEALER;
                        break;
                    }

                    case LFG_ROLE_DPS:
                    {
                        if(qGroup->second.dpsCount < 3)
                        {
                            // Update dps count first.
                            ++qGroup->second.dpsCount;

                            // Remove dps flag if there is enough dps in group.
                            if(qGroup->second.dpsCount >= 3)
                                qGroup->second.availableRoles &= ~LFG_ROLE_DPS;
                        }
                        break;
                    }

                    default:
                    {
                        return false;
                    }
                }
            }

            WorldPacket data;
            BuildMemberAddedPacket(data, plr->GetObjectGuid());
            grp->BroadcastPacket(&data, true);

            // Add member to the group.
            grp->AddMember(plr->GetObjectGuid(), plr->GetName(), GROUP_LFG);

            // Remove player from queue.
            RemovePlayerFromQueue(qPlayer->first, PLAYER_SYSTEM_LEAVE);

            // Found player return true.
            return true;
        }
        else if (getPriority((Classes)plr->getClass(), role) < LFG_PRIORITY_HIGH)
        {
            bool hasFoundPriority = false;
            bool hasBeenLongerInQueue = false;

            // Iterate over QueuedPlayersMap to find if players in queue have higher priority or they have been longer in Queue.
            for (QueuedPlayersMap::iterator qPlayer_loop = m_QueuedPlayers.begin(); qPlayer_loop != m_QueuedPlayers.end(); ++qPlayer_loop)
            {
                if (qPlayer->first == qPlayer_loop->first)
                    continue;

                Player* m_loopMember = sObjectMgr.GetPlayer(qPlayer_loop->first);

                // If there is anyone in group for class with higher priority then ignore current member.
                if (getPriority((Classes)plr->getClass(), role) < getPriority((Classes)m_loopMember->getClass(), role))
                    hasFoundPriority = true;

                if(qPlayer->second.timeInLFG > qPlayer_loop->second.timeInLFG)
                    hasBeenLongerInQueue = true;
            }

            // If there were no one in group for role with higher priority add this member to group
            if(!hasFoundPriority && hasBeenLongerInQueue)
            {
                switch(role)
                {
                    case LFG_ROLE_TANK:
                    {
                        // Remove tank flag if player can perform tank role.
                        qGroup->second.availableRoles &= ~LFG_ROLE_TANK;
                        break;
                    }

                    case LFG_ROLE_HEALER:
                    {
                        // Remove healer flag if player can perform healer role.
                        qGroup->second.availableRoles &= ~LFG_ROLE_HEALER;
                        break;
                    }

                    case LFG_ROLE_DPS:
                    {
                        if(qGroup->second.dpsCount < 3)
                        {
                            // Update dps count first.
                            ++qGroup->second.dpsCount;

                            // Remove dps flag if there is enough dps in group.
                            if(qGroup->second.dpsCount >= 3)
                                qGroup->second.availableRoles &= ~LFG_ROLE_DPS;
                        }
                        break;
                    }

                    // This is impossible...
                    default:
                    {
                        return false;
                    }
                }

                WorldPacket data;
                BuildMemberAddedPacket(data, plr->GetObjectGuid());
                grp->BroadcastPacket(&data, true);

                // Add member to the group.
                grp->AddMember(plr->GetObjectGuid(), plr->GetName(), GROUP_LFG);

                // Now remove player from queue
                RemovePlayerFromQueue(qPlayer->first, PLAYER_SYSTEM_LEAVE);

                // found player return true
                return true;
            }
        }
    }

    return false;
}

void LFGQueue::RemovePlayerFromQueue(ObjectGuid plrGuid, PlayerLeaveMethod leaveMethod)
{
    Player * plr = sObjectMgr.GetPlayer(plrGuid);

    if(!plr)
        return;

    QueuedPlayersMap::iterator qPlayer;
    qPlayer = m_QueuedPlayers.find(plrGuid);

    if(qPlayer != m_QueuedPlayers.end())
    {
        if(leaveMethod == PLAYER_CLIENT_LEAVE)
        {
            WorldPacket data;
            BuildSetQueuePacket(data, 0, MEETINGSTONE_STATUS_LEAVE_QUEUE);
            plr->GetSession()->SendPacket(&data);
        }

        m_QueuedPlayers.erase(qPlayer);
    }
}

void LFGQueue::RemoveGroupFromQueue(uint32 groupId, GroupLeaveMethod leaveMethod)
{
    Group* grp = sObjectMgr.GetGroupById(groupId);

    if(!grp)
        return;

    QueuedGroupsMap::iterator qGroup;
    qGroup = m_QueuedGroups.find(groupId);

    if(qGroup != m_QueuedGroups.end())
    {
        if(leaveMethod == GROUP_CLIENT_LEAVE)
        {
            WorldPacket data;
            BuildSetQueuePacket(data, 0, MEETINGSTONE_STATUS_LEAVE_QUEUE);
            grp->BroadcastPacket(&data, true);
        }
        else
        {
            // Send complete information to party
            WorldPacket data;
            BuildCompletePacket(data);
            grp->BroadcastPacket(&data, true);

            // Reset UI for party
            BuildSetQueuePacket(data, 0, MEETINGSTONE_STATUS_NONE);
            grp->BroadcastPacket(&data, true);
        }

        grp->SetLFGAreaId(0);
        m_QueuedGroups.erase(qGroup);
    }
}

void LFGQueue::BuildSetQueuePacket(WorldPacket &data, uint32 areaId, uint8 status)
{
    data.Initialize(SMSG_MEETINGSTONE_SETQUEUE, 5);
    data << uint32(areaId);
    data << uint8(status);
}

void LFGQueue::BuildMemberAddedPacket(WorldPacket &data, ObjectGuid plrGuid)
{
    data.Initialize(SMSG_MEETINGSTONE_MEMBER_ADDED, 8);
    data << uint64(plrGuid);
}

void LFGQueue::BuildInProgressPacket(WorldPacket &data)
{
    data.Initialize(SMSG_MEETINGSTONE_IN_PROGRESS, 0);
}

void LFGQueue::BuildCompletePacket(WorldPacket &data)
{
    data.Initialize(SMSG_MEETINGSTONE_COMPLETE, 0);
}
