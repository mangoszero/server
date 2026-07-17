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
 * @file BattleGroundMgr.cpp
 * @brief Implementation of the battleground manager and queue system.
 *
 * This file contains the implementation of the BattleGroundMgr singleton class and
 * the BattleGroundQueue class, which handle:
 * - Battleground instance creation and management
 * - Player queue management and matching
 * - Team balancing for battleground invitations
 * - Average wait time calculations
 * - Bracket-based queue organization
 * - Premade group matching
 */



#include "BattleGroundMgr.h"
#include "Common.h"
#include "SharedDefines.h"
#include "Player.h"
#include "BattleGroundAV.h"
#include "BattleGroundAB.h"
#include "BattleGroundWS.h"
#include "MapManager.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Chat.h"
#include "World.h"
#include "WorldPacket.h"
#include "GameEventMgr.h"
#include "Formulas.h"
#include "DisableMgr.h"
#include "GameTime.h"
#include "Policies/Singleton.h"
#include "Language.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Initializes the selection pool for team balancing.
 *
 * Clears the list of selected groups and resets the player count to prepare
 * for a new team building cycle.
 */
void BattleGroundQueue::SelectionPool::Init()
{
    SelectedGroups.clear();
    PlayerCount = 0;
}

/**
 * @brief Removes a group from the selection pool.
 *
 * Attempts to remove a group of approximately the specified size from the selection pool
 * to balance team composition. Prefers to remove larger groups or groups of similar size
 * to the target size.
 *
 * @param size The target group size to remove.
 * @return true if more groups should be added to maintain balance, false otherwise.
 */
bool BattleGroundQueue::SelectionPool::KickGroup(uint32 size)
{
    // find maxgroup or LAST group with size == size and kick it
    bool found = false;
    GroupsQueueType::iterator groupToKick = SelectedGroups.begin();
    for (GroupsQueueType::iterator itr = groupToKick; itr != SelectedGroups.end(); ++itr)
    {
        if (abs((int32)((*itr)->Players.size() - size)) <= 1)
        {
            groupToKick = itr;
            found = true;
        }
        else if (!found && (*itr)->Players.size() >= (*groupToKick)->Players.size())
        {
            groupToKick = itr;
        }
    }
    // if pool is empty, do nothing
    if (GetPlayerCount())
    {
        // update player count
        GroupQueueInfo* ginfo = (*groupToKick);
        SelectedGroups.erase(groupToKick);
        PlayerCount -= ginfo->Players.size();
        // return false if we kicked smaller group or there are enough players in selection pool
        if (ginfo->Players.size() <= size + 1)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief Adds a group to the selection pool if space is available.
 *
 * Attempts to add a group to the selection pool for battleground invitation.
 * Only adds the group if doing so won't exceed the desired player count, or
 * if the pool still needs more players to reach the desired count.
 *
 * @param ginfo Pointer to the group queue info to add.
 * @param desiredCount The target number of players for this team.
 * @return true if the group was added or if more players are still needed, false if pool is full.
 */
bool BattleGroundQueue::SelectionPool::AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount)
{
    // if group is larger than desired count - don't allow to add it to pool
    if (!ginfo->IsInvitedToBGInstanceGUID && desiredCount >= PlayerCount + ginfo->Players.size())
    {
        SelectedGroups.push_back(ginfo);
        // increase selected players count
        PlayerCount += ginfo->Players.size();
        return true;
    }
    if (PlayerCount < desiredCount)
    {
        return true;
    }
    return false;
}

/**
 * @brief Adds a group or solo player to the battleground queue.
 *
 * Creates a new group queue info structure and adds all players from the group
 * (or the solo player if grp is NULL) to the appropriate bracket and queue type.
 * Handles queue announcements if configured.
 *
 * @param leader The group leader or solo player joining the queue.
 * @param grp The group joining (NULL for solo players).
 * @param BgTypeId The type of battleground being queued for.
 * @param bracketId The level bracket for this group.
 * @param isPremade Whether this is a premade group (rated, etc.).
 * @return GroupQueueInfo* Pointer to the created group queue info structure.
 */
GroupQueueInfo* BattleGroundQueue::AddGroup(Player* leader, Group* grp, BattleGroundTypeId BgTypeId, BattleGroundBracketId bracketId, bool isPremade)
{
    // create new ginfo
    GroupQueueInfo* ginfo = new GroupQueueInfo;
    ginfo->BgTypeId                  = BgTypeId;
    ginfo->IsInvitedToBGInstanceGUID = 0;
    ginfo->JoinTime                  = GameTime::GetGameTimeMS();
    ginfo->RemoveInviteTime          = 0;
    ginfo->GroupTeam                 = leader->GetTeam();

    ginfo->Players.clear();

    // compute index (if group is premade or joined a rated match) to queues
    uint32 index = 0;
    if (!isPremade)
    {
        index += PVP_TEAM_COUNT;                             // BG_QUEUE_PREMADE_* -> BG_QUEUE_NORMAL_*
    }

    if (ginfo->GroupTeam == HORDE)
    {
        ++index; // BG_QUEUE_*_ALLIANCE -> BG_QUEUE_*_HORDE
    }

    DEBUG_LOG("Adding Group to BattleGroundQueue bgTypeId : %u, bracket_id : %u, index : %u", BgTypeId, bracketId, index);

    uint32 lastOnlineTime = GameTime::GetGameTimeMS();

    // add players from group to ginfo
    {
        if (grp)
        {
            for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
            {
                Player* member = itr->getSource();
                if (!member)
                {
                    continue; // this should never happen
                }
                PlayerQueueInfo& pl_info = m_QueuedPlayers[member->GetObjectGuid()];
                pl_info.LastOnlineTime   = lastOnlineTime;
                pl_info.GroupInfo        = ginfo;
                // add the pinfo to ginfo's list
                ginfo->Players[member->GetObjectGuid()]  = &pl_info;
            }
        }
        else
        {
            PlayerQueueInfo& pl_info = m_QueuedPlayers[leader->GetObjectGuid()];
            pl_info.LastOnlineTime   = lastOnlineTime;
            pl_info.GroupInfo        = ginfo;
            ginfo->Players[leader->GetObjectGuid()]  = &pl_info;
        }

        // add GroupInfo to m_QueuedGroups
        m_QueuedGroups[bracketId][index].push_back(ginfo);

        // announce to world, this code needs mutex
        if (!isPremade && sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN))
        {
            if (BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(ginfo->BgTypeId))
            {
                char const* bgName = bg->GetName();
                uint32 MinPlayers = bg->GetMinPlayersPerTeam();
                uint32 qHorde = 0;
                uint32 qAlliance = 0;
                uint32 q_min_level = leader->GetMinLevelForBattleGroundBracketId(bracketId, BgTypeId);
                GroupsQueueType::const_iterator itr;
                for (itr = m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].begin(); itr != m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_ALLIANCE].end(); ++itr)
                {
                    if (!(*itr)->IsInvitedToBGInstanceGUID)
                    {
                        qAlliance += (*itr)->Players.size();
                    }
                }

                for (itr = m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].begin(); itr != m_QueuedGroups[bracketId][BG_QUEUE_NORMAL_HORDE].end(); ++itr)
                {
                    if (!(*itr)->IsInvitedToBGInstanceGUID)
                    {
                        qHorde += (*itr)->Players.size();
                    }
                }

                // Show queue status to player only (when joining queue)
                if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_QUEUE_ANNOUNCER_JOIN) == 1)
                {
                    ChatHandler(leader).PSendSysMessage(LANG_BG_QUEUE_ANNOUNCE_SELF, bgName, q_min_level, q_min_level + 10,
                        qAlliance, (MinPlayers > qAlliance) ? MinPlayers - qAlliance : (uint32)0, qHorde, (MinPlayers > qHorde) ? MinPlayers - qHorde : (uint32)0);
                }
                // System message
                else
                {
                    sWorld.SendWorldText(LANG_BG_QUEUE_ANNOUNCE_WORLD, bgName, q_min_level, q_min_level + 10,
                        qAlliance, (MinPlayers > qAlliance) ? MinPlayers - qAlliance : (uint32)0, qHorde, (MinPlayers > qHorde) ? MinPlayers - qHorde : (uint32)0);
                }
            }
        }
        // release mutex
    }

    return ginfo;
}

/**
 * @brief Updates the average wait time for a group after invitation.
 *
 * Records the time this group spent in the queue and updates the rolling average
 * wait times for their team and bracket. This data is used to show queue wait
 * estimates to new players.
 *
 * @param ginfo Pointer to the group queue info to update.
 * @param bracket_id The bracket the group is in.
 */
void BattleGroundQueue::PlayerInvitedToBGUpdateAverageWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id)
{
    uint32 timeInQueue = getMSTimeDiff(ginfo->JoinTime, GameTime::GetGameTimeMS());
    uint8 team_index = TEAM_INDEX_ALLIANCE;                    // default set to BG_TEAM_ALLIANCE - or non rated arenas!

    if (ginfo->GroupTeam == HORDE)
    {
        team_index = TEAM_INDEX_HORDE;
    }

    // store pointer to arrayindex of player that was added first
    uint32* lastPlayerAddedPointer = &(m_WaitTimeLastPlayer[team_index][bracket_id]);
    // remove his time from sum
    m_SumOfWaitTimes[team_index][bracket_id] -= m_WaitTimes[team_index][bracket_id][(*lastPlayerAddedPointer)];
    // set average time to new
    m_WaitTimes[team_index][bracket_id][(*lastPlayerAddedPointer)] = timeInQueue;
    // add new time to sum
    m_SumOfWaitTimes[team_index][bracket_id] += timeInQueue;
    // set index of last player added to next one
    (*lastPlayerAddedPointer)++;
    (*lastPlayerAddedPointer) %= COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME;
}

/**
 * @brief Calculates the average queue wait time for a team and bracket.
 *
 * Returns the rolling average of wait times for players who were recently
 * invited to battlegrounds in this team/bracket combination. Useful for
 * showing queue wait estimates to new players.
 *
 * @param ginfo Pointer to the group queue info (for team identification).
 * @param bracket_id The bracket to get wait time for.
 * @return uint32 The average queue wait time in milliseconds, or 0 if not enough data.
 */
uint32 BattleGroundQueue::GetAverageQueueWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id)
{
    uint8 team_index = TEAM_INDEX_ALLIANCE;                    // default set to BG_TEAM_ALLIANCE - or non rated arenas!
    if (ginfo->GroupTeam == HORDE)
    {
        team_index = TEAM_INDEX_HORDE;
    }
    // check if there is enought values(we always add values > 0)
    if (m_WaitTimes[team_index][bracket_id][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME - 1])
    {
        return (m_SumOfWaitTimes[team_index][bracket_id] / COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME);
    }
    else
        // if there aren't enough values return 0 - not available
    {
        return 0;
    }
}

/**
 * @brief Removes a player from the battleground queue.
 *
 * Locates and removes a player from their group's queue information. If the group
 * becomes empty, removes the group as well. Optionally decreases the invited count
 * for the battleground if the group has been invited but not yet accepted.
 *
 * @param guid The GUID of the player to remove.
 * @param decreaseInvitedCount If true, decreases the invited count for their team's battleground.
 */
void BattleGroundQueue::RemovePlayer(ObjectGuid guid, bool decreaseInvitedCount)
{
    int32 bracket_id = -1;                                  // signed for proper for-loop finish
    QueuedPlayersMap::iterator itr;

    // remove player from map, if he's there
    itr = m_QueuedPlayers.find(guid);
    if (itr == m_QueuedPlayers.end())
    {
        sLog.outError("BattleGroundQueue: couldn't find for remove: %s", guid.GetString().c_str());
        return;
    }

    GroupQueueInfo* group = itr->second.GroupInfo;
    GroupsQueueType::iterator group_itr, group_itr_tmp;
    // mostly people with the highest levels are in battlegrounds, thats why
    // we count from MAX_BATTLEGROUND_QUEUES - 1 to 0
    // variable index removes useless searching in other team's queue
    uint32 index = BattleGround::GetTeamIndexByTeamId(group->GroupTeam);

    for (int8 bracket_id_tmp = MAX_BATTLEGROUND_BRACKETS - 1; bracket_id_tmp >= 0 && bracket_id == -1; --bracket_id_tmp)
    {
        // we must check premade and normal team's queue - because when players from premade are joining the bg
        // they leave groupinfo so we can't use its players size to find out index
        for (uint8 j = index; j < BG_QUEUE_GROUP_TYPES_COUNT; j += BG_QUEUE_NORMAL_ALLIANCE)
        {
            for (group_itr_tmp = m_QueuedGroups[bracket_id_tmp][j].begin(); group_itr_tmp != m_QueuedGroups[bracket_id_tmp][j].end(); ++group_itr_tmp)
            {
                if ((*group_itr_tmp) == group)
                {
                    bracket_id = bracket_id_tmp;
                    group_itr = group_itr_tmp;
                    // we must store index to be able to erase iterator
                    index = j;
                    break;
                }
            }
        }
    }
    // player can't be in queue without group, but just in case
    if (bracket_id == -1)
    {
        sLog.outError("BattleGroundQueue: ERROR Can not find groupinfo for %s", guid.GetString().c_str());
        return;
    }
    DEBUG_LOG("BattleGroundQueue: Removing %s, from bracket_id %u", guid.GetString().c_str(), (uint32)bracket_id);

    // ALL variables are correctly set
    // We can ignore leveling up in queue - it should not cause crash
    // remove player from group
    // if only one player there, remove group

    // remove player queue info from group queue info
    GroupQueueInfoPlayers::iterator pitr = group->Players.find(guid);
    if (pitr != group->Players.end())
    {
        group->Players.erase(pitr);
    }

    // if invited to bg, and should decrease invited count, then do it
    if (decreaseInvitedCount && group->IsInvitedToBGInstanceGUID)
    {
        BattleGround* bg = sBattleGroundMgr.GetBattleGround(group->IsInvitedToBGInstanceGUID, group->BgTypeId);
        if (bg)
        {
            bg->DecreaseInvitedCount(group->GroupTeam);
        }
    }

    // remove player queue info
    m_QueuedPlayers.erase(itr);

    // remove group queue info if needed
    if (group->Players.empty())
    {
        m_QueuedGroups[bracket_id][index].erase(group_itr);
        delete group;
    }
}

/**
 * @brief Checks if a player is invited to a specific battleground instance.
 *
 * Verifies that the player is in the queue and has been invited to the specified
 * battleground instance with the matching removal time, indicating the invitation
 * is still valid and hasn't expired.
 *
 * @param pl_guid The GUID of the player to check.
 * @param bgInstanceGuid The instance GUID of the battleground.
 * @param removeTime The invitation removal time to verify.
 * @return true if the player is invited to this battleground instance, false otherwise.
 */
bool BattleGroundQueue::IsPlayerInvited(ObjectGuid pl_guid, const uint32 bgInstanceGuid, const uint32 removeTime)
{
    QueuedPlayersMap::const_iterator qItr = m_QueuedPlayers.find(pl_guid);
    return (qItr != m_QueuedPlayers.end() &&
        qItr->second.GroupInfo->IsInvitedToBGInstanceGUID == bgInstanceGuid &&
        qItr->second.GroupInfo->RemoveInviteTime == removeTime);
}

/**
 * @brief Retrieves the group queue information for a player.
 *
 * Looks up the player in the queue and copies their group's queue information
 * to the provided output parameter.
 *
 * @param guid The GUID of the player to look up.
 * @param[out] ginfo Pointer to a GroupQueueInfo structure to fill with the player's group data.
 * @return true if the player was found and data was copied, false if player not in queue.
 */
bool BattleGroundQueue::GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* ginfo)
{
    QueuedPlayersMap::const_iterator qItr = m_QueuedPlayers.find(guid);
    if (qItr == m_QueuedPlayers.end())
    {
        return false;
    }
    *ginfo = *(qItr->second.GroupInfo);
    return true;
}

/**
 * @brief Invites a group to a battleground instance.
 *
 * Sends an invitation to all players in the group to join a specific battleground.
 * Updates the group's invitation status and creates reminder and auto-removal events
 * for the invitation. Also updates the battleground's invited count for team balancing.
 *
 * @param ginfo Pointer to the group queue info to invite.
 * @param bg Pointer to the battleground instance to invite to.
 * @param side Optional team to assign to the group (overrides their current team).
 * @return true if the group was successfully invited, false if already invited.
 */
bool BattleGroundQueue::InviteGroupToBG(GroupQueueInfo* ginfo, BattleGround* bg, Team side)
{
    // set side if needed
    if (side)
    {
        ginfo->GroupTeam = side;
    }

    if (!ginfo->IsInvitedToBGInstanceGUID)
    {
        // not yet invited
        // set invitation
        ginfo->IsInvitedToBGInstanceGUID = bg->GetInstanceID();
        BattleGroundTypeId bgTypeId = bg->GetTypeID();
        BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(bgTypeId);
        BattleGroundBracketId bracket_id = bg->GetBracketId();

        ginfo->RemoveInviteTime = GameTime::GetGameTimeMS() + INVITE_ACCEPT_WAIT_TIME;

        // loop through the players
        for (GroupQueueInfoPlayers::iterator itr = ginfo->Players.begin(); itr != ginfo->Players.end(); ++itr)
        {
            // get the player
            Player* plr = sObjectMgr.GetPlayer(itr->first);
            // if offline, skip him, this should not happen - player is removed from queue when he logs out
            if (!plr)
            {
                continue;
            }

            // invite the player
            PlayerInvitedToBGUpdateAverageWaitTime(ginfo, bracket_id);
            // sBattleGroundMgr.InvitePlayer(plr, bg, ginfo->Team);

            // set invited player counters
            bg->IncreaseInvitedCount(ginfo->GroupTeam);

            plr->SetInviteForBattleGroundQueueType(bgQueueTypeId, ginfo->IsInvitedToBGInstanceGUID);

            // create remind invite events
            BGQueueInviteEvent* inviteEvent = new BGQueueInviteEvent(plr->GetObjectGuid(), ginfo->IsInvitedToBGInstanceGUID, bgTypeId, ginfo->RemoveInviteTime);
            plr->m_Events.AddEvent(inviteEvent, plr->m_Events.CalculateTime(INVITATION_REMIND_TIME));
            // create automatic remove events
            BGQueueRemoveEvent* removeEvent = new BGQueueRemoveEvent(plr->GetObjectGuid(), ginfo->IsInvitedToBGInstanceGUID, bgTypeId, bgQueueTypeId, ginfo->RemoveInviteTime);
            plr->m_Events.AddEvent(removeEvent, plr->m_Events.CalculateTime(INVITE_ACCEPT_WAIT_TIME));

            WorldPacket data;

            uint32 queueSlot = plr->GetBattleGroundQueueIndex(bgQueueTypeId);

            DEBUG_LOG("Battleground: invited %s to BG instance %u queueindex %u bgtype %u, I can't help it if they don't press the enter battle button.",
                plr->GetGuidStr().c_str(), bg->GetInstanceID(), queueSlot, bg->GetTypeID());

            // send status packet
            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_JOIN, INVITE_ACCEPT_WAIT_TIME, 0);
            plr->GetSession()->SendPacket(&data);
        }
        return true;
    }

    return false;
}

/**
 * @brief Fills a battleground with players from the queue.
 *
 * Attempts to populate an in-progress battleground with additional players from the queue.
 * Selects groups based on available slots for each team, attempting to balance team composition
 * using the selection pool system. Large groups may be broken apart to maintain balance
 * based on configuration settings.
 *
 * @param bg Pointer to the battleground to fill with players.
 * @param bracket_id The bracket to select players from.
 */
void BattleGroundQueue::FillPlayersToBG(BattleGround* bg, BattleGroundBracketId bracket_id)
{
    int32 hordeFree = bg->GetFreeSlotsForTeam(HORDE);
    int32 aliFree   = bg->GetFreeSlotsForTeam(ALLIANCE);

    // iterator for iterating through bg queue
    GroupsQueueType::const_iterator Ali_itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].begin();
    // count of groups in queue - used to stop cycles
    uint32 aliCount = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].size();
    // index to queue which group is current
    uint32 aliIndex = 0;
    for (; aliIndex < aliCount && m_SelectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*Ali_itr), aliFree); ++aliIndex)
    {
        ++Ali_itr;
    }
    // the same thing for horde
    GroupsQueueType::const_iterator Horde_itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].begin();
    uint32 hordeCount = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].size();
    uint32 hordeIndex = 0;
    for (; hordeIndex < hordeCount && m_SelectionPools[TEAM_INDEX_HORDE].AddGroup((*Horde_itr), hordeFree); ++hordeIndex)
    {
        ++Horde_itr;
    }

    // if ofc like BG queue invitation is set in config, then we are happy
    if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE) == 0)
    {
        return;
    }

    /**
     * If we reached this code, then we have to solve NP - complete problem called Subset sum problem
     * So one solution is to check all possible invitation subgroups, or we can use these conditions:
     * 1. Last time when BattleGroundQueue::Update was executed we invited all possible players - so there is only small possibility
     * that we will invite now whole queue, because only 1 change has been made to queues from the last BattleGroundQueue::Update call
     * 2. Other thing we should consider is group order in queue
     */

    // At first we need to compare free space in bg and our selection pool
    int32 diffAli   = aliFree   - int32(m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount());
    int32 diffHorde = hordeFree - int32(m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
    while (abs(diffAli - diffHorde) > 1 && (m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() > 0 || m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() > 0))
    {
        // each cycle execution we need to kick at least 1 group
        if (diffAli < diffHorde)
        {
            // kick alliance group, add to pool new group if needed
            if (m_SelectionPools[TEAM_INDEX_ALLIANCE].KickGroup(diffHorde - diffAli))
            {
                for (; aliIndex < aliCount && m_SelectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*Ali_itr), (aliFree >= diffHorde) ? aliFree - diffHorde : 0); ++aliIndex)
                {
                    ++Ali_itr;
                }
            }
            // if ali selection is already empty, then kick horde group, but if there are less horde than ali in bg - break;
            if (!m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())
            {
                if (aliFree <= diffHorde + 1)
                {
                    break;
                }
                m_SelectionPools[TEAM_INDEX_HORDE].KickGroup(diffHorde - diffAli);
            }
        }
        else
        {
            // kick horde group, add to pool new group if needed
            if (m_SelectionPools[TEAM_INDEX_HORDE].KickGroup(diffAli - diffHorde))
            {
                for (; hordeIndex < hordeCount && m_SelectionPools[TEAM_INDEX_HORDE].AddGroup((*Horde_itr), (hordeFree >= diffAli) ? hordeFree - diffAli : 0); ++hordeIndex)
                {
                    ++Horde_itr;
                }
            }
            if (!m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount())
            {
                if (hordeFree <= diffAli + 1)
                {
                    break;
                }
                m_SelectionPools[TEAM_INDEX_ALLIANCE].KickGroup(diffAli - diffHorde);
            }
        }
        // count diffs after small update
        diffAli   = aliFree   - int32(m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount());
        diffHorde = hordeFree - int32(m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
    }
}

/**
 * @brief Checks if a premade versus premade battleground match can be made.
 *
 * Attempts to create a premade versus premade battleground match between groups that have
 * been waiting. After 30 minutes (default), premade groups are moved to the normal queue
 * if a premade match cannot be created. Groups are invited to a new battleground instance
 * up to the maximum players per team.
 *
 * @param bracket_id The bracket to check for premade matches.
 * @param MinPlayersPerTeam The minimum players required per team.
 * @param MaxPlayersPerTeam The maximum players allowed per team.
 * @return true if a match was successfully created or handled, false otherwise.
 */
bool BattleGroundQueue::CheckPremadeMatch(BattleGroundBracketId bracket_id, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam)
{
    // check match
    if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].empty() && !m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].empty())
    {
        // start premade match
        // if groups aren't invited
        GroupsQueueType::const_iterator ali_group, horde_group;
        for (ali_group = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].begin(); ali_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].end(); ++ali_group)
        {
            if (!(*ali_group)->IsInvitedToBGInstanceGUID)
            {
                break;
            }
        }

        for (horde_group = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].begin(); horde_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].end(); ++horde_group)
        {
            if (!(*horde_group)->IsInvitedToBGInstanceGUID)
            {
                break;
            }
        }

        if (ali_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].end() && horde_group != m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].end())
        {
            m_SelectionPools[TEAM_INDEX_ALLIANCE].AddGroup((*ali_group), MaxPlayersPerTeam);
            m_SelectionPools[TEAM_INDEX_HORDE].AddGroup((*horde_group), MaxPlayersPerTeam);
            // add groups/players from normal queue to size of bigger group
            uint32 maxPlayers = std::max(m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount(), m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount());
            GroupsQueueType::const_iterator itr;
            for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
            {
                for (itr = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].begin(); itr != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++itr)
                {
                    // if itr can join BG and player count is less that maxPlayers, then add group to selectionpool
                    if (!(*itr)->IsInvitedToBGInstanceGUID && !m_SelectionPools[i].AddGroup((*itr), maxPlayers))
                    {
                        break;
                    }
                }
            }
            // premade selection pools are set
            return true;
        }
    }
    // now check if we can move group from Premade queue to normal queue (timer has expired) or group size lowered!!
    // this could be 2 cycles but i'm checking only first team in queue - it can cause problem -
    // if first is invited to BG and seconds timer expired, but we can ignore it, because players have only 80 seconds to click to enter bg
    // and when they click or after 80 seconds the queue info is removed from queue
    uint32 time_before = GameTime::GetGameTimeMS() - sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_PREMADE_GROUP_WAIT_FOR_MATCH);
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        if (!m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].empty())
        {
            GroupsQueueType::iterator itr = m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].begin();
            if (!(*itr)->IsInvitedToBGInstanceGUID && ((*itr)->JoinTime < time_before || (*itr)->Players.size() < MinPlayersPerTeam))
            {
                // we must insert group to normal queue and erase pointer from premade queue
                m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].push_front((*itr));
                m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE + i].erase(itr);
            }
        }
    }
    // selection pools are not set
    return false;
}

/**
 * @brief Checks if a normal (non-premade) match can be created.
 *
 * Attempts to build balanced teams from the normal queue with at least minPlayers on each side.
 * Uses selection pools to collect groups and balance team sizes. If configured to allow
 * invitation type balancing, may invite additional groups to the team with fewer players.
 *
 * @param bracket_id The bracket to check for normal matches.
 * @param minPlayers The minimum players required per team.
 * @param maxPlayers The maximum players allowed per team.
 * @return true if a match was successfully created, false otherwise.
 */
bool BattleGroundQueue::CheckNormalMatch(BattleGroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers)
{
    GroupsQueueType::const_iterator itr_team[PVP_TEAM_COUNT];
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        itr_team[i] = m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].begin();
        for (; itr_team[i] != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + i].end(); ++(itr_team[i]))
        {
            if (!(*(itr_team[i]))->IsInvitedToBGInstanceGUID)
            {
                m_SelectionPools[i].AddGroup(*(itr_team[i]), maxPlayers);
                if (m_SelectionPools[i].GetPlayerCount() >= minPlayers)
                {
                    break;
                }
            }
        }
    }
    // try to invite same number of players - this cycle may cause longer wait time even if there are enough players in queue, but we want ballanced bg
    uint32 j = TEAM_INDEX_ALLIANCE;
    if (m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() < m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())
    {
        j = TEAM_INDEX_HORDE;
    }

    if (sWorld.getConfig(CONFIG_UINT32_BATTLEGROUND_INVITATION_TYPE) != 0 &&
        m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() >= minPlayers && m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() >= minPlayers)
    {
        // we will try to invite more groups to team with less players indexed by j
        ++(itr_team[j]);                                    // this will not cause a crash, because for cycle above reached break;
        for (; itr_team[j] != m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE + j].end(); ++(itr_team[j]))
        {
            if (!(*(itr_team[j]))->IsInvitedToBGInstanceGUID)
            {
                if (!m_SelectionPools[j].AddGroup(*(itr_team[j]), m_SelectionPools[(j + 1) % PVP_TEAM_COUNT].GetPlayerCount()))
                {
                    break;
                }
            }
        }
        // do not allow to start bg with more than 2 players more on 1 faction
        if (abs((int32)(m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() - m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount())) > 2)
        {
            return false;
        }
    }
    // allow 1v0 if debug bg
    if (sBattleGroundMgr.isTesting() && (m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() || m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount()))
    {
        return true;
    }
    // return true if there are enough players in selection pools - enable to work .debug bg command correctly
    return m_SelectionPools[TEAM_INDEX_ALLIANCE].GetPlayerCount() >= minPlayers && m_SelectionPools[TEAM_INDEX_HORDE].GetPlayerCount() >= minPlayers;
}

/**
 * This method is called when group is inserted, or player / group is removed from BG Queue - there is only one player's status changed, so we don't use while (true) cycles to invite whole queue
 * it must be called after fully adding the members of a group to ensure group joining
 * should be called from BattleGround::RemovePlayer function in some cases
 */
void BattleGroundQueue::Update(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{
    // if no players in queue - do nothing
    if (m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_ALLIANCE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_PREMADE_HORDE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_ALLIANCE].empty() &&
        m_QueuedGroups[bracket_id][BG_QUEUE_NORMAL_HORDE].empty())
    {
        return;
    }

    // battleground with free slot for player should be always in the beggining of the queue
    // maybe it would be better to create bgfreeslotqueue for each bracket_id
    BGFreeSlotQueueType::iterator itr, next;
    for (itr = sBattleGroundMgr.BGFreeSlotQueue[bgTypeId].begin(); itr != sBattleGroundMgr.BGFreeSlotQueue[bgTypeId].end(); itr = next)
    {
        next = itr;
        ++next;
        // battleground is running, so if:
        if ((*itr)->GetTypeID() == bgTypeId && (*itr)->GetBracketId() == bracket_id &&
            (*itr)->GetStatus() > STATUS_WAIT_QUEUE && (*itr)->GetStatus() < STATUS_WAIT_LEAVE)
        {
            BattleGround* bg = *itr; // we have to store battleground pointer here, because when battleground is full, it is removed from free queue (not yet implemented!!)
            // and iterator is invalid

            // clear selection pools
            m_SelectionPools[TEAM_INDEX_ALLIANCE].Init();
            m_SelectionPools[TEAM_INDEX_HORDE].Init();

            // call a function that does the job for us
            FillPlayersToBG(bg, bracket_id);

            // now everything is set, invite players
            for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_ALLIANCE].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_ALLIANCE].SelectedGroups.end(); ++citr)
            {
                InviteGroupToBG((*citr), bg, (*citr)->GroupTeam);
            }
            for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_HORDE].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_HORDE].SelectedGroups.end(); ++citr)
            {
                InviteGroupToBG((*citr), bg, (*citr)->GroupTeam);
            }

            if (!bg->HasFreeSlots())
            {
                // remove BG from BGFreeSlotQueue
                bg->RemoveFromBGFreeSlotQueue();
            }
        }
    }

    // finished iterating through the bgs with free slots, maybe we need to create a new bg

    BattleGround* bg_template = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    if (!bg_template)
    {
        sLog.outError("Battleground: Update: bg template not found for %u", bgTypeId);
        return;
    }

    // get the min. players per team, properly for larger arenas as well. (must have full teams for arena matches!)
    uint32 MinPlayersPerTeam = bg_template->GetMinPlayersPerTeam();
    uint32 MaxPlayersPerTeam = bg_template->GetMaxPlayersPerTeam();
    if (sBattleGroundMgr.isTesting())
    {
        MinPlayersPerTeam = 1;
    }

    m_SelectionPools[TEAM_INDEX_ALLIANCE].Init();
    m_SelectionPools[TEAM_INDEX_HORDE].Init();

    // check if there is premade against premade match
    if (CheckPremadeMatch(bracket_id, MinPlayersPerTeam, MaxPlayersPerTeam))
    {
        // create new battleground
        BattleGround* bg2 = sBattleGroundMgr.CreateNewBattleGround(bgTypeId, bracket_id);
        if (!bg2)
        {
            sLog.outError("BattleGroundQueue::Update - Can not create battleground: %u", bgTypeId);
            return;
        }
        // invite those selection pools
        for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
        {
            for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.end(); ++citr)
            {
                InviteGroupToBG((*citr), bg2, (*citr)->GroupTeam);
            }
        }

        // start bg
        bg2->StartBattleGround();
        // clear structures
        m_SelectionPools[TEAM_INDEX_ALLIANCE].Init();
        m_SelectionPools[TEAM_INDEX_HORDE].Init();
    }

    // now check if there are in queues enough players to start new game of (normal battleground, or non-rated arena)
    {
        // if there are enough players in pools, start new battleground or non rated arena
        if (CheckNormalMatch(bracket_id, MinPlayersPerTeam, MaxPlayersPerTeam))
        {
            // we successfully created a pool
            BattleGround* bg2 = sBattleGroundMgr.CreateNewBattleGround(bgTypeId, bracket_id);
            if (!bg2)
            {
                sLog.outError("BattleGroundQueue::Update - Can not create battleground: %u", bgTypeId);
                return;
            }

            // invite those selection pools
            for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
            {
                for (GroupsQueueType::const_iterator citr = m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.begin(); citr != m_SelectionPools[TEAM_INDEX_ALLIANCE + i].SelectedGroups.end(); ++citr)
                {
                    InviteGroupToBG((*citr), bg2, (*citr)->GroupTeam);
                }
            }

            // start bg
            bg2->StartBattleGround();
        }
    }
}

/**
 * @brief Executes the queue invitation reminder event.
 *
 * Sends a reminder notification to the player about their pending battleground invitation.
 * Only proceeds if the player is online and the invitation is still valid.
 *
 * @param e_time The event execution time (unused).
 * @param p_time The processing time (unused).
 * @return true to delete the event, false to keep it.
 */
bool BGQueueInviteEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    Player* plr = sObjectMgr.GetPlayer(m_PlayerGuid);
    // player logged off (we should do nothing, he is correctly removed from queue in another procedure)
    if (!plr)
    {
        return true;
    }

    BattleGround* bg = sBattleGroundMgr.GetBattleGround(m_BgInstanceGUID, m_BgTypeId);
    // if battleground ended and its instance deleted - do nothing
    if (!bg)
    {
        return true;
    }

    BattleGroundQueueTypeId bgQueueTypeId = BattleGroundMgr::BGQueueTypeId(bg->GetTypeID());
    uint32 queueSlot = plr->GetBattleGroundQueueIndex(bgQueueTypeId);
    if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)         // player is in queue or in battleground
    {
        // check if player is invited to this bg
        BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[bgQueueTypeId];
        if (bgQueue.IsPlayerInvited(m_PlayerGuid, m_BgInstanceGUID, m_RemoveTime))
        {
            WorldPacket data;
            // we must send remaining time in queue
            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_JOIN, INVITE_ACCEPT_WAIT_TIME - INVITATION_REMIND_TIME, 0);
            plr->GetSession()->SendPacket(&data);
        }
    }
    return true;                                            // event will be deleted
}

/**
 * @brief Aborts the queue invitation reminder event.
 *
 * Called when the invitation reminder event is aborted before execution. No action is needed.
 *
 * @param e_time The event execution time (unused).
 */
void BGQueueInviteEvent::Abort(uint64 /*e_time*/)
{
    // do nothing
}

/**
 * @brief Executes the queue removal event for an invited player.
 *
 * Removes a player from the battleground queue if they don't accept the invitation
 * within the timeout period. Handles multiple scenarios including logging off, rejoining,
 * and accepting the invitation.
 *
 * @param e_time The event execution time (unused).
 * @param p_time The processing time (unused).
 * @return true to delete the event, false to keep it.
 */
bool BGQueueRemoveEvent::Execute(uint64 /*e_time*/, uint32 /*p_time*/)
{
    Player* plr = sObjectMgr.GetPlayer(m_PlayerGuid);
    if (!plr)
    {
        // player logged off (we should do nothing, he is correctly removed from queue in another procedure)
        return true;
    }

    BattleGround* bg = sBattleGroundMgr.GetBattleGround(m_BgInstanceGUID, m_BgTypeId);
    // battleground can be deleted already when we are removing queue info
    // bg pointer can be NULL! so use it carefully!

    uint32 queueSlot = plr->GetBattleGroundQueueIndex(m_BgQueueTypeId);
    if (queueSlot < PLAYER_MAX_BATTLEGROUND_QUEUES)         // player is in queue, or in Battleground
    {
        // check if player is in queue for this BG and if we are removing his invite event
        BattleGroundQueue& bgQueue = sBattleGroundMgr.m_BattleGroundQueues[m_BgQueueTypeId];
        if (bgQueue.IsPlayerInvited(m_PlayerGuid, m_BgInstanceGUID, m_RemoveTime))
        {
            DEBUG_LOG("Battleground: removing player %u from bg queue for instance %u because of not pressing enter battle in time.", plr->GetGUIDLow(), m_BgInstanceGUID);

            plr->RemoveBattleGroundQueueId(m_BgQueueTypeId);
            bgQueue.RemovePlayer(m_PlayerGuid, true);
            // update queues if battleground isn't ended
            if (bg && bg->GetStatus() != STATUS_WAIT_LEAVE)
            {
                sBattleGroundMgr.ScheduleQueueUpdate(m_BgQueueTypeId, m_BgTypeId, bg->GetBracketId());
            }

            WorldPacket data;
            sBattleGroundMgr.BuildBattleGroundStatusPacket(&data, bg, queueSlot, STATUS_NONE, 0, 0);
            plr->GetSession()->SendPacket(&data);
        }
    }

    // event will be deleted
    return true;
}

/**
 * @brief Aborts the queue removal event.
 *
 * Called when the event is aborted before execution. No action is needed for this event type.
 *
 * @param e_time The event execution time (unused).
 */
void BGQueueRemoveEvent::Abort(uint64 /*e_time*/)
{
    // do nothing
}
