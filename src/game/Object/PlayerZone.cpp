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

#define ZONE_UPDATE_INTERVAL (1*IN_MILLISECONDS)

/**
 * @brief Updates outdoor-only effects and exploration discovery for the current position.
 */
void Player::CheckAreaExploreAndOutdoor()
{
    if (!IsAlive())
    {
        return;
    }

    if (IsTaxiFlying())
    {
        return;
    }

    bool isOutdoor;
    uint16 areaFlag = GetMap()->GetTerrain()->GetAreaFlag(GetPositionX(), GetPositionY(), GetPositionZ(), &isOutdoor);

    if (isOutdoor)
    {
        if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) && GetRestType() == REST_TYPE_IN_TAVERN)
        {
            AreaTriggerEntry const* at = sAreaTriggerStore.LookupEntry(inn_trigger_id);
            if (!at || !IsPointInAreaTriggerZone(at, GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ()))
            {
                // Player left inn (REST_TYPE_IN_CITY overrides REST_TYPE_IN_TAVERN, so just clear rest)
                SetRestType(REST_TYPE_NO);
            }
        }
        // Check if we need to reaply outdoor only passive spells
        const PlayerSpellMap& sp_list = GetSpellMap();
        for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
        {
            if (itr->second.state == PLAYERSPELL_REMOVED)
            {
                continue;
            }
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);
            if (!spellInfo || !IsNeedCastSpellAtOutdoor(spellInfo) || HasAura(itr->first))
            {
                continue;
            }

            ShapeshiftForm form = GetShapeshiftForm();
            if (!(spellInfo->Stances & (1 << (form - 1))))
            {
                continue;
            }
            if ((spellInfo->Stances || spellInfo->StancesNot) && !IsNeedCastSpellAtFormApply(spellInfo, GetShapeshiftForm()))
            {
                continue;
            }
            CastSpell(this, itr->first, true, NULL);
        }
    }
    else if (sWorld.getConfig(CONFIG_BOOL_VMAP_INDOOR_CHECK) && !isGameMaster())
    {
        RemoveAurasWithAttribute(SPELL_ATTR_OUTDOORS_ONLY);
    }

    if (areaFlag == 0xffff)
    {
        return;
    }
    int offset = areaFlag / 32;

    if (offset >= PLAYER_EXPLORED_ZONES_SIZE)
    {
        sLog.outError("Wrong area flag %u in map data for (X: %f Y: %f) point to field PLAYER_EXPLORED_ZONES_1 + %u ( %u must be < %u ).", areaFlag, GetPositionX(), GetPositionY(), offset, offset, PLAYER_EXPLORED_ZONES_SIZE);
        return;
    }

    uint32 val = (uint32)(1 << (areaFlag % 32));
    uint32 currFields = GetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset);

    if (!(currFields & val))
    {
        SetUInt32Value(PLAYER_EXPLORED_ZONES_1 + offset, (uint32)(currFields | val));

        AreaTableEntry const* p = GetAreaEntryByAreaFlagAndMap(areaFlag, GetMapId());
        if (!p)
        {
            sLog.outError("PLAYER: Player %u discovered unknown area (x: %f y: %f map: %u", GetGUIDLow(), GetPositionX(), GetPositionY(), GetMapId());
        }
        else if (p->area_level > 0)
        {
            uint32 area = p->ID;
            if (getLevel() >= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                SendExplorationExperience(area, 0);
            }
            else
            {
                int32 diff = int32(getLevel()) - p->area_level;
                uint32 XP = 0;
                if (diff < -5)
                {
                    XP = uint32(sObjectMgr.GetBaseXP(getLevel() + 5) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_EXPLORE));
                }
                else if (diff > 5)
                {
                    int32 exploration_percent = (100 - ((diff - 5) * 5));
                    if (exploration_percent > 100)
                    {
                        exploration_percent = 100;
                    }
                    else if (exploration_percent < 0)
                    {
                        exploration_percent = 0;
                    }

                    XP = uint32(sObjectMgr.GetBaseXP(p->area_level) * exploration_percent / 100 * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_EXPLORE));
                }
                else
                {
                    XP = uint32(sObjectMgr.GetBaseXP(p->area_level) * sWorld.getConfig(CONFIG_FLOAT_RATE_XP_EXPLORE));
                }

                GiveXP(XP, NULL);
                SendExplorationExperience(area, XP);
            }
            DETAIL_LOG("PLAYER: Player %u discovered a new area: %u", GetGUIDLow(), area);
        }
    }
}

/**
 * @brief Updates area-specific player state and auras.
 *
 * @param newArea The new area identifier.
 */
void Player::UpdateArea(uint32 newArea)
{
    m_areaUpdateId    = newArea;

    AreaTableEntry const* area = GetAreaEntryByAreaID(newArea);

    // FFA_PVP flags are area and not zone id dependent
    // so apply them accordingly
    if (area && (area->flags & AREA_FLAG_ARENA))
    {
        if (!isGameMaster())
        {
            SetFFAPvP(true);
        }
    }
    else
    {
        // remove ffa flag only if not ffapvp realm
        // removal in sanctuaries and capitals is handled in zone update
        if (IsFFAPvP() && !sWorld.IsFFAPvPRealm())
        {
            SetFFAPvP(false);
        }
    }

    UpdateAreaDependentAuras();
}

/**
 * @brief Updates zone and area state after the player changes location.
 *
 * @param newZone The new zone identifier.
 * @param newArea The new area identifier.
 */
void Player::UpdateZone(uint32 newZone, uint32 newArea)
{
    /* If we're trying to update into a zone that doesn't exist, just return */
    AreaTableEntry const* zone = GetAreaEntryByAreaID(newZone);
    if (!zone)
    {
        return;
    }

    /* If we're moving into a different zone */
    if (m_zoneUpdateId != newZone)
    {
        // handle outdoor pvp zones
        sOutdoorPvPMgr.HandlePlayerLeaveZone(this, m_zoneUpdateId);
        sOutdoorPvPMgr.HandlePlayerEnterZone(this, newZone);

#ifdef ENABLE_PLAYERBOTS
        sRandomPlayerbotMgr.OnPlayerZoneChange(this, newZone);
#endif

        SendInitWorldStates(newZone);                       // only if really enters to new zone, not just area change, works strange...

        if (sWorld.getConfig(CONFIG_BOOL_WEATHER))
        {
            Weather* wth = GetMap()->GetWeatherSystem()->FindOrCreateWeather(newZone);
            wth->SendWeatherUpdateToPlayer(this);
        }
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnUpdateZone(this, newZone, newArea);
    }
#endif /* ENABLE_ELUNA */

    m_zoneUpdateId    = newZone;
    m_zoneUpdateTimer = ZONE_UPDATE_INTERVAL;

    // zone changed, so area changed as well, update it
    UpdateArea(newArea);

    // in PvP, any not controlled zone (except zone->team == 6, default case)
    // in PvE, only opposition team capital
    switch (zone->team)
    {
        case AREATEAM_ALLY:
            pvpInfo.inHostileArea = GetTeam() != ALLIANCE && (sWorld.IsPvPRealm() || zone->flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_HORDE:
            pvpInfo.inHostileArea = GetTeam() != HORDE && (sWorld.IsPvPRealm() || zone->flags & AREA_FLAG_CAPITAL);
            break;
        case AREATEAM_NONE:
            // overwrite for battlegrounds, maybe batter some zone flags but current known not 100% fit to this
            pvpInfo.inHostileArea = sWorld.IsPvPRealm() || InBattleGround();
            break;
        default:                                            // 6 in fact
            pvpInfo.inHostileArea = false;
            break;
    }

    if (pvpInfo.inHostileArea)                              // in hostile area
    {
        if (!IsPvP() || pvpInfo.endTimer != 0)
        {
            UpdatePvP(true, true);
        }
    }
    else                                                    // in friendly area
    {
        if (IsPvP() && !HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_IN_PVP) && pvpInfo.endTimer == 0)
        {
            pvpInfo.endTimer = time(0); // start toggle-off
        }
    }

    if (zone->flags & AREA_FLAG_CAPITAL)                    // in capital city
    {
        SetRestType(REST_TYPE_IN_CITY);
    }
    else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_RESTING) && GetRestType() != REST_TYPE_IN_TAVERN)
    {
        // resting and not in tavern (leave city then); tavern leave handled in CheckAreaExploreAndOutdoor
        SetRestType(REST_TYPE_NO);
    }

    // remove items with area/map limitations (delete only for alive player to allow back in ghost mode)
    // if player resurrected at teleport this will be applied in resurrect code
    if (IsAlive())
    {
        DestroyZoneLimitedItem(true, newZone);
    }

    // recent client version not send leave/join channel packets for built-in local channels
    // When flying in a taxi we don't change channels in zero, for a proof video see:
    // youtu.be/iUFpZeNGPSs?t=32m where it doesn't change the channel until he lands
    if (!IsTaxiFlying())
    {
        UpdateLocalChannels(newZone);
    }

    // group update
    if (GetGroup())
    {
        SetGroupUpdateFlag(GROUP_UPDATE_FLAG_ZONE);
    }

    UpdateZoneDependentAuras();
}

/**
 * @brief Updates the invalid-instance homebind timer and teleports when it expires.
 *
 * @param time The elapsed update time in milliseconds.
 */
void Player::UpdateHomebindTime(uint32 time)
{
    // GMs never get homebind timer online
    if (m_InstanceValid || isGameMaster())
    {
        if (m_HomebindTimer)                                // instance valid, but timer not reset
        {
            // hide reminder
            WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
            data << uint32(0);
            data << uint32(ERR_RAID_GROUP_REQUIRED);        // error used only when timer = 0
            GetSession()->SendPacket(&data);
        }
        // instance is valid, reset homebind timer
        m_HomebindTimer = 0;
    }
    else if (m_HomebindTimer > 0)
    {
        if (time >= m_HomebindTimer)
        {
            // teleport to homebind location
            TeleportTo(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, GetOrientation());
        }
        else
        {
            m_HomebindTimer -= time;
        }
    }
    else
    {
        // instance is invalid, start homebind timer
        m_HomebindTimer = 60000;
        // send message to player
        WorldPacket data(SMSG_RAID_GROUP_ONLY, 4 + 4);
        data << uint32(m_HomebindTimer);
        data << uint32(ERR_RAID_GROUP_REQUIRED);        // error used only when timer = 0
        GetSession()->SendPacket(&data);
        DEBUG_LOG("PLAYER: Player '%s' (GUID: %u) will be teleported to homebind in 60 seconds", GetName(), GetGUIDLow());
    }
}

/**
 * @brief Applies or removes auras that depend on the player's current zone.
 */
void Player::UpdateZoneDependentAuras()
{
    // Some spells applied at enter into zone (with subzones), aura removed in UpdateAreaDependentAuras that called always at zone->area update
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(m_zoneUpdateId);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, m_zoneUpdateId, 0, true);
    }
}

/**
 * @brief Applies or removes auras that depend on the player's current subzone.
 */
void Player::UpdateAreaDependentAuras()
{
    // remove auras from spells with area limitations
    for (SpellAuraHolderMap::iterator iter = m_spellAuraHolders.begin(); iter != m_spellAuraHolders.end();)
    {
        // use m_zoneUpdateId for speed: UpdateArea called from UpdateZone or instead UpdateZone in both cases m_zoneUpdateId up-to-date
        if (sSpellMgr.GetSpellAllowedInLocationError(iter->second->GetSpellProto(), GetMapId(), m_zoneUpdateId, m_areaUpdateId, this) != SPELL_CAST_OK)
        {
            RemoveSpellAuraHolder(iter->second);
            iter = m_spellAuraHolders.begin();
        }
        else
        {
            ++iter;
        }
    }

    // some auras applied at subzone enter
    SpellAreaForAreaMapBounds saBounds = sSpellMgr.GetSpellAreaForAreaMapBounds(m_areaUpdateId);
    for (SpellAreaForAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
    {
        itr->second->ApplyOrRemoveSpellIfCan(this, m_zoneUpdateId, m_areaUpdateId, true);
    }
}
