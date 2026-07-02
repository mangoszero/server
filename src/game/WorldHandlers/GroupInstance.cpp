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
#include "Formulas.h"
#include "ObjectAccessor.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "Util.h"
#include "LootMgr.h"
#include "LFGMgr.h"
#include "LFGHandler.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

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
        {
            if (map->IsDungeon() && !(method == INSTANCE_RESET_GROUP_DISBAND && !state->CanReset()))
            {
                isEmpty = ((DungeonMap*)map)->Reset(method);
            }
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
            {
                if (!load)
                {
                    CharacterDatabase.PExecute("UPDATE `group_instance` SET `instance` = '%u', `permanent` = '%u' WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
                        state->GetInstanceId(), permanent, GetLeaderGuid().GetCounter(), bind.state->GetInstanceId());
                }
            }
        }
        else if (!load)
        {
            CharacterDatabase.PExecute("INSERT INTO `group_instance` (`leaderGuid`, `instance`, `permanent`) VALUES ('%u', '%u', '%u')",
                GetLeaderGuid().GetCounter(), state->GetInstanceId(), permanent);
        }

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
        {
            DEBUG_LOG("Group::BindToInstance: Group (Id: %d) is now bound to map %d, instance %d",
                GetId(), state->GetMapId(), state->GetInstanceId());
        }
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
        {
            CharacterDatabase.PExecute("DELETE FROM `group_instance` WHERE `leaderGuid` = '%u' AND `instance` = '%u'",
                GetLeaderGuid().GetCounter(), itr->second.state->GetInstanceId());
        }
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
