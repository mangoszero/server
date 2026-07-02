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



#include "Group.h"
#include "Common.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "ObjectAccessor.h"
#include "Util.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "LootMgr.h"
#include "LFGMgr.h"
#include "LFGHandler.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

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
            {
                if (bind->state->GetInstanceId() == player->GetInstanceId())
                {
                    player->m_InstanceValid = true;
                }
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
    {
        CharacterDatabase.PExecute("UPDATE `groups` SET `mainAssistant`='%u' WHERE `groupId`='%u'",
            m_mainAssistantGuid.GetCounter(), m_Id);
    }

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
