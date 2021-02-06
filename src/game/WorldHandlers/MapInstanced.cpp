/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2021 MaNGOS <https://getmangos.eu>
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

#include "MapInstanced.h"

#include "World.h"

MapInstanced::MapInstanced(uint32 id, time_t expiry, uint32 InstanceId) :
	Map(id, expiry, InstanceId), m_resetAfterUnload(false), m_unloadWhenEmpty(false)
{
    MANGOS_ASSERT(i_mapEntry->IsDungeon());

    // lets initialize visibility distance for dungeons
    MapInstanced::InitVisibilityDistance();

    // the timer is started by default, and stopped when the first player joins
    // this make sure it gets unloaded if for some reason no player joins
    m_unloadTimer = std::max(sWorld.getConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY), uint32(MIN_UNLOAD_DELAY));
}

MapInstanced::~MapInstanced() { }

void MapInstanced::InitVisibilityDistance()
{
    // init visibility distance for instances
    m_VisibleDistance = World::GetMaxVisibleDistanceInInstances();
}

/*
    Do map specific checks and add the player to the map if successful.
*/
bool MapInstanced::Add(Player* player)
{
    // TODO: Not sure about checking player level: already done in HandleAreaTriggerOpcode
    // GMs still can teleport player in instance.
    // Is it needed?

    if (!CanEnter(player))
    {
        return false;
    }

    // check for existing instance binds
    InstancePlayerBind* playerBind = player->GetBoundInstance(GetId());
    if (playerBind && playerBind->perm)
    {
        // can not enter other instances if bound permanently
        if (playerBind->state != GetPersistanceState())
        {
            sLog.outError("InstanceMap::Add: player %s(%d) is permanently bound to instance %d,%d,%d,%d,%d but he is being put in instance %d,%d,%d,%d,%d",
                player->GetName(), player->GetGUIDLow(), playerBind->state->GetMapId(),
                playerBind->state->GetInstanceId(),
                playerBind->state->GetPlayerCount(), playerBind->state->GetGroupCount(),
                playerBind->state->CanReset(),
                GetPersistanceState()->GetMapId(), GetPersistanceState()->GetInstanceId(),
                GetPersistanceState()->GetPlayerCount(),
                GetPersistanceState()->GetGroupCount(), GetPersistanceState()->CanReset());
            MANGOS_ASSERT(false);
        }
    }
    else
    {
        Group* pGroup = player->GetGroup();
        if (pGroup)
        {
            // solo saves should be reset when entering a group
            InstanceGroupBind* groupBind = pGroup->GetBoundInstance(GetId());
            if (playerBind)
            {
                sLog.outError("InstanceMap::Add: %s is being put in instance %d,%d,%d,%d,%d but he is in group (Id: %d) and is bound to instance %d,%d,%d,%d,%d!",
                    player->GetObjectGuid().GetString().c_str(), playerBind->state->GetMapId(), playerBind->state->GetInstanceId(),
                    playerBind->state->GetPlayerCount(), playerBind->state->GetGroupCount(),
                    playerBind->state->CanReset(), pGroup->GetId(),
                    playerBind->state->GetMapId(), playerBind->state->GetInstanceId(),
                    playerBind->state->GetPlayerCount(), playerBind->state->GetGroupCount(), playerBind->state->CanReset());

                if (groupBind)
                    sLog.outError("InstanceMap::Add: the group (Id: %d) is bound to instance %d,%d,%d,%d,%d",
                        pGroup->GetId(),
                        groupBind->state->GetMapId(), groupBind->state->GetInstanceId(),
                        groupBind->state->GetPlayerCount(), groupBind->state->GetGroupCount(), groupBind->state->CanReset());

                // no reason crash if we can fix state
                player->UnbindInstance(GetId());
            }

            // bind to the group or keep using the group save
            if (!groupBind)
            {
                pGroup->BindToInstance(GetPersistanceState(), false);
            }
            else
            {
                // can not jump to a different instance without resetting it
                if (groupBind->state != GetPersistentState())
                {
                    sLog.outError("InstanceMap::Add: %s is being put in instance %d,%d but he is in group (Id: %d) which is bound to instance %d,%d!",
                        player->GetObjectGuid().GetString().c_str(), GetPersistentState()->GetMapId(),
                        GetPersistentState()->GetInstanceId(),
                        pGroup->GetId(), groupBind->state->GetMapId(),
                        groupBind->state->GetInstanceId());

                    sLog.outError("MapSave players: %d, group count: %d",
                        GetPersistanceState()->GetPlayerCount(), GetPersistanceState()->GetGroupCount());

                    if (groupBind->state)
                    {
                        sLog.outError("GroupBind save players: %d, group count: %d", groupBind->state->GetPlayerCount(), groupBind->state->GetGroupCount());
                    }
                    else
                    {
                        sLog.outError("GroupBind save NULL");
                    }
                    MANGOS_ASSERT(false);
                }
                // if the group/leader is permanently bound to the instance
                // players also become permanently bound when they enter
                if (groupBind->perm)
                {
                    WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
                    data << uint32(0);
                    player->GetSession()->SendPacket(&data);
                    player->BindToInstance(GetPersistanceState(), true);
                }
            }
        }
        else
        {
            // set up a solo bind or continue using it
            if (!playerBind)
            {
                player->BindToInstance(GetPersistanceState(), false);
            }
            else
                // can not jump to a different instance without resetting it
            {
                MANGOS_ASSERT(playerBind->state == GetPersistentState());
            }
        }
    }

    // for normal instances cancel the reset schedule when the
    // first player enters (no players yet)
    SetResetSchedule(false);

    DETAIL_LOG("MAP: Player '%s' is entering instance '%u' of map '%s'", player->GetName(), GetInstanceId(), GetMapName());
    // initialize unload state
    m_unloadTimer = 0;
    m_resetAfterUnload = false;
    m_unloadWhenEmpty = false;

    // this will acquire the same mutex so it can not be in the previous block
    Map::Add(player);

    return true;
}

void MapInstanced::Update(const uint32& t_diff)
{
    Map::Update(t_diff);
}

void MapInstanced::Remove(Player* player, bool remove)
{
    DETAIL_LOG("MAP: Removing player '%s' from instance '%u' of map '%s' before relocating to other map", player->GetName(), GetInstanceId(), GetMapName());

    // if last player set unload timer
    if (!m_unloadTimer && m_mapRefManager.getSize() == 1)
    {
        m_unloadTimer = m_unloadWhenEmpty ? MIN_UNLOAD_DELAY : std::max(sWorld.getConfig(CONFIG_UINT32_INSTANCE_UNLOAD_DELAY), uint32(MIN_UNLOAD_DELAY));
    }

    Map::Remove(player, remove);

    // for normal instances schedule the reset after all players have left
    SetResetSchedule(true);
}

/*
    Returns true if there are no players in the instance
*/
bool MapInstanced::Reset(InstanceResetMethod method)
{
    // note: since the map may not be loaded when the instance needs to be reset
    // the instance must be deleted from the DB by InstanceSaveManager

    if (HavePlayers())
    {
        if (method == INSTANCE_RESET_ALL)
        {
            // notify the players to leave the instance so it can be reset
            for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
            {
                itr->getSource()->SendResetFailedNotify(GetId());
            }
        }
        else
        {
            if (method == INSTANCE_RESET_GLOBAL)
            {
                // set the homebind timer for players inside (1 minute)
                for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
                {
                    itr->getSource()->m_InstanceValid = false;
                }
            }

            // the unload timer is not started
            // instead the map will unload immediately after the players have left
            m_unloadWhenEmpty = true;
            m_resetAfterUnload = true;
        }
    }
    else
    {
        // unloaded at next update
        m_unloadTimer = MIN_UNLOAD_DELAY;
        m_resetAfterUnload = true;
    }

    return m_mapRefManager.isEmpty();
}

void MapInstanced::PermBindAllPlayers(Player* player)
{
    Group* group = player->GetGroup();
    // group members outside the instance group don't get bound
    for (MapRefManager::iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
    {
        Player* plr = itr->getSource();
        // players inside an instance can not be bound to other instances
        // some players may already be permanently bound, in this case nothing happens
        InstancePlayerBind* bind = plr->GetBoundInstance(GetId());
        if (!bind || !bind->perm)
        {
            plr->BindToInstance(GetPersistanceState(), true);
            WorldPacket data(SMSG_INSTANCE_SAVE_CREATED, 4);
            data << uint32(0);
            plr->GetSession()->SendPacket(&data);
        }

        // if the leader is not in the instance the group will not get a perm bind
        if (group && group->GetLeaderGuid() == plr->GetObjectGuid())
        {
            group->BindToInstance(GetPersistanceState(), true);
        }
    }
}

void MapInstanced::UnloadAll(bool pForce)
{
    TeleportAllPlayersTo(TELEPORT_LOCATION_HOMEBIND);

    if (m_resetAfterUnload == true)
    {
        GetPersistanceState()->DeleteRespawnTimes();
    }

    Map::UnloadAll(pForce);
}

void MapInstanced::SendResetWarnings(uint32 timeLeft) const
{
    for (MapRefManager::const_iterator itr = m_mapRefManager.begin(); itr != m_mapRefManager.end(); ++itr)
    {
        itr->getSource()->SendInstanceResetWarning(GetId(), timeLeft);
    }
}

void MapInstanced::SetResetSchedule(bool on)
{
    // only for normal instances
    // the reset time is only scheduled when there are no payers inside
    // it is assumed that the reset time will rarely (if ever) change while the reset is scheduled
    if (!HavePlayers() && !IsRaid())
    {
        sMapPersistentStateMgr.GetScheduler().ScheduleReset(on, GetPersistanceState()->GetResetTime(), DungeonResetEvent(RESET_EVENT_NORMAL_DUNGEON, GetId(), GetInstanceId()));
    }
}

uint32 MapInstanced::GetMaxPlayers() const
{
    InstanceTemplate const* iTemplate = ObjectMgr::GetInstanceTemplate(GetId());
    if (!iTemplate)
    {
        return 0;
    }

    return iTemplate->maxPlayers;
}

DungeonPersistentState* MapInstanced::GetPersistanceState() const
{
    return (DungeonPersistentState*)Map::GetPersistentState();
}
