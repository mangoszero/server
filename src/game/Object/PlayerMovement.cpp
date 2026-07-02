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
 * @brief Forces or clears rooted movement for the player.
 *
 * @param enable True to root the player; false to unroot them.
 */
void Player::SetRoot(bool enable)
{
    WorldPacket data(enable ? SMSG_FORCE_MOVE_ROOT : SMSG_FORCE_MOVE_UNROOT, GetPackGUID().size() + 4);
    data << GetPackGUID();
    data << uint32(0);
    SendMessageToSet(&data, true);
}

/**
 * @brief Enables or disables water walking for the player.
 *
 * @param enable True to enable water walking; false to restore normal movement.
 */
void Player::SetWaterWalk(bool enable)
{
    WorldPacket data(enable ? SMSG_MOVE_WATER_WALK : SMSG_MOVE_LAND_WALK, GetPackGUID().size() + 4);
    data << GetPackGUID();
    data << uint32(0);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Placeholder for levitation support on this client version.
 *
 * @param enable Unused levitation state flag.
 */
void Player::SetLevitate(bool /*enable*/)
{
    // TODO: check if there is something similar for 2.4.3.
    // WorldPacket data;
    // if (enable)
    //    data.Initialize(SMSG_MOVE_GRAVITY_DISABLE, 12);
    // else
    //    data.Initialize(SMSG_MOVE_GRAVITY_ENABLE, 12);
    //
    // data << GetPackGUID();
    // data << uint32(0);                                      // unk
    // SendMessageToSet(&data, true);

    // data.Initialize(MSG_MOVE_GRAVITY_CHNG, 64);
    // data << GetPackGUID();
    // m_movementInfo.Write(data);
    // SendMessageToSet(&data, false);
}

/**
 * @brief Enables or disables flying movement flags for the player.
 *
 * @param enable True to enable flight-related movement flags; false to clear them.
 */
void Player::SetCanFly(bool enable)
{
    //     TODO: check if there is something similar for 1.12.x (99% chance there is not)
    if (enable)
    {
        m_movementInfo.SetMovementFlags((MovementFlags)(MOVEFLAG_LEVITATING | MOVEFLAG_SWIMMING | MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING));
    }
    else
    {
        m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
    }

    SendHeartBeat();
}

/**
 * @brief Enables or disables feather fall movement for the player.
 *
 * @param enable True to enable feather fall; false to restore normal falling.
 */
void Player::SetFeatherFall(bool enable)
{
    WorldPacket data;
    if (enable)
    {
        data.Initialize(SMSG_MOVE_FEATHER_FALL, 8 + 4);
    }
    else
    {
        data.Initialize(SMSG_MOVE_NORMAL_FALL, 8 + 4);
    }

    data << GetPackGUID();
    data << uint32(0);
    SendMessageToSet(&data, true);

    // start fall from current height
    if (!enable)
    {
        SetFallInformation(0, GetPositionZ());
    }
}

/**
 * @brief Enables or disables hover movement for the player.
 *
 * @param enable True to enable hovering; false to disable it.
 */
void Player::SetHover(bool enable)
{
    WorldPacket data;
    if (enable)
    {
        data.Initialize(SMSG_MOVE_SET_HOVER, 8 + 4);
    }
    else
    {
        data.Initialize(SMSG_MOVE_UNSET_HOVER, 8 + 4);
    }

    data << GetPackGUID();
    data << uint32(0);
    SendMessageToSet(&data, true);
}
