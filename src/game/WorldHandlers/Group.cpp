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
 * @file Group.cpp
 * @brief Player group/party implementation
 *
 * This file implements the Group class which manages player parties:
 *
 * - Group creation and disbanding
 * - Member invite/accept/decline/kick
 * - Leadership transfer
 * - Loot method and master selection
 * - Experience sharing
 * - Quest credit sharing
 * - Group chat
 * - Roll-based loot distribution
 *
 * Groups support up to 5 members (regular) or 40 members (raid).
 *
 * @see Group for the group class
 * @see GroupMgr for group management
 */

#include "Common.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Group.h"
#include "Formulas.h"
#include "ObjectAccessor.h"
#include "BattleGround/BattleGround.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "LootMgr.h"
#include "LFGMgr.h"
#include "LFGHandler.h"

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#define LOOT_ROLL_TIMEOUT  (1*MINUTE*IN_MILLISECONDS)

//===================================================
//============== Roll ===============================
//===================================================

void Roll::targetObjectBuildLink()
{
    // called from link()
    getTarget()->addLootValidatorRef(this);
}

//===================================================
//============== Group ==============================
//===================================================

Group::Group() : m_Id(0), m_groupType(GROUPTYPE_NORMAL),
    m_bgGroup(NULL), m_lootMethod(FREE_FOR_ALL), m_lootThreshold(ITEM_QUALITY_UNCOMMON),
    m_subGroupsCounts(NULL), m_LFGAreaId(0)
{
}

Group::~Group()
{
    if (m_bgGroup)
    {
        DEBUG_LOG("Group::~Group: battleground group being deleted.");
        if (m_bgGroup->GetBgRaid(ALLIANCE) == this)
        {
            m_bgGroup->SetBgRaid(ALLIANCE, NULL);
        }
        else if (m_bgGroup->GetBgRaid(HORDE) == this)
        {
            m_bgGroup->SetBgRaid(HORDE, NULL);
        }
        else
        {
            sLog.outError("Group::~Group: battleground group is not linked to the correct battleground.");
        }
    }
    Rolls::iterator itr;
    while (!RollId.empty())
    {
        itr = RollId.begin();
        Roll* r = *itr;
        RollId.erase(itr);
        delete(r);
    }

    // it is undefined whether objectmgr (which stores the groups) or instancesavemgr
    // will be unloaded first so we must be prepared for both cases
    // this may unload some dungeon persistent state
    for (BoundInstancesMap::iterator itr2 = m_boundInstances.begin(); itr2 != m_boundInstances.end(); ++itr2)
    {
        itr2->second.state->RemoveGroup(this);
    }

    // Sub group counters clean up
    delete[] m_subGroupsCounts;
}

/**
 * @brief Creates a new group with the specified leader and persists it when needed.
 *
 * @param guid The leader player GUID.
 * @param name The leader player name.
 * @return true if the group and its first member were created successfully; otherwise false.
 */
bool Group::Create(ObjectGuid guid, const char* name)
{
    m_leaderGuid = guid;
    m_leaderName = name;

    m_groupType  = isBGGroup() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
    {
        _initRaidSubGroupsCounter();
    }

    m_lootMethod = GROUP_LOOT;
    m_lootThreshold = ITEM_QUALITY_UNCOMMON;
    m_looterGuid = guid;

    if (!isBGGroup())
    {
        m_Id = sObjectMgr.GenerateGroupId();

        Player* leader = sObjectMgr.GetPlayer(guid);

        Player::ConvertInstancesToGroup(leader, this, guid);

        // store group in database
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId` ='%u'", m_Id);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId` ='%u'", m_Id);

        CharacterDatabase.PExecute("INSERT INTO `groups` (`groupId`,`leaderGuid`,`mainTank`,`mainAssistant`,`lootMethod`,`looterGuid`,`lootThreshold`,`icon1`,`icon2`,`icon3`,`icon4`,`icon5`,`icon6`,`icon7`,`icon8`,`isRaid`) "
                                   "VALUES ('%u','%u','%u','%u','%u','%u','%u','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','" UI64FMTD "','%u')",
                                   m_Id, m_leaderGuid.GetCounter(), m_mainTankGuid.GetCounter(), m_mainAssistantGuid.GetCounter(), uint32(m_lootMethod),
                                   m_looterGuid.GetCounter(), uint32(m_lootThreshold),
                                   m_targetIcons[0].GetRawValue(), m_targetIcons[1].GetRawValue(),
                                   m_targetIcons[2].GetRawValue(), m_targetIcons[3].GetRawValue(),
                                   m_targetIcons[4].GetRawValue(), m_targetIcons[5].GetRawValue(),
                                   m_targetIcons[6].GetRawValue(), m_targetIcons[7].GetRawValue(),
                                   isRaidGroup());
    }

    if (!AddMember(guid, name))
    {
        return false;
    }

    if (!isBGGroup())
    {
        CharacterDatabase.CommitTransaction();
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnCreate(this, m_leaderGuid, m_groupType);
    }
#endif /* ENABLE_ELUNA */

    return true;
}

/**
 * @brief Loads the core group record from a database row.
 *
 * @param fields The database fields containing group metadata.
 * @return true if the group data was loaded successfully; otherwise false.
 */
bool Group::LoadGroupFromDB(Field* fields)
{
    //                                          0         1              2           3           4              5      6      7      8      9      10     11     12     13      14          15
    // result = CharacterDatabase.Query("SELECT `mainTank`, `mainAssistant`, `lootMethod`, `looterGuid`, `lootThreshold`, `icon1`, `icon2`, `icon3`, `icon4`, `icon5`, `icon6`, `icon7`, `icon8`, `isRaid`, `leaderGuid`, `groupId` FROM `groups`");

    m_Id = fields[15].GetUInt32();
    m_leaderGuid = ObjectGuid(HIGHGUID_PLAYER, fields[14].GetUInt32());

    // group leader not exist
    if (!sObjectMgr.GetPlayerNameByGUID(m_leaderGuid, m_leaderName))
    {
        return false;
    }

    m_groupType  = fields[13].GetBool() ? GROUPTYPE_RAID : GROUPTYPE_NORMAL;

    if (m_groupType == GROUPTYPE_RAID)
    {
        _initRaidSubGroupsCounter();
    }

    m_mainTankGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
    m_mainAssistantGuid = ObjectGuid(HIGHGUID_PLAYER, fields[1].GetUInt32());
    m_lootMethod = LootMethod(fields[2].GetUInt8());
    m_looterGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
    m_lootThreshold = ItemQualities(fields[4].GetUInt16());

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        m_targetIcons[i] = ObjectGuid(fields[5 + i].GetUInt64());
    }

    return true;
}

/**
 * @brief Loads a member slot from database data and updates subgroup counters.
 *
 * @param guidLow The low GUID of the member player.
 * @param subgroup The subgroup assignment.
 * @param assistant True if the member is an assistant.
 * @return true if the member was loaded successfully; otherwise false.
 */
bool Group::LoadMemberFromDB(uint32 guidLow, uint8 subgroup, bool assistant)
{
    MemberSlot member;
    member.guid      = ObjectGuid(HIGHGUID_PLAYER, guidLow);

    // skip nonexistent member
    if (!sObjectMgr.GetPlayerNameByGUID(member.guid, member.name))
    {
        return false;
    }

    member.group     = subgroup;
    member.assistant = assistant;
    member.joinTime = time(NULL);
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(subgroup);

    return true;
}

/**
 * @brief Converts the group to raid mode and refreshes related state.
 */
void Group::ConvertToRaid()
{
    m_groupType = GROUPTYPE_RAID;

    _initRaidSubGroupsCounter();

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `isRaid` = 1 WHERE `groupId`='%u'", m_Id);
    }
    SendUpdate();

    // update quest related GO states (quest activity dependent from raid membership)
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
        if (Player* player = sObjectMgr.GetPlayer(citr->guid))
        {
            player->UpdateForQuestWorldObjects();
        }
}

/**
 * @brief Adds a pending invitation for a player.
 *
 * @param player The invited player.
 * @return true if the invite was recorded; otherwise false.
 */
bool Group::AddInvite(Player* player)
{
    if (!player || player->GetGroupInvite())
    {
        return false;
    }
    Group* group = player->GetGroup();
    if (group && group->isBGGroup())
    {
        group = player->GetOriginalGroup();
    }
    if (group)
    {
        return false;
    }

    RemoveInvite(player);

    m_invitees.insert(player);

    player->SetGroupInvite(this);

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnInviteMember(this, player->GetObjectGuid());
    }
#endif /* ENABLE_ELUNA */

    return true;
}

/**
 * @brief Adds an invitation and assigns the invited player as provisional leader.
 *
 * @param player The invited player.
 * @return true if the invite was added; otherwise false.
 */
bool Group::AddLeaderInvite(Player* player)
{
    if (!AddInvite(player))
    {
        return false;
    }

    m_leaderGuid = player->GetObjectGuid();
    m_leaderName = player->GetName();
    return true;
}

/**
 * @brief Removes a pending invitation from the group.
 *
 * @param player The player whose invite is being removed.
 * @return uint32 The current member count.
 */
uint32 Group::RemoveInvite(Player* player)
{
    m_invitees.erase(player);

    player->SetGroupInvite(NULL);
    return GetMembersCount();
}

/**
 * @brief Clears all pending invitations from the group.
 */
void Group::RemoveAllInvites()
{
    for (InvitesList::iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
    {
        (*itr)->SetGroupInvite(NULL);
    }

    m_invitees.clear();
}

/**
 * @brief Finds an invited player by GUID.
 *
 * @param guid The invited player GUID.
 * @return Player* The invited player if present; otherwise NULL.
 */
Player* Group::GetInvited(ObjectGuid guid) const
{
    for (InvitesList::const_iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
        if ((*itr)->GetObjectGuid() == guid)
        {
            return (*itr);
        }

    return NULL;
}

/**
 * @brief Finds an invited player by name.
 *
 * @param name The invited player name.
 * @return Player* The invited player if present; otherwise NULL.
 */
Player* Group::GetInvited(const std::string& name) const
{
    for (InvitesList::const_iterator itr = m_invitees.begin(); itr != m_invitees.end(); ++itr)
    {
        if ((*itr)->GetName() == name)
        {
            return (*itr);
        }
    }
    return NULL;
}

/**
 * @brief Adds a member to the group and synchronizes related player and LFG state.
 *
 * @param guid The member player GUID.
 * @param name The member player name.
 * @param joinMethod The join source indicator.
 * @return true if the member was added successfully; otherwise false.
 */
bool Group::AddMember(ObjectGuid guid, const char* name, uint8 joinMethod)
{
    if (!_addMember(guid, name))
    {
        return false;
    }

    SendUpdate();

    if (Player* player = sObjectMgr.GetPlayer(guid))
    {
        if (!IsLeader(player->GetObjectGuid()) && !isBGGroup())
        {
            // reset the new member's instances, unless he is currently in one of them
            // including raid instances that they are not permanently bound to!
            player->ResetInstances(INSTANCE_RESET_GROUP_JOIN);
        }
        player->SetGroupUpdateFlag(GROUP_UPDATE_FULL);
        UpdatePlayerOutOfRange(player);

        // Used by Eluna
#ifdef ENABLE_ELUNA
        if (Eluna* e = sWorld.GetEluna())
        {
            e->OnAddMember(this, player->GetObjectGuid());
        }
#endif /* ENABLE_ELUNA */

        // quest related GO state dependent from raid membership
        if (isRaidGroup())
        {
            player->UpdateForQuestWorldObjects();
        }

        if (isInLFG())
        {
            if (joinMethod == GROUP_LFG)
            {

            }
            else
            {
                player->GetSession()->SendMeetingstoneSetqueue(m_LFGAreaId, MEETINGSTONE_STATUS_JOINED_QUEUE);

                sLFGMgr.UpdateGroup(m_Id);
            }
        }
    }

    return true;
}

/**
 * @brief Removes a member from the group or disbands the group if too few members remain.
 *
 * @param guid The member player GUID.
 * @param removeMethod The reason or removal method.
 * @return uint32 The remaining member count.
 */
uint32 Group::RemoveMember(ObjectGuid guid, uint8 removeMethod)
{
    // remove member and change leader (if need) only if strong more 2 members _before_ member remove
    if (GetMembersCount() > uint32(isBGGroup() ? 1 : 2))    // in BG group case allow 1 members group
    {
        bool leaderChanged = _removeMember(guid);

        if (Player* player = sObjectMgr.GetPlayer(guid))
        {
            // quest related GO state dependent from raid membership
            if (isRaidGroup())
            {
                player->UpdateForQuestWorldObjects();
            }

            WorldPacket data;

            if (removeMethod == GROUP_KICK)
            {
                data.Initialize(SMSG_GROUP_UNINVITE, 0);
                player->GetSession()->SendPacket(&data);

                if (isInLFG())
                {
                    data.Initialize(SMSG_MEETINGSTONE_SETQUEUE, 5);
                    data << 0 << uint8(MEETINGSTONE_STATUS_PARTY_MEMBER_REMOVED_PARTY_REMOVED);

                    BroadcastPacket(&data, true);
                    sLFGMgr.RemoveGroupFromQueue(m_Id);

                    player->GetSession()->SendMeetingstoneSetqueue(m_LFGAreaId, MEETINGSTONE_STATUS_LOOKING_FOR_NEW_PARTY_IN_QUEUE);
                    sLFGMgr.AddToQueue(player, m_LFGAreaId);
                }
            }

            if (removeMethod == GROUP_LEAVE && isInLFG())
            {
                player->GetSession()->SendMeetingstoneSetqueue(0, MEETINGSTONE_STATUS_NONE);

                data.Initialize(SMSG_MEETINGSTONE_SETQUEUE, 5);
                data << m_LFGAreaId << uint8(MEETINGSTONE_STATUS_PARTY_MEMBER_LEFT_LFG);
                BroadcastPacket(&data, true);
            }

            // we already removed player from group and in player->GetGroup() is his original group!
            if (Group* group = player->GetGroup())
            {
                group->SendUpdate();
            }
            else
            {
                data.Initialize(SMSG_GROUP_LIST, 24);
                data << uint64(0) << uint64(0) << uint64(0);
                player->GetSession()->SendPacket(&data);
            }

            _homebindIfInstance(player);
        }

        if (leaderChanged)
        {
            WorldPacket data(SMSG_GROUP_SET_LEADER, (m_memberSlots.front().name.size() + 1));
            data << m_memberSlots.front().name;
            BroadcastPacket(&data, true);

            sLFGMgr.RemoveGroupFromQueue(m_Id);
        }

        if (isInLFG())
        {
            sLFGMgr.UpdateGroup(m_Id);
        }

        SendUpdate();
    }
    // if group before remove <= 2 disband it
    else
    {
        Disband(true);
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnRemoveMember(this, guid, removeMethod); // Kicker and Reason not a part of Mangos, implement?
    }
#endif /* ENABLE_ELUNA */

    return m_memberSlots.size();
}

/**
 * @brief Transfers group leadership to another member.
 *
 * @param guid The new leader GUID.
 */
void Group::ChangeLeader(ObjectGuid guid)
{
    member_citerator slot = _getMemberCSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return;
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnChangeLeader(this, guid, GetLeaderGuid());
    }
#endif /* ENABLE_ELUNA */

    _setLeader(guid);

    WorldPacket data(SMSG_GROUP_SET_LEADER, slot->name.size() + 1);
    data << slot->name;
    BroadcastPacket(&data, true);
    SendUpdate();
}

/**
 * @brief Disbands the group, removes all members, and clears persistent state.
 *
 * @param hideDestroy True to suppress the destroyed notification packet.
 */
void Group::Disband(bool hideDestroy)
{
    Player* player;

    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        player = sObjectMgr.GetPlayer(citr->guid);
        if (!player)
        {
            continue;
        }

        // we can not call _removeMember because it would invalidate member iterator
        // if we are removing player from battleground raid
        if (isBGGroup())
        {
            player->RemoveFromBattleGroundRaid();
        }
        else
        {
            // we can remove player who is in battleground from his original group
            if (player->GetOriginalGroup() == this)
            {
                player->SetOriginalGroup(NULL);
            }
            else
            {
                player->SetGroup(NULL);
            }
        }

        // quest related GO state dependent from raid membership
        if (isRaidGroup())
        {
            player->UpdateForQuestWorldObjects();
        }

        if (!player->GetSession())
        {
            continue;
        }

        WorldPacket data;
        if (!hideDestroy)
        {
            data.Initialize(SMSG_GROUP_DESTROYED, 0);
            player->GetSession()->SendPacket(&data);
        }

        // we already removed player from group and in player->GetGroup() is his original group, send update
        if (Group* group = player->GetGroup())
        {
            group->SendUpdate();
        }
        else
        {
            data.Initialize(SMSG_GROUP_LIST, 24);
            data << uint64(0) << uint64(0) << uint64(0);
            player->GetSession()->SendPacket(&data);

            if (isInLFG())
            {
                sLFGMgr.RemoveGroupFromQueue(m_Id);

                data.Initialize(SMSG_MEETINGSTONE_SETQUEUE, 5);
                data << 0 << MEETINGSTONE_STATUS_NONE;

                player->GetSession()->SendPacket(&data);
            }
        }

        _homebindIfInstance(player);
    }
    RollId.clear();
    m_memberSlots.clear();

    RemoveAllInvites();

    if (!isBGGroup())
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId`='%u'", m_Id);
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId`='%u'", m_Id);
        CharacterDatabase.CommitTransaction();
        ResetInstances(INSTANCE_RESET_GROUP_DISBAND, NULL);
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        e->OnDisband(this);
    }
#endif /* ENABLE_ELUNA */

    m_leaderGuid.Clear();
    m_leaderName.clear();
}

/**
 * \fn void Group::SendUpdateToPlayer(Player * player)
 * \brief This method notifies the player of his group status.
 *
 * \param pPlayer Pointer to the player towards the update needs to be sent.
 *
*/
void Group::SendUpdateToPlayer(Player* pPlayer)
{
    if (!pPlayer || !pPlayer->GetSession() || !pPlayer->IsInWorld() || pPlayer->GetGroup() != this)
    {
        return;
    }

    if (pPlayer->GetGroupUpdateFlag() == GROUP_UPDATE_FLAG_NONE)
    {
        return;
    }


    uint8 subGroup;
    // looking for player's subgroup
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        if (citr->guid == pPlayer->GetObjectGuid())
        {
            subGroup=citr->group;
        }
    }

    // guess size
    WorldPacket data(SMSG_GROUP_LIST, (1 + 1 + 1 + 4 + GetMembersCount() * 20) + 8 + 1 + 8 + 1);
    data << (uint8)m_groupType;                         // group type
    data << (uint8)(subGroup | (IsAssistant(pPlayer->GetObjectGuid()) ? 0x80 : 0)); // own flags (groupid | (assistant?0x80:0))

    data << uint32(GetMembersCount() - 1);
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        if (citr->guid == pPlayer->GetObjectGuid())
        {
            continue;
        }

        Player* member = sObjectMgr.GetPlayer(citr->guid);
        uint8 onlineState = (member) ? MEMBER_STATUS_ONLINE : MEMBER_STATUS_OFFLINE;
        onlineState = onlineState | ((isBGGroup()) ? MEMBER_STATUS_PVP : 0);

        data << citr->name;
        data << citr->guid;
        // online-state
        data << uint8(sObjectMgr.GetPlayer(citr->guid) ? 1 : 0);
        data << (uint8)(citr->group | (citr->assistant ? 0x80 : 0));
    }

    data << m_leaderGuid;                               // leader guid
    if (GetMembersCount() - 1)
    {
        data << uint8(m_lootMethod);                    // loot method
        if (m_lootMethod == MASTER_LOOT)
        {
            data << m_looterGuid;                            // looter guid
        }
        else
        {
            data << uint64(0);
        }
        data << uint8(m_lootThreshold);                 // loot threshold
    }

    pPlayer->GetSession()->SendPacket(&data);
}

/*********************************************************/
/***                   LFG SYSTEM                      ***/
/*********************************************************/

void Group::CalculateLFGRoles(LFGGroupQueueInfo& data)
{
    uint32 m_initRoles = (LFG_ROLE_TANK | LFG_ROLE_DPS | LFG_ROLE_HEALER);
    std::vector<ObjectGuid> m_processed;
    uint32 dpsCount = 0;

    for (member_citerator citr = GetMemberSlots().begin(); citr != GetMemberSlots().end(); ++citr)
    {
        ClassRoles lfgRole;

        lfgRole = sLFGMgr.CalculateRoles((Classes)sObjectMgr.GetPlayerClassByGUID(citr->guid));

        if ((sLFGMgr.canPerformRole(lfgRole, LFG_ROLE_TANK) & m_initRoles) == LFG_ROLE_TANK && !inLFGGroup(m_processed, citr->guid))
        {
            FillPremadeLFG(citr->guid, LFG_ROLE_TANK, m_initRoles, dpsCount, m_processed);
        }

        if ((sLFGMgr.canPerformRole(lfgRole, LFG_ROLE_HEALER) & m_initRoles) == LFG_ROLE_HEALER && !inLFGGroup(m_processed, citr->guid))
        {
            FillPremadeLFG(citr->guid, LFG_ROLE_HEALER, m_initRoles, dpsCount, m_processed);
        }

        if ((sLFGMgr.canPerformRole(lfgRole, LFG_ROLE_DPS) & m_initRoles) == LFG_ROLE_DPS && !inLFGGroup(m_processed, citr->guid))
        {
            FillPremadeLFG(citr->guid, LFG_ROLE_DPS, m_initRoles, dpsCount, m_processed);
        }
    }

    data.availableRoles = (ClassRoles)m_initRoles;
    data.dpsCount = dpsCount;
}

/**
 * @brief Assigns a premade group member toward a required LFG role if appropriate.
 *
 * @param plrGuid The player GUID being evaluated.
 * @param requiredRole The role currently being filled.
 * @param InitRoles Bitmask of still-required roles.
 * @param DpsCount Current number of assigned damage dealers.
 * @param playersProcessed Collection of players already assigned.
 */
void Group::FillPremadeLFG(ObjectGuid plrGuid, ClassRoles requiredRole, uint32& InitRoles, uint32& DpsCount, std::vector<ObjectGuid>& playersProcessed)
{
    Classes plrClass = (Classes)sObjectMgr.GetPlayerClassByGUID(plrGuid);

    if (sLFGMgr.getPriority(plrClass, requiredRole) >= LFG_PRIORITY_HIGH && !inLFGGroup(playersProcessed, plrGuid))
    {
        switch (requiredRole)
        {
            case LFG_ROLE_TANK:
            {
                InitRoles &= ~LFG_ROLE_TANK;
                break;
            }

            case LFG_ROLE_HEALER:
            {
                InitRoles &= ~LFG_ROLE_HEALER;
                break;
            }

            case LFG_ROLE_DPS:
            {
                if (DpsCount < 3)
                {
                    ++DpsCount;

                    if (DpsCount >= 3)
                    {
                        InitRoles &= ~LFG_ROLE_DPS;
                    }
                }
                break;
            }
            default:
                break;
        }
        playersProcessed.push_back(plrGuid);
    }
    else if (sLFGMgr.getPriority(plrClass, requiredRole) < LFG_PRIORITY_HIGH && !inLFGGroup(playersProcessed, plrGuid))
    {
        bool hasFoundPriority = false;

        for (member_citerator citr = GetMemberSlots().begin(); citr != GetMemberSlots().end(); ++citr)
        {
            if (plrGuid == citr->guid)
            {
                continue;
            }

            Classes memberClass = (Classes)sObjectMgr.GetPlayerClassByGUID(plrGuid);

            if (sLFGMgr.getPriority(plrClass, requiredRole) < sLFGMgr.getPriority(memberClass, requiredRole) && !inLFGGroup(playersProcessed, plrGuid))
            {
                hasFoundPriority = true;
            }
        }

        if (!hasFoundPriority)
        {
            switch (requiredRole)
            {
                case LFG_ROLE_TANK:
                {
                    InitRoles &= ~LFG_ROLE_TANK;
                    break;
                }

                case LFG_ROLE_HEALER:
                {
                    InitRoles &= ~LFG_ROLE_HEALER;
                    break;

                }

                case LFG_ROLE_DPS:
                {
                    if (DpsCount < 3)
                    {
                        ++DpsCount;

                        if (DpsCount >= 3)
                        {
                            InitRoles &= ~LFG_ROLE_DPS;
                        }
                    }
                    break;
                }
                default:
                    break;
            }

            playersProcessed.push_back(plrGuid);
        }
    }
}

/*********************************************************/
/***                   LOOT SYSTEM                     ***/
/*********************************************************/

void Group::SendLootStartRoll(uint32 CountDown, const Roll& r)
{
    WorldPacket data(SMSG_LOOT_START_ROLL, (8 + 4 + 4 + 4 + 4 + 4));
    data << r.lootedTargetGUID;                             // creature guid what we're looting
    data << uint32(r.itemSlot);                             // item slot in loot
    data << uint32(r.itemid);                               // the itemEntryId for the item that shall be rolled for
    data << uint32(0);                                      // randomSuffix - not used ?
    data << uint32(r.itemRandomPropId);                     // item random property ID
    data << uint32(CountDown);                              // the countdown time to choose "need" or "greed"

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second == ROLL_NOT_VALID)
        {
            continue;
        }

        p->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Sends a roll result update to all players participating in a loot roll.
 *
 * @param targetGuid The player GUID associated with the roll update.
 * @param rollNumber The rolled number or pass marker.
 * @param rollType The roll type being reported.
 * @param r The roll state.
 */
void Group::SendLootRoll(ObjectGuid const& targetGuid, uint8 rollNumber, uint8 rollType, const Roll& r)
{
    WorldPacket data(SMSG_LOOT_ROLL, (8 + 4 + 8 + 4 + 4 + 4 + 1 + 1));
    data << r.lootedTargetGUID;                             // object guid what we're looting
    data << uint32(r.itemSlot);                             // unknown, maybe amount of players, or item slot in loot
    data << targetGuid;
    data << uint32(r.itemid);                               // the itemEntryId for the item that shall be rolled for
    data << uint32(0);                                      // randomSuffix - not used?
    data << uint32(r.itemRandomPropId);                     // Item random property ID
    data << uint8(rollNumber);                              // 0: "Need for: [item name]" > 127: "you passed on: [item name]"      Roll number
    data << uint8(rollType);                                // 0: "Need for: [item name]" 0: "You have selected need for [item name] 1: need roll 2: greed roll

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Sends the final winner notification for a completed loot roll.
 *
 * @param targetGuid The winning player GUID.
 * @param rollNumber The winning roll number.
 * @param rollType The winning roll type.
 * @param r The completed roll state.
 */
void Group::SendLootRollWon(ObjectGuid const& targetGuid, uint8 rollNumber, RollVote rollType, const Roll& r)
{
    WorldPacket data(SMSG_LOOT_ROLL_WON, (8 + 4 + 4 + 4 + 4 + 8 + 1 + 1));
    data << r.lootedTargetGUID;                             // object guid what we're looting
    data << uint32(r.itemSlot);                             // item slot in loot
    data << uint32(r.itemid);                               // the itemEntryId for the item that shall be rolled for
    data << uint32(0);                                      // randomSuffix - not used ?
    data << uint32(r.itemRandomPropId);                     // Item random property
    data << targetGuid;                                     // guid of the player who won.
    data << uint8(rollNumber);                              // rollnumber related to SMSG_LOOT_ROLL
    data << uint8(rollType);                                // Rolltype related to SMSG_LOOT_ROLL

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Sends the notification that all players passed on a loot roll.
 *
 * @param r The completed roll state.
 */
void Group::SendLootAllPassed(Roll const& r)
{
    WorldPacket data(SMSG_LOOT_ALL_PASSED, (8 + 4 + 4 + 4 + 4));
    data << r.lootedTargetGUID;                             // object guid what we're looting
    data << uint32(r.itemSlot);                             // item slot in loot
    data << uint32(r.itemid);                               // The itemEntryId for the item that shall be rolled for
    data << uint32(r.itemRandomPropId);                     // Item random property ID
    data << uint32(0);                                      // Item random suffix ID - not used ?

    for (Roll::PlayerVote::const_iterator itr = r.playerVote.begin(); itr != r.playerVote.end(); ++itr)
    {
        Player* p = sObjectMgr.GetPlayer(itr->first);
        if (!p || !p->GetSession())
        {
            continue;
        }

        if (itr->second != ROLL_NOT_VALID)
        {
            p->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Starts group-loot rolls for loot items above the threshold.
 *
 * @param pSource The looted world object.
 * @param loot The loot container being processed.
 */
void Group::GroupLoot(WorldObject* pSource, Loot* loot)
{
    for (uint8 itemSlot = 0; itemSlot < loot->items.size(); ++itemSlot)
    {
        LootItem& lootItem = loot->items[itemSlot];
        ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(lootItem.itemid);
        if (!itemProto)
        {
            DEBUG_LOG("Group::GroupLoot: missing item prototype for item with id: %d", lootItem.itemid);
            continue;
        }

        // only roll for one-player items, not for ones everyone can get
        if (itemProto->Quality >= uint32(m_lootThreshold) && !lootItem.freeforall)
            {
                lootItem.is_underthreshold = 0;
                StartLootRoll(pSource, GROUP_LOOT, loot, itemSlot);
            }
        else
        {
            lootItem.is_underthreshold = 1;
        }
    }
}

/**
 * @brief Starts need-before-greed rolls for loot items above the threshold.
 *
 * @param pSource The looted world object.
 * @param loot The loot container being processed.
 */
void Group::NeedBeforeGreed(WorldObject* pSource, Loot* loot)
{
    for (uint8 itemSlot = 0; itemSlot < loot->items.size(); ++itemSlot)
    {
        LootItem& lootItem = loot->items[itemSlot];
        ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(lootItem.itemid);
        if (!itemProto)
        {
            DEBUG_LOG("Group::NeedBeforeGreed: missing item prototype for item with id: %d", lootItem.itemid);
            continue;
        }

        // only roll for one-player items, not for ones everyone can get
        if (itemProto->Quality >= uint32(m_lootThreshold) && !lootItem.freeforall)
            {
                lootItem.is_underthreshold = 0;
                StartLootRoll(pSource, NEED_BEFORE_GREED, loot, itemSlot);
            }
        else
        {
            lootItem.is_underthreshold = 1;
        }
    }
}

/**
 * @brief Prepares master-loot distribution data for nearby group members.
 *
 * @param pSource The looted world object.
 * @param loot The loot container being processed.
 */
void Group::MasterLoot(WorldObject* pSource, Loot* loot)
{
    for (LootItemList::iterator i = loot->items.begin(); i != loot->items.end(); ++i)
    {
        ItemPrototype const* item = ObjectMgr::GetItemPrototype(i->itemid);
        if (!item)
        {
            continue;
        }
        if (item->Quality >= uint32(m_lootThreshold))
        {
            i->is_underthreshold = 0;
        }
    }

    uint32 real_count = 0;

    WorldPacket data(SMSG_LOOT_MASTER_LIST, 330);
    data << uint8(GetMembersCount());

    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* looter = itr->getSource();
        if (!looter->IsInWorld())
        {
            continue;
        }

        if (looter->IsWithinDist(pSource, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
        {
            data << looter->GetObjectGuid();
            ++real_count;
        }
    }

    data.put<uint8>(0, real_count);

    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* looter = itr->getSource();
        if (looter->IsWithinDist(pSource, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
        {
            looter->GetSession()->SendPacket(&data);
        }
    }
}

/**
 * @brief Records a loot-roll vote by locating the matching roll entry.
 *
 * @param player The player casting the vote.
 * @param lootedTarget The GUID of the looted object.
 * @param itemSlot The loot slot being rolled on.
 * @param vote The selected roll vote.
 * @return true if a matching roll was found; otherwise false.
 */
bool Group::CountRollVote(Player* player, ObjectGuid const& lootedTarget, uint32 itemSlot, RollVote vote)
{
    Rolls::iterator rollI = RollId.begin();
    for (; rollI != RollId.end(); ++rollI)
        if ((*rollI)->isValid() && (*rollI)->lootedTargetGUID == lootedTarget && (*rollI)->itemSlot == itemSlot)
        {
            break;
        }

    if (rollI == RollId.end())
    {
        return false;
    }

    CountRollVote(player->GetObjectGuid(), rollI, vote);    // result not related this function result meaning, ignore
    return true;
}

/**
 * @brief Applies a loot-roll vote to an existing roll entry.
 *
 * @param playerGUID The voting player GUID.
 * @param rollI Iterator pointing to the roll entry.
 * @param vote The selected roll vote.
 * @return true if processing should continue safely; otherwise false.
 */
bool Group::CountRollVote(ObjectGuid const& playerGUID, Rolls::iterator& rollI, RollVote vote)
{
    Roll* roll = *rollI;

    Roll::PlayerVote::iterator itr = roll->playerVote.find(playerGUID);
    // this condition means that player joins to the party after roll begins
    if (itr == roll->playerVote.end())
    {
        return true;                                         // result used for need iterator ++, so avoid for end of list
    }

    if (roll->getLoot())
        if (roll->getLoot()->items.empty())
        {
            return false;
        }

    switch (vote)
    {
        case ROLL_PASS:                                     // Player choose pass
        {
            SendLootRoll(playerGUID, 128, 128, *roll);
            ++roll->totalPass;
            itr->second = ROLL_PASS;
            break;
        }
        case ROLL_NEED:                                     // player choose Need
        {
            SendLootRoll(playerGUID, 0, 0, *roll);
            ++roll->totalNeed;
            itr->second = ROLL_NEED;
            break;
        }
        case ROLL_GREED:                                    // player choose Greed
        {
            SendLootRoll(playerGUID, 128, 2, *roll);
            ++roll->totalGreed;
            itr->second = ROLL_GREED;
            break;
        }
        default:                                            // Roll removed case
            break;
    }

    if (roll->totalPass + roll->totalNeed + roll->totalGreed >= roll->totalPlayersRolling)
    {
        CountTheRoll(rollI);
        return true;
    }

    return false;
}

/**
 * @brief Starts a loot roll for a specific item slot and eligible nearby members.
 *
 * @param lootTarget The looted world object.
 * @param method The loot method driving the roll.
 * @param loot The loot container.
 * @param itemSlot The loot slot to roll on.
 */
void Group::StartLootRoll(WorldObject* lootTarget, LootMethod method, Loot* loot, uint8 itemSlot)
{
    if (itemSlot >= loot->items.size())
    {
        return;
    }

    LootItem const& lootItem = loot->items[itemSlot];

    ItemPrototype const* item = ObjectMgr::GetItemPrototype(lootItem.itemid);

    Roll* r = new Roll(lootTarget->GetObjectGuid(), lootItem);

    // a vector is filled with only near party members
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* playerToRoll = itr->getSource();
        if (!playerToRoll || !playerToRoll->GetSession())
        {
            continue;
        }

        if ((method != NEED_BEFORE_GREED || playerToRoll->CanUseItem(item) == EQUIP_ERR_OK) && lootItem.AllowedForPlayer(playerToRoll, lootTarget))
        {
            if (playerToRoll->IsWithinDistInMap(lootTarget, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
            {
                r->playerVote[playerToRoll->GetObjectGuid()] = ROLL_NOT_EMITED_YET;
                ++r->totalPlayersRolling;
            }
        }
    }

    if (r->totalPlayersRolling > 0)                         // has looters
    {
        r->setLoot(loot);
        r->itemSlot = itemSlot;

        if (r->totalPlayersRolling == 1)                    // single looter
        {
            r->playerVote.begin()->second = ROLL_NEED;
        }
        else
        {
            // Only GO-group looting and NPC-group looting possible
            MANGOS_ASSERT(lootTarget->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT));

            SendLootStartRoll(LOOT_ROLL_TIMEOUT, *r);
            loot->items[itemSlot].is_blocked = true;

            lootTarget->StartGroupLoot(this, LOOT_ROLL_TIMEOUT);
        }

        RollId.push_back(r);
    }
    else                                            // no looters??
    {
        delete r;
    }
}

// called when roll timer expires
void Group::EndRoll()
{
    while (!RollId.empty())
    {
        // need more testing here, if rolls disappear
        Rolls::iterator itr = RollId.begin();
        CountTheRoll(itr);                                  // i don't have to edit player votes, who didn't vote ... he will pass
    }
}

/**
 * @brief Resolves a completed loot roll and awards or unlocks the item.
 *
 * @param rollI Iterator pointing to the roll entry.
 */
void Group::CountTheRoll(Rolls::iterator& rollI)
{
    Roll* roll = *rollI;


    if (!roll->isValid())                                   // is loot already deleted ?
    {
        rollI = RollId.erase(rollI);
        delete roll;
        return;
    }

    // end of the roll
    bool won = false;
    if (roll->totalNeed > 0)
    {
        if (!roll->playerVote.empty())
        {
            uint8 maxresul = 0;
            ObjectGuid maxguid  = (*roll->playerVote.begin()).first;

            for (Roll::PlayerVote::const_iterator itr = roll->playerVote.begin(); itr != roll->playerVote.end(); ++itr)
            {
                if (itr->second != ROLL_NEED)
                {
                    continue;
                }

                uint8 randomN = urand(1, 100);
                SendLootRoll(itr->first, randomN, ROLL_NEED, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                }
            }

            if (Player* player = sObjectMgr.GetPlayer(maxguid))
            {
                if (WorldObject* object = player->GetMap()->GetWorldObject(roll->lootedTargetGUID))
                {
                    SendLootRollWon(maxguid, maxresul, ROLL_NEED, *roll);
                    won = true;
                    if (player->GetSession())
                    {
                        ItemPosCountVec dest;
                        LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
                        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count);
                        if (msg == EQUIP_ERR_OK)
                        {
                            item->is_looted = true;
                            roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                            --roll->getLoot()->unlootedCount;
                            Item* newitem = player->StoreNewItem(dest, roll->itemid, true, item->randomPropertyId);
                            player->SendNewItem(newitem, uint32(item->count), false, false, true);

                            if (Creature* creature = object->ToCreature())
                            {
                                /// If creature has been fully looted, remove flag.
                                if (creature->loot.isLooted())
                                {
                                    creature->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                                }
                            }
                        }
                        else
                        {
                            item->is_blocked = false;
                            player->SendEquipError(msg, NULL, NULL, roll->itemid);
                            item->winner = player->GetObjectGuid();
                        }
                    }
                }
            }
        }
    }

    if (!won && roll->totalGreed > 0)
    {
        if (!roll->playerVote.empty())
        {
            uint8 maxresul = 0;
            ObjectGuid maxguid = (*roll->playerVote.begin()).first;

            Roll::PlayerVote::iterator itr;
            for (itr = roll->playerVote.begin(); itr != roll->playerVote.end(); ++itr)
            {
                if (itr->second != ROLL_GREED)
                {
                    continue;
                }

                uint8 randomN = urand(1, 100);
                SendLootRoll(itr->first, randomN, itr->second, *roll);
                if (maxresul < randomN)
                {
                    maxguid  = itr->first;
                    maxresul = randomN;
                }
            }

            if (Player* player = sObjectMgr.GetPlayer(maxguid))
            {
                if (WorldObject* object = player->GetMap()->GetWorldObject(roll->lootedTargetGUID))
                {
                    SendLootRollWon(maxguid, maxresul, ROLL_GREED, *roll);
                    won = true;
                    if (player->GetSession())
                    {
                        ItemPosCountVec dest;
                        LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
                        InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, roll->itemid, item->count);
                        if (msg == EQUIP_ERR_OK)
                        {
                            item->is_looted = true;
                            roll->getLoot()->NotifyItemRemoved(roll->itemSlot);
                            --roll->getLoot()->unlootedCount;
                            Item* newitem = player->StoreNewItem(dest, roll->itemid, true, item->randomPropertyId);
                            player->SendNewItem(newitem, uint32(item->count), false, false, true);
                            if (Creature* creature = object->ToCreature())
                            {
                                /// If creature has been fully looted, remove flag.
                                if (creature->loot.isLooted())
                                {
                                    creature->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                                }
                            }
                        }
                        else
                        {
                            item->is_blocked = false;
                            player->SendEquipError(msg, NULL, NULL, roll->itemid);

                            // Storing the winner to recall in LootView.
                            item->winner = player->GetObjectGuid();
                        }
                    }
                }
            }
        }
    }

    if (!won)
    {
        SendLootAllPassed(*roll);
        LootItem* item = &(roll->getLoot()->items[roll->itemSlot]);
        if (item)
        {
            item->is_blocked = false;
        }
    }

    rollI = RollId.erase(rollI);
    delete roll;
}

/**
 * @brief Checks whether rolling is complete for a specific loot item.
 *
 * @param pObject The looted world object.
 * @param pItem The loot item being checked.
 * @return true if no active multi-player roll remains for the item; otherwise false.
 */
bool Group::IsRollDoneForItem(WorldObject * pObject, const LootItem * pItem)
{
    if (RollId.empty())
    {
        return true;
    }


    for (Rolls::iterator i = RollId.begin(); i != RollId.end(); ++i)
    {
        Roll *roll = *i;
        if (roll->lootedTargetGUID == pObject->GetObjectGuid() && roll->itemid == pItem->itemid && roll->totalPlayersRolling > 1)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Sets or clears a raid target icon and broadcasts the change.
 *
 * @param id The icon slot index.
 * @param targetGuid The target GUID assigned to the icon.
 */
void Group::SetTargetIcon(uint8 id, ObjectGuid targetGuid)
{
    if (id >= TARGET_ICON_COUNT)
    {
        return;
    }

    // clean other icons
    if (targetGuid)
        for (int i = 0; i < TARGET_ICON_COUNT; ++i)
            if (m_targetIcons[i] == targetGuid)
            {
                SetTargetIcon(i, ObjectGuid());
            }

    m_targetIcons[id] = targetGuid;

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (1 + 1 + 8));
    data << uint8(0);                                       // set targets
    data << uint8(id);
    data << targetGuid;
    BroadcastPacket(&data, true);
}

/**
 * @brief Accumulates group XP reward data for a single qualifying player.
 *
 * @param player The player contributing to the calculation.
 * @param victim The defeated unit.
 * @param sum_level Running sum of qualifying player levels.
 * @param member_with_max_level Tracks the highest-level qualifying member.
 * @param not_gray_member_with_max_level Tracks the highest-level non-gray qualifying member.
 */
static void GetDataForXPAtKill_helper(Player* player, Unit const* victim, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level)
{
    sum_level += player->getLevel();
    if (!member_with_max_level || member_with_max_level->getLevel() < player->getLevel())
    {
        member_with_max_level = player;
    }

    uint32 gray_level = MaNGOS::XP::GetGrayLevel(player->getLevel());
    if (victim->getLevel() > gray_level && (!not_gray_member_with_max_level
                                            || not_gray_member_with_max_level->getLevel() < player->getLevel()))
    {
        not_gray_member_with_max_level = player;
    }
}

/**
 * @brief Collects qualifying group member data used for XP distribution on kill.
 *
 * @param victim The defeated unit.
 * @param count Running count of qualifying players.
 * @param sum_level Running sum of qualifying player levels.
 * @param member_with_max_level Tracks the highest-level qualifying member.
 * @param not_gray_member_with_max_level Tracks the highest-level non-gray qualifying member.
 * @param additional Optional extra player to include after iterating group members.
 */
void Group::GetDataForXPAtKill(Unit const* victim, uint32& count, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level, Player* additional)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* member = itr->getSource();
        if (!member || !member->IsAlive())                  // only for alive
        {
            continue;
        }

        // will proccesed later
        if (member == additional)
        {
            continue;
        }

        if (!member->IsAtGroupRewardDistance(victim))       // at req. distance
        {
            continue;
        }

        ++count;
        GetDataForXPAtKill_helper(member, victim, sum_level, member_with_max_level, not_gray_member_with_max_level);
    }

    if (additional)
    {
        if (additional->IsAtGroupRewardDistance(victim))    // at req. distance
        {
            ++count;
            GetDataForXPAtKill_helper(additional, victim, sum_level, member_with_max_level, not_gray_member_with_max_level);
        }
    }
}

/**
 * @brief Sends the current raid target icon assignments to a session.
 *
 * @param session The session receiving the icon list.
 */
void Group::SendTargetIconList(WorldSession* session)
{
    if (!session)
    {
        return;
    }

    WorldPacket data(MSG_RAID_TARGET_UPDATE, (1 + TARGET_ICON_COUNT * 9));
    data << uint8(1);

    for (int i = 0; i < TARGET_ICON_COUNT; ++i)
    {
        if (!m_targetIcons[i])
        {
            continue;
        }

        data << uint8(i);
        data << m_targetIcons[i];
    }

    session->SendPacket(&data);
}

/**
 * @brief Sends a full group list update to every connected member.
 */
void Group::SendUpdate()
{
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        Player* player = sObjectMgr.GetPlayer(citr->guid);
        if (!player || !player->GetSession() || player->GetGroup() != this)
        {
            continue;
        }
        // guess size
        WorldPacket data(SMSG_GROUP_LIST, (1 + 1 + 1 + 4 + GetMembersCount() * 20) + 8 + 1 + 8 + 1);
        data << (uint8)m_groupType;                         // group type
        data << (uint8)(citr->group | (citr->assistant ? 0x80 : 0)); // own flags (groupid | (assistant?0x80:0))

        data << uint32(GetMembersCount() - 1);
        for (member_citerator citr2 = m_memberSlots.begin(); citr2 != m_memberSlots.end(); ++citr2)
        {
            if (citr->guid == citr2->guid)
            {
                continue;
            }
            Player* member = sObjectMgr.GetPlayer(citr2->guid);
            uint8 onlineState = (member) ? MEMBER_STATUS_ONLINE : MEMBER_STATUS_OFFLINE;
            onlineState = onlineState | ((isBGGroup()) ? MEMBER_STATUS_PVP : 0);

            data << citr2->name;
            data << citr2->guid;
            // online-state
            data << uint8(sObjectMgr.GetPlayer(citr2->guid) ? 1 : 0);
            data << (uint8)(citr2->group | (citr2->assistant ? 0x80 : 0));
        }

        data << m_leaderGuid;                               // leader guid
        if (GetMembersCount() - 1)
        {
            data << uint8(m_lootMethod);                    // loot method
            if (m_lootMethod == MASTER_LOOT)
            {
                data << m_looterGuid;                            // looter guid
            }
            else
            {
                data << uint64(0);
            }
            data << uint8(m_lootThreshold);                 // loot threshold
        }
        player->GetSession()->SendPacket(&data);
    }
}

/**
 * @brief Sends updated party member stats to members who do not currently see the player.
 *
 * @param pPlayer The player whose stats changed.
 */
void Group::UpdatePlayerOutOfRange(Player* pPlayer)
{
    if (!pPlayer || !pPlayer->IsInWorld())
    {
        return;
    }

    if (pPlayer->GetGroupUpdateFlag() == GROUP_UPDATE_FLAG_NONE)
    {
        return;
    }

    WorldPacket data;
    pPlayer->GetSession()->BuildPartyMemberStatsChangedPacket(pPlayer, &data);

    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
        if (Player* player = itr->getSource())
            if (player != pPlayer && !player->HaveAtClient(pPlayer))
            {
                player->GetSession()->SendPacket(&data);
            }
}

/**
 * @brief Broadcasts a packet to group members with optional subgroup and ignore filters.
 *
 * @param packet The packet to broadcast.
 * @param ignorePlayersInBGRaid True to skip players whose active group differs from this one.
 * @param group The subgroup filter, or -1 for all members.
 * @param ignore A player GUID to exclude from delivery.
 */
void Group::BroadcastPacket(WorldPacket* packet, bool ignorePlayersInBGRaid, int group, ObjectGuid ignore)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pl = itr->getSource();
        if (!pl || (ignore && pl->GetObjectGuid() == ignore) || (ignorePlayersInBGRaid && pl->GetGroup() != this))
        {
            continue;
        }

        if (pl->GetSession() && (group == -1 || itr->getSubGroup() == group))
        {
            pl->GetSession()->SendPacket(packet);
        }
    }
}

/**
 * @brief Sends a ready-check packet to the leader and assistants.
 *
 * @param packet The ready-check packet to broadcast.
 */
void Group::BroadcastReadyCheck(WorldPacket* packet)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pl = itr->getSource();
        if (pl && pl->GetSession())
            if (IsLeader(pl->GetObjectGuid()) || IsAssistant(pl->GetObjectGuid()))
            {
                pl->GetSession()->SendPacket(packet);
            }
    }
}

/**
 * @brief Marks offline members as not ready during a ready check.
 */
void Group::OfflineReadyCheck()
{
    for (member_citerator citr = m_memberSlots.begin(); citr != m_memberSlots.end(); ++citr)
    {
        Player* pl = sObjectMgr.GetPlayer(citr->guid);
        if (!pl || !pl->GetSession())
        {
            WorldPacket data(MSG_RAID_READY_CHECK_CONFIRM, 9);
            data << citr->guid;
            data << uint8(0);
            BroadcastReadyCheck(&data);
        }
    }
}

/**
 * @brief Adds a member to the first available subgroup.
 *
 * @param guid The member player GUID.
 * @param name The member player name.
 * @param isAssistant True if the member should be marked as assistant.
 * @return true if a suitable subgroup was found and the member was added; otherwise false.
 */
bool Group::_addMember(ObjectGuid guid, const char* name, bool isAssistant)
{
    // get first not-full group
    uint8 groupid = 0;
    if (m_subGroupsCounts)
    {
        bool groupFound = false;
        for (; groupid < MAX_RAID_SUBGROUPS; ++groupid)
        {
            if (m_subGroupsCounts[groupid] < MAX_GROUP_SIZE)
            {
                groupFound = true;
                break;
            }
        }
        // We are raid group and no one slot is free
        if (!groupFound)
        {
            return false;
        }
    }

    return _addMember(guid, name, isAssistant, groupid);
}

/**
 * @brief Adds a member to a specific subgroup and updates player references.
 *
 * @param guid The member player GUID.
 * @param name The member player name.
 * @param isAssistant True if the member should be marked as assistant.
 * @param group The subgroup assignment.
 * @return true if the member was added; otherwise false.
 */
bool Group::_addMember(ObjectGuid guid, const char* name, bool isAssistant, uint8 group)
{
    if (IsFull())
    {
        return false;
    }

    if (!guid)
    {
        return false;
    }

    Player* player = sObjectMgr.GetPlayer(guid, false);

    MemberSlot member;
    member.guid      = guid;
    member.name      = name;
    member.group     = group;
    member.assistant = isAssistant;
    member.joinTime = time(NULL);
    m_memberSlots.push_back(member);

    SubGroupCounterIncrease(group);

    if (player)
    {
        player->SetGroupInvite(NULL);
        // if player is in group and he is being added to BG raid group, then call SetBattleGroundRaid()
        if (player->GetGroup() && isBGGroup())
        {
            player->SetBattleGroundRaid(this, group);
        }
        // if player is in bg raid and we are adding him to normal group, then call SetOriginalGroup()
        else if (player->GetGroup())
        {
            player->SetOriginalGroup(this, group);
        }
        // if player is not in group, then call set group
        else
        {
            player->SetGroup(this, group);
        }

        if (player->IsInWorld())
        {
            // if the same group invites the player back, cancel the homebind timer
            if (InstanceGroupBind* bind = GetBoundInstance(player->GetMapId()))
                if (bind->state->GetInstanceId() == player->GetInstanceId())
                {
                    player->m_InstanceValid = true;
                }
        }
    }

    if (!isRaidGroup())                                     // reset targetIcons for non-raid-groups
    {
        for (int i = 0; i < TARGET_ICON_COUNT; ++i)
        {
            m_targetIcons[i].Clear();
        }
    }

    if (!isBGGroup())
    {
        // insert into group table
        CharacterDatabase.PExecute("INSERT INTO `group_member` (`groupId`,`memberGuid`,`assistant`,`subgroup`) VALUES ('%u','%u','%u','%u')",
                                   m_Id, member.guid.GetCounter(), ((member.assistant == 1) ? 1 : 0), member.group);
    }

    return true;
}

/**
 * @brief Removes a member from internal group state and updates leadership if needed.
 *
 * @param guid The member player GUID.
 * @return true if the removed member was the leader and leadership changed; otherwise false.
 */
bool Group::_removeMember(ObjectGuid guid)
{
    Player* player = sObjectMgr.GetPlayer(guid);
    if (player)
    {
        // if we are removing player from battleground raid
        if (isBGGroup())
        {
            player->RemoveFromBattleGroundRaid();
        }
        else
        {
            // we can remove player who is in battleground from his original group
            if (player->GetOriginalGroup() == this)
            {
                player->SetOriginalGroup(NULL);
            }
            else
            {
                player->SetGroup(NULL);
            }
        }
    }

    _removeRolls(guid);

    member_witerator slot = _getMemberWSlot(guid);
    if (slot != m_memberSlots.end())
    {
        SubGroupCounterDecrease(slot->group);

        m_memberSlots.erase(slot);
    }

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid`='%u'", guid.GetCounter());
    }

    if (m_leaderGuid == guid)                               // leader was removed
    {
        if (GetMembersCount() > 0)
        {
            _setLeader(m_memberSlots.front().guid);
        }
        return true;
    }

    return false;
}

/**
 * @brief Updates the stored group leader and migrates instance bindings as needed.
 *
 * @param guid The new leader GUID.
 */
void Group::_setLeader(ObjectGuid guid)
{
    member_citerator slot = _getMemberCSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return;
    }

    if (!isBGGroup())
    {
        uint32 slot_lowguid = slot->guid.GetCounter();

        uint32 leader_lowguid = m_leaderGuid.GetCounter();

        // TODO: set a time limit to have this function run rarely cause it can be slow
        CharacterDatabase.BeginTransaction();

        // update the group's bound instances when changing leaders

        // remove all permanent binds from the group
        // in the DB also remove solo binds that will be replaced with permbinds
        // from the new leader
        CharacterDatabase.PExecute(
            "DELETE FROM `group_instance` WHERE `leaderguid`='%u' AND (`permanent` = 1 OR "
            "`instance` IN (SELECT `instance` FROM `character_instance` WHERE `guid` = '%u')"
            ")", leader_lowguid, slot_lowguid);

        Player* player = sObjectMgr.GetPlayer(slot->guid);

        if (player)
        {
            for (BoundInstancesMap::iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end();)
            {
                if (itr->second.perm)
                {
                    itr->second.state->RemoveGroup(this);
                    m_boundInstances.erase(itr++);
                }
                else
                {
                    ++itr;
                }
            }
        }

        // update the group's solo binds to the new leader
        CharacterDatabase.PExecute("UPDATE `group_instance` SET `leaderGuid`='%u' WHERE `leaderGuid` = '%u'",
                                   slot_lowguid, leader_lowguid);

        // copy the permanent binds from the new leader to the group
        // overwriting the solo binds with permanent ones if necessary
        // in the DB those have been deleted already
        Player::ConvertInstancesToGroup(player, this, slot->guid);

        // update the group leader
        CharacterDatabase.PExecute("UPDATE `groups` SET `leaderGuid`='%u' WHERE `groupId`='%u'", slot_lowguid, m_Id);
        CharacterDatabase.CommitTransaction();
    }

    m_leaderGuid = slot->guid;
    m_leaderName = slot->name;
}

/**
 * @brief Removes a player's participation from all active loot rolls.
 *
 * @param guid The player GUID to remove from roll tracking.
 */
void Group::_removeRolls(ObjectGuid guid)
{
    for (Rolls::iterator it = RollId.begin(); it != RollId.end();)
    {
        Roll* roll = *it;
        Roll::PlayerVote::iterator itr2 = roll->playerVote.find(guid);
        if (itr2 == roll->playerVote.end())
        {
            ++it;
            continue;
        }

        if (itr2->second == ROLL_GREED)
        {
            --roll->totalGreed;
        }
        if (itr2->second == ROLL_NEED)
        {
            --roll->totalNeed;
        }
        if (itr2->second == ROLL_PASS)
        {
            --roll->totalPass;
        }
        if (itr2->second != ROLL_NOT_VALID)
        {
            --roll->totalPlayersRolling;
        }

        roll->playerVote.erase(itr2);

        if (!CountRollVote(guid, it, ROLL_NOT_EMITED_YET))
        {
            ++it;
        }
    }
}

/**
 * @brief Changes a member's stored subgroup assignment.
 *
 * @param guid The member player GUID.
 * @param group The new subgroup.
 * @return true if the subgroup was updated; otherwise false.
 */
bool Group::_setMembersGroup(ObjectGuid guid, uint8 group)
{
    member_witerator slot = _getMemberWSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return false;
    }

    slot->group = group;

    SubGroupCounterIncrease(group);

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `group_member` SET `subgroup`='%u' WHERE `memberGuid`='%u'", group, guid.GetCounter());
    }

    return true;
}

/**
 * @brief Sets or clears the assistant flag for a member.
 *
 * @param guid The member player GUID.
 * @param state The desired assistant state.
 * @return true if the flag was updated; otherwise false.
 */
bool Group::_setAssistantFlag(ObjectGuid guid, const bool& state)
{
    member_witerator slot = _getMemberWSlot(guid);
    if (slot == m_memberSlots.end())
    {
        return false;
    }

    slot->assistant = state;
    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `group_member` SET `assistant`='%u' WHERE `memberGuid`='%u'", (state == true) ? 1 : 0, guid.GetCounter());
    }
    return true;
}

/**
 * @brief Sets the group's main tank designation.
 *
 * @param guid The selected main tank GUID, or an empty GUID to clear it.
 * @return true if the designation changed; otherwise false.
 */
bool Group::_setMainTank(ObjectGuid guid)
{
    if (m_mainTankGuid == guid)
    {
        return false;
    }

    if (guid)
    {
        member_citerator slot = _getMemberCSlot(guid);
        if (slot == m_memberSlots.end())
        {
            return false;
        }

        if (m_mainAssistantGuid == guid)
        {
            _setMainAssistant(ObjectGuid());
        }
    }

    m_mainTankGuid = guid;

    if (!isBGGroup())
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `mainTank`='%u' WHERE `groupId`='%u'", m_mainTankGuid.GetCounter(), m_Id);
    }

    return true;
}

/**
 * @brief Sets the group's main assistant designation.
 *
 * @param guid The selected main assistant GUID, or an empty GUID to clear it.
 * @return true if the designation changed; otherwise false.
 */
bool Group::_setMainAssistant(ObjectGuid guid)
{
    if (m_mainAssistantGuid == guid)
    {
        return false;
    }

    if (guid)
    {
        member_witerator slot = _getMemberWSlot(guid);
        if (slot == m_memberSlots.end())
        {
            return false;
        }

        if (m_mainTankGuid == guid)
        {
            _setMainTank(ObjectGuid());
        }
    }

    m_mainAssistantGuid = guid;

    if (!isBGGroup())
        CharacterDatabase.PExecute("UPDATE `groups` SET `mainAssistant`='%u' WHERE `groupId`='%u'",
                                   m_mainAssistantGuid.GetCounter(), m_Id);

    return true;
}

/**
 * @brief Checks whether two players belong to the same subgroup of this group.
 *
 * @param member1 The first player.
 * @param member2 The second player.
 * @return true if both players belong to this group and share a subgroup; otherwise false.
 */
bool Group::SameSubGroup(Player const* member1, Player const* member2) const
{
    if (!member1 || !member2)
    {
        return false;
    }
    if (member1->GetGroup() != this || member2->GetGroup() != this)
    {
        return false;
    }
    else
    {
        return member1->GetSubGroup() == member2->GetSubGroup();
    }
}

// allows setting subgroup for offline members
void Group::ChangeMembersGroup(ObjectGuid guid, uint8 group)
{
    if (!isRaidGroup())
    {
        return;
    }

    Player* player = sObjectMgr.GetPlayer(guid);

    if (!player)
    {
        uint8 prevSubGroup = GetMemberGroup(guid);
        if (prevSubGroup == group)
        {
            return;
        }

        if (_setMembersGroup(guid, group))
        {
            SubGroupCounterDecrease(prevSubGroup);
            SendUpdate();
        }
    }
    else
        // This methods handles itself groupcounter decrease
    {
        ChangeMembersGroup(player, group);
    }
}

// only for online members
void Group::ChangeMembersGroup(Player* player, uint8 group)
{
    if (!player || !isRaidGroup())
    {
        return;
    }

    uint8 prevSubGroup = player->GetSubGroup();
    if (prevSubGroup == group)
    {
        return;
    }

    if (_setMembersGroup(player->GetObjectGuid(), group))
    {
        if (player->GetGroup() == this)
        {
            player->GetGroupRef().setSubGroup(group);
        }
        // if player is in BG raid, it is possible that he is also in normal raid - and that normal raid is stored in m_originalGroup reference
        else
        {
            prevSubGroup = player->GetOriginalSubGroup();
            player->GetOriginalGroupRef().setSubGroup(group);
        }
        SubGroupCounterDecrease(prevSubGroup);

        SendUpdate();
    }
}

/**
 * @brief Advances the round-robin looter to the next eligible nearby member.
 *
 * @param pSource The looted world object.
 * @param ifneed True to keep the current looter when still eligible.
 */
void Group::UpdateLooterGuid(WorldObject* pSource, bool ifneed)
{
    switch (GetLootMethod())
    {
        case MASTER_LOOT:
        case FREE_FOR_ALL:
            return;
        default:
            // round robin style looting applies for all low
            // quality items in each loot method except free for all and master loot
            break;
    }

    member_citerator guid_itr = _getMemberCSlot(GetLooterGuid());
    if (guid_itr != m_memberSlots.end())
    {
        if (ifneed)
        {
            // not update if only update if need and ok
            Player* looter = sObjectAccessor.FindPlayer(guid_itr->guid);
            if (looter && looter->IsWithinDist(pSource, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
            {
                return;
            }
        }
        ++guid_itr;
    }

    // search next after current
    if (guid_itr != m_memberSlots.end())
    {
        for (member_citerator itr = guid_itr; itr != m_memberSlots.end(); ++itr)
        {
            if (Player* pl = sObjectAccessor.FindPlayer(itr->guid))
            {
                if (pl->IsWithinDist(pSource, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
                {
                    bool refresh = pl->GetLootGuid() == pSource->GetObjectGuid();

                    // if (refresh)                          // update loot for new looter
                    //    pl->GetSession()->DoLootRelease(pl->GetLootGUID());
                    SetLooterGuid(pl->GetObjectGuid());
                    SendUpdate();
                    if (refresh)                            // update loot for new looter
                    {
                        pl->SendLoot(pSource->GetObjectGuid(), LOOT_CORPSE);
                    }
                    return;
                }
            }
        }
    }

    // search from start
    for (member_citerator itr = m_memberSlots.begin(); itr != guid_itr; ++itr)
    {
        if (Player* pl = sObjectAccessor.FindPlayer(itr->guid))
        {
            if (pl->IsWithinDist(pSource, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
            {
                bool refresh = pl->GetLootGuid() == pSource->GetObjectGuid();

                // if (refresh)                              // update loot for new looter
                //    pl->GetSession()->DoLootRelease(pl->GetLootGUID());
                SetLooterGuid(pl->GetObjectGuid());
                SendUpdate();
                if (refresh)                                // update loot for new looter
                {
                    pl->SendLoot(pSource->GetObjectGuid(), LOOT_CORPSE);
                }
                return;
            }
        }
    }

    SetLooterGuid(ObjectGuid());
    SendUpdate();
}

/**
 * @brief Validates whether the full group can join a battleground queue together.
 *
 * @param bgTypeId The battleground type identifier.
 * @param bgQueueTypeId The battleground queue type identifier.
 * @param MinPlayerCount The minimum allowed group size.
 * @param MaxPlayerCount The maximum allowed group size.
 * @return uint32 A battleground join error code.
 */
uint32 Group::CanJoinBattleGroundQueue(BattleGroundTypeId bgTypeId, BattleGroundQueueTypeId bgQueueTypeId, uint32 MinPlayerCount, uint32 MaxPlayerCount)
{
    // check for min / max count
    uint32 memberscount = GetMembersCount();
    if (memberscount < MinPlayerCount)
    {
        return BG_JOIN_ERR_GROUP_NOT_ENOUGH;
    }
    if (memberscount > MaxPlayerCount)
    {
        return BG_JOIN_ERR_GROUP_TOO_MANY;
    }

    // get a player as reference, to compare other players' stats to (queue id based on level, etc.)
    Player* reference = GetFirstMember()->getSource();
    // no reference found, can't join this way
    if (!reference)
    {
        return BG_JOIN_ERR_OFFLINE_MEMBER;
    }

    BattleGroundBracketId bracket_id = reference->GetBattleGroundBracketIdFromLevel(bgTypeId);
    Team team = reference->GetTeam();

    // check every member of the group to be able to join
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* member = itr->getSource();
        // offline member? don't let join
        if (!member)
        {
            return BG_JOIN_ERR_OFFLINE_MEMBER;
        }
        // don't allow cross-faction join as group
        if (member->GetTeam() != team)
        {
            return BG_JOIN_ERR_MIXED_FACTION;
        }
        // not in the same battleground level bracket, don't let join
        if (member->GetBattleGroundBracketIdFromLevel(bgTypeId) != bracket_id)
        {
            return BG_JOIN_ERR_MIXED_LEVELS;
        }
        // don't let join if someone from the group is already in that bg queue
        if (member->InBattleGroundQueueForBattleGroundQueueType(bgQueueTypeId))
        {
            return BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE;
        }
        // check for deserter debuff
        if (!member->CanJoinToBattleground())
        {
            return BG_JOIN_ERR_GROUP_DESERTER;
        }
        // check if member can join any more battleground queues
        if (!member->HasFreeBattleGroundQueueId())
        {
            return BG_JOIN_ERR_ALL_QUEUES_USED;
        }
    }
    return BG_JOIN_ERR_OK;
}

/**
 * @brief Checks whether any group member is in combat inside a specific instance.
 *
 * @param instanceId The instance identifier to test.
 * @return true if a member in that instance currently has attackers; otherwise false.
 */
bool Group::InCombatToInstance(uint32 instanceId)
{
    for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pPlayer = itr->getSource();
        if (pPlayer->getAttackers().size() && pPlayer->GetInstanceId() == instanceId)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Resets or unbinds group dungeon instances according to the requested method.
 *
 * @param method The reset method to apply.
 * @param SendMsgTo Optional player who should receive reset result messages.
 */
void Group::ResetInstances(InstanceResetMethod method, Player* SendMsgTo)
{
    if (isBGGroup())
    {
        return;
    }

    // method can be INSTANCE_RESET_ALL, INSTANCE_RESET_GROUP_DISBAND

    for (BoundInstancesMap::iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end();)
    {
        DungeonPersistentState* state = itr->second.state;
        const MapEntry* entry = sMapStore.LookupEntry(itr->first);
        if (!entry || (!state->CanReset() && method != INSTANCE_RESET_GROUP_DISBAND))
        {
            ++itr;
            continue;
        }

        if (method == INSTANCE_RESET_ALL)
        {
            // the "reset all instances" method can only reset normal maps
            if (entry->map_type == MAP_RAID)
            {
                ++itr;
                continue;
            }
        }

        bool isEmpty = true;
        // if the map is loaded, reset it
        if (Map* map = sMapMgr.FindMap(state->GetMapId(), state->GetInstanceId()))
            if (map->IsDungeon() && !(method == INSTANCE_RESET_GROUP_DISBAND && !state->CanReset()))
            {
                isEmpty = ((DungeonMap*)map)->Reset(method);
            }

        if (SendMsgTo)
        {
            if (isEmpty)
            {
                SendMsgTo->SendResetInstanceSuccess(state->GetMapId());
            }
            else
            {
                SendMsgTo->SendResetInstanceFailed(INSTANCERESET_FAIL_ZONING, state->GetMapId());
            }
        }

        if (isEmpty || method == INSTANCE_RESET_GROUP_DISBAND)
        {
            // do not reset the instance, just unbind if others are permanently bound to it
            if (state->CanReset())
            {
                state->DeleteFromDB();
            }
            else
            {
                CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `instance` = '%u'", state->GetInstanceId());
            }
            // i don't know for sure if hash_map iterators
            m_boundInstances.erase(itr);
            itr = m_boundInstances.begin();
            // this unloads the instance save unless online players are bound to it
            // (eg. permanent binds or GM solo binds)
            state->RemoveGroup(this);
        }
        else
        {
            ++itr;
        }
    }
}

/**
 * @brief Retrieves the group's binding information for a dungeon map.
 *
 * @param mapid The map identifier.
 * @return InstanceGroupBind* The bind information if present; otherwise NULL.
 */
InstanceGroupBind* Group::GetBoundInstance(uint32 mapid)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry)
    {
        return NULL;
    }

    BoundInstancesMap::iterator itr = m_boundInstances.find(mapid);
    if (itr != m_boundInstances.end())
    {
        return &itr->second;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Binds the group to a dungeon instance persistent state.
 *
 * @param state The dungeon persistent state to bind.
 * @param permanent True to create a permanent bind.
 * @param load True when reconstructing binds from storage.
 * @return InstanceGroupBind* The updated bind information, or NULL on failure.
 */
InstanceGroupBind* Group::BindToInstance(DungeonPersistentState* state, bool permanent, bool load)
{
    if (state && !isBGGroup())
    {
        InstanceGroupBind& bind = m_boundInstances[state->GetMapId()];
        if (bind.state)
        {
            // when a boss is killed or when copying the players's binds to the group
            if (permanent != bind.perm || state != bind.state)
                if (!load)
                    CharacterDatabase.PExecute("UPDATE `group_instance` SET `instance` = '%u', `permanent` = '%u' WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
                                               state->GetInstanceId(), permanent, GetLeaderGuid().GetCounter(), bind.state->GetInstanceId());
        }
        else if (!load)
            CharacterDatabase.PExecute("INSERT INTO `group_instance` (`leaderGuid`, `instance`, `permanent`) VALUES ('%u', '%u', '%u')",
                                       GetLeaderGuid().GetCounter(), state->GetInstanceId(), permanent);

        if (bind.state != state)
        {
            if (bind.state)
            {
                bind.state->RemoveGroup(this);
            }
            state->AddGroup(this);
        }

        bind.state = state;
        bind.perm = permanent;
        if (!load)
            DEBUG_LOG("Group::BindToInstance: Group (Id: %d) is now bound to map %d, instance %d",
                      GetId(), state->GetMapId(), state->GetInstanceId());
        return &bind;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Removes the group's binding to a dungeon instance.
 *
 * @param mapid The map identifier to unbind.
 * @param unload True when the unbind happens as part of loading or teardown logic.
 */
void Group::UnbindInstance(uint32 mapid, bool unload)
{
    BoundInstancesMap::iterator itr = m_boundInstances.find(mapid);
    if (itr != m_boundInstances.end())
    {
        if (!unload)
            CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
                                       GetLeaderGuid().GetCounter(), itr->second.state->GetInstanceId());
        itr->second.state->RemoveGroup(this);               // save can become invalid
        m_boundInstances.erase(itr);
    }
}

/**
 * @brief Marks a player as invalid for their current instance when group removal requires homebinding.
 *
 * @param player The player leaving the group.
 */
void Group::_homebindIfInstance(Player* player)
{
    if (player && !player->isGameMaster())
    {
        Map* map = player->GetMap();
        if (map->IsDungeon())
        {
            // leaving the group in an instance, the homebind timer is started
            // unless the player is permanently saved to the instance
            InstancePlayerBind* playerBind = player->GetBoundInstance(map->GetId());
            if (!playerBind || !playerBind->perm)
            {
                player->m_InstanceValid = false;
            }
        }
    }
}

/**
 * @brief Awards honor, reputation, XP, pet XP, and kill credit to one group member.
 *
 * @param pGroupGuy The group member receiving rewards.
 * @param pVictim The defeated unit.
 * @param count The number of qualifying group members.
 * @param PvP True when the kill is treated as PvP.
 * @param group_rate The group XP scaling factor.
 * @param sum_level The summed levels of qualifying members.
 * @param is_dungeon True when the kill occurred in a dungeon.
 * @param not_gray_member_with_max_level The highest-level member still eligible for XP.
 * @param member_with_max_level The highest-level qualifying group member.
 * @param xp The base XP amount for the kill.
 */
static void RewardGroupAtKill_helper(Player* pGroupGuy, Unit* pVictim, uint32 count, bool PvP, float group_rate, uint32 sum_level, bool is_dungeon, Player* not_gray_member_with_max_level, Player* member_with_max_level, uint32 xp)
{
    // honor can be in PvP and !PvP (racial leader) cases (for alive)
    if (pGroupGuy->IsAlive())
    {
        pGroupGuy->RewardHonor(pVictim, count);
    }

    // xp and reputation only in !PvP case
    if (!PvP)
    {
        float rate = group_rate * float(pGroupGuy->getLevel()) / sum_level;

        // if is in dungeon then all receive full reputation at kill
        // rewarded any alive/dead/near_corpse group member
        pGroupGuy->RewardReputation(pVictim, is_dungeon ? 1.0f : rate);

        // XP updated only for alive group member
        if (pGroupGuy->IsAlive() && not_gray_member_with_max_level &&
            pGroupGuy->getLevel() <= not_gray_member_with_max_level->getLevel())
        {
            uint32 itr_xp = (member_with_max_level == not_gray_member_with_max_level) ? uint32(xp * rate) : uint32((xp * rate / 2) + 1);

            pGroupGuy->GiveXP(itr_xp, pVictim);
            if (Pet* pet = pGroupGuy->GetPet())
            {
                pet->GivePetXP(itr_xp);
            }
        }

        // quest objectives updated only for alive group member or dead but with not released body
        if (pGroupGuy->IsAlive() || !pGroupGuy->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
        {
            // normal creature (not pet/etc) can be only in !PvP case
            if (pVictim->GetTypeId() == TYPEID_UNIT)
            {
                pGroupGuy->KilledMonster(((Creature*)pVictim)->GetCreatureInfo(), pVictim->GetObjectGuid());
            }
        }
    }
}

/** Provide rewards to group members at unit kill
 *
 * @param pVictim       Killed unit
 * @param player_tap    Player who tap unit if online, it can be group member or can be not if leaved after tap but before kill target
 *
 * Rewards received by group members and player_tap
 */
void Group::RewardGroupAtKill(Unit* pVictim, Player* player_tap)
{
    bool PvP = pVictim->IsCharmedOwnedByPlayerOrPlayer();

    // prepare data for near group iteration (PvP and !PvP cases)
    uint32 count = 0;
    uint32 sum_level = 0;
    Player* member_with_max_level = NULL;
    Player* not_gray_member_with_max_level = NULL;

    GetDataForXPAtKill(pVictim, count, sum_level, member_with_max_level, not_gray_member_with_max_level, player_tap);

    if (member_with_max_level)
    {
        /// not get Xp in PvP or no not gray players in group
        uint32 xp = (PvP || !not_gray_member_with_max_level) ? 0 : MaNGOS::XP::Gain(not_gray_member_with_max_level, pVictim);

        /// skip in check PvP case (for speed, not used)
        bool is_raid = PvP ? false : sMapStore.LookupEntry(pVictim->GetMapId())->IsRaid() && isRaidGroup();
        bool is_dungeon = PvP ? false : sMapStore.LookupEntry(pVictim->GetMapId())->IsDungeon();
        float group_rate = MaNGOS::XP::xp_in_group_rate(count, is_raid);

        for (GroupReference* itr = GetFirstMember(); itr != NULL; itr = itr->next())
        {
            Player* pGroupGuy = itr->getSource();
            if (!pGroupGuy)
            {
                continue;
            }

            // will proccessed later
            if (pGroupGuy == player_tap)
            {
                continue;
            }

            if (!pGroupGuy->IsAtGroupRewardDistance(pVictim))
            {
                continue;                                // member (alive or dead) or his corpse at req. distance
            }

            RewardGroupAtKill_helper(pGroupGuy, pVictim, count, PvP, group_rate, sum_level, is_dungeon, not_gray_member_with_max_level, member_with_max_level, xp);
        }

        if (player_tap)
        {
            // member (alive or dead) or his corpse at req. distance
            if (player_tap->IsAtGroupRewardDistance(pVictim))
            {
                RewardGroupAtKill_helper(player_tap, pVictim, count, PvP, group_rate, sum_level, is_dungeon, not_gray_member_with_max_level, member_with_max_level, xp);
            }
        }
    }
}
