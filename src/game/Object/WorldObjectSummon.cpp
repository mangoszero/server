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

/**
 * @file Object.cpp
 * @brief Base implementation for all game objects
 *
 * This file implements the Object class, which is the base class for all
 * entities in the game world. It provides:
 * - Update field management (synchronized with clients)
 * - Object GUID handling
 * - Update data building for network transmission
 * - Object visibility and spawning
 * - Type identification
 *
 * The Object class uses an array of uint32 values (update fields) that
 * mirror the client's object state. Changes to these values are sent to
 * players who can see the object.
 */



#include "Geometry/Placement.h"
#include <cmath>
#include "Utilities/Errors.h"
#include "Utilities/MathDefines.h"
#include "Object.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "MapManager.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
#include "Corpse.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Assigns the current map context to the world object.
 *
 * @param map The map to assign.
 */
void WorldObject::SetMap(Map* map)
{
    MANGOS_ASSERT(map);
    m_currMap = map;
    // lets save current map's Id/instanceId
    m_mapId = map->GetId();
    m_InstanceId = map->GetInstanceId();
    RefreshFrame();
}

/**
 * @brief Resets the world object's map state.
 */
void WorldObject::ResetMap()
{
}

TerrainInfo const* WorldObject::GetTerrain() const
{
    MANGOS_ASSERT(m_currMap);
    return m_currMap->GetTerrain();
}

/**
 * @brief Schedules the object for removal from the map.
 */
void WorldObject::AddObjectToRemoveList()
{
    GetMap()->AddObjectToRemoveList(this);
}

/**
 * @brief Summons a temporary creature near or at the requested position.
 *
 * @param id The creature template id.
 * @param x The summon x coordinate.
 * @param y The summon y coordinate.
 * @param z The summon z coordinate.
 * @param ang The summon orientation.
 * @param spwtype The temporary spawn type.
 * @param despwtime The despawn time in milliseconds.
 * @param asActiveObject true to mark the summon as active.
 * @param setRun true to make the summon run.
 * @return The summoned creature, or null on failure.
 */
Creature* WorldObject::SummonCreature(uint32 id, float x, float y, float z, float ang, TempSpawnType spwtype, uint32 despwtime, bool asActiveObject, bool setRun)
{
    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(id);
    if (!cinfo)
    {
        sLog.outErrorDb("WorldObject::SummonCreature: Creature (Entry: %u) not existed for summoner: %s. ", id, GetGuidStr().c_str());
        return NULL;
    }

    TemporarySummon* pCreature = new TemporarySummon(GetObjectGuid());

    Team team = TEAM_NONE;
    if (GetTypeId() == TYPEID_PLAYER)
    {
        team = ((Player*)this)->GetTeam();
    }

    CreatureCreatePos pos(GetMap(), x, y, z, ang);

    if (x == 0.0f && y == 0.0f && z == 0.0f)
    {
        pos = CreatureCreatePos(this, Where().Facing(), CONTACT_DISTANCE, ang);
    }

    if (!pCreature->Create(GetMap()->GenerateLocalLowGuid(cinfo->GetHighGuid()), pos, cinfo, team))
    {
        delete pCreature;
        return NULL;
    }

    pCreature->SetSpawn(pos);

    // Set run or walk before any other movement starts
    pCreature->SetWalk(!setRun);

    // Active state set before added to map
    pCreature->SetActiveObjectState(asActiveObject);

    pCreature->Summon(spwtype, despwtime);                  // Also initializes the AI and MMGen

    if (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->AI())
    {
        ((Creature*)this)->AI()->JustSummoned(pCreature);
    }

#ifdef ENABLE_ELUNA
    if (Unit* summoner = ToUnit())
    {
        if (Eluna* e = GetEluna())
        {
            e->OnSummoned(pCreature, summoner);
        }
    }
#endif /* ENABLE_ELUNA */

    // Creature Linking, Initial load is handled like respawn
    if (pCreature->IsLinkingEventTrigger())
    {
        GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_RESPAWN, pCreature);
    }

    // return the creature therewith the summoner has access to it
    return pCreature;
}

/**
 * @brief Summons a temporary game object at the requested position.
 *
 * @param id The gameobject entry id.
 * @param x The summon x coordinate.
 * @param y The summon y coordinate.
 * @param z The summon z coordinate.
 * @param angle The summon orientation.
 * @param despwtime The despawn time in milliseconds.
 * @return The summoned game object, or null on failure.
 */
GameObject* WorldObject::SummonGameObject(uint32 id, float x, float y, float z, float angle, uint32 despwtime)
{
    GameObject* pGameObj = new GameObject;

    Map *map = GetMap();

    if (!map)
    {
        return NULL;
    }

    if (!pGameObj->Create(map->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), id, map, x, y, z, angle))
    {
        delete pGameObj;
        return NULL;
    }

    pGameObj->SetRespawnTime(despwtime/IN_MILLISECONDS);

    map->Add(pGameObj);
    pGameObj->AIM_Initialize();

    return pGameObj;
}

// how much space should be left in front of/ behind a mob that already uses a space
#define OCCUPY_POS_DEPTH_FACTOR                          1.8f

namespace MaNGOS
{

    /**
     * @brief Near used position functor
     *
     * Checks for used positions near an object for position selection.
     */
    class NearUsedPosDo
    {
        public:
            /**
             * @brief Constructor
             * @param obj Source object
             * @param searcher Object searching for position
             * @param absAngle Absolute angle
             * @param selector Position selector
             */
            NearUsedPosDo(WorldObject const& obj, WorldObject const* searcher, float absAngle, ObjectPosSelector& selector)
                : i_object(obj), i_searcher(searcher), i_absAngle(Geometry::Placement::NormalizeOrientation(absAngle)), i_selector(selector) {}

            void operator()(Corpse*) const {}
            void operator()(DynamicObject*) const {}

            /**
             * @brief Process creature
             * @param c Creature to process
             */
            void operator()(Creature* c) const
            {
                // skip self or target
                if (c == i_searcher || c == &i_object)
                {
                    return;
                }

                float x, y, z;

                if (c->IsStopped() || !c->GetMotionMaster()->GetDestination(x, y, z))
                {
                    x = c->Where().X();
                    y = c->Where().Y();
                }

                add(c, x, y);
            }

            /**
             * @brief Process generic unit
             * @param u Unit to process
             */
            template<class T>
                void operator()(T* u) const
            {
                // skip self or target
                if (u == i_searcher || u == &i_object)
                {
                    return;
                }

                float x, y;

                x = u->Where().X();
                y = u->Where().Y();

                add(u, x, y);
            }

            /**
             * @brief Add used position
             * @param u Object to add
             * @param x X coordinate
             * @param y Y coordinate
             *
             * Adds a used position to the selector.
             */
            void add(WorldObject* u, float x, float y) const
            {
                float dx = i_object.Where().X() - x;
                float dy = i_object.Where().Y() - y;
                float dist2d = sqrt((dx * dx) + (dy * dy));

                // It is ok for the objects to require a bit more space
                float delta = u->Where().Extent();
                if (i_selector.m_searchPosFor && i_selector.m_searchPosFor != u)
                {
                    delta += i_selector.m_searchPosFor->Where().Extent();
                }

                delta *= OCCUPY_POS_DEPTH_FACTOR;           // Increase by factor

                // u is too near/far away from i_object. Do not consider it to occupy space
                if (fabs(i_selector.m_searcherDist - dist2d) > delta)
                {
                    return;
                }

                float angle = i_object.Where().BearingTo(u->Where()) - i_absAngle;

                // move angle to range -pi ... +pi, range before is -2Pi..2Pi
                if (angle > M_PI_F)
                {
                    angle -= 2.0f * M_PI_F;
                }
                else if (angle < -M_PI_F)
                {
                    angle += 2.0f * M_PI_F;
                }

                i_selector.AddUsedArea(u, angle, dist2d);
            }
        private:
            WorldObject const& i_object;
            WorldObject const* i_searcher;
            float              i_absAngle;
            ObjectPosSelector& i_selector;
    };
}                                                           // namespace MaNGOS

// A point the component constructed, pulled back inside the map's coordinate bounds --
// which is the map's business, not the geometry's.
Geometry::Vector3 PointNear(WorldObject const& anchor, float distance2d, float absAngle)
{
    Geometry::Vector3 point = anchor.Where().PointAt(distance2d, absAngle);
    MaNGOS::NormalizeMapCoord(point.x);
    MaNGOS::NormalizeMapCoord(point.y);
    return point;
}

/**
 * @brief Finds a nearby point while accounting for collisions and line of sight.
 *
 * @param searcher The object requesting the position.
 * @param x Receives the resulting x coordinate.
 * @param y Receives the resulting y coordinate.
 * @param z Receives the resulting z coordinate.
 * @param searcher_bounding_radius The requester's bounding radius.
 * @param distance2d The desired distance from the anchor.
 * @param absAngle The preferred absolute angle.
 */
void FindFreeSpotNear(WorldObject const& anchor, WorldObject const* searcher, float& x, float& y, float& z,
                      float searcher_bounding_radius, float distance2d, float absAngle)
{
    const Geometry::Vector3 first = PointNear(anchor, distance2d, absAngle);
    x = first.x;
    y = first.y;
    const float init_z = z = anchor.Where().Z();

    // if detection disabled, return first point
    if (!sWorld.getConfig(CONFIG_BOOL_DETECT_POS_COLLISION))
    {
        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }
        return;
    }

    // or remember first point
    float first_x = x;
    float first_y = y;
    bool first_los_conflict = false;                        // first point LOS problems

    const float dist = distance2d + searcher_bounding_radius + anchor.Where().Extent();

    // prepare selector for work
    ObjectPosSelector selector(anchor.Where().X(), anchor.Where().Y(), distance2d, searcher_bounding_radius, searcher);

    // adding used positions around object
    {
        MaNGOS::NearUsedPosDo u_do(anchor, searcher, absAngle, selector);
        MaNGOS::WorldObjectWorker<MaNGOS::NearUsedPosDo> worker(u_do);

        Cell::VisitAllObjects(&anchor, worker, dist);
    }

    // maybe can just place in primary position
    if (selector.CheckOriginalAngle())
    {
        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }

        if (fabs(init_z - z) < dist && HasLineOfSight(anchor, Geometry::Vector3(x, y, z)))
        {
            return;
        }

        first_los_conflict = true;                          // first point have LOS problems
    }

    // set first used pos in lists
    selector.InitializeAngle();

    float angle;                                            // candidate of angle for free pos

    // select in positions after current nodes (selection one by one)
    while (selector.NextAngle(angle))                       // angle for free pos
    {
        const Geometry::Vector3 candidate = PointNear(anchor, distance2d, absAngle + angle);
        x = candidate.x;
        y = candidate.y;
        z = anchor.Where().Z();

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }

        if (fabs(init_z - z) < dist && HasLineOfSight(anchor, Geometry::Vector3(x, y, z)))
        {
            return;
        }
    }

    // BAD NEWS: not free pos (or used or have LOS problems)
    // Attempt find _used_ pos without LOS problem
    if (!first_los_conflict)
    {
        x = first_x;
        y = first_y;

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }
        return;
    }

    // set first used pos in lists
    selector.InitializeAngle();

    // select in positions after current nodes (selection one by one)
    while (selector.NextUsedAngle(angle))                   // angle for used pos but maybe without LOS problem
    {
        const Geometry::Vector3 candidate = PointNear(anchor, distance2d, absAngle + angle);
        x = candidate.x;
        y = candidate.y;
        z = anchor.Where().Z();

        if (searcher)
        {
            ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());       // update to LOS height if available
        }
        else
        {
            DropToGround(anchor, x, y, z);
        }

        if (fabs(init_z - z) < dist && HasLineOfSight(anchor, Geometry::Vector3(x, y, z)))
        {
            return;
        }
    }

    // BAD BAD NEWS: all found pos (free and used) have LOS problem :(
    x = first_x;
    y = first_y;

    if (searcher)
    {
        ClampToAllowedZ(*searcher, x, y, z, anchor.GetMap());           // update to LOS height if available
    }
    else
    {
        DropToGround(anchor, x, y, z);
    }
}

void ClosePointNear(WorldObject const& anchor, float& x, float& y, float& z, float bounding_radius,
                    float distance2d, float angle, WorldObject const* searcher)
{
    FindFreeSpotNear(anchor, searcher, x, y, z, bounding_radius,
                     Geometry::Placement::ContactSpread(distance2d, anchor.Where().Extent(), bounding_radius),
                     anchor.Where().Facing() + angle);
}

void ContactPointNear(WorldObject const& anchor, WorldObject const* obj, float& x, float& y, float& z,
                      float distance2d)
{
    FindFreeSpotNear(anchor, obj, x, y, z, obj->Where().Extent(),
                     Geometry::Placement::ContactSpread(distance2d, anchor.Where().Extent(),
                                                        obj->Where().Extent()),
                     anchor.Where().BearingTo(obj->Where()));
}

/**
 * @brief Plays a positional sound for one player or nearby players.
 *
 * @param sound_id The sound entry id.
 * @param target Optional single-player target.
 */
void WorldObject::PlayDistanceSound(uint32 sound_id, Player const* target /*= NULL*/) const
{
    WorldPacket data(SMSG_PLAY_OBJECT_SOUND, 4 + 8);
    data << uint32(sound_id);
    data << GetObjectGuid();
    if (target)
    {
        target->SendDirectMessage(&data);
    }
    else
    {
        SendMessageToSet(&data, true);
    }
}

/**
 * @brief Plays a direct sound for one player or nearby players.
 *
 * @param sound_id The sound entry id.
 * @param target Optional single-player target.
 */
void WorldObject::PlayDirectSound(uint32 sound_id, Player const* target /*= NULL*/) const
{
    WorldPacket data(SMSG_PLAY_SOUND, 4);
    data << uint32(sound_id);
    if (target)
    {
        target->SendDirectMessage(&data);
    }
    else
    {
        SendMessageToSet(&data, true);
    }
}

/**
 * @brief Plays music for one player or nearby players.
 *
 * @param sound_id The music entry id.
 * @param target Optional single-player target.
 */
void WorldObject::PlayMusic(uint32 sound_id, Player const* target /*= NULL*/) const
{
    WorldPacket data(SMSG_PLAY_MUSIC, 4);
    data << uint32(sound_id);
    if (target)
    {
        target->SendDirectMessage(&data);
    }
    else
    {
        SendMessageToSet(&data, true);
    }
}

/**
 * @brief Refreshes both visibility and viewpoint-dependent visibility state.
 */
void WorldObject::UpdateVisibilityAndView()
{
    GetViewPoint().Call_UpdateVisibilityForOwner();
    UpdateObjectVisibility();
    GetViewPoint().Event_ViewPointVisibilityChanged();
}

/**
 * @brief Recomputes this object's visibility for nearby clients.
 */
void WorldObject::UpdateObjectVisibility()
{
    CellPair p = MaNGOS::ComputeCellPair(Where().X(), Where().Y());
    Cell cell(p);

    GetMap()->UpdateObjectVisibility(this, cell, p);
}

/**
 * @brief Adds the world object to the map's update queue.
 */
void WorldObject::AddToClientUpdateList()
{
    GetMap()->AddUpdateObject(this);
}

/**
 * @brief Remove from client update list
 *
 * Removes this object from the map's update list.
 */
void WorldObject::RemoveFromClientUpdateList()
{
    GetMap()->RemoveUpdateObject(this);
}

/**
 * @brief World object change accumulator
 *
 * Accumulates update data for a world object and nearby players.
 */
struct WorldObjectChangeAccumulator
{
    UpdateDataMapType& i_updateDatas; ///< Update data map
    WorldObject& i_object; ///< World object

    /**
     * @brief Constructor
     * @param obj World object
     * @param d Update data map
     */
    WorldObjectChangeAccumulator(WorldObject& obj, UpdateDataMapType& d) : i_updateDatas(d), i_object(obj)
    {
        // send self fields changes in another way, otherwise
        // with new camera system when player's camera too far from player, camera wouldn't receive packets and changes from player
        if (i_object.isType(TYPEMASK_PLAYER))
        {
            i_object.BuildUpdateDataForPlayer((Player*)&i_object, i_updateDatas);
        }
    }

    /**
     * @brief Visit cameras
     * @param m Camera map
     *
     * Builds update data for all camera owners that can see this object.
     */
    void Visit(CameraMapType& m)
    {
        for (CameraMapType::iterator iter = m.begin(); iter != m.end(); ++iter)
        {
            Player* owner = iter->getSource()->GetOwner();
            if (owner != &i_object && owner->HaveAtClient(&i_object))
            {
                i_object.BuildUpdateDataForPlayer(owner, i_updateDatas);
            }
        }
    }

    /**
     * @brief Visit other grid references (no-op)
     */
    template<class SKIP> void Visit(GridRefManager<SKIP>&) {}
};

/**
 * @brief Build update data
 * @param update_players Map of players to their update data
 *
 * Builds update data for all players who can see this object.
 */
void WorldObject::BuildUpdateData(UpdateDataMapType& update_players)
{
    WorldObjectChangeAccumulator notifier(*this, update_players);
    Cell::VisitWorldObjects(this, notifier, GetMap()->GetBroadcastRadius());

    ClearUpdateMask(false);
}

/**
 * @brief Print coordinates error
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param descr Description of the operation
 * @return Always false
 *
 * Logs an error when invalid coordinates are encountered.
 */
bool WorldObject::PrintCoordinatesError(float x, float y, float z, char const* descr) const
{
    sLog.outError("%s with invalid %s coordinates: mapid = %uu, x = %f, y = %f, z = %f", GetGuidStr().c_str(), descr, GetMapId(), x, y, z);
    return false;                                           // always false for continue assert fail
}

/**
 * @brief Set active object state
 * @param active If true, set as active object
 *
 * Sets whether this object is an active object (updated even when no players nearby).
 */
void WorldObject::SetActiveObjectState(bool active)
{
    if (m_isActiveObject == active || (isType(TYPEMASK_PLAYER) && !active))  // player shouldn't became inactive, never
    {
        return;
    }

    // player's update implemented in a different from other active worldobject's way
    // it's considered to use generic way in future
    if (IsInWorld() && !isType(TYPEMASK_PLAYER))
    {
        if (IsActiveObject() && !active)
        {
            GetMap()->RemoveFromActive(this);
        }
        else if (IsActiveObject() && active)
        {
            GetMap()->AddToActive(this);
        }
    }
    m_isActiveObject = active;
}

#ifdef ENABLE_ELUNA
/**
 * @brief Get Eluna instance
 * @return Eluna instance pointer or nullptr
 *
 * Returns the Eluna scripting engine instance for this object's map.
 *
 * Guarded to match the declaration in Object.h. Unguarded, a -DSCRIPT_LIB_ELUNA=0
 * build fails here on an undeclared Eluna and a Map that has no GetEluna().
 */
Eluna* WorldObject::GetEluna() const
{
    if (IsInWorld())
    {
        return GetMap()->GetEluna();
    }

    return nullptr;
}
#endif /* ENABLE_ELUNA */
