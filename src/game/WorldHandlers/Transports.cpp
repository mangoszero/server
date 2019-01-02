/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "Common.h"

#include "Transports.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Path.h"

#include "WorldPacket.h"
#include "DBCStores.h"
#include "ProgressBar.h"

#include <G3D/Quat.h>


void Map::LoadLocalTransports()
{
    //load local transports for this map
    std::pair<LocalTransportGuidsOnMap::const_iterator, LocalTransportGuidsOnMap::const_iterator> bounds = sObjectMgr.GetLocalTransportGuids()->equal_range(GetId());
    for (LocalTransportGuidsOnMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        LocalTransport* lt = new LocalTransport();
        if (lt->Initialize(itr->second, this))
        {
            i_transports.insert(lt);
        }
        else
        {
            delete lt;
        }
    }
/*
    sLog.outString();
    for (std::set<Transport*>::const_iterator i = i_transports.begin(); i != i_transports.end(); ++i)
        sLog.outString(">>>%s initialized", (*i)->GetGuidStr().c_str());
*/
    sLog.outString(">> Loaded " SIZEFMTD " local transports for map %u", i_transports.size(), GetId());

}

void MapManager::LoadTransports()
{
    QueryResult* result = WorldDatabase.Query("SELECT entry, name, period FROM transports");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 global transports");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 entry     = fields[0].GetUInt32();
        std::string name = fields[1].GetCppString();
        uint32 period    = fields[2].GetUInt32();

        GlobalTransport* t = new GlobalTransport();
        if (!t->Initialize(entry, period, name))
        {
            delete t;
            continue;
        }

        for (std::set<uint32>::const_iterator i = t->GetMapsUsed()->begin(); i != t->GetMapsUsed()->end(); ++i)
            { m_TransportsByMap[*i].insert(t); }

        m_Transports.insert(t);

        ++count;
    }
    while (result->NextRow());

    delete result;

    // check transport data DB integrity
    result = WorldDatabase.Query("SELECT gameobject.guid,gameobject.id,transports.name FROM gameobject,transports WHERE gameobject.id = transports.entry");
    if (result)                                             // wrong data found
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 guid  = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();
            std::string name = fields[2].GetCppString();
            sLog.outErrorDb("Transport %u '%s' have record (GUID: %u) in `gameobject`. Transports DON'T must have any records in `gameobject` or its behavior will be unpredictable/bugged.", entry, name.c_str(), guid);
        }
        while (result->NextRow());

        delete result;
    }

    sLog.outString();
    for (std::set<Transport*>::const_iterator i = m_Transports.begin(); i != m_Transports.end(); ++i)
        sLog.outString(">>>Global transporter %s (id = %u) initialized", (*i)->GetName(), (*i)->GetEntry());

    sLog.outString(">> Loaded %u global transports", count);
}


//*****************************//
// Base Transport
//*****************************//
Transport::Transport() : GameObject()
{
    m_updateFlag = (UPDATEFLAG_TRANSPORT | UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);
}

Transport::~Transport()
{
}

bool Transport::AddPassenger(Unit* passenger)
{
    if (m_passengers.find(passenger) == m_passengers.end())
    {
        DETAIL_LOG("%s boarded transport %s.", passenger->GetGuidStr().c_str(), GetName());
        m_passengers.insert(passenger);
    }
    return true;
}

bool Transport::RemovePassenger(Unit* passenger)
{
    if (m_passengers.erase(passenger))
        { DETAIL_LOG("%s removed from transport %s.", passenger->GetGuidStr().c_str(), GetName()); }
    return true;
}


//*****************************//
// LocalTransport
//*****************************//
LocalTransport::LocalTransport() : Transport()
{
}

LocalTransport::~LocalTransport()
{
    //sLog.outString("Deleting %s, map %u", GetGuidStr().c_str(), GetMap()->GetId());
}

bool LocalTransport::Initialize(uint32 guid, Map* m)
{
    if (m == NULL)
      { return false; }

    GameObjectData const* gdata = sObjectMgr.GetGOData(guid);
    if (!gdata)
    {
        sLog.outErrorDb("Local transport GUID %u does not exist into `gameobject` table", guid);
        return false;
    }

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(gdata->id);
    if (goinfo->type != GAMEOBJECT_TYPE_TRANSPORT)
    {
        sLog.outErrorDb("Local transport GUID:%u, Name:%s, Entry:%u  will not be loaded, `gameobject_template` type record is wrong", guid, goinfo->name, goinfo->id);
        return false;
    }

    Object::_Create(guid, goinfo->id, HIGHGUID_TRANSPORT);

    float r0, r1, r2, r3;
    r0 = gdata->rotation0;
    r1 = gdata->rotation1;
    r2 = gdata->rotation2;
    r3 = gdata->rotation3;

    if (r0 == 0.0f && r1 == 0.0f && r2 == 0.0f)      //TODO make a method for this
    {
        r2 = sin(gdata->orientation/2);
        r3 = cos(gdata->orientation/2);
    }

    G3D::Quat q(r0, r1, r2, r3);
    q.unitize();

    float o = GetOrientationFromQuat(q);
    SetMap(m);
    Relocate(gdata->posX, gdata->posY, gdata->posZ, o);

    if (!IsPositionValid())
    {
        sLog.outError("Local transport GUID:%u, Name:%s, Entry:%u not created. Suggested coordinates are not valid: (X, Y, Z)=(%f, %f, %f)",
                      guid, goinfo->name, goinfo->id, gdata->posX, gdata->posY, gdata->posZ);
        return false;
    }

    SetQuaternion(q);
    SetGOInfo(goinfo);
    SetObjectScale(goinfo->size);
    SetGoType(GAMEOBJECT_TYPE_TRANSPORT);
    SetEntry(goinfo->id);

    SetFloatValue(GAMEOBJECT_POS_X, gdata->posX);
    SetFloatValue(GAMEOBJECT_POS_Y, gdata->posY);
    SetFloatValue(GAMEOBJECT_POS_Z, gdata->posZ);
    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);
    SetUInt32Value(GAMEOBJECT_DISPLAYID, goinfo->displayId);

    SetGoState(GO_STATE_READY);
    SetGoAnimProgress(GO_ANIMPROGRESS_DEFAULT);
    SetName(goinfo->name);
    return true;
}

//*****************************//
// GlobalTransport
//*****************************//
GlobalTransport::GlobalTransport() : Transport()
{
}

GlobalTransport::~GlobalTransport()
{
}

bool GlobalTransport::Initialize(uint32 entry, uint32 period, std::string const& name)
{
    const GameObjectInfo* goinfo = ObjectMgr::GetGameObjectInfo(entry);
    if (!goinfo)
    {
        sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template missing", entry, name.c_str());
        return false;
    }

    if (goinfo->type != GAMEOBJECT_TYPE_MO_TRANSPORT)
    {
        sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template type wrong", entry, name.c_str());
        return false;
    }

    m_period = period;
    SetGOInfo(goinfo); //order is important. GenerateWaypoints needs m_goInfo access

    if (!GenerateWaypoints())
    {
        sLog.outErrorDb("Transport (path id %u) path size = 0. Transport ignored, check DBC files or transport GO data0 field.", goinfo->moTransport.taxiPathId);
        return false;
    }

    uint32 mapid = m_WayPoints[0].mapid;

    // no global transports in dungeons
    const MapEntry* pMapInfo = sMapStore.LookupEntry(mapid);
    if (!pMapInfo || pMapInfo->Instanceable())
    {
        return false;
    }
    Map* m = sMapMgr.CreateMap(mapid, this);
    if (m == NULL)
    {
        return false;
    }

    float x = m_WayPoints[0].x;
    float y = m_WayPoints[0].y;
    float z = m_WayPoints[0].z;
    float o = 0.0f;

    SetMap(m);
    Relocate(x, y, z, o);

    if (!IsPositionValid())
    {
        sLog.outError("Transport ID:%u not created. Suggested coordinates are not valid: (X, Y, Z)=(%f, %f, %f)", goinfo->id, x, y, z);
        return false;
    }

    Object::_Create(goinfo->id, 0, HIGHGUID_MO_TRANSPORT);

    SetObjectScale(goinfo->size);
    SetGoType(GAMEOBJECT_TYPE_MO_TRANSPORT);
    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);
    SetEntry(goinfo->id);
    SetUInt32Value(GAMEOBJECT_DISPLAYID, goinfo->displayId);
    SetGoState(GO_STATE_READY);
    SetGoAnimProgress(GO_ANIMPROGRESS_DEFAULT);
    SetName(goinfo->name);

    return true;
}

struct keyFrame
{
    explicit keyFrame(TaxiPathNodeEntry const& _node) : node(&_node),
        distSinceStop(-1.0f), distUntilStop(-1.0f), distFromPrev(-1.0f), tFrom(0.0f), tTo(0.0f)
    {
    }

    TaxiPathNodeEntry const* node;

    float distSinceStop;
    float distUntilStop;
    float distFromPrev;
    float tFrom, tTo;
};

bool GlobalTransport::GenerateWaypoints()
{
    uint32 pathid = GetGOInfo()->moTransport.taxiPathId;

    if (pathid >= sTaxiPathNodesByPath.size())
    {
        return false;
    }

    TaxiPathNodeList const& path = sTaxiPathNodesByPath[pathid];

    std::vector<keyFrame> keyFrames;
    int mapChange = 0;

    m_mapsUsed.clear();
    for (size_t i = 1; i < path.size() - 1; ++i)
    {
        if (mapChange == 0)
        {
            TaxiPathNodeEntry const& node_i = path[i];
            if (node_i.mapid == path[i + 1].mapid)
            {
                keyFrame k(node_i);
                keyFrames.push_back(k);
                m_mapsUsed.insert(k.node->mapid);
            }
            else
            {
                mapChange = 1;
            }
        }
        else
        {
            --mapChange;
        }
    }

    int lastStop = -1;
    int firstStop = -1;

    // first cell is arrived at by teleportation :S
    keyFrames[0].distFromPrev = 0;
    if (keyFrames[0].node->actionFlag == 2)
    {
        lastStop = 0;
    }

    // find the rest of the distances between key points
    for (size_t i = 1; i < keyFrames.size(); ++i)
    {
        if ((keyFrames[i].node->actionFlag == 1) || (keyFrames[i].node->mapid != keyFrames[i - 1].node->mapid))
        {
            keyFrames[i].distFromPrev = 0;
        }
        else
        {
            keyFrames[i].distFromPrev =
                sqrt(pow(keyFrames[i].node->x - keyFrames[i - 1].node->x, 2) +
                     pow(keyFrames[i].node->y - keyFrames[i - 1].node->y, 2) +
                     pow(keyFrames[i].node->z - keyFrames[i - 1].node->z, 2));
        }
        if (keyFrames[i].node->actionFlag == 2)
        {
            // remember first stop frame
            if (firstStop == -1)
                { firstStop = i; }
            lastStop = i;
        }
    }

    float tmpDist = 0;
    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        int j = (i + lastStop) % keyFrames.size();
        if (keyFrames[j].node->actionFlag == 2)
            { tmpDist = 0; }
        else
            { tmpDist += keyFrames[j].distFromPrev; }
        keyFrames[j].distSinceStop = tmpDist;
    }

    for (int i = int(keyFrames.size()) - 1; i >= 0; --i)
    {
        int j = (i + (firstStop + 1)) % keyFrames.size();
        tmpDist += keyFrames[(j + 1) % keyFrames.size()].distFromPrev;
        keyFrames[j].distUntilStop = tmpDist;
        if (keyFrames[j].node->actionFlag == 2)
            { tmpDist = 0; }
    }

    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        if (keyFrames[i].distSinceStop < (30 * 30 * 0.5f))
            { keyFrames[i].tFrom = sqrt(2 * keyFrames[i].distSinceStop); }
        else
            { keyFrames[i].tFrom = ((keyFrames[i].distSinceStop - (30 * 30 * 0.5f)) / 30) + 30; }

        if (keyFrames[i].distUntilStop < (30 * 30 * 0.5f))
            { keyFrames[i].tTo = sqrt(2 * keyFrames[i].distUntilStop); }
        else
            { keyFrames[i].tTo = ((keyFrames[i].distUntilStop - (30 * 30 * 0.5f)) / 30) + 30; }

        keyFrames[i].tFrom *= 1000;
        keyFrames[i].tTo *= 1000;
    }

    //    for (int i = 0; i < keyFrames.size(); ++i) {
    //        sLog.outString("%f, %f, %f, %f, %f, %f, %f", keyFrames[i].x, keyFrames[i].y, keyFrames[i].distUntilStop, keyFrames[i].distSinceStop, keyFrames[i].distFromPrev, keyFrames[i].tFrom, keyFrames[i].tTo);
    //    }

    // Now we're completely set up; we can move along the length of each waypoint at 100 ms intervals
    // speed = max(30, t) (remember x = 0.5s^2, and when accelerating, a = 1 unit/s^2
    int t = 0;
    bool teleport = false;
    if (keyFrames[keyFrames.size() - 1].node->mapid != keyFrames[0].node->mapid)
        { teleport = true; }

    WayPoint pos(keyFrames[0].node->mapid, keyFrames[0].node->x, keyFrames[0].node->y, keyFrames[0].node->z, teleport);
    m_WayPoints[0] = pos;
    t += keyFrames[0].node->delay * 1000;

    uint32 cM = keyFrames[0].node->mapid;
    for (size_t i = 0; i < keyFrames.size() - 1; ++i)
    {
        float d = 0;
        float tFrom = keyFrames[i].tFrom;
        float tTo = keyFrames[i].tTo;

        // keep the generation of all these points; we use only a few now, but may need the others later
        if (((d < keyFrames[i + 1].distFromPrev) && (tTo > 0)))
        {
            while ((d < keyFrames[i + 1].distFromPrev) && (tTo > 0))
            {
                tFrom += 100;
                tTo -= 100;

                if (d > 0)
                {
                    float newX, newY, newZ;
                    newX = keyFrames[i].node->x + (keyFrames[i + 1].node->x - keyFrames[i].node->x) * d / keyFrames[i + 1].distFromPrev;
                    newY = keyFrames[i].node->y + (keyFrames[i + 1].node->y - keyFrames[i].node->y) * d / keyFrames[i + 1].distFromPrev;
                    newZ = keyFrames[i].node->z + (keyFrames[i + 1].node->z - keyFrames[i].node->z) * d / keyFrames[i + 1].distFromPrev;

                    teleport = false;
                    if (keyFrames[i].node->mapid != cM)
                    {
                        teleport = true;
                        cM = keyFrames[i].node->mapid;
                    }

                    //                    sLog.outString("T: %d, D: %f, x: %f, y: %f, z: %f", t, d, newX, newY, newZ);
                    pos = WayPoint(keyFrames[i].node->mapid, newX, newY, newZ, teleport);
                    if (teleport)
                        { m_WayPoints[t] = pos; }
                }

                if (tFrom < tTo)                            // caught in tFrom dock's "gravitational pull"
                {
                    if (tFrom <= 30000)
                    {
                        d = 0.5f * (tFrom / 1000) * (tFrom / 1000);
                    }
                    else
                    {
                        d = 0.5f * 30 * 30 + 30 * ((tFrom - 30000) / 1000);
                    }
                    d = d - keyFrames[i].distSinceStop;
                }
                else
                {
                    if (tTo <= 30000)
                    {
                        d = 0.5f * (tTo / 1000) * (tTo / 1000);
                    }
                    else
                    {
                        d = 0.5f * 30 * 30 + 30 * ((tTo - 30000) / 1000);
                    }
                    d = keyFrames[i].distUntilStop - d;
                }
                t += 100;
            }
            t -= 100;
        }

        if (keyFrames[i + 1].tFrom > keyFrames[i + 1].tTo)
            { t += 100 - ((long)keyFrames[i + 1].tTo % 100); }
        else
            { t += (long)keyFrames[i + 1].tTo % 100; }

        teleport = false;
        if ((keyFrames[i + 1].node->actionFlag == 1) || (keyFrames[i + 1].node->mapid != keyFrames[i].node->mapid))
        {
            teleport = true;
            cM = keyFrames[i + 1].node->mapid;
        }

        pos = WayPoint(keyFrames[i + 1].node->mapid, keyFrames[i + 1].node->x, keyFrames[i + 1].node->y, keyFrames[i + 1].node->z, teleport);

        //        sLog.outString("T: %d, x: %f, y: %f, z: %f, t:%d", t, pos.x, pos.y, pos.z, teleport);

        // if (teleport)
        m_WayPoints[t] = pos;

        t += keyFrames[i + 1].node->delay * 1000;
        //        sLog.outString("------");
    }

    uint32 timer = t;

    //    sLog.outDetail("    Generated %lu waypoints, total time %u.", (unsigned long)m_WayPoints.size(), timer);

    m_next = m_WayPoints.begin();                           // will used in MoveToNextWayPoint for init m_curr
    MoveToNextWayPoint();                                   // m_curr -> first point
    MoveToNextWayPoint();                                   // skip first point

    m_pathTime = timer;

    m_nextNodeTime = m_curr->first;

    return true;
}

void GlobalTransport::MoveToNextWayPoint()
{
    m_curr = m_next;

    ++m_next;
    if (m_next == m_WayPoints.end())
        { m_next = m_WayPoints.begin(); }
}

void GlobalTransport::TeleportTransport(uint32 newMapid, float x, float y, float z)
{
    Map const* oldMap = GetMap();
    Map* newMap = sMapMgr.CreateMap(newMapid, this);
    SetMap(newMap);
    Relocate(x, y, z);

    for (UnitSet::iterator itr = m_passengers.begin(); itr != m_passengers.end();)
    {
        UnitSet::iterator it2 = itr;
        ++itr;

        Unit* unit = *it2;
        if (!unit)
        {
            m_passengers.erase(it2);
            continue;
        }

        if (Player* plr = unit->ToPlayer())
        {
            if (plr->IsDead() && !plr->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
            {
                plr->ResurrectPlayer(1.0);
            }
            plr->TeleportTo(newMapid, x, y, z, GetOrientation(), TELE_TO_NOT_LEAVE_TRANSPORT);
        }
    }

    if (oldMap != newMap)
    {
        UpdateForMap(oldMap);
        UpdateForMap(newMap);
    }
}

void GlobalTransport::Update(uint32 /*update_diff*/, uint32 /*p_time*/)
{
    if (m_WayPoints.size() <= 1)
        { return; }

    m_timer = WorldTimer::getMSTime() % m_period;
    while (((m_timer - m_curr->first) % m_pathTime) > ((m_next->first - m_curr->first) % m_pathTime))
    {
        MoveToNextWayPoint();

        // first check help in case client-server transport coordinates de-synchronization
        if (m_curr->second.mapid != GetMapId() || m_curr->second.teleport)
        {
            TeleportTransport(m_curr->second.mapid, m_curr->second.x, m_curr->second.y, m_curr->second.z);
        }
        else
        {
            Relocate(m_curr->second.x, m_curr->second.y, m_curr->second.z);
        }

        m_nextNodeTime = m_curr->first;

        if (m_curr == m_WayPoints.begin())
            { DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, " ************ BEGIN ************** %s", GetName()); }

        DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, "%s moved to %f %f %f %d", GetName(), m_curr->second.x, m_curr->second.y, m_curr->second.z, m_curr->second.mapid);
    }
}

void GlobalTransport::UpdateForMap(Map const* targetMap)
{
    Map::PlayerList const& pl = targetMap->GetPlayers();
    if (pl.isEmpty())
        { return; }

    if (GetMapId() == targetMap->GetId())
    {
        for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
        {
            if (this != itr->getSource()->GetTransport())
            {
                UpdateData transData;
                BuildCreateUpdateBlockForPlayer(&transData, itr->getSource());
                WorldPacket packet;
                transData.BuildPacket(&packet, true);
                itr->getSource()->SendDirectMessage(&packet);
            }
        }
    }
    else
    {
        UpdateData transData;
        BuildOutOfRangeUpdateBlock(&transData);
        WorldPacket out_packet;
        transData.BuildPacket(&out_packet, true);

        for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
            if (this != itr->getSource()->GetTransport())
                { itr->getSource()->SendDirectMessage(&out_packet); }
    }
}
