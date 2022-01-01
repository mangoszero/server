/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Chat.h"
#include "Language.h"
#include "World.h"
#include "MapManager.h"
#include "CellImpl.h"


#ifdef _DEBUG_VMAPS
#include "VMapFactory.h"
#endif
 /*
     All commands related to Teleportation
 */

/*
    Utilities methods an enums
*/

enum CreatureLinkType
{
    CREATURE_LINK_RAW = -1,                   // non-link case
    CREATURE_LINK_GUID = 0,
    CREATURE_LINK_ENTRY = 1,
};

static char const* const creatureKeys[] =
{
    "Hcreature",
    "Hcreature_entry",
    NULL
};

enum GameobjectLinkType
{
    GAMEOBJECT_LINK_RAW = -1,                   // non-link case
    GAMEOBJECT_LINK_GUID = 0,
    GAMEOBJECT_LINK_ENTRY = 1,
};

static char const* const gameobjectKeys[] =
{
    "Hgameobject",
    "Hgameobject_entry",
    NULL
};

static char const* const areatriggerKeys[] =
{
    "Hareatrigger",
    "Hareatrigger_target",
    NULL
};

bool ChatHandler::HandleGoHelper(Player* player, uint32 mapid, float x, float y, float const zPtr, float const ortPtr)
{
    float z;
    float ort = player->GetOrientation();
    z = zPtr;
    if (zPtr > 0.0f)
    {
        z = zPtr;

        if (ortPtr > 0.0f)
        {
            ort = ortPtr;
        }

        // check full provided coordinates
        if (!MapManager::IsValidMapCoord(mapid, x, y, z, ort))
        {
            PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapid);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        // we need check x,y before ask Z or can crash at invalide coordinates
        if (!MapManager::IsValidMapCoord(mapid, x, y))
        {
            PSendSysMessage(LANG_INVALID_TARGET_COORD, x, y, mapid);
            SetSentErrorMessage(true);
            return false;
        }

        // COmmented since it can be a problem when exploring zones !
        //TerrainInfo const* map = sTerrainMgr.LoadTerrain(mapid);
        //z = map->GetWaterOrGroundLevel(x, y, MAX_HEIGHT);
    }

    // stop flight if need
    if (player->IsTaxiFlying())
    {
        player->GetMotionMaster()->MovementExpired();
        player->m_taxi.ClearTaxiDestinations();
    }
    // save only in non-flight case
    else
    {
        player->SaveRecallPosition();
    }

    player->TeleportTo(mapid, x, y, z, ort);

    return true;
}

 /**********************************************************************
     CommandTable : commandTable
 /***********************************************************************/

 // Summon Player
bool ChatHandler::HandleSummonCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    Player* player = m_session->GetPlayer();
    if (target == player || target_guid == player->GetObjectGuid())
    {
        PSendSysMessage(LANG_CANT_TELEPORT_SELF);
        SetSentErrorMessage(true);
        return false;
    }

    if (target)
    {
        std::string nameLink = playerLink(target_name);
        // check online security
        if (HasLowerSecurity(target))
        {
            return false;
        }

        if (target->IsBeingTeleported())
        {
            PSendSysMessage(LANG_IS_TELEPORTED, nameLink.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        Map* pMap = player->GetMap();

        if (pMap->IsBattleGround())
        {
            // only allow if gm mode is on
            if (!target->isGameMaster())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_GM, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
            // if both players are in different bgs
            else if (target->GetBattleGroundId() && player->GetBattleGroundId() != target->GetBattleGroundId())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_FROM_BG, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
            // all's well, set bg id
            // when porting out from the bg, it will be reset to 0
            target->SetBattleGroundId(player->GetBattleGroundId(), player->GetBattleGroundTypeId());
            // remember current position as entry point for return at bg end teleportation
            if (!target->GetMap()->IsBattleGround())
            {
                target->SetBattleGroundEntryPoint();
            }
        }
        else if (pMap->IsDungeon())
        {
            Map* cMap = target->GetMap();
            if (cMap->Instanceable() && cMap->GetInstanceId() != pMap->GetInstanceId())
            {
                // can not summon from instance to instance
                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }

            // we are in instance, and can summon only player in our group with us as lead
            if (!player->GetGroup() || !target->GetGroup() ||
                    (target->GetGroup()->GetLeaderGuid() != player->GetObjectGuid()) ||
                    (player->GetGroup()->GetLeaderGuid() != player->GetObjectGuid()))
                // the last check is a bit excessive, but let it be, just in case
            {
                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, nameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
        }

        PSendSysMessage(LANG_SUMMONING, nameLink.c_str(), "");
        if (needReportToTarget(target))
        {
            ChatHandler(target).PSendSysMessage(LANG_SUMMONED_BY, playerLink(player->GetName()).c_str());
        }

        // stop flight if need
        if (target->IsTaxiFlying())
        {
            target->GetMotionMaster()->MovementExpired();
            target->m_taxi.ClearTaxiDestinations();
        }
        // save only in non-flight case
        else
        {
            target->SaveRecallPosition();
        }

        // before GM
        float x, y, z;
        player->GetClosePoint(x, y, z, target->GetObjectBoundingRadius());
        target->TeleportTo(player->GetMapId(), x, y, z, target->GetOrientation());
    }
    else
    {
        // check offline security
        if (HasLowerSecurity(NULL, target_guid))
        {
            return false;
        }

        std::string nameLink = playerLink(target_name);

        PSendSysMessage(LANG_SUMMONING, nameLink.c_str(), GetMangosString(LANG_OFFLINE));

        // in point where GM stay
        Player::SavePositionInDB(target_guid, player->GetMapId(),
                                 player->GetPositionX(),
                                 player->GetPositionY(),
                                 player->GetPositionZ(),
                                 player->GetOrientation(),
                                 player->GetZoneId());
    }

    return true;
}

// Teleport to Player
bool ChatHandler::HandleAppearCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();
    if (target == _player || target_guid == _player->GetObjectGuid())
    {
        SendSysMessage(LANG_CANT_TELEPORT_SELF);
        SetSentErrorMessage(true);
        return false;
    }


    if (target)
    {
        // check online security
        if (HasLowerSecurity(target))
        {
            return false;
        }

        std::string chrNameLink = playerLink(target_name);

        Map* cMap = target->GetMap();
        if (cMap->IsBattleGround())
        {
            // only allow if gm mode is on
            if (!_player->isGameMaster())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_GM, chrNameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
            // if both players are in different bgs
            else if (_player->GetBattleGroundId() && _player->GetBattleGroundId() != target->GetBattleGroundId())
            {
                PSendSysMessage(LANG_CANNOT_GO_TO_BG_FROM_BG, chrNameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
            // all's well, set bg id
            // when porting out from the bg, it will be reset to 0
            _player->SetBattleGroundId(target->GetBattleGroundId(), target->GetBattleGroundTypeId());
            // remember current position as entry point for return at bg end teleportation
            if (!_player->GetMap()->IsBattleGround())
            {
                _player->SetBattleGroundEntryPoint();
            }
        }
        else if (cMap->IsDungeon())
        {
            // we have to go to instance, and can go to player only if:
            //   1) we are in his group (either as leader or as member)
            //   2) we are not bound to any group and have GM mode on
            if (_player->GetGroup())
            {
                // we are in group, we can go only if we are in the player group
                if (_player->GetGroup() != target->GetGroup())
                {
                    PSendSysMessage(LANG_CANNOT_GO_TO_INST_PARTY, chrNameLink.c_str());
                    SetSentErrorMessage(true);
                    return false;
                }
            }
            else
            {
                // we are not in group, let's verify our GM mode
                if (!_player->isGameMaster())
                {
                    PSendSysMessage(LANG_CANNOT_GO_TO_INST_GM, chrNameLink.c_str());
                    SetSentErrorMessage(true);
                    return false;
                }
            }

            // if the player or the player's group is bound to another instance
            // the player will not be bound to another one
            InstancePlayerBind* pBind = _player->GetBoundInstance(target->GetMapId());
            if (!pBind)
            {
                Group* group = _player->GetGroup();
                // if no bind exists, create a solo bind
                InstanceGroupBind* gBind = group ? group->GetBoundInstance(target->GetMapId()) : NULL;
                // if no bind exists, create a solo bind
                if (!gBind)
                {
                    DungeonPersistentState* save = ((DungeonMap*)target->GetMap())->GetPersistanceState();

                    // if player is group leader then we need add group bind
                    if (group && group->IsLeader(_player->GetObjectGuid()))
                    {
                        group->BindToInstance(save, !save->CanReset());
                    }
                    else
                    {
                        _player->BindToInstance(save, !save->CanReset());
                    }
                }
            }
        }

        PSendSysMessage(LANG_APPEARING_AT, chrNameLink.c_str());
        if (needReportToTarget(target))
        {
            ChatHandler(target).PSendSysMessage(LANG_APPEARING_TO, GetNameLink().c_str());
        }

        // stop flight if need
        if (_player->IsTaxiFlying())
        {
            _player->GetMotionMaster()->MovementExpired();
            _player->m_taxi.ClearTaxiDestinations();
        }
        // save only in non-flight case
        else
        {
            _player->SaveRecallPosition();
        }

        // to point to see at target with same orientation
        float x, y, z;
        target->GetContactPoint(_player, x, y, z);

        _player->TeleportTo(target->GetMapId(), x, y, z, _player->GetAngle(target), TELE_TO_GM_MODE);
    }
    else
    {
        // check offline security
        if (HasLowerSecurity(NULL, target_guid))
        {
            return false;
        }

        std::string nameLink = playerLink(target_name);

        PSendSysMessage(LANG_APPEARING_AT, nameLink.c_str());

        // to point where player stay (if loaded)
        float x, y, z, o;
        uint32 map;
        bool in_flight;
        if (!Player::LoadPositionFromDB(target_guid, map, x, y, z, o, in_flight))
        {
            return false;
        }

        return HandleGoHelper(_player, map, x, y, z);
    }

    return true;
}

// Summon group of player
bool ChatHandler::HandleGroupgoCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
    {
        return false;
    }

    Group* grp = target->GetGroup();

    std::string nameLink = GetNameLink(target);

    if (!grp)
    {
        PSendSysMessage(LANG_NOT_IN_GROUP, nameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    Player* player = m_session->GetPlayer();
    Map* gmMap = player->GetMap();
    bool to_instance = gmMap->Instanceable();

    // we are in instance, and can summon only player in our group with us as lead
    if (to_instance && (
                !player->GetGroup() || (grp->GetLeaderGuid() != player->GetObjectGuid()) ||
                (player->GetGroup()->GetLeaderGuid() != player->GetObjectGuid())))
        // the last check is a bit excessive, but let it be, just in case
    {
        SendSysMessage(LANG_CANNOT_SUMMON_TO_INST);
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pl = itr->getSource();

        if (!pl || pl == m_session->GetPlayer() || !pl->GetSession())
        {
            continue;
        }

        // check online security
        if (HasLowerSecurity(pl))
        {
            return false;
        }

        std::string plNameLink = GetNameLink(pl);

        if (pl->IsBeingTeleported() == true)
        {
            PSendSysMessage(LANG_IS_TELEPORTED, plNameLink.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        if (to_instance)
        {
            Map* plMap = pl->GetMap();

            if (plMap->Instanceable() && plMap->GetInstanceId() != gmMap->GetInstanceId())
            {
                // can not summon from instance to instance
                PSendSysMessage(LANG_CANNOT_SUMMON_TO_INST, plNameLink.c_str());
                SetSentErrorMessage(true);
                return false;
            }
        }

        PSendSysMessage(LANG_SUMMONING, plNameLink.c_str(), "");
        if (needReportToTarget(pl))
        {
            ChatHandler(pl).PSendSysMessage(LANG_SUMMONED_BY, nameLink.c_str());
        }

        // stop flight if need
        if (pl->IsTaxiFlying())
        {
            pl->GetMotionMaster()->MovementExpired();
            pl->m_taxi.ClearTaxiDestinations();
        }
        // save only in non-flight case
        else
        {
            pl->SaveRecallPosition();
        }

        // before GM
        float x, y, z;
        m_session->GetPlayer()->GetClosePoint(x, y, z, pl->GetObjectBoundingRadius());
        pl->TeleportTo(m_session->GetPlayer()->GetMapId(), x, y, z, pl->GetOrientation());
    }

    return true;
}

// Teleport player to last position
bool ChatHandler::HandleRecallCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
    {
        return false;
    }

    if (target->IsBeingTeleported())
    {
        PSendSysMessage(LANG_IS_TELEPORTED, GetNameLink(target).c_str());
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(target, target->m_recallMap, target->m_recallX, target->m_recallY, target->m_recallZ, target->m_recallO);
}

bool ChatHandler::HandleGPSCommand(char* args)
{
    WorldObject* obj = NULL;
    if (*args)
    {
        if (ObjectGuid guid = ExtractGuidFromLink(&args))
        {
            obj = (WorldObject*)m_session->GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
        }

        if (!obj)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        obj = getSelectedUnit();

        if (!obj)
        {
            SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }
    }
    CellPair cell_val = MaNGOS::ComputeCellPair(obj->GetPositionX(), obj->GetPositionY());
    Cell cell(cell_val);

    uint32 zone_id, area_id;
    obj->GetZoneAndAreaId(zone_id, area_id);

    MapEntry const* mapEntry = sMapStore.LookupEntry(obj->GetMapId());
    AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zone_id);
    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(area_id);

    float zone_x = obj->GetPositionX();
    float zone_y = obj->GetPositionY();

    if (!Map2ZoneCoordinates(zone_x, zone_y, zone_id))
    {
        zone_x = 0;
        zone_y = 0;
    }

    Map const* map = obj->GetMap();
    float ground_z = map->GetHeight(obj->GetPositionX(), obj->GetPositionY(), MAX_HEIGHT);
    float floor_z = map->GetHeight(obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ());

    GridPair p = MaNGOS::ComputeGridPair(obj->GetPositionX(), obj->GetPositionY());

    int gx = 63 - p.x_coord;
    int gy = 63 - p.y_coord;

    uint32 have_map = GridMap::ExistMap(obj->GetMapId(), gx, gy) ? 1 : 0;
    uint32 have_vmap = GridMap::ExistVMap(obj->GetMapId(), gx, gy) ? 1 : 0;

    TerrainInfo const* terrain = obj->GetMap()->GetTerrain();

    if (have_vmap)
    {
        if (terrain->IsOutdoors(obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ()))
        {
            PSendSysMessage("You are OUTdoor");
        }
        else
        {
            PSendSysMessage("You are INdoor");
        }
    }
    else
    {
        PSendSysMessage("no VMAP available for area info");
    }

    PSendSysMessage(LANG_MAP_POSITION,
                    obj->GetMapId(), (mapEntry ? mapEntry->name[GetSessionDbcLocale()] : "<unknown>"),
                    zone_id, (zoneEntry ? zoneEntry->area_name[GetSessionDbcLocale()] : "<unknown>"),
                    area_id, (areaEntry ? areaEntry->area_name[GetSessionDbcLocale()] : "<unknown>"),
                    obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), obj->GetOrientation(),
                    cell.GridX(), cell.GridY(), cell.CellX(), cell.CellY(), obj->GetInstanceId(),
                    zone_x, zone_y, ground_z, floor_z, have_map, have_vmap);

    DEBUG_LOG("Player %s GPS call for %s '%s' (%s: %u):",
              m_session ? GetNameLink().c_str() : GetMangosString(LANG_CONSOLE_COMMAND),
              (obj->GetTypeId() == TYPEID_PLAYER ? "player" : "creature"), obj->GetName(),
              (obj->GetTypeId() == TYPEID_PLAYER ? "GUID" : "Entry"), (obj->GetTypeId() == TYPEID_PLAYER ? obj->GetGUIDLow() : obj->GetEntry()));

    DEBUG_LOG(GetMangosString(LANG_MAP_POSITION),
              obj->GetMapId(), (mapEntry ? mapEntry->name[sWorld.GetDefaultDbcLocale()] : "<unknown>"),
              zone_id, (zoneEntry ? zoneEntry->area_name[sWorld.GetDefaultDbcLocale()] : "<unknown>"),
              area_id, (areaEntry ? areaEntry->area_name[sWorld.GetDefaultDbcLocale()] : "<unknown>"),
              obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), obj->GetOrientation(),
              cell.GridX(), cell.GridY(), cell.CellX(), cell.CellY(), obj->GetInstanceId(),
              zone_x, zone_y, ground_z, floor_z, have_map, have_vmap);

    GridMapLiquidData liquid_status;
    GridMapLiquidStatus res = terrain->getLiquidStatus(obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), MAP_ALL_LIQUIDS, &liquid_status);
    if (res)
    {
        PSendSysMessage(LANG_LIQUID_STATUS, liquid_status.level, liquid_status.depth_level, liquid_status.type_flags, res);
    }

    // Additional vmap debugging help
#ifdef _DEBUG_VMAPS
    PSendSysMessage("Static terrain height (maps only): %f", obj->GetTerrain()->GetHeightStatic(obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ(), false));

    if (VMAP::IVMapManager* vmgr = VMAP::VMapFactory::createOrGetVMapManager())
    {
        PSendSysMessage("Vmap Terrain Height %f", vmgr->getHeight(obj->GetMapId(), obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ() + 2.0f, 10000.0f));
    }

    PSendSysMessage("Static map height (maps and vmaps): %f", obj->GetTerrain()->GetHeightStatic(obj->GetPositionX(), obj->GetPositionY(), obj->GetPositionZ()));
#endif

    return true;
}

bool ChatHandler::HandleGetDistanceCommand(char* args)
{
    WorldObject* obj = NULL;

    if (*args)
    {
        if (ObjectGuid guid = ExtractGuidFromLink(&args))
        {
            obj = (WorldObject*)m_session->GetPlayer()->GetObjectByTypeMask(guid, TYPEMASK_CREATURE_OR_GAMEOBJECT);
        }

        if (!obj)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        obj = getSelectedUnit();

        if (!obj)
        {
            SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
            SetSentErrorMessage(true);
            return false;
        }
    }

    Player* player = m_session->GetPlayer();
    // Calculate point-to-point distance
    float dx, dy, dz;
    dx = player->GetPositionX() - obj->GetPositionX();
    dy = player->GetPositionY() - obj->GetPositionY();
    dz = player->GetPositionZ() - obj->GetPositionZ();

    PSendSysMessage(LANG_DISTANCE, player->GetDistance(obj), player->GetDistance2d(obj), sqrt(dx * dx + dy * dy + dz * dz));

    return true;
}

bool ChatHandler::HandleNearGraveCommand(char* args)
{
    Team g_team;

    size_t argslen = strlen(args);

    if (!*args)
    {
        g_team = TEAM_BOTH_ALLOWED;
    }
    else if (strncmp(args, "horde", argslen) == 0)
    {
        g_team = HORDE;
    }
    else if (strncmp(args, "alliance", argslen) == 0)
    {
        g_team = ALLIANCE;
    }
    else
    {
        return false;
    }

    Player* player = m_session->GetPlayer();
    uint32 zone_id = player->GetZoneId();

    WorldSafeLocsEntry const* graveyard = sObjectMgr.GetClosestGraveYard(player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetMapId(), g_team);

    if (graveyard)
    {
        uint32 g_id = graveyard->ID;

        GraveYardData const* data = sObjectMgr.FindGraveYardData(g_id, zone_id);
        if (!data)
        {
            PSendSysMessage(LANG_COMMAND_GRAVEYARDERROR, g_id);
            SetSentErrorMessage(true);
            return false;
        }

        std::string team_name;

        if (data->team == TEAM_BOTH_ALLOWED)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ANY);
        }
        else if (data->team == HORDE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_HORDE);
        }
        else if (data->team == ALLIANCE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ALLIANCE);
        }
        else                                                // Actually, this case can not happen
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_NOTEAM);
        }

        PSendSysMessage(LANG_COMMAND_GRAVEYARDNEAREST, g_id, team_name.c_str(), zone_id);
    }
    else
    {
        std::string team_name;

        if (g_team == TEAM_BOTH_ALLOWED)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ANY);
        }
        else if (g_team == HORDE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_HORDE);
        }
        else if (g_team == ALLIANCE)
        {
            team_name = GetMangosString(LANG_COMMAND_GRAVEYARD_ALLIANCE);
        }

        if (g_team == TEAM_BOTH_ALLOWED)
        {
            PSendSysMessage(LANG_COMMAND_ZONENOGRAVEYARDS, zone_id);
        }
        else
        {
            PSendSysMessage(LANG_COMMAND_ZONENOGRAFACTION, zone_id, team_name.c_str());
        }
    }

    return true;
}

/**********************************************************************
    CommandTable : goCommandTable
/***********************************************************************/

bool ChatHandler::HandleGoTaxinodeCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    uint32 nodeId;
    if (!ExtractUint32KeyFromLink(&args, "Htaxinode", nodeId))
    {
        return false;
    }

    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(nodeId);
    if (!node)
    {
        PSendSysMessage(LANG_COMMAND_GOTAXINODENOTFOUND, nodeId);
        SetSentErrorMessage(true);
        return false;
    }

    if (node->x == 0.0f && node->y == 0.0f && node->z == 0.0f)
    {
        PSendSysMessage(LANG_INVALID_TARGET_COORD, node->x, node->y, node->map_id);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, node->map_id, node->x, node->y, node->z);
}

bool ChatHandler::HandleGoCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();

    uint32 mapid;
    float x, y, z;

    // raw coordinates case
    if (ExtractFloat(&args, x))
    {
        if (!ExtractFloat(&args, y))
        {
            return false;
        }

        if (!ExtractFloat(&args, z))
        {
            return false;
        }

        if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
        {
            return false;
        }
    }
    // link case
    else if (!ExtractLocationFromLink(&args, mapid, x, y, z))
    {
        return false;
    }

    return HandleGoHelper(_player, mapid, x, y, z);
}

// teleport at coordinates
bool ChatHandler::HandleGoXYCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float x;
    if (!ExtractFloat(&args, x))
    {
        return false;
    }

    float y;
    if (!ExtractFloat(&args, y))
    {
        return false;
    }

    uint32 mapid;
    if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
    {
        return false;
    }

    return HandleGoHelper(_player, mapid, x, y);
}

// teleport at coordinates, including Z
bool ChatHandler::HandleGoXYZCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float x;
    if (!ExtractFloat(&args, x))
    {
        return false;
    }

    float y;
    if (!ExtractFloat(&args, y))
    {
        return false;
    }

    float z;
    if (!ExtractFloat(&args, z))
    {
        return false;
    }

    uint32 mapid;
    if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
    {
        return false;
    }

    return HandleGoHelper(_player, mapid, x, y, z);
}

// teleport at coordinates
bool ChatHandler::HandleGoZoneXYCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float x;
    if (!ExtractFloat(&args, x))
    {
        return false;
    }

    float y;
    if (!ExtractFloat(&args, y))
    {
        return false;
    }

    uint32 areaid;
    if (*args)
    {
        if (!ExtractUint32KeyFromLink(&args, "Harea", areaid))
        {
            return false;
        }
    }
    else
    {
        areaid = _player->GetZoneId();
    }

    AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(areaid);

    if (x < 0 || x > 100 || y < 0 || y > 100 || !areaEntry)
    {
        PSendSysMessage(LANG_INVALID_ZONE_COORD, x, y, areaid);
        SetSentErrorMessage(true);
        return false;
    }

    // update to parent zone if exist (client map show only zones without parents)
    AreaTableEntry const* zoneEntry = areaEntry->zone ? GetAreaEntryByAreaID(areaEntry->zone) : areaEntry;

    MapEntry const* mapEntry = sMapStore.LookupEntry(zoneEntry->mapid);

    if (mapEntry->Instanceable())
    {
        PSendSysMessage(LANG_INVALID_ZONE_MAP, areaEntry->ID, areaEntry->area_name[GetSessionDbcLocale()],
                        mapEntry->MapID, mapEntry->name[GetSessionDbcLocale()]);
        SetSentErrorMessage(true);
        return false;
    }

    if (!Zone2MapCoordinates(x, y, zoneEntry->ID))
    {
        PSendSysMessage(LANG_INVALID_ZONE_MAP, areaEntry->ID, areaEntry->area_name[GetSessionDbcLocale()],
                        mapEntry->MapID, mapEntry->name[GetSessionDbcLocale()]);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, mapEntry->MapID, x, y);
}

// teleport to grid
bool ChatHandler::HandleGoGridCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    float grid_x;
    if (!ExtractFloat(&args, grid_x))
    {
        return false;
    }

    float grid_y;
    if (!ExtractFloat(&args, grid_y))
    {
        return false;
    }

    uint32 mapid;
    if (!ExtractOptUInt32(&args, mapid, _player->GetMapId()))
    {
        return false;
    }

    // center of grid
    float x = (grid_x - CENTER_GRID_ID + 0.5f) * SIZE_OF_GRIDS;
    float y = (grid_y - CENTER_GRID_ID + 0.5f) * SIZE_OF_GRIDS;

    return HandleGoHelper(_player, mapid, x, y);
}

/** \brief Teleport the GM to the specified creature
*
* .go creature <GUID>     --> TP using creature.guid
* .go creature azuregos   --> TP player to the mob with this name
*                             Warning: If there is more than one mob with this name
*                                      you will be teleported to the first one that is found.
* .go creature id 6109    --> TP player to the mob, that has this creature_template.entry
*                             Warning: If there is more than one mob with this "id"
*                                      you will be teleported to the first one that is found.
*/
// teleport to creature
bool ChatHandler::HandleGoCreatureCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();
    ObjectGuid targetMobGuid;

    // "id" or number or [name] Shift-click form |color|Hcreature:creature_id|h[name]|h|r
    int crType;
    char* pParam1 = ExtractKeyFromLink(&args, creatureKeys, &crType);
    if (!pParam1)
    {
        return false;
    }

    // User wants to teleport to the NPC's template entry
    if (crType == CREATURE_LINK_RAW && strcmp(pParam1, "id") == 0)
    {
        // number or [name] Shift-click form |color|Hcreature_entry:creature_id|h[name]|h|r
        pParam1 = ExtractKeyFromLink(&args, "Hcreature_entry");
        if (!pParam1)
        {
            return false;
        }

        crType = CREATURE_LINK_ENTRY;
    }

    CreatureData const* data = NULL;

    switch (crType)
    {
        case CREATURE_LINK_ENTRY:
        {
            uint32 tEntry;
            if (!ExtractUInt32(&pParam1, tEntry))
            {
                return false;
            }

            if (!tEntry)
            {
                return false;
            }

            if (!ObjectMgr::GetCreatureTemplate(tEntry))
            {
                SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            FindCreatureData worker(tEntry, m_session ? m_session->GetPlayer() : NULL);

            sObjectMgr.DoCreatureData(worker);

            CreatureDataPair const* dataPair = worker.GetResult();
            if (!dataPair)
            {
                SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            data = &dataPair->second;
            targetMobGuid = data->GetObjectGuid(dataPair->first);
            break;
        }
        case CREATURE_LINK_GUID:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&pParam1, lowguid))
            {
                return false;
            }

            data = sObjectMgr.GetCreatureData(lowguid);

            if (!data)
            {
                SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            targetMobGuid = data->GetObjectGuid(lowguid);
            break;
        }
        case CREATURE_LINK_RAW:
        {
            uint32 lowguid;
            if (ExtractUInt32(&pParam1, lowguid))
            {
                data = sObjectMgr.GetCreatureData(lowguid);

                if (!data)
                {
                    SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                targetMobGuid = data->GetObjectGuid(lowguid);
            }
            // Number is invalid - maybe the user specified the mob's name
            else
            {
                std::string name = pParam1;
                WorldDatabase.escape_string(name);
                QueryResult* result = WorldDatabase.PQuery("SELECT `guid` FROM `creature`, `creature_template` WHERE `creature`.`id` = `creature_template`.`entry` AND `creature_template`.`name` " _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name.c_str());
                if (!result)
                {
                    SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                FindCreatureData worker(0, m_session ? m_session->GetPlayer() : NULL);

                do
                {
                    Field* fields = result->Fetch();
                    uint32 guid = fields[0].GetUInt32();

                    CreatureDataPair const* cr_data = sObjectMgr.GetCreatureDataPair(guid);
                    if (!cr_data)
                    {
                        continue;
                    }

                    worker(*cr_data);
                }
                while (result->NextRow());

                delete result;

                CreatureDataPair const* dataPair = worker.GetResult();
                if (!dataPair)
                {
                    SendSysMessage(LANG_COMMAND_GOCREATNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                data = &dataPair->second;
                targetMobGuid = data->GetObjectGuid(dataPair->first);
            }
            break;
        }
    }

    // If we are on the same map then we can teleport to the creature
    Creature* targetMob = _player->GetMap()->GetAnyTypeCreature(targetMobGuid);
    if (targetMob)
    {
        HandleGoHelper(_player, targetMob->GetMapId(), targetMob->GetPositionX(), targetMob->GetPositionY(), targetMob->GetPositionZ(), _player->GetOrientation());
    }
    else
    {
        // Go to creature initial pos to be on teh right Map
        HandleGoHelper(_player, data->mapid, data->posX, data->posY, data->posZ);

        // Inform player that he will need to make the command another time to go directly to the NPC
        PSendSysMessage(LANG_COMMAND_EXECUTE_GOCRE_ANOTHER_TIME, targetMobGuid.GetCounter());
    }

    return true;
}

// teleport to gameobject
bool ChatHandler::HandleGoObjectCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    // number or [name] Shift-click form |color|Hgameobject:go_guid|h[name]|h|r
    int goType;
    char* pParam1 = ExtractKeyFromLink(&args, gameobjectKeys, &goType);
    if (!pParam1)
    {
        return false;
    }

    // User wants to teleport to the GO's template entry
    if (goType == GAMEOBJECT_LINK_RAW && strcmp(pParam1, "id") == 0)
    {
        // number or [name] Shift-click form |color|Hgameobject_entry:creature_id|h[name]|h|r
        pParam1 = ExtractKeyFromLink(&args, "Hgameobject_entry");
        if (!pParam1)
        {
            return false;
        }

        goType = GAMEOBJECT_LINK_ENTRY;
    }

    GameObjectData const* data = NULL;

    switch (goType)
    {
        case CREATURE_LINK_ENTRY:
        {
            uint32 tEntry;
            if (!ExtractUInt32(&pParam1, tEntry))
            {
                return false;
            }

            if (!tEntry)
            {
                return false;
            }

            if (!ObjectMgr::GetGameObjectInfo(tEntry))
            {
                SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            FindGOData worker(tEntry, m_session ? m_session->GetPlayer() : NULL);

            sObjectMgr.DoGOData(worker);

            GameObjectDataPair const* dataPair = worker.GetResult();

            if (!dataPair)
            {
                SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }

            data = &dataPair->second;
            break;
        }
        case GAMEOBJECT_LINK_GUID:
        {
            uint32 lowguid;
            if (!ExtractUInt32(&pParam1, lowguid))
            {
                return false;
            }

            // by DB guid
            data = sObjectMgr.GetGOData(lowguid);
            if (!data)
            {
                SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                SetSentErrorMessage(true);
                return false;
            }
            break;
        }
        case GAMEOBJECT_LINK_RAW:
        {
            uint32 lowguid;
            if (ExtractUInt32(&pParam1, lowguid))
            {
                // by DB guid
                data = sObjectMgr.GetGOData(lowguid);
                if (!data)
                {
                    SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }
            }
            else
            {
                std::string name = pParam1;
                WorldDatabase.escape_string(name);
                QueryResult* result = WorldDatabase.PQuery("SELECT `guid` FROM `gameobject`, `gameobject_template` WHERE `gameobject`.`id` = `gameobject_template`.`entry` AND `gameobject_template`.`name `" _LIKE_ " " _CONCAT3_("'%%'", "'%s'", "'%%'"), name.c_str());
                if (!result)
                {
                    SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                FindGOData worker(0, m_session ? m_session->GetPlayer() : NULL);

                do
                {
                    Field* fields = result->Fetch();
                    uint32 guid = fields[0].GetUInt32();

                    GameObjectDataPair const* go_data = sObjectMgr.GetGODataPair(guid);
                    if (!go_data)
                    {
                        continue;
                    }

                    worker(*go_data);
                }
                while (result->NextRow());

                delete result;

                GameObjectDataPair const* dataPair = worker.GetResult();
                if (!dataPair)
                {
                    SendSysMessage(LANG_COMMAND_GOOBJNOTFOUND);
                    SetSentErrorMessage(true);
                    return false;
                }

                data = &dataPair->second;
            }
            break;
        }
    }

    return HandleGoHelper(_player, data->mapid, data->posX, data->posY, data->posZ);
}

bool ChatHandler::HandleGoGraveyardCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    uint32 gyId;
    if (!ExtractUInt32(&args, gyId))
    {
        return false;
    }

    WorldSafeLocsEntry const* gy = sWorldSafeLocsStore.LookupEntry(gyId);
    if (!gy)
    {
        PSendSysMessage(LANG_COMMAND_GRAVEYARDNOEXIST, gyId);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, gy->map_id, gy->x, gy->y, gy->z);
}

bool ChatHandler::HandleGoTriggerCommand(char* args)
{
    Player* _player = m_session->GetPlayer();

    if (!*args)
    {
        return false;
    }

    char* atIdStr = ExtractKeyFromLink(&args, areatriggerKeys);
    if (!atIdStr)
    {
        return false;
    }

    uint32 atId;
    if (!ExtractUInt32(&atIdStr, atId))
    {
        return false;
    }

    if (!atId)
    {
        return false;
    }

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(atId);
    if (!atEntry)
    {
        PSendSysMessage(LANG_COMMAND_GOAREATRNOTFOUND, atId);
        SetSentErrorMessage(true);
        return false;
    }

    bool to_target = ExtractLiteralArg(&args, "target");
    if (!to_target && *args)                                // can be fail also at syntax error
    {
        return false;
    }

    if (to_target)
    {
        AreaTrigger const* at = sObjectMgr.GetAreaTrigger(atId);
        if (!at)
        {
            PSendSysMessage(LANG_AREATRIGER_NOT_HAS_TARGET, atId);
            SetSentErrorMessage(true);
            return false;
        }

        return HandleGoHelper(_player, at->target_mapId, at->target_X, at->target_Y, at->target_Z);
    }
    else
    {
        return HandleGoHelper(_player, atEntry->mapid, atEntry->x, atEntry->y, atEntry->z);
    }
}

bool ChatHandler::HandleTeleDelCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string name = args;

    if (!sObjectMgr.DeleteGameTele(name))
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    SendSysMessage(LANG_COMMAND_TP_DELETED);
    return true;
}

bool ChatHandler::HandleTeleAddCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* player = m_session->GetPlayer();
    if (!player)
    {
        return false;
    }

    std::string name = args;

    if (sObjectMgr.GetGameTele(name))
    {
        SendSysMessage(LANG_COMMAND_TP_ALREADYEXIST);
        SetSentErrorMessage(true);
        return false;
    }

    GameTele tele;
    tele.position_x = player->GetPositionX();
    tele.position_y = player->GetPositionY();
    tele.position_z = player->GetPositionZ();
    tele.orientation = player->GetOrientation();
    tele.mapId = player->GetMapId();
    tele.name = name;

    if (sObjectMgr.AddGameTele(tele))
    {
        SendSysMessage(LANG_COMMAND_TP_ADDED);
    }
    else
    {
        SendSysMessage(LANG_COMMAND_TP_ADDEDERR);
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

// teleport player to given game_tele.entry
bool ChatHandler::HandleTeleNameCommand(char* args)
{
    char* nameStr = ExtractOptNotLastArg(&args);

    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
    {
        return false;
    }

    // id, or string, or [name] Shift-click form |color|Htele:id|h[name]|h|r
    GameTele const* tele = ExtractGameTeleFromLink(&args);
    if (!tele)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    if (target)
    {
        // check online security
        if (HasLowerSecurity(target))
        {
            return false;
        }

        std::string chrNameLink = playerLink(target_name);

        if (target->IsBeingTeleported() == true)
        {
            PSendSysMessage(LANG_IS_TELEPORTED, chrNameLink.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        PSendSysMessage(LANG_TELEPORTING_TO, chrNameLink.c_str(), "", tele->name.c_str());
        if (needReportToTarget(target))
        {
            ChatHandler(target).PSendSysMessage(LANG_TELEPORTED_TO_BY, GetNameLink().c_str());
        }

        return HandleGoHelper(target, tele->mapId, tele->position_x, tele->position_y, tele->position_z, tele->orientation);
    }
    else
    {
        // check offline security
        if (HasLowerSecurity(NULL, target_guid))
        {
            return false;
        }

        std::string nameLink = playerLink(target_name);

        PSendSysMessage(LANG_TELEPORTING_TO, nameLink.c_str(), GetMangosString(LANG_OFFLINE), tele->name.c_str());
        Player::SavePositionInDB(target_guid, tele->mapId,
                                 tele->position_x, tele->position_y, tele->position_z, tele->orientation,
                                 sTerrainMgr.GetZoneId(tele->mapId, tele->position_x, tele->position_y, tele->position_z));
    }

    return true;
}


bool ChatHandler::HandleTeleCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* _player = m_session->GetPlayer();

    // id, or string, or [name] Shift-click form |color|Htele:id|h[name]|h|r
    GameTele const* tele = ExtractGameTeleFromLink(&args);

    if (!tele)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return HandleGoHelper(_player, tele->mapId, tele->position_x, tele->position_y, tele->position_z, tele->orientation);
}

// Teleport group to given game_tele.entry
bool ChatHandler::HandleTeleGroupCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(player))
    {
        return false;
    }

    // id, or string, or [name] Shift-click form |color|Htele:id|h[name]|h|r
    GameTele const* tele = ExtractGameTeleFromLink(&args);
    if (!tele)
    {
        SendSysMessage(LANG_COMMAND_TELE_NOTFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    std::string nameLink = GetNameLink(player);

    Group* grp = player->GetGroup();
    if (!grp)
    {
        PSendSysMessage(LANG_NOT_IN_GROUP, nameLink.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    for (GroupReference* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        Player* pl = itr->getSource();

        if (!pl || !pl->GetSession())
        {
            continue;
        }

        // check online security
        if (HasLowerSecurity(pl))
        {
            return false;
        }

        std::string plNameLink = GetNameLink(pl);

        if (pl->IsBeingTeleported())
        {
            PSendSysMessage(LANG_IS_TELEPORTED, plNameLink.c_str());
            continue;
        }

        PSendSysMessage(LANG_TELEPORTING_TO, plNameLink.c_str(), "", tele->name.c_str());
        if (needReportToTarget(pl))
        {
            ChatHandler(pl).PSendSysMessage(LANG_TELEPORTED_TO_BY, nameLink.c_str());
        }

        // stop flight if need
        if (pl->IsTaxiFlying())
        {
            pl->GetMotionMaster()->MovementExpired();
            pl->m_taxi.ClearTaxiDestinations();
        }
        // save only in non-flight case
        else
        {
            pl->SaveRecallPosition();
        }

        pl->TeleportTo(tele->mapId, tele->position_x, tele->position_y, tele->position_z, tele->orientation);
    }

    return true;
}
