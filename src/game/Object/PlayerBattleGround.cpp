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
 * @brief Teleports the player back to the saved battleground entry point.
 *
 * @return true if the teleport was initiated successfully; otherwise, false.
 */
bool Player::TeleportToBGEntryPoint()
{
    return TeleportTo(m_bgData.joinPos);
}

/**
 * @brief Stores the location used to return the player after leaving a battleground.
 *
 * @param leader The group leader to mirror entry positioning from, or NULL.
 */
void Player::SetBattleGroundEntryPoint(Player* leader /*= NULL*/)
{
    // chat command use case, or non-group join
    if (!leader || !leader->IsInWorld() || leader->IsTaxiFlying() || leader->GetMap()->IsDungeon() || leader->GetMap()->IsBattleGround())
    {
        leader = this;
    }

    if (leader->IsInWorld() && !leader->IsTaxiFlying())
    {
        // If map is dungeon find linked graveyard
        if (leader->GetMap()->IsDungeon())
        {
            if (const WorldSafeLocsEntry* entry = sObjectMgr.GetClosestGraveYard(leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ(), leader->GetMapId(), leader->GetTeam()))
            {
                m_bgData.joinPos = WorldLocation(entry->map_id, entry->x, entry->y, entry->z, 0.0f);
                m_bgData.m_needSave = true;
                return;
            }
            else
            {
                sLog.outError("SetBattleGroundEntryPoint: Dungeon map %u has no linked graveyard, setting home location as entry point.", leader->GetMapId());
            }
        }
        // If new entry point is not BG or arena set it
        else if (!leader->GetMap()->IsBattleGround())
        {
            m_bgData.joinPos = WorldLocation(leader->GetMapId(), leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ(), leader->GetOrientation());
            m_bgData.m_needSave = true;
            return;
        }
    }

    // In error cases use homebind position
    m_bgData.joinPos = WorldLocation(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, 0.0f);
    m_bgData.m_needSave = true;
}

/**
 * @brief Removes the player from the battleground and handles deserter penalties.
 *
 * @param teleportToEntryPoint True to teleport the player back to the saved entry point.
 */
void Player::LeaveBattleground(bool teleportToEntryPoint)
{
    if (BattleGround* bg = GetBattleGround())
    {
        bg->RemovePlayerAtLeave(GetObjectGuid(), teleportToEntryPoint, true);

        // call after remove to be sure that player resurrected for correct cast
        if (!isGameMaster() && sWorld.getConfig(CONFIG_BOOL_BATTLEGROUND_CAST_DESERTER))
        {
            if (bg->GetStatus() == STATUS_IN_PROGRESS || bg->GetStatus() == STATUS_WAIT_JOIN)
            {
                // lets check if player was teleported from BG and schedule delayed Deserter spell cast
                if (IsBeingTeleportedFar())
                {
                    ScheduleDelayedOperation(DELAYED_SPELL_CAST_DESERTER);
                    return;
                }

                CastSpell(this, 26013, true);               // Deserter
            }
        }
    }
}

/**
 * @brief Checks whether the player may currently join a battleground.
 *
 * @return True if the player can join; otherwise, false.
 */
bool Player::CanJoinToBattleground() const
{
    // check Deserter debuff
    if (GetDummyAura(26013))
    {
        return false;
    }

    return true;
}

/**
 * @brief Gets the battleground instance the player is currently associated with.
 *
 * @return The active battleground instance, or NULL if none exists.
 */
BattleGround* Player::GetBattleGround() const
{
    if (GetBattleGroundId() == 0)
    {
        return NULL;
    }

    return sBattleGroundMgr.GetBattleGround(GetBattleGroundId(), m_bgData.bgTypeID);
}

/**
 * @brief Checks whether the player's level fits a battleground's allowed range.
 *
 * @param bgTypeId The battleground type to evaluate.
 * @return True if the player's level is within range; otherwise, false.
 */
bool Player::GetBGAccessByLevel(BattleGroundTypeId bgTypeId) const
{
    // get a template bg instead of running one
    BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
    if (!bg)
    {
        return false;
    }

    if (getLevel() < bg->GetMinLevel() || getLevel() > bg->GetMaxLevel())
    {
        return false;
    }

    return true;
}

/**
 * @brief Moves the player into a battleground raid group.
 *
 * @param group The battleground raid group.
 * @param subgroup The battleground subgroup index.
 */
void Player::SetBattleGroundRaid(Group* group, int8 subgroup)
{
    // we must move references from m_group to m_originalGroup
    SetOriginalGroup(GetGroup(), GetSubGroup());

    m_group.unlink();
    m_group.link(group, this);
    m_group.setSubGroup((uint8)subgroup);
}

/**
 * @brief Restores the player's original group after leaving a battleground raid.
 */
void Player::RemoveFromBattleGroundRaid()
{
    // remove existing reference
    m_group.unlink();
    if (Group* group = GetOriginalGroup())
    {
        m_group.link(group, this);
        m_group.setSubGroup(GetOriginalSubGroup());
    }
    SetOriginalGroup(NULL);
}

/**
 * @brief Checks whether the player can interact with a battleground object.
 *
 * @return True if the player can use the object; otherwise, false.
 */
bool Player::CanUseBattleGroundObject()
{
    // TODO : some spells gives player ForceReaction to one faction (ReputationMgr::ApplyForceReaction)
    // maybe gameobject code should handle that ForceReaction usage
    return (IsAlive() &&                                    // living
        // the following two are incorrect, because invisible/stealthed players should get visible when they click on flag
        !HasStealthAura() &&                            // not stealthed
        !HasInvisibilityAura() &&                       // visible
        !isTotalImmune());                              // vulnerable (not immune)
}
