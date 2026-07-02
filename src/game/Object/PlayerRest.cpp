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
 * @brief Consumes and returns the rested experience bonus for an XP award.
 *
 * @param xp The base experience amount being awarded.
 * @return The rested bonus experience amount.
 */
uint32 Player::GetXPRestBonus(uint32 xp)
{
    uint32 rested_bonus = (uint32)GetRestBonus();           // xp for each rested bonus

    if (rested_bonus > xp)                                  // max rested_bonus == xp or (r+x) = 200% xp
    {
        rested_bonus = xp;
    }

    SetRestBonus(GetRestBonus() - rested_bonus);

    DETAIL_LOG("Player gain %u xp (+ %u Rested Bonus). Rested points=%f", xp + rested_bonus, rested_bonus, GetRestBonus());
    return rested_bonus;
}

/**
 * @brief Updates the player's current resting state.
 *
 * @param n_r_type The new rest type.
 * @param areaTriggerId The inn or rest area trigger identifier, if applicable.
 */
void Player::SetRestType(RestType n_r_type, uint32 areaTriggerId /*= 0*/)
{
    rest_type = n_r_type;

    if (rest_type == REST_TYPE_NO)
    {
        RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

        // Set player to FFA PVP when not in rested environment.
        if (sWorld.IsFFAPvPRealm())
        {
            SetFFAPvP(true);
        }
    }
    else
    {
        SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING);

        inn_trigger_id = areaTriggerId;
        time_inn_enter = time(NULL);

        if (sWorld.IsFFAPvPRealm())
        {
            SetFFAPvP(false);
        }
    }
}

/**
 * @brief Computes rested experience gained over a period of time.
 *
 * @param timePassed The elapsed time in seconds.
 * @param offline True when the gain is being computed for offline time.
 * @param inRestPlace True when the player is in a rest area while offline.
 * @return The amount of rested experience bonus gained.
 */
float Player::ComputeRest(time_t timePassed, bool offline /*= false*/, bool inRestPlace /*= false*/)
{
    // Every 8h in resting zone we gain a bubble
    // A bubble is 5% of the total xp so there is 20 bubbles
    // So we gain (total XP/20 every 8h) (8h = 288800 sec)
    // (TotalXP/20)/28800; simplified to (TotalXP/576000) per second
    // Client automatically double the value sent so we have to divide it by 2
    // So final formula (TotalXP/1152000)
    float bonus = timePassed * (GetUInt32Value(PLAYER_NEXT_LEVEL_XP) / 1152000.0f); // Get the gained rest xp for given second
    if (!offline)
    {
        bonus *= sWorld.getConfig(CONFIG_FLOAT_RATE_REST_INGAME);                   // Apply the custom setting
    }
    else
    {
        if (inRestPlace)
        {
            bonus *= sWorld.getConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_TAVERN_OR_CITY);
        }
        else
        {
            bonus *= sWorld.getConfig(CONFIG_FLOAT_RATE_REST_OFFLINE_IN_WILDERNESS) / 4.0f; // bonus is reduced by 4 when not in rest place
        }
    }
    return bonus;
}
