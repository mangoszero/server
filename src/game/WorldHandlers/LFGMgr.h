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

#include <list>
#include <map>

#include "Policies/Singleton.h"
#include "Common.h"

#ifndef MANGOSSERVER_LFGMGR_H
#define MANGOSSERVER_LFGMGR_H

enum ClassRoles
{
    LFG_ROLE_NONE   = 0x00,
    LFG_ROLE_TANK   = 0x01,
    LFG_ROLE_HEALER = 0x02,
    LFG_ROLE_DPS    = 0x04
};

enum RolesPriority
{
    LFG_PRIORITY_NONE   = 0,
    LFG_PRIORITY_LOW    = 1,
    LFG_PRIORITY_NORMAL = 2,
    LFG_PRIORITY_HIGH   = 3
};

enum PlayerLeaveMethod
{
    PLAYER_CLIENT_LEAVE = 0,
    PLAYER_SYSTEM_LEAVE = 1
};

enum GroupLeaveMethod
{
    GROUP_CLIENT_LEAVE  = 0,
    GROUP_SYSTEM_LEAVE  = 1
};

#define MAX_DPS_COUNT = 3

struct LFGPlayerQueueInfo
{
    ClassRoles roleMask;
   uint32 team;
    uint32 areaId;
    uint32 timeInLFG;
    bool hasQueuePriority;
};

struct LFGGroupQueueInfo
{
    uint32 availableRoles;
    uint32 dpsCount;
   uint32 team;
    uint32 areaId;
    uint32 groupTimer;
};

class LFGQueue
{
    public:
        LFGQueue() {}
        ~LFGQueue() {}

       void AddToQueue(Player* leader, uint32 queAreaID);
        void RestoreOfflinePlayer(ObjectGuid plrGuid);
        bool FindRoleToGroup(Player* plr, Group* grp, ClassRoles role);
        void RemovePlayerFromQueue(ObjectGuid plrGuid, PlayerLeaveMethod leaveMethod = PLAYER_CLIENT_LEAVE); // 0 == by default system (cmsg, leader leave), 1 == by lfg system (no need report text you left queu)
        void RemoveGroupFromQueue(uint32 groupId, GroupLeaveMethod leaveMethod = GROUP_CLIENT_LEAVE);
        void Update(uint32 diff);
        void UpdateGroup(uint32 groupId);

        static void BuildSetQueuePacket(WorldPacket &data, uint32 areaId, uint8 status);
        static void BuildMemberAddedPacket(WorldPacket &data, ObjectGuid plrGuid);
        static void BuildInProgressPacket(WorldPacket &data);
        static void BuildCompletePacket(WorldPacket &data);

        ClassRoles CalculateRoles(Classes playerClass);
        ClassRoles canPerformRole(ClassRoles playerRole, ClassRoles requiredRole)
        {
            if ((playerRole & requiredRole) == requiredRole)
                return requiredRole;

            return playerRole;
        }

        RolesPriority getPriority(Classes playerClass, ClassRoles playerRoles);

    private:
        typedef std::map<ObjectGuid, LFGPlayerQueueInfo> QueuedPlayersMap;
        QueuedPlayersMap m_QueuedPlayers;
        QueuedPlayersMap m_OfflinePlayers;

        typedef std::map<uint32, LFGGroupQueueInfo> QueuedGroupsMap;
        QueuedGroupsMap m_QueuedGroups;

        uint32 findInArea(uint32 areaId)
        {
            uint32 m_QueueSize = 0;

            for(QueuedPlayersMap::iterator itr = m_QueuedPlayers.begin(); itr != m_QueuedPlayers.end(); ++itr)
            {
                if(itr->second.areaId == areaId)
                    ++m_QueueSize;
            }

            return m_QueueSize;
        }
};

#define sLFGMgr MaNGOS::Singleton<LFGQueue>::Instance()

#endif
