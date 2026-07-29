/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

// The bodies behind ScriptApiCompat.inl. Compiled only when a script library is built, and
// every one of them forwards -- there is no geometry here, only translation.

#include "Object.h"

#ifdef MANGOS_SCRIPT_COMPAT

#include "Creature.h"
#include "GridMap.h"
#include "Map.h"
#include "Player.h"

float WorldObject::GetPositionX() const { return Where().X(); }
float WorldObject::GetPositionY() const { return Where().Y(); }
float WorldObject::GetPositionZ() const { return Where().Z(); }
float WorldObject::GetOrientation() const { return Where().Facing(); }

void WorldObject::GetPosition(float& x, float& y, float& z) const
{
    x = Where().X();
    y = Where().Y();
    z = Where().Z();
}

float WorldObject::GetObjectBoundingRadius() const { return Where().Extent(); }

bool WorldObject::IsInMap(WorldObject const* obj) const
{
    return obj && Where().ShareFrame(obj->Where());
}

float WorldObject::GetDistance(WorldObject const* obj) const
{
    return obj ? Where().DistanceTo(obj->Where()) : 0.0f;
}

float WorldObject::GetDistance(float x, float y, float z) const
{
    return Where().DistanceTo(Geometry::Vector3(x, y, z));
}

float WorldObject::GetDistance2d(WorldObject const* obj) const
{
    return obj ? Where().DistanceTo(obj->Where(), false) : 0.0f;
}

float WorldObject::GetDistance2d(float x, float y) const
{
    return Where().DistanceTo(Geometry::Vector2(x, y));
}

float WorldObject::GetDistanceZ(WorldObject const* obj) const
{
    return obj ? Where().HeightGapTo(obj->Where()) : 0.0f;
}

bool WorldObject::GetDistanceOrder(WorldObject const* obj1, WorldObject const* obj2,
                                   bool is3D) const
{
    return obj1 && obj2 && Where().IsNearer(obj1->Where(), obj2->Where(), is3D);
}

bool WorldObject::IsWithinDist3d(float x, float y, float z, float dist) const
{
    return Where().WithinDist(Geometry::Vector3(x, y, z), dist);
}

bool WorldObject::IsWithinDist2d(float x, float y, float dist) const
{
    return Where().WithinDist(Geometry::Vector2(x, y), dist);
}

bool WorldObject::IsWithinDist(WorldObject const* obj, float dist, bool is3D) const
{
    return obj && Where().WithinDist(obj->Where(), dist, is3D);
}

bool WorldObject::_IsWithinDist(WorldObject const* obj, float dist, bool is3D) const
{
    return obj && Where().WithinDist(obj->Where(), dist, is3D);
}

/// Was "same map AND in range". The frame check inside the component IS the map check, so
/// the two collapsed into one call and this now says the same thing twice.
bool WorldObject::IsWithinDistInMap(WorldObject const* obj, float dist, bool is3D) const
{
    return obj && InReach(*this, *obj, dist, is3D);
}

bool WorldObject::IsWithinLOS(float x, float y, float z) const
{
    return HasLineOfSight(*this, Geometry::Vector3(x, y, z));
}

bool WorldObject::IsWithinLOSInMap(WorldObject const* obj) const
{
    return obj && HasLineOfSight(*this, *obj);
}

float WorldObject::GetAngle(WorldObject const* obj) const
{
    return obj ? Where().BearingTo(obj->Where()) : 0.0f;
}

float WorldObject::GetAngle(float x, float y) const
{
    return Where().BearingTo(Geometry::Vector2(x, y));
}

bool WorldObject::HasInArc(float arc, WorldObject const* obj) const
{
    return obj && Where().HasInArc(obj->Where(), arc);
}

bool WorldObject::IsInFront(WorldObject const* obj, float dist, float arc) const
{
    return obj && InFrontPhased(*this, *obj, dist, arc);
}

bool WorldObject::IsInBack(WorldObject const* obj, float dist, float arc) const
{
    return obj && InBackPhased(*this, *obj, dist, arc);
}

bool WorldObject::IsInFrontInMap(WorldObject const* obj, float dist, float arc) const
{
    return obj && InFrontPhased(*this, *obj, dist, arc);
}

bool WorldObject::IsInBackInMap(WorldObject const* obj, float dist, float arc) const
{
    return obj && InBackPhased(*this, *obj, dist, arc);
}

bool WorldObject::IsInRange(WorldObject const* obj, float minRange, float maxRange,
                            bool is3D) const
{
    return obj && Where().WithinRange(obj->Where(), minRange, maxRange, is3D);
}

bool WorldObject::IsInRange2d(float x, float y, float minRange, float maxRange) const
{
    return Where().WithinRange(Geometry::Vector2(x, y), minRange, maxRange);
}

bool WorldObject::IsInRange3d(float x, float y, float z, float minRange,
                              float maxRange) const
{
    return Where().WithinRange(Geometry::Vector3(x, y, z), minRange, maxRange);
}

bool WorldObject::IsNearWaypoint(float x, float y, float z, float wpX, float wpY,
                                 float wpZ, float tolX, float tolY, float tolZ) const
{
    Geometry::Placement at;
    at.EnterFrame(Where().CurrentFrame(), Geometry::Vector3(x, y, z), 0.0f);
    return at.WithinBox(Geometry::Vector3(wpX, wpY, wpZ),
                        Geometry::Vector3(tolX, tolY, tolZ));
}

bool WorldObject::IsPositionValid() const { return IsPlaceable(*this); }

void WorldObject::UpdateGroundPositionZ(float x, float y, float& z) const
{
    DropToGround(*this, x, y, z);
}

void WorldObject::UpdateAllowedPositionZ(float x, float y, float& z, Map* atMap) const
{
    ClampToAllowedZ(*this, x, y, z, atMap);
}

void WorldObject::GetNearPoint2D(float& x, float& y, float distance2d, float absAngle) const
{
    const Geometry::Vector3 p = Where().PointAt(distance2d, absAngle);
    x = p.x;
    y = p.y;
}

void WorldObject::GetNearPoint(WorldObject const* searcher, float& x, float& y, float& z,
                               float searcherBounding, float distance2d,
                               float absAngle) const
{
    FindFreeSpotNear(*this, searcher, x, y, z, searcherBounding, distance2d, absAngle);
}

void WorldObject::GetClosePoint(float& x, float& y, float& z, float bounding,
                                float distance2d, float angle, WorldObject const* obj) const
{
    ClosePointNear(*this, x, y, z, bounding, distance2d, angle, obj);
}

void WorldObject::GetContactPoint(WorldObject const* obj, float& x, float& y, float& z,
                                  float distance2d) const
{
    ContactPointNear(*this, obj, x, y, z, distance2d);
}

void WorldObject::GetRandomPoint(float x, float y, float z, float distance,
                                 float& randX, float& randY, float& randZ,
                                 float minDist, float const* ori) const
{
    const Geometry::Vector3 p =
        RandomGroundPointNear(*this, Geometry::Vector3(x, y, z), distance, minDist, ori);
    randX = p.x;
    randY = p.y;
    randZ = p.z;
}

uint32 WorldObject::GetZoneId() const
{
    return GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z());
}

uint32 WorldObject::GetAreaId() const
{
    return GetTerrain()->GetAreaId(Where().X(), Where().Y(), Where().Z());
}

void WorldObject::GetZoneAndAreaId(uint32& zoneid, uint32& areaid) const
{
    GetTerrain()->GetZoneAndAreaId(zoneid, areaid, Where().X(), Where().Y(), Where().Z());
}

void WorldObject::Relocate(float x, float y, float z, float o) { Place().MoveTo(x, y, z, o); }
void WorldObject::Relocate(float x, float y, float z) { Place().MoveTo(x, y, z); }
void WorldObject::SetOrientation(float o) { Place().Face(o); }

#endif // MANGOS_SCRIPT_COMPAT
