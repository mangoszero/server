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



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Checks whether another player should be visible through group visibility rules.
 *
 * @param p The player to test visibility against.
 * @return True if the player is group-visible; otherwise, false.
 */
bool Player::IsGroupVisibleFor(Player* p) const
{
    switch (sWorld.getConfig(CONFIG_UINT32_GROUP_VISIBILITY))
    {
        default:
            return IsInSameGroupWith(p);
        case 1:
            return IsInSameRaidWith(p);
        case 2:
            return GetTeam() == p->GetTeam();
    }
}

/**
 * @brief Checks whether this player and another player are in the same subgroup.
 *
 * @param p The other player to compare.
 * @return True if both players share the same group context; otherwise, false.
 */
bool Player::IsInSameGroupWith(Player const* p) const
{
    return (p == this ||
        (GetGroup() != NULL &&
        GetGroup()->SameSubGroup(this, p)));
}

///- If the player is invited, remove him. If the group if then only 1 person, disband the group.
/// \todo Shouldn't we also check if there is no other invitees before disbanding the group?
void Player::UninviteFromGroup()
{
    Group* group = GetGroupInvite();
    if (!group)
    {
        return;
    }

    group->RemoveInvite(this);

    if (group->GetMembersCount() <= 1)                      // group has just 1 member => disband
    {
        if (group->IsCreated())
        {
            group->Disband(true);
            sObjectMgr.RemoveGroup(group);
        }
        else
        {
            group->RemoveAllInvites();
        }

        delete group;
    }
}

/**
 * @brief Removes a member from a group and disposes of the group if it becomes empty.
 *
 * @param group The group to remove the member from.
 * @param guid The GUID of the member to remove.
 * @param removeMethod The group removal method to apply.
 */
void Player::RemoveFromGroup(Group* group, ObjectGuid guid, uint8 removeMethod)
{
    if (group)
    {
        if (group->RemoveMember(guid, removeMethod) <= 1)
        {
            // group->Disband(); already disbanded in RemoveMember
            sObjectMgr.RemoveGroup(group);
            delete group;
            // RemoveMember sets the player's group pointer to NULL
        }
    }
}

/**
 * @brief Assigns the player to a group and subgroup.
 *
 * @param group The group to join, or NULL to clear membership.
 * @param subgroup The subgroup index when joining a group.
 */
void Player::SetGroup(Group* group, int8 subgroup)
{
    if (group == NULL)
    {
        m_group.unlink();
    }
    else
    {
        // never use SetGroup without a subgroup unless you specify NULL for group
        MANGOS_ASSERT(subgroup >= 0);
        m_group.link(group, this);
        m_group.setSubGroup((uint8)subgroup);
    }

#ifdef ENABLE_PLAYERBOTS
    if (GetPlayerbotAI())
    {
        GetPlayerbotAI()->ResetStrategies();
    }
#endif
}

/**
 * @brief Flushes pending group update data to out-of-range group members.
 */
void Player::SendUpdateToOutOfRangeGroupMembers()
{
    if (m_groupUpdateMask == GROUP_UPDATE_FLAG_NONE)
    {
        return;
    }
    if (Group* group = GetGroup())
    {
        group->UpdatePlayerOutOfRange(this);
    }

    m_groupUpdateMask = GROUP_UPDATE_FLAG_NONE;
    m_auraUpdateMask = 0;
    if (Pet* pet = GetPet())
    {
        pet->ResetAuraUpdateMask();
    }
}

/**
 * @brief Selects a random nearby raid member within a radius.
 *
 * @param radius The maximum search radius.
 * @return A random eligible raid member, or NULL if none are found.
 */
Player* Player::GetNextRandomRaidMember(float radius)
{
    Group* pGroup = GetGroup();
    if (!pGroup)
    {
        return NULL;
    }

    std::vector<Player*> nearMembers;
    nearMembers.reserve(pGroup->GetMembersCount());

    for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* Target = itr->getSource();

        // IsHostileTo check duel and controlled by enemy
        if (Target && Target != this && IsWithinDistInMap(Target, radius) &&
            !Target->HasInvisibilityAura() && !IsHostileTo(Target))
        {
            nearMembers.push_back(Target);
        }
    }

    if (nearMembers.empty())
    {
        return NULL;
    }

    uint32 randTarget = urand(0, nearMembers.size() - 1);
    return nearMembers[randTarget];
}

/**
 * @brief Checks whether the player is allowed to uninvite someone from the group.
 *
 * @return The party result code describing whether uninvite is allowed.
 */
PartyResult Player::CanUninviteFromGroup() const
{
    const Group* grp = GetGroup();
    if (!grp)
    {
        return ERR_NOT_IN_GROUP;
    }

    if (!grp->IsLeader(GetObjectGuid()) && !grp->IsAssistant(GetObjectGuid()))
    {
        return ERR_NOT_LEADER;
    }

    if (InBattleGround())
    {
        return ERR_NOT_IN_GROUP; // error message is not so appropriated but no other option for classic
    }

    return ERR_PARTY_RESULT_OK;
}

/**
 * @brief Stores the player's original non-battleground group reference.
 *
 * @param group The original group, or NULL to clear it.
 * @param subgroup The original subgroup index.
 */
void Player::SetOriginalGroup(Group* group, int8 subgroup)
{
    if (group == NULL)
    {
        m_originalGroup.unlink();
    }
    else
    {
        // never use SetOriginalGroup without a subgroup unless you specify NULL for group
        MANGOS_ASSERT(subgroup >= 0);
        m_originalGroup.link(group, this);
        m_originalGroup.setSubGroup((uint8)subgroup);
    }
}
