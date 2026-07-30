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
#include "Transports.h"
#include "TransportMap.h"
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
 * @brief Cleanups before delete
 *
 * Removes the object from the world before deletion.
 */
void WorldObject::CleanupsBeforeDelete()
{
    RemoveFromWorld();
}

/**
 * @brief Update world object
 * @param update_diff Time since last update
 * @param time_diff Time parameter (unused)
 *
 * Updates Eluna events if enabled.
 */
void WorldObject::Update(uint32 update_diff, uint32 /*time_diff*/)
{
#ifdef ENABLE_ELUNA
    if (elunaEvents) // can be null on maps without eluna
    {
        elunaEvents->Update(update_diff);
    }
#endif /* ENABLE_ELUNA */
}

/**
 * @brief Create world object
 * @param guidlow Low GUID
 * @param guidhigh High GUID type
 *
 * Creates the world object with the specified GUID.
 */
void WorldObject::_Create(uint32 guidlow, HighGuid guidhigh)
{
    Object::_Create(guidlow, 0, guidhigh);
}

/**
 * @brief Get instance data
 * @return Instance data pointer
 *
 * Returns the instance data for the map this object is on.
 */
InstanceData* WorldObject::GetInstanceData() const
{
    return GetMap()->GetInstanceData();
}

/**
 * @brief A random ground point around a centre, in this object's own frame.
 *
 * The roll is injected rather than drawn here, so the pick stays pinnable in a test.
 */
Geometry::Vector3 RandomGroundPointNear(WorldObject const& obj, Geometry::Vector3 const& centre,
                                        float distance, float minDist, float const* ori)
{
    if (distance == 0.0f)
    {
        return centre;
    }

    const float angle = ori ? *ori : (rand_norm_f() * Geometry::Placement::TwoPi());

    Geometry::Placement around;
    around.EnterFrame(obj.Where().CurrentFrame(), centre, angle);

    Geometry::Vector3 point = around.RandomPointAround(minDist, distance, angle, rand_norm_f());
    MaNGOS::NormalizeMapCoord(point.x);
    MaNGOS::NormalizeMapCoord(point.y);
    DropToGround(obj, point.x, point.y, point.z);
    return point;
}

/**
 * @brief Put z on the floor under (x, y), if there is one.
 *
 * Nothing happens where the map has no floor to offer: an absent answer is absent, not a
 * sentinel height that arithmetic will happily consume.
 */
void DropToGround(WorldObject const& obj, float x, float y, float& z)
{
    if (auto floor = obj.GetMap()->Floor(x, y, z))
    {
        z = *floor + 0.05f;                                 // just to be sure that we are not a few pixel under the surface
    }
}

/**
 * @brief Hold z between the floor and the highest surface this object may occupy.
 */
void ClampToAllowedZ(WorldObject const& obj, float x, float y, float& z, Map* atMap /*=NULL*/)
{
    if (!atMap)
    {
        atMap = obj.GetMap();
    }

    const auto floor = atMap->Floor(x, y, z);
    if (!floor)
    {
        return;
    }

    // Anything that is not a unit has no say in the matter: it sits on the floor.
    const bool isUnit = obj.GetTypeId() == TYPEID_UNIT || obj.GetTypeId() == TYPEID_PLAYER;
    if (!isUnit)
    {
        z = *floor;
        return;
    }

    const Unit& unit = static_cast<const Unit&>(obj);
    if (unit.CanFly())
    {
        if (z < *floor)
        {
            z = *floor;
        }
        return;
    }

    // Held between the floor and the highest surface this unit may occupy: the water it
    // can swim in, or the floor itself when it cannot.
    float ceiling = *floor;
    if (unit.CanSwim())
    {
        ceiling = atMap->GetTerrain()->GetWaterOrGroundLevel(
                      x, y, z, NULL, !unit.HasAuraType(SPELL_AURA_WATER_WALK));
    }

    if (z > ceiling)
    {
        z = ceiling;
    }
    else if (z < *floor)
    {
        z = *floor;
    }
}

// ---- not geometry, so neither the object's nor the component's --------------
//
// World membership is game state; line of sight and a map's coordinate bounds are the
// terrain engine's. Each of these asks the placement for the geometry and contributes
// only the part the placement must never know about.

/**
 * @brief The frame both objects can be answered for, if one exists.
 *
 * Vanilla has no vehicle seats and no vessel-as-map, so every placement is anchored to a
 * map instance and a shared frame is simply the same map. The seam is here rather than
 * inlined at the call sites so that carrying the transport rework across changes this
 * function and nothing else.
 */
static bool InCommonFrame(WorldObject const& a, WorldObject const& b,
                          Geometry::Placement& outA, Geometry::Placement& outB)
{
    // THE FRAME IS THE AUTHORITY, not the passenger registry. Two things on the same deck
    // map already speak the same coordinates whether or not either was ever boarded -- a
    // creature summoned straight onto a deck is exactly that, and asking the roster about
    // it answers no while the geometry answers yes.
    if (a.Where().ShareFrame(b.Where()))
    {
        outA = a.Where();
        outB = b.Where();
        return true;
    }

    TransportMap* va = a.GetMap() ? a.GetMap()->AsTransport() : NULL;
    TransportMap* vb = b.GetMap() ? b.GetMap()->AsTransport() : NULL;

    if (!va && !vb)
    {
        outA = a.Where();
        outB = b.Where();
        return true;
    }

    if (va != vb)
    {
        return false;
    }

    const auto la = va->PositionOf(a);
    const auto lb = va->PositionOf(b);
    if (!la || !lb)
    {
        return false;
    }

    outA = *la;
    outB = *lb;
    return true;
}

/**
 * @brief CAN A REACH B AT ALL -- the question every melee swing, spell, threat entry and
 *        aggro check is really asking.
 *
 * It demands a COMMON FRAME. This is NOT the question "can B see A": seeing a crow
 * overhead is not being able to hit it. For that, ask CanBeSeen.
 */
bool CanInteract(WorldObject const& a, WorldObject const& b)
{
    Geometry::Placement pa, pb;
    return a.IsInWorld() && b.IsInWorld() &&
           InCommonFrame(a, b, pa, pb) && pa.ShareFrame(pb);
}

/**
 * @brief CAN B BE SHOWN A -- a wider question, and a cheaper one.
 *
 * Wider than reach because a thing may be drawn without being touchable. In this core the
 * two coincide, since nothing here is measured in a frame it cannot also be reached in;
 * they are kept apart all the same, because every caller means one or the other and the
 * distinction is what the transport rework needs already drawn.
 */
bool CanBeSeen(WorldObject const& seen, WorldObject const& viewer)
{
    if (!seen.IsInWorld() || !viewer.IsInWorld())
    {
        return false;
    }

    if (seen.Where().ShareFrame(viewer.Where()))
    {
        return true;
    }

    if (Transport* aboard = Transport::VesselOf(seen))
    {
        if (aboard->GetMap() == viewer.GetMap() || aboard == &viewer)
        {
            return true;
        }
    }

    if (Transport* watching = Transport::VesselOf(viewer))
    {
        if (watching->GetMap() == seen.GetMap() || watching == &seen)
        {
            return true;
        }
    }

    return false;
}

/// The object a proximity question must be asked from. A passenger has no pose the shore
/// can measure against, so the vessel answers for him -- and its hull radius is added as
/// slack, because he may stand anywhere on it.
static WorldObject const& ProximityAnchor(WorldObject const& obj, float& slack)
{
    TransportMap* hull = obj.GetMap() ? obj.GetMap()->AsTransport() : NULL;
    Transport* vessel = hull ? hull->Vessel() : NULL;

    if (vessel)
    {
        slack += hull->HullRadius();
        return *vessel;
    }
    return obj;
}

bool SeenWithin(WorldObject const& seen, WorldObject const& viewer, float dist, bool is3D)
{
    if (!CanBeSeen(seen, viewer))
    {
        return false;
    }

    // Same frame -- the world's, or one deck's. Exact, so measure it and be done.
    if (seen.Where().ShareFrame(viewer.Where()))
    {
        return seen.Where().WithinDist(viewer.Where(), dist, is3D);
    }

    // Across a vessel's boundary. Whatever is aboard answers with its hull.
    float slack = 0.0f;
    WorldObject const& a = ProximityAnchor(seen, slack);
    WorldObject const& b = ProximityAnchor(viewer, slack);

    // One of them IS the anchor: he is standing on the very thing he is looking at.
    if (&a == &b)
    {
        return true;
    }

    return a.Where().WithinDist(b.Where(), dist + slack, is3D);
}

bool InReach(WorldObject const& a, WorldObject const& b, float dist, bool is3D)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.WithinDist(pb, dist, is3D);
}

bool InFrontPhased(WorldObject const& a, WorldObject const& b, float dist, float arc)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.IsInFront(pb, dist, arc);
}

bool InBackPhased(WorldObject const& a, WorldObject const& b, float dist, float arc)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.IsInBack(pb, dist, arc);
}

bool HasLineOfSight(WorldObject const& a, Geometry::Vector3 const& point)
{
    // The two-yard lift is eye height: a sight line is cast between heads, not feet.
    return a.GetMap()->IsInLineOfSight(a.Where().X(), a.Where().Y(), a.Where().Z() + 2.0f,
                                       point.x, point.y, point.z + 2.0f);
}

bool HasLineOfSight(WorldObject const& a, WorldObject const& b)
{
    if (!CanInteract(a, b))
    {
        return false;
    }

    // Aboard, the sight line is cast through the HULL's own geometry, on the ship's own map,
    // in the coordinates both passengers already speak.
    if (TransportMap* hull = a.GetMap() ? a.GetMap()->AsTransport() : NULL)
    {
        Geometry::Placement pa, pb;
        if (!InCommonFrame(a, b, pa, pb))
        {
            return false;
        }
        return !hull->IsBlocked(
            Geometry::Vector3(pa.X(), pa.Y(), pa.Z() + 2.0f),
            Geometry::Vector3(pb.X(), pb.Y(), pb.Z() + 2.0f));
    }

    return HasLineOfSight(a, b.Where().Pos());
}

bool IsPlaceable(WorldObject const& obj)
{
    return obj.Where().IsFinite() &&
           MaNGOS::IsValidMapCoord(obj.Where().X(), obj.Where().Y(),
                                   obj.Where().Z(), obj.Where().Facing());
}
