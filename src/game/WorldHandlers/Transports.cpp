/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Platform/Define.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>

#include "Transports.h"
#include "TransportMap.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Path.h"
#include "GameTime.h"
#include "terrain/TileSerializer.hpp"
#include <list>

#include "DBCStores.h"
#include "ProgressBar.h"
#include "ScriptMgr.h"

/**
 * @brief Loads and initializes all configured global transports.
 */
void MapManager::LoadTransports()
{

    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `name`, `period` FROM `transports`");

    uint32 count = 0;
    uint32 mapped = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u transports", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Transport* t = new Transport;

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        std::string name = fields[1].GetCppString();
        t->m_period = fields[2].GetUInt32();

        const GameObjectInfo* goinfo = ObjectMgr::GetGameObjectInfo(entry);

        if (!goinfo)
        {
            sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template missing", entry, name.c_str());
            delete t;
            continue;
        }

        if (goinfo->type != GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template type wrong", entry, name.c_str());
            delete t;
            continue;
        }

        // sLog.outString("Loading transport %d between %s, %s", entry, name.c_str(), goinfo->name);

        std::set<uint32> mapsUsed;

        if (!t->GenerateWaypoints(goinfo->moTransport.taxiPathId, mapsUsed))
            // skip transports with empty waypoints list
        {
            sLog.outErrorDb("Transport (path id %u) path size = 0. Transport ignored, check DBC files or transport GO data0 field.", goinfo->moTransport.taxiPathId);
            delete t;
            continue;
        }

        float x, y, z, o;
        uint32 mapid;
        x = t->m_WayPoints[0].x; y = t->m_WayPoints[0].y; z = t->m_WayPoints[0].z; mapid = t->m_WayPoints[0].mapid; o = 1;

        // current code does not support transports in dungeon!
        const MapEntry* pMapInfo = sMapStore.LookupEntry(mapid);
        if (!pMapInfo || pMapInfo->Instanceable())
        {
            delete t;
            continue;
        }

        // A map for this vessel, resolved from the client or minted, before Create asks for
        // it.
        Transport::RegisterVesselMap(entry, name.c_str());

        // creates the Gameobject
        if (!t->Create(entry, mapid, x, y, z, o, GO_ANIMPROGRESS_DEFAULT))
        {
            delete t;
            continue;
        }

        m_Transports.insert(t);

        for (std::set<uint32>::const_iterator i = mapsUsed.begin(); i != mapsUsed.end(); ++i)
        {
            m_TransportsByMap[*i].insert(t);
        }

        // If we someday decide to use the grid to track transports, here:
        t->SetMap(sMapMgr.CreateMap(mapid, t));

        // INTO THE WORLD'S GRID, as an ordinary object in a cell of the map it sails. That
        // is what ticks it in phase one, what lets the shore's own visibility sweep find
        // it, and what makes IsInWorld() true -- without which SharesWorld, InReach and
        // every searcher built on them refuse to see the vessel at all, which is what left
        // the relay gathering nobody.
        //
        // Active as well, so the water it is crossing stays awake with no player near it.
        // In the world -- IsInWorld() true, so SharesWorld and every searcher built on it
        // can see the vessel -- and active, so the water it crosses stays awake. Not filed
        // in a cell: nothing in this core relocates a game object's cell, and the tick it
        // needs comes from the map's own update instead.
        t->AddToWorld();
        t->SetActiveObjectState(true);
        t->GetMap()->AddToActive(t);

        t->PinRouteGrids();

        // The failure is reported by Create, which can tell a missing Map.dbc row from a
        // map that would not open; here we only count what succeeded.
        if (t->AsMap())
        {
            ++mapped;
            DETAIL_LOG("Transport %u '%s' is map %u", entry, name.c_str(),
                       t->VesselMapId());
        }

        // t->GetMap()->Add<GameObject>((GameObject *)t);
        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u transports, %u with a map of their own", count, mapped);

    // check transport data DB integrity
    result = WorldDatabase.Query("SELECT `gameobject`.`guid`,`gameobject`.`id`,`transports`.`name` FROM `gameobject`,`transports` WHERE `gameobject`.`id` = `transports`.`entry`");
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
}

Transport::Transport() : GameObject()
{
    m_updateFlag = (UPDATEFLAG_TRANSPORT | UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);
}

bool Transport::Create(uint32 guidlow, uint32 mapid, float x, float y, float z, float ang, uint8 animprogress)
{
    Place().MoveTo(x, y, z, ang);
    // instance id and phaseMask isn't set to values different from std.

    if (!IsPlaceable(*this))
    {
        sLog.outError("Transport (GUID: %u) not created. Suggested coordinates isn't valid (X: %f Y: %f)",
                      guidlow, x, y);
        return false;
    }

    Object::_Create(guidlow, 0, HIGHGUID_MO_TRANSPORT);

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(guidlow);

    if (!goinfo)
    {
        sLog.outErrorDb("Transport not created: entry in `gameobject_template` not found, guidlow: %u map: %u  (X: %f Y: %f Z: %f) ang: %f", guidlow, mapid, x, y, z, ang);
        return false;
    }

    SetGOInfo(goinfo);

    SetObjectScale(goinfo->size);

    SetGoType(GAMEOBJECT_TYPE_MO_TRANSPORT);
    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);
    SetEntry(goinfo->id);
    SetUInt32Value(GAMEOBJECT_DISPLAYID, goinfo->displayId);
    SetGoState(GO_STATE_READY);
    SetGoAnimProgress(animprogress);
    SetName(goinfo->name);

    // THE VESSEL IS A MAP. Blizzard gave her a Map.dbc row and no terrain for it; the baker
    // fills that in from the hull's own model, so from here she answers height, collision and
    // routing through the ordinary engines. Model space is that map's space, which is why
    // nothing in the chain applies a transform.
    if (const uint32 mapId = VesselMapIdOf(goinfo->id))
    {
        m_map = sMapMgr.CreateMap(mapId, this)->AsTransport();

        // A map that could not be commissioned is kept all the same: it is still the relay,
        // and it is what refuses to take anyone aboard.
        if (m_map)
        {
            m_map->Commission();
        }
    }
    else
    {
        sLog.outErrorDb("Transport %u (%s, display %u) has no map of its own.",
                        goinfo->id, goinfo->name, goinfo->displayId);
    }

    return true;
}

void Transport::PinRouteGrids()
{
    // THE ROUTE'S GRIDS, LOADED AT START-UP AND NEVER LET GO. The vessel is an active object
    // in them, so what the relay finds ashore is whatever those cells hold -- and a cell that
    // had expired holds nothing, silently, and only once she was already sailing past it.
    uint32 pinned = 0;
    for (WayPointMap::value_type const& node : m_WayPoints)
    {
        if (Map* sailed = sMapMgr.CreateMap(node.second.mapid, this))
        {
            sailed->ForceLoadGrid(node.second.x, node.second.y);
            ++pinned;
        }
    }

    DETAIL_LOG("Transport %u '%s': %u route node(s) pinned.", GetEntry(), GetName(), pinned);
}

Transport* Transport::GetTransport(Map const* map, ObjectGuid guid)
{
    if (!map || !guid)
    {
        return NULL;
    }

    MapManager::TransportsByMapType::const_iterator vessels =
        sMapMgr.m_TransportsByMap.find(map->GetId());
    if (vessels == sMapMgr.m_TransportsByMap.end())
    {
        return NULL;
    }

    for (Transport* vessel : vessels->second)
    {
        if (vessel->GetObjectGuid() == guid)
        {
            return vessel;
        }
    }

    return NULL;
}

namespace
{
    /// Resolved once per vessel entry and then authoritative: the store it is derived from
    /// is MUTATED below (minted rows are injected into it), so re-deriving would see a
    /// different world each time.
    std::unordered_map<uint32, uint32> s_vesselMapByEntry;
    std::unordered_set<uint32> s_vesselMapIds;
}

void Transport::RegisterVesselMap(uint32 goEntry, char const* vesselName)
{
    if (s_vesselMapByEntry.find(goEntry) != s_vesselMapByEntry.end())
    {
        return;
    }

    // EVERY VESSEL MAP HERE IS MINTED. Blizzard keys a hull's map by its directory string,
    // "Transport<entry>" -- and 1.12's Map.dbc format string skips that field, so the
    // server never loads it and there is nothing to match against. The hull is in the
    // client all the same, so it gets a map of its own: an id minted here and a Map.dbc
    // entry injected to carry it. Nothing on the wire ever carries this number.
    //
    // The baker must agree, which is what tools/vessels.txt is for -- every line in it is
    // a minted id for the same reason.
    const uint32 minted = world::terrain::MintedVesselMapId(goEntry);

    MapEntry* row = new MapEntry();
    row->MapID = minted;
    row->InstanceType = MAP_COMMON;
    row->AreaTableID = 0;
    row->LoadingScreenID = 0;

    static std::list<std::string> s_names;
    s_names.push_back(vesselName ? vesselName : "Vessel");
    for (uint32 i = 0; i < 8; ++i)
    {
        row->MapName_lang[i] = const_cast<char*>(s_names.back().c_str());
    }

    sMapStore.SetEntry(minted, row);

    s_vesselMapByEntry[goEntry] = minted;
    s_vesselMapIds.insert(minted);

    DETAIL_LOG("Transport %u '%s' has no Map.dbc row; map %u minted.", goEntry,
               vesselName ? vesselName : "", minted);
}

uint32 Transport::VesselMapIdOf(uint32 goEntry)
{
    const auto found = s_vesselMapByEntry.find(goEntry);
    return found != s_vesselMapByEntry.end() ? found->second : 0;
}

bool Transport::IsVesselMapId(uint32 mapId)
{
    return s_vesselMapIds.find(mapId) != s_vesselMapIds.end();
}

Transport* Transport::VesselOf(WorldObject const& obj)
{
    // DERIVED, NEVER STORED. Being aboard is not a fact anyone records: it is what having
    // this map means. A creature summoned at sea, a crew member read from `creature`, a
    // player who walked up the gangplank -- all the same answer, from the same question,
    // with nothing to keep in step.
    //
    // A lift passenger and a vehicle rider are NOT here: their map is the world's, and the
    // seat transform is the vehicle system's business, not this one's.
    Map* map = obj.GetMap();
    TransportMap* hull = map ? map->AsTransport() : NULL;
    return hull ? hull->Vessel() : NULL;
}

void Transport::WithdrawFromWorld()
{
    // Guarded because both teardown paths call it, and the second runs after the maps have
    // been deleted -- GetMap() would then point at freed memory.
    if (m_withdrawn)
    {
        return;
    }
    m_withdrawn = true;
    m_crossing = false;

    // Nothing else does this. A vessel is in no cell, so no grid unload reaches it, and the
    // map that owns it is deleted before the vessel is -- ~Object then asserts on an object
    // still flagged in-world, against a map that no longer exists.
    if (m_map)
    {
        m_map->ReleaseCrew();
    }

    if (Map* sailed = GetMap())
    {
        sailed->RemoveFromActive(this);
    }

    if (IsInWorld())
    {
        RemoveFromWorld();
    }

    m_map = NULL;
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

/**
 * @brief Builds the waypoint timeline used by a global transport route.
 *
 * @return true if waypoint generation succeeded.
 */
bool Transport::GenerateWaypoints(uint32 pathid, std::set<uint32>& mapids)
{
    if (pathid >= sTaxiPathNodesByPath.size())
    {
        return false;
    }

    TaxiPathNodeList const& path = sTaxiPathNodesByPath[pathid];

    std::vector<keyFrame> keyFrames;
    int mapChange = 0;
    mapids.clear();
    for (size_t i = 1; i < path.size() - 1; ++i)
    {
        if (mapChange == 0)
        {
            TaxiPathNodeEntry const& node_i = path[i];
            if (node_i.ContinentID == path[i + 1].ContinentID)
            {
                keyFrame k(node_i);
                keyFrames.push_back(k);
                mapids.insert(k.node->ContinentID);
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
    if (keyFrames[0].node->Flags == 2)
    {
        lastStop = 0;
    }

    // find the rest of the distances between key points
    for (size_t i = 1; i < keyFrames.size(); ++i)
    {
        if ((keyFrames[i].node->Flags == 1) || (keyFrames[i].node->ContinentID != keyFrames[i - 1].node->ContinentID))
        {
            keyFrames[i].distFromPrev = 0;
        }
        else
        {
            keyFrames[i].distFromPrev =
                sqrt(pow(keyFrames[i].node->LocX - keyFrames[i - 1].node->LocX, 2) +
                     pow(keyFrames[i].node->LocY - keyFrames[i - 1].node->LocY, 2) +
                     pow(keyFrames[i].node->LocZ - keyFrames[i - 1].node->LocZ, 2));
        }
        if (keyFrames[i].node->Flags == 2)
        {
            // remember first stop frame
            if (firstStop == -1)
            {
                firstStop = i;
            }
            lastStop = i;
        }
    }

    float tmpDist = 0;
    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        int j = (i + lastStop) % keyFrames.size();
        if (keyFrames[j].node->Flags == 2)
        {
            tmpDist = 0;
        }
        else
        {
            tmpDist += keyFrames[j].distFromPrev;
        }
        keyFrames[j].distSinceStop = tmpDist;
    }

    for (int i = int(keyFrames.size()) - 1; i >= 0; --i)
    {
        int j = (i + (firstStop + 1)) % keyFrames.size();
        tmpDist += keyFrames[(j + 1) % keyFrames.size()].distFromPrev;
        keyFrames[j].distUntilStop = tmpDist;
        if (keyFrames[j].node->Flags == 2)
        {
            tmpDist = 0;
        }
    }

    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        if (keyFrames[i].distSinceStop < (30 * 30 * 0.5f))
        {
            keyFrames[i].tFrom = sqrt(2 * keyFrames[i].distSinceStop);
        }
        else
        {
            keyFrames[i].tFrom = ((keyFrames[i].distSinceStop - (30 * 30 * 0.5f)) / 30) + 30;
        }

        if (keyFrames[i].distUntilStop < (30 * 30 * 0.5f))
        {
            keyFrames[i].tTo = sqrt(2 * keyFrames[i].distUntilStop);
        }
        else
        {
            keyFrames[i].tTo = ((keyFrames[i].distUntilStop - (30 * 30 * 0.5f)) / 30) + 30;
        }

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
    if (keyFrames[keyFrames.size() - 1].node->ContinentID != keyFrames[0].node->ContinentID)
    {
        teleport = true;
    }

    WayPoint pos(keyFrames[0].node->ContinentID, keyFrames[0].node->LocX, keyFrames[0].node->LocY, keyFrames[0].node->LocZ, teleport);
    m_WayPoints[0] = pos;
    t += keyFrames[0].node->Delay * 1000;

    uint32 cM = keyFrames[0].node->ContinentID;
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
                    newX = keyFrames[i].node->LocX + (keyFrames[i + 1].node->LocX - keyFrames[i].node->LocX) * d / keyFrames[i + 1].distFromPrev;
                    newY = keyFrames[i].node->LocY + (keyFrames[i + 1].node->LocY - keyFrames[i].node->LocY) * d / keyFrames[i + 1].distFromPrev;
                    newZ = keyFrames[i].node->LocZ + (keyFrames[i + 1].node->LocZ - keyFrames[i].node->LocZ) * d / keyFrames[i + 1].distFromPrev;

                    bool teleport = false;
                    if (keyFrames[i].node->ContinentID != cM)
                    {
                        teleport = true;
                        cM = keyFrames[i].node->ContinentID;
                    }

                    //                    sLog.outString("T: %d, D: %f, x: %f, y: %f, z: %f", t, d, newX, newY, newZ);
                    WayPoint pos(keyFrames[i].node->ContinentID, newX, newY, newZ, teleport);
                    if (teleport)
                    {
                        m_WayPoints[t] = pos;
                    }
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
        {
            t += 100 - ((long)keyFrames[i + 1].tTo % 100);
        }
        else
        {
            t += (long)keyFrames[i + 1].tTo % 100;
        }

        bool teleport = false;
        if ((keyFrames[i + 1].node->Flags == 1) || (keyFrames[i + 1].node->ContinentID != keyFrames[i].node->ContinentID))
        {
            teleport = true;
            cM = keyFrames[i + 1].node->ContinentID;
        }

        WayPoint pos(keyFrames[i + 1].node->ContinentID, keyFrames[i + 1].node->LocX, keyFrames[i + 1].node->LocY, keyFrames[i + 1].node->LocZ, teleport);

        //        sLog.outString("T: %d, x: %f, y: %f, z: %f, t:%d", t, pos.x, pos.y, pos.z, teleport);

        // if (teleport)
        m_WayPoints[t] = pos;

        t += keyFrames[i + 1].node->Delay * 1000;
        //        sLog.outString("------");
    }

    uint32 timer = t;

    //    sLog.outDetail("    Generated %lu waypoints, total time %u.", (unsigned long)m_WayPoints.size(), timer);

    m_next = m_WayPoints.begin();                           // will used in MoveToNextWayPoint for init m_curr
    MoveToNextWayPoint();                                   // m_curr -> first point
    MoveToNextWayPoint();                                   // skip first point

    m_pathTime = timer;

    m_nextNodeTime = m_curr->first;

    // How wrong the estimate is allowed to be. The pose snaps from node to node and is
    // never interpolated, so at worst it sits half a segment away from where the client
    // draws the hull. Every proximity question about this vessel is widened by that.
    m_nodeSlack = 0.0f;
    for (WayPointMap::const_iterator it = m_WayPoints.begin(); it != m_WayPoints.end(); ++it)
    {
        WayPointMap::const_iterator nxt = it;
        if (++nxt == m_WayPoints.end() || nxt->second.mapid != it->second.mapid)
        {
            continue;
        }

        const float dx = nxt->second.x - it->second.x;
        const float dy = nxt->second.y - it->second.y;
        m_nodeSlack = std::max(m_nodeSlack, std::sqrt(dx * dx + dy * dy) * 0.5f);
    }

    return true;
}

/**
 * @brief Advances the current and next transport waypoint pointers.
 */
void Transport::MoveToNextWayPoint()
{
    m_curr = m_next;

    ++m_next;
    if (m_next == m_WayPoints.end())
    {
        m_next = m_WayPoints.begin();
    }
}

/**
 * @brief Teleports the transport and its player passengers to another map position.
 *
 * @param newMapid The destination map id.
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 */
void Transport::TeleportTransport(uint32 newMapid, float x, float y, float z)
{
    Map* oldMap = GetMap();

    // The route decided WHEN; what crossing means for anyone standing on her is not this
    // object's business and never was. Her own map is told and owns every consequence.
    Place().MoveTo(x, y, z);

    // A node flagged for teleport that does not leave this map: nothing changes for anyone.
    // Her passengers' coordinates are her own map's and do not move, and the client draws
    // the jump itself out of the path progress.
    if (!oldMap || oldMap->GetId() == newMapid)
    {
        return;
    }

    // AND NOT ONE STEP FURTHER ON THIS THREAD. Crossing writes into another map's active
    // list, object store and player list, and that map may be running right now on another
    // core. Worse, half a crossing is a vessel that reports a map she is no longer on: the
    // passengers arriving from her own map were handed the OTHER continent's ships, and their
    // clients then tried to sail them along paths that do not exist there.
    //
    // So the route only says GO. All of it happens at once, at the barrier.
    m_crossingTo = newMapid;
    m_crossingX = x;
    m_crossingY = y;
    m_crossingZ = z;
    m_crossing = true;
}

void Transport::CompleteCrossing()
{
    if (!m_crossing)
    {
        return;
    }

    m_crossing = false;

    const uint32 newMapid = m_crossingTo;
    m_crossingTo = 0;

    Map* oldMap = GetMap();
    Map* newMap = sMapMgr.CreateMap(newMapid, this);
    if (!oldMap || !newMap || oldMap == newMap)
    {
        return;
    }

    // THIS SIDE FIRST, while she is still on it: the shore loses her, and her passengers are
    // started on their way. The transfer packet they get names the map they are leaving,
    // which one line later would already be the map they are going to.
    if (m_map)
    {
        // The node's own coordinates, not the pose. Same number the client's path is built
        // from, so nobody is put down anywhere the ship is not.
        m_map->VesselLeavingWorld(oldMap, newMapid, m_crossingX, m_crossingY, m_crossingZ,
                                  Where().Facing());
    }

    // Off the old grid properly: a game object left in a cell of a map it is no longer on is
    // a dangling entry the next visit of that cell walks straight into.
    oldMap->RemoveFromActive(this);
    RemoveFromWorld();

    SetMap(newMap);

    AddToWorld();
    newMap->AddToActive(this);

    if (m_map)
    {
        m_map->VesselEnteredWorld(newMap);
    }
}

/**
 * @brief Updates global transport position along its generated path.
 *
 * @param update_diff The elapsed update time.
 * @param p_time The current path time parameter.
 */
void Transport::Update(uint32 update_diff, uint32 /*p_time*/)
{
    // Between two world maps: she belongs to neither until the barrier hands her over, and
    // her own map does not tick without her.
    if (m_crossing)
    {
        return;
    }

    // The route clock, and nothing else. This vessel is never redrawn, never repositioned
    // and never composed with anything: her pose is advanced only so that the cell sweep in
    // her own map's tick knows WHICH GRID of the world to look in for the objects ashore.
    // What the client is sent is the path progress below and her entry, and it draws her
    // itself, from an animation the server does not have.
    if (m_WayPoints.size() > 1)
    {
        // Absolute wall-clock, NOT milliseconds since this process started. The phase of a
        // route has to survive a restart: keyed off uptime, every vessel on the server sails
        // from the beginning of its path each time we come up, while the client -- which
        // interpolates the hull from the value we hand it -- draws her somewhere else
        // entirely, and the two then argue about a ship neither has.
        const uint32 mapBefore = GetMapId();

        m_timer = uint32(GameTime::GetAbsoluteTimeMS() % m_period);
        while (((m_timer - m_curr->first) % m_pathTime) > ((m_next->first - m_curr->first) % m_pathTime))
        {
            MoveToNextWayPoint();

            // THE TIME OF THE TRANSFER. The one thing the route has to decide: this node
            // belongs to another world map, so the ship changes which map she sails, and
            // everyone aboard follows.
            if (m_curr->second.mapid != GetMapId() || m_curr->second.teleport)
            {
                TeleportTransport(m_curr->second.mapid, m_curr->second.x, m_curr->second.y, m_curr->second.z);
            }
            else
            {
                Place().MoveTo(m_curr->second.x, m_curr->second.y, m_curr->second.z);
            }

            m_nextNodeTime = m_curr->first;

            if (m_curr == m_WayPoints.begin())
            {
                DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, " ************ BEGIN ************** %s", GetName());
            }

            DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, "%s moved to %f %f %f %d", GetName(), m_curr->second.x, m_curr->second.y, m_curr->second.z, m_curr->second.mapid);
        }

        // A seam moved us, and everything below belongs to the new map's tick.
        if (GetMapId() != mapBefore)
        {
            return;
        }
    }

    // LAST, AND IT MUST STAY LAST. The ship's own map runs nested inside this tick, on the
    // thread of the world map she sails and after that map has finished walking its own
    // containers -- which is what lets everything it sends go straight out, with no queue
    // and no tick of latency.
    if (m_map)
    {
        m_map->Update(update_diff);
    }
}
