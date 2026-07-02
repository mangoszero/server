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
 * @brief Sends the appropriate transfer-aborted feedback for an area lock failure.
 *
 * @param mapEntry The destination map entry.
 * @param at The triggering area trigger, if any.
 * @param lockStatus The evaluated area lock status.
 * @param miscRequirement Extra requirement data used by some messages.
 */
void Player::SendTransferAbortedByLockStatus(MapEntry const* mapEntry, AreaTrigger const* at, AreaLockStatus lockStatus, uint32 miscRequirement)
{
    MANGOS_ASSERT(mapEntry);

    DEBUG_LOG("SendTransferAbortedByLockStatus: Called for %s on map %u, LockAreaStatus %u, miscRequirement %u)", GetGuidStr().c_str(), mapEntry->MapID, lockStatus, miscRequirement);

    if (at && at->failed_text_mangos_string_id > 0)
    {
        GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(at->failed_text_mangos_string_id));
        return;
    }

    switch (lockStatus)
    {
        case AREA_LOCKSTATUS_LEVEL_TOO_LOW:
            GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_MINREQUIRED), miscRequirement);
            break;
        case AREA_LOCKSTATUS_LEVEL_TOO_HIGH:
            GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_MAXREQUIRED), miscRequirement);
            break;
        case AREA_LOCKSTATUS_LEVEL_NOT_EQUAL:
            GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_EQUALREQUIRED), miscRequirement);
            break;
        case AREA_LOCKSTATUS_ZONE_IN_COMBAT:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_ZONE_IN_COMBAT);
            break;
        case AREA_LOCKSTATUS_INSTANCE_IS_FULL:
            GetSession()->SendTransferAborted(mapEntry->MapID, TRANSFER_ABORT_MAX_PLAYERS);
            break;
        case AREA_LOCKSTATUS_WRONG_TEAM:
            if (miscRequirement == 469)
            {
                GetSession()->SendAreaTriggerMessage("%s", GetSession()->GetMangosString(LANG_WRONG_TEAM_ALLIANCE));
            }
            else
            {
                GetSession()->SendAreaTriggerMessage("%s", GetSession()->GetMangosString(LANG_WRONG_TEAM_HORDE));
            }
            break;
        case AREA_LOCKSTATUS_QUEST_NOT_COMPLETED:
            if (mapEntry->IsContinent())               // do not report anything for quest areatrigge
            {
                DEBUG_LOG("SendTransferAbortedByLockStatus: LockAreaStatus %u, do not teleport, no message sent (mapId %u)", lockStatus, mapEntry->MapID);
                break;
            }
            // ToDo: SendAreaTriggerMessage or Transfer Abort for these cases!
            break;
        case AREA_LOCKSTATUS_MISSING_ITEM:
            if (sObjectMgr.GetMapEntranceTrigger(mapEntry->MapID))
            {
                GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_REQUIRED_ITEM), sObjectMgr.GetItemPrototype(miscRequirement)->Name1);
            }
            break;
        case AREA_LOCKSTATUS_NOT_ALLOWED:
        case AREA_LOCKSTATUS_RAID_LOCKED:
        case AREA_LOCKSTATUS_UNKNOWN_ERROR:
            // ToDo: SendAreaTriggerMessage or Transfer Abort for these cases!
            break;
        case AREA_LOCKSTATUS_PVP_RANK:
        {
            // This portion of code should never be hit anymore since an AreaTrigger should handle that.
            const std::string msg = "You cannot enter this zone"; // fallback message
            GetSession()->SendAreaTriggerMessage(msg.c_str());
            break;
        }

        case AREA_LOCKSTATUS_OK:
            sLog.outError("SendTransferAbortedByLockStatus: LockAreaStatus AREA_LOCKSTATUS_OK received for %s (mapId %u)", GetGuidStr().c_str(), mapEntry->MapID);
            MANGOS_ASSERT(false);
            break;
        default:
            sLog.outError("SendTransfertAbortedByLockstatus: unhandled LockAreaStatus %u, when %s attempts to enter in map %u", lockStatus, GetGuidStr().c_str(), mapEntry->MapID);
            break;
    }
}

/**
 * @brief Evaluates whether the player can use an area trigger into another map.
 *
 * @param at The area trigger being used.
 * @param miscRequirement Output requirement data for failure messaging.
 * @return The evaluated area lock status.
 */
AreaLockStatus Player::GetAreaTriggerLockStatus(AreaTrigger const* at, uint32& miscRequirement)
{
    miscRequirement = 0;

    if (!at)
    {
        return AREA_LOCKSTATUS_UNKNOWN_ERROR;
    }

    MapEntry const* mapEntry = sMapStore.LookupEntry(at->target_mapId);
    if (!mapEntry)
    {
        return AREA_LOCKSTATUS_UNKNOWN_ERROR;
    }

    // Gamemaster can always enter
    if (isGameMaster())
    {
        return AREA_LOCKSTATUS_OK;
    }

    // Raid Requirements
    if (mapEntry->IsRaid() && !sWorld.getConfig(CONFIG_BOOL_INSTANCE_IGNORE_RAID))
    {
        if (!GetGroup() || !GetGroup()->isRaidGroup())
        {
            return AREA_LOCKSTATUS_RAID_LOCKED;
        }
    }

    if (at->condition) //condition validity is checked at startup
    {
        ConditionEntry fault;
        if (!sObjectMgr.IsPlayerMeetToCondition(at->condition, this, GetMap(),NULL, CONDITION_AREA_TRIGGER, &fault))
        {
            switch (fault.type)
            {
                case CONDITION_LEVEL:
                {
                    if (sWorld.getConfig(CONFIG_BOOL_INSTANCE_IGNORE_LEVEL))
                    {
                        break;
                    }
                    else
                    {
                        miscRequirement = fault.param1;
                        switch (fault.param2)
                        {
                            case 0: { return AREA_LOCKSTATUS_LEVEL_NOT_EQUAL; }
                            case 1: { return AREA_LOCKSTATUS_LEVEL_TOO_LOW; }
                            case 2: { return AREA_LOCKSTATUS_LEVEL_TOO_HIGH; }
                        }
                    }
                }

                case CONDITION_ITEM:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_MISSING_ITEM;
                }

                case CONDITION_QUESTREWARDED:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_QUEST_NOT_COMPLETED;
                }

                case CONDITION_TEAM:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_WRONG_TEAM;
                }

                case CONDITION_PVP_RANK:
                {
                    miscRequirement = fault.param1;
                    return AREA_LOCKSTATUS_PVP_RANK;
                }

                default:
                    return AREA_LOCKSTATUS_UNKNOWN_ERROR;
            }
        }
    }

    // If the map is not created, assume it is possible to enter it.
    DungeonPersistentState* state = GetBoundInstanceSaveForSelfOrGroup(at->target_mapId);
    Map* map = sMapMgr.FindMap(at->target_mapId, state ? state->GetInstanceId() : 0);

    // Map's state check
    if (map && map->IsDungeon())
    {
        // can not enter if the instance is full (player cap), GMs don't count
        if (((DungeonMap*)map)->GetPlayersCountExceptGMs() >= ((DungeonMap*)map)->GetMaxPlayers())
        {
            return AREA_LOCKSTATUS_INSTANCE_IS_FULL;
        }

        // In Combat check
        if (map && map->GetInstanceData() && map->GetInstanceData()->IsEncounterInProgress())
        {
            return AREA_LOCKSTATUS_ZONE_IN_COMBAT;
        }

        // Bind Checks
        InstancePlayerBind* pBind = GetBoundInstance(at->target_mapId);
        if (pBind && pBind->perm && pBind->state != state)
        {
            return AREA_LOCKSTATUS_HAS_BIND;
        }
        if (pBind && pBind->perm && pBind->state != map->GetPersistentState())
        {
            return AREA_LOCKSTATUS_HAS_BIND;
        }
    }

    return AREA_LOCKSTATUS_OK;
};
