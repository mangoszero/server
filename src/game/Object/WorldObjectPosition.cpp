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
#include "VMapFactory.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"
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
 * @brief Relocate world object
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param orientation Orientation
 *
 * Moves the object to a new position and orientation.
 * Updates movement info for units.
 */
void WorldObject::Relocate(float x, float y, float z, float orientation)
{
    m_position.x = x;
    m_position.y = y;
    m_position.z = z;
    m_position.o = MapManager::NormalizeOrientation(orientation);

    if (isType(TYPEMASK_UNIT))
    {
        ((Unit*)this)->m_movementInfo.ChangePosition(x, y, z, orientation);
    }
}

/**
 * @brief Relocate world object (position only)
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 *
 * Moves the object to a new position without changing orientation.
 * Updates movement info for units.
 */
void WorldObject::Relocate(float x, float y, float z)
{
    m_position.x = x;
    m_position.y = y;
    m_position.z = z;

    if (isType(TYPEMASK_UNIT))
    {
        ((Unit*)this)->m_movementInfo.ChangePosition(x, y, z, GetOrientation());
    }
}

/**
 * @brief Set orientation
 * @param orientation New orientation
 *
 * Sets the object's orientation and updates movement info for units.
 */
void WorldObject::SetOrientation(float orientation)
{
    m_position.o = MapManager::NormalizeOrientation(orientation);

    if (isType(TYPEMASK_UNIT))
    {
        ((Unit*)this)->m_movementInfo.ChangeOrientation(orientation);
    }
}

/**
 * @brief Get zone ID
 * @return Zone ID
 *
 * Returns the zone ID based on the object's position.
 */
uint32 WorldObject::GetZoneId() const
{
    return GetMap()->GetTerrain()->GetZoneId(m_position.x, m_position.y, m_position.z);
}

/**
 * @brief Get area ID
 * @return Area ID
 *
 * Returns the area ID based on the object's position.
 */
uint32 WorldObject::GetAreaId() const
{
    return GetMap()->GetTerrain()->GetAreaId(m_position.x, m_position.y, m_position.z);
}

/**
 * @brief Get zone and area IDs
 * @param zoneid Output zone ID
 * @param areaid Output area ID
 *
 * Returns both zone and area IDs based on the object's position.
 */
void WorldObject::GetZoneAndAreaId(uint32& zoneid, uint32& areaid) const
{
    GetMap()->GetTerrain()->GetZoneAndAreaId(zoneid, areaid, m_position.x, m_position.y, m_position.z);
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
 * @brief Get distance to another object
 * @param obj Target object
 * @return Distance between objects
 *
 * Calculates the 3D distance between this object and another,
 * accounting for bounding radii.
 */
float WorldObject::GetDistance(const WorldObject* obj) const
{
    float dx = GetPositionX() - obj->GetPositionX();
    float dy = GetPositionY() - obj->GetPositionY();
    float dz = GetPositionZ() - obj->GetPositionZ();
    float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy) + (dz * dz)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

/**
 * @brief Get 2D distance to point
 * @param x X coordinate
 * @param y Y coordinate
 * @return 2D distance to point
 *
 * Calculates the 2D distance between this object and a point,
 * accounting for bounding radius.
 */
float WorldObject::GetDistance2d(float x, float y) const
{
    float dx = GetPositionX() - x;
    float dy = GetPositionY() - y;
    float sizefactor = GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

/**
 * @brief Get 3D distance to point
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @return 3D distance to point
 *
 * Calculates the 3D distance between this object and a point,
 * accounting for bounding radius.
 */
float WorldObject::GetDistance(float x, float y, float z) const
{
    float dx = GetPositionX() - x;
    float dy = GetPositionY() - y;
    float dz = GetPositionZ() - z;
    float sizefactor = GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy) + (dz * dz)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

/**
 * @brief Get 2D distance to another object
 * @param obj Target object
 * @return 2D distance to object
 *
 * Calculates the 2D distance between this object and another,
 * accounting for bounding radii.
 */
float WorldObject::GetDistance2d(const WorldObject* obj) const
{
    float dx = GetPositionX() - obj->GetPositionX();
    float dy = GetPositionY() - obj->GetPositionY();
    float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
    float dist = sqrt((dx * dx) + (dy * dy)) - sizefactor;
    return (dist > 0 ? dist : 0);
}

/**
 * @brief Get vertical distance to another object
 * @param obj Target object
 * @return Vertical distance to object
 *
 * Calculates the vertical (Z-axis) distance between this object
 * and another, accounting for bounding radii.
 */
float WorldObject::GetDistanceZ(const WorldObject* obj) const
{
    float dz = fabs(GetPositionZ() - obj->GetPositionZ());
    float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
    float dist = dz - sizefactor;
    return (dist > 0 ? dist : 0);
}

/**
 * @brief Check if within 3D distance of point
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param dist2compare Distance to compare against
 * @return True if within distance
 *
 * Checks if this object is within the specified 3D distance
 * of the given point.
 */
bool WorldObject::IsWithinDist3d(float x, float y, float z, float dist2compare) const
{
    float dx = GetPositionX() - x;
    float dy = GetPositionY() - y;
    float dz = GetPositionZ() - z;
    float distsq = dx * dx + dy * dy + dz * dz;

    float sizefactor = GetObjectBoundingRadius();
    float maxdist = dist2compare + sizefactor;

    return distsq < maxdist * maxdist;
}

/**
 * @brief Check if within 2D distance of point
 * @param x X coordinate
 * @param y Y coordinate
 * @param dist2compare Distance to compare against
 * @return True if within distance
 *
 * Checks if this object is within the specified 2D distance
 * of the given point.
 */
bool WorldObject::IsWithinDist2d(float x, float y, float dist2compare) const
{
    float dx = GetPositionX() - x;
    float dy = GetPositionY() - y;
    float distsq = dx * dx + dy * dy;

    float sizefactor = GetObjectBoundingRadius();
    float maxdist = dist2compare + sizefactor;

    return distsq < maxdist * maxdist;
}

/**
 * @brief Internal check if within distance of object
 * @param obj Target object
 * @param dist2compare Distance to compare against
 * @param is3D If true, check 3D distance; if false, check 2D
 * @return True if within distance
 *
 * Internal helper for distance checking with optional 3D.
 */
bool WorldObject::_IsWithinDist(WorldObject const* obj, float dist2compare, bool is3D) const
{
    float dx = GetPositionX() - obj->GetPositionX();
    float dy = GetPositionY() - obj->GetPositionY();
    float distsq = dx * dx + dy * dy;
    if (is3D)
    {
        float dz = GetPositionZ() - obj->GetPositionZ();
        distsq += dz * dz;
    }
    float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();
    float maxdist = dist2compare + sizefactor;

    return distsq < maxdist * maxdist;
}

/**
 * @brief Check if within line of sight in map
 * @param obj Target object
 * @return True if within line of sight
 *
 * Checks if this object has line of sight to the target object
 * within the same map.
 */
bool WorldObject::IsWithinLOSInMap(const WorldObject* obj) const
{
    if (!IsInMap(obj))
    {
        return false;
    }
    float ox, oy, oz;
    obj->GetPosition(ox, oy, oz);
    return(IsWithinLOS(ox, oy, oz));
}

/**
 * @brief Check if within line of sight to point
 * @param ox Target X coordinate
 * @param oy Target Y coordinate
 * @param oz Target Z coordinate
 * @return True if within line of sight
 *
 * Checks if this object has line of sight to the specified point.
 */
bool WorldObject::IsWithinLOS(float ox, float oy, float oz) const
{
    float x, y, z;
    GetPosition(x, y, z);
    return GetMap()->IsInLineOfSight(x, y, z + 2.0f, ox, oy, oz + 2.0f);
}

/**
 * @brief Compare distance order to two objects
 * @param obj1 First object
 * @param obj2 Second object
 * @param is3D If true, use 3D distance; if false, use 2D
 * @return True if obj1 is closer than obj2
 *
 * Compares distances to two objects to determine which is closer.
 */
bool WorldObject::GetDistanceOrder(WorldObject const* obj1, WorldObject const* obj2, bool is3D /* = true */) const
{
    float dx1 = GetPositionX() - obj1->GetPositionX();
    float dy1 = GetPositionY() - obj1->GetPositionY();
    float distsq1 = dx1 * dx1 + dy1 * dy1;
    if (is3D)
    {
        float dz1 = GetPositionZ() - obj1->GetPositionZ();
        distsq1 += dz1 * dz1;
    }

    float dx2 = GetPositionX() - obj2->GetPositionX();
    float dy2 = GetPositionY() - obj2->GetPositionY();
    float distsq2 = dx2 * dx2 + dy2 * dy2;
    if (is3D)
    {
        float dz2 = GetPositionZ() - obj2->GetPositionZ();
        distsq2 += dz2 * dz2;
    }

    return distsq1 < distsq2;
}

/**
 * @brief Check if within range of object
 * @param obj Target object
 * @param minRange Minimum distance
 * @param maxRange Maximum distance
 * @param is3D If true, use 3D distance; if false, use 2D
 * @return True if within range
 *
 * Checks if this object is within the specified distance range
 * of the target object.
 */
bool WorldObject::IsInRange(WorldObject const* obj, float minRange, float maxRange, bool is3D /* = true */) const
{
    float dx = GetPositionX() - obj->GetPositionX();
    float dy = GetPositionY() - obj->GetPositionY();
    float distsq = dx * dx + dy * dy;
    if (is3D)
    {
        float dz = GetPositionZ() - obj->GetPositionZ();
        distsq += dz * dz;
    }

    float sizefactor = GetObjectBoundingRadius() + obj->GetObjectBoundingRadius();

    // check only for real range
    if (minRange > 0.0f)
    {
        float mindist = minRange + sizefactor;
        if (distsq < mindist * mindist)
        {
            return false;
        }
    }

    float maxdist = maxRange + sizefactor;
    return distsq < maxdist * maxdist;
}

/**
 * @brief Check if within 2D range of point
 * @param x X coordinate
 * @param y Y coordinate
 * @param minRange Minimum distance
 * @param maxRange Maximum distance
 * @return True if within range
 *
 * Checks if this object is within the specified 2D distance range
 * of the target point.
 */
bool WorldObject::IsInRange2d(float x, float y, float minRange, float maxRange) const
{
    float dx = GetPositionX() - x;
    float dy = GetPositionY() - y;
    float distsq = dx * dx + dy * dy;

    float sizefactor = GetObjectBoundingRadius();

    // check only for real range
    if (minRange > 0.0f)
    {
        float mindist = minRange + sizefactor;
        if (distsq < mindist * mindist)
        {
            return false;
        }
    }

    float maxdist = maxRange + sizefactor;
    return distsq < maxdist * maxdist;
}

/**
 * @brief Check if within 3D range of point
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param minRange Minimum distance
 * @param maxRange Maximum distance
 * @return True if within range
 *
 * Checks if this object is within the specified 3D distance range
 * of the target point.
 */
bool WorldObject::IsInRange3d(float x, float y, float z, float minRange, float maxRange) const
{
    float dx = GetPositionX() - x;
    float dy = GetPositionY() - y;
    float dz = GetPositionZ() - z;
    float distsq = dx * dx + dy * dy + dz * dz;

    float sizefactor = GetObjectBoundingRadius();

    // check only for real range
    if (minRange > 0.0f)
    {
        float mindist = minRange + sizefactor;
        if (distsq < mindist * mindist)
        {
            return false;
        }
    }

    float maxdist = maxRange + sizefactor;
    return distsq < maxdist * maxdist;
}

/**
 * @brief Get angle to object
 * @param obj Target object
 * @return Angle in radians (0 to 2*PI)
 *
 * Returns the angle from this object to the target object.
 */
float WorldObject::GetAngle(const WorldObject* obj) const
{
    if (!obj)
    {
        return 0.0f;
    }

    // Rework the assert, when more cases where such a call can happen have been fixed
    // MANGOS_ASSERT(obj != this || PrintEntryError("GetAngle (for self)"));
    if (obj == this)
    {
        sLog.outError("INVALID CALL for GetAngle for %s", obj->GetGuidStr().c_str());
        return 0.0f;
    }
    return GetAngle(obj->GetPositionX(), obj->GetPositionY());
}

/**
 * @brief Get angle to point
 * @param x X coordinate
 * @param y Y coordinate
 * @return Angle in radians (0 to 2*PI)
 *
 * Returns the angle from this object to the specified point.
 */
float WorldObject::GetAngle(const float x, const float y) const
{
    float dx = x - GetPositionX();
    float dy = y - GetPositionY();

    float ang = atan2(dy, dx);                              // returns value between -Pi..Pi
    ang = (ang >= 0) ? ang : 2 * M_PI_F + ang;
    return ang;
}

/**
 * @brief Check if object is within arc
 * @param arcangle Arc angle in radians
 * @param obj Target object
 * @return True if object is within arc
 *
 * Checks if the target object is within the specified arc
 * in front of this object.
 */
bool WorldObject::HasInArc(const float arcangle, const WorldObject* obj) const
{
    // always have self in arc
    if (obj == this)
    {
        return true;
    }

    float arc = arcangle;

    // move arc to range 0.. 2*pi
    arc = MapManager::NormalizeOrientation(arc);

    float angle = GetAngle(obj);
    angle -= m_position.o;

    // move angle to range -pi ... +pi
    angle = MapManager::NormalizeOrientation(angle);
    if (angle > M_PI_F)
    {
        angle -= 2.0f * M_PI_F;
    }

    float lborder =  -1 * (arc / 2.0f);                     // in range -pi..0
    float rborder = (arc / 2.0f);                           // in range 0..pi
    return ((angle >= lborder) && (angle <= rborder));
}

/**
 * @brief Check if target is in front in same map
 * @param target Target object
 * @param distance Maximum distance
 * @param arc Arc angle in radians
 * @return True if target is in front
 *
 * Checks if the target is in front of this object within
 * the specified distance and arc, in the same map.
 */
bool WorldObject::IsInFrontInMap(WorldObject const* target, float distance,  float arc) const
{
    return IsWithinDistInMap(target, distance) && HasInArc(arc, target);
}

/**
 * @brief Check if target is in back in same map
 * @param target Target object
 * @param distance Maximum distance
 * @param arc Arc angle in radians
 * @return True if target is in back
 *
 * Checks if the target is behind this object within
 * the specified distance and arc, in the same map.
 */
bool WorldObject::IsInBackInMap(WorldObject const* target, float distance, float arc) const
{
    return IsWithinDistInMap(target, distance) && !HasInArc(2 * M_PI_F - arc, target);
}

/**
 * @brief Check if target is in front
 * @param target Target object
 * @param distance Maximum distance
 * @param arc Arc angle in radians
 * @return True if target is in front
 *
 * Checks if the target is in front of this object within
 * the specified distance and arc.
 */
bool WorldObject::IsInFront(WorldObject const* target, float distance,  float arc) const
{
    return IsWithinDist(target, distance) && HasInArc(arc, target);
}

/**
 * @brief Check if target is in back
 * @param target Target object
 * @param distance Maximum distance
 * @param arc Arc angle in radians
 * @return True if target is in back
 *
 * Checks if the target is behind this object within
 * the specified distance and arc.
 */
bool WorldObject::IsInBack(WorldObject const* target, float distance, float arc) const
{
    return IsWithinDist(target, distance) && !HasInArc(2 * M_PI_F - arc, target);
}

/**
 * @brief Get random point near position
 * @param x Center X coordinate
 * @param y Center Y coordinate
 * @param z Center Z coordinate
 * @param distance Maximum distance from center
 * @param rand_x Output random X coordinate
 * @param rand_y Output random Y coordinate
 * @param rand_z Output random Z coordinate
 * @param minDist Minimum distance from center
 * @param ori Optional orientation to use instead of random
 *
 * Generates a random point within the specified distance
 * of the center position.
 */
void WorldObject::GetRandomPoint(float x, float y, float z, float distance, float& rand_x, float& rand_y, float& rand_z, float minDist /*=0.0f*/, float const* ori /*=NULL*/) const
{
    if (distance == 0)
    {
        rand_x = x;
        rand_y = y;
        rand_z = z;
        return;
    }

    // angle to face `obj` to `this`
    float angle;
    if (!ori)
    {
        angle = rand_norm_f() * 2 * M_PI_F;
    }
    else
    {
        angle = *ori;
    }

    float new_dist;
    if (minDist == 0.0f)
    {
        new_dist = rand_norm_f() * distance;
    }
    else
    {
        new_dist = minDist + rand_norm_f() * (distance - minDist);
    }

    rand_x = x + new_dist * cos(angle);
    rand_y = y + new_dist * sin(angle);
    rand_z = z;

    MaNGOS::NormalizeMapCoord(rand_x);
    MaNGOS::NormalizeMapCoord(rand_y);
    UpdateGroundPositionZ(rand_x, rand_y, rand_z);          // update to LOS height if available
}

/**
 * @brief Update ground position Z
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z-coordinate to update
 *
 * Updates the Z-coordinate to the ground height at the
 * specified position.
 */
void WorldObject::UpdateGroundPositionZ(float x, float y, float& z) const
{
    float new_z = GetMap()->GetHeight(x, y, z);
    if (new_z > INVALID_HEIGHT)
    {
        z = new_z + 0.05f;                                   // just to be sure that we are not a few pixel under the surface
    }
}

/**
 * @brief Update allowed position Z
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z-coordinate to update
 * @param atMap Map to use for height calculation (optional)
 *
 * Updates the Z-coordinate to a valid height based on the
 * object's movement capabilities (flying, swimming, etc.).
 */
void WorldObject::UpdateAllowedPositionZ(float x, float y, float& z, Map* atMap /*=NULL*/) const
{
    if (!atMap)
    {
        atMap = GetMap();
    }

    switch (GetTypeId())
    {
        case TYPEID_UNIT:
        {
            // non fly unit don't must be in air
            // non swim unit must be at ground (mostly speedup, because it don't must be in water and water level check less fast
            if (!((Creature const*)this)->CanFly())
            {
                bool canSwim = ((Creature const*)this)->CanSwim() && ((Creature const*)this)->IsInWater();
                float ground_z = z;
                float max_z = canSwim
                    ? atMap->GetTerrain()->GetWaterOrGroundLevel(x, y, z, &ground_z, !((Unit const*)this)->HasAuraType(SPELL_AURA_WATER_WALK))
                    : ((ground_z = atMap->GetHeight(x, y, z)));
                if (max_z > INVALID_HEIGHT)
                {
                    if (z > max_z)
                    {
                        z = max_z;
                    }
                    else if (z < ground_z)
                    {
                        z = ground_z;
                    }
                }
            }
            else
            {
                float ground_z = atMap->GetHeight(x, y, z);
                if (z < ground_z)
                {
                    z = ground_z;
                }
            }
            break;
        }
        case TYPEID_PLAYER:
        {
            // for server controlled moves player work same as creature (but it can always swim)
            {
                float ground_z = z;
                float max_z = atMap->GetTerrain()->GetWaterOrGroundLevel(x, y, z, &ground_z, !((Unit const*)this)->HasAuraType(SPELL_AURA_WATER_WALK));
                if (max_z > INVALID_HEIGHT)
                {
                    if (z > max_z)
                    {
                        z = max_z;
                    }
                    else if (z < ground_z)
                    {
                        z = ground_z;
                    }
                }
            }
            break;
        }
        default:
        {
            float ground_z = atMap->GetHeight(x, y, z);
            if (ground_z > INVALID_HEIGHT)
            {
                z = ground_z;
            }
            break;
        }
    }
}

/**
 * @brief Check if position is valid
 * @return True if position is valid
 *
 * Checks if the object's current position is valid.
 */
bool WorldObject::IsPositionValid() const
{
    return MaNGOS::IsValidMapCoord(m_position.x, m_position.y, m_position.z, m_position.o);
}
