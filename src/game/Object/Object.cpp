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
#include "ElunaConfig.h"
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Construct a new Object
 *
 * Initializes the object to a default state:
 * - Type set to TYPEID_OBJECT (base type)
 * - Type mask set to TYPEMASK_OBJECT
 * - Update fields array set to NULL (allocated by derived classes)
 * - Not in world, not marked for update
 *
 * @note Derived classes must call _InitValues() to allocate update fields
 */
Object::Object()
{
    m_objectTypeId      = TYPEID_OBJECT;
    m_objectType        = TYPEMASK_OBJECT;

    m_uint32Values      = NULL;
    m_valuesCount       = 0;

    m_inWorld           = false;
    m_objectUpdated     = false;
}

/**
 * @brief Destroy the Object
 *
 * Validates object state before destruction:
 * - Asserts that object is not in world (must be removed first)
 * - Asserts that object is not marked for update (must be cleared first)
 *
 * If either condition fails, an error is logged and the server asserts
 * to prevent memory corruption or undefined behavior.
 *
 * @warning Objects MUST be removed from world before destruction
 */
Object::~Object()
{
    if (IsInWorld())
    {
        ///- Do NOT call RemoveFromWorld here, if the object is a player it will crash
        sLog.outError("Object::~Object (GUID: %u TypeId: %u) deleted but still in world!!", GetGUIDLow(), GetTypeId());
        MANGOS_ASSERT(false);
    }

    if (m_objectUpdated)
    {
        sLog.outError("Object::~Object (GUID: %u TypeId: %u) deleted but still have updated status!!", GetGUIDLow(), GetTypeId());
        MANGOS_ASSERT(false);
    }

    delete[] m_uint32Values;
}

/**
 * @brief Initialize update field values array
 *
 * Allocates the uint32 array that stores all update field values
 * and initializes them to zero. Also initializes the changed values
 * tracking bitset.
 *
 * @note m_valuesCount must be set by derived class before calling
 * @note This should only be called once per object lifetime
 */
void Object::_InitValues()
{
    m_uint32Values = new uint32[ m_valuesCount ];
    memset(m_uint32Values, 0, m_valuesCount * sizeof(uint32));

    m_changedValues.resize(m_valuesCount, false);

    m_objectUpdated = false;
}

/**
 * @brief Create object with specific GUID
 * @param guidlow Low part of GUID (counter)
 * @param entry Entry ID from database (0 for objects without entry)
 * @param guidhigh High GUID type (item, creature, gameobject, etc.)
 *
 * Initializes the object's GUID and type. Creates the ObjectGuid
 * from components and stores it in update fields. Also sets up the
 * packed GUID for network transmission.
 *
 * @note This is the primary method for spawning new objects
 */
void Object::_Create(uint32 guidlow, uint32 entry, HighGuid guidhigh)
{
    if (!m_uint32Values)
    {
        _InitValues();
    }

    ObjectGuid guid = ObjectGuid(guidhigh, entry, guidlow);
    SetGuidValue(OBJECT_FIELD_GUID, guid);
    SetUInt32Value(OBJECT_FIELD_TYPE, m_objectType);
    m_PackGUID.Set(guid);
}

/**
 * @brief Recreate object with new entry
 * @param entry New entry ID
 *
 * Updates the object's entry field. Used when an object's type/entry
 * changes without destroying and recreating the object (e.g.,
 * creature respawns with different template).
 *
 * @note Preserves existing GUID, only changes entry
 */
void Object::_ReCreate(uint32 entry)
{
    if (!m_uint32Values)
    {
        _InitValues();
    }

    SetUInt32Value(OBJECT_FIELD_TYPE, m_objectType);
    SetUInt32Value(OBJECT_FIELD_ENTRY, entry);
}

/**
 * @brief Set object visual scale
 * @param newScale Scale factor (1.0 = normal size)
 *
 * Changes the object's visual scale. Affects how the object appears
 * in the game world. Scale changes are sent to all visible players.
 *
 * @note Values outside reasonable range may cause visual issues
 */
void Object::SetObjectScale(float newScale)
{
    SetFloatValue(OBJECT_FIELD_SCALE_X, newScale);
}


























/**
 * @brief Mark flag field for client update
 * @param index Field index
 *
 * Marks a flag field as changed and schedules client update.
 */
void Object::MarkFlagUpdateForClient(uint16 index)
{
    MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index, true));

    m_changedValues[index] = true;
    MarkForClientUpdate();
}

/**
 * @brief Force values update at index
 * @param index Field index
 *
 * Forces a field to be marked as changed and adds to
 * update list if object is in world.
 */
void Object::ForceValuesUpdateAtIndex(uint16 index)
{
    m_changedValues[index] = true;
    if (m_inWorld && !m_objectUpdated)
    {
        AddToClientUpdateList();
        m_objectUpdated = true;
    }
}














/**
 * @brief WorldObject constructor
 *
 * Initializes a new WorldObject with default values.
 */
WorldObject::WorldObject() :
#ifdef ENABLE_ELUNA
elunaEvents(nullptr),
#endif /* ENABLE_ELUNA */
    m_currMap(NULL),
    m_mapId(0), m_InstanceId(0),
    m_isActiveObject(false),
    m_visibilityDistanceOverride(0.0f)
{
}

/**
 * @brief WorldObject destructor
 *
 * Cleans up Eluna events if enabled.
 */
WorldObject::~WorldObject()
{
#ifdef ENABLE_ELUNA
    delete elunaEvents;
    elunaEvents = nullptr;
#endif /* ENABLE_ELUNA */
}

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






/**
 * @brief Send localized text around source
 * @param source Source object
 * @param textData Localized text data
 * @param msgtype Chat message type
 * @param language Language
 * @param target Target unit
 * @param range Range to send message
 *
 * Helper function to create localized chat around a source.
 */


/**
 * @brief Send monster text
 * @param textData Localized text data
 * @param target Target unit
 *
 * Sends a text message associated with a MangosString,
 * localized for each player's locale.
 */


/**
 * @brief Send message to set
 * @param data Packet to send
 * @param bToSelf If true, send to self (unused)
 *
 * Broadcasts a packet to all players who can see this object.
 */


/**
 * @brief Send message to set in range
 * @param data Packet to send
 * @param dist Maximum distance
 * @param bToSelf If true, send to self (unused)
 *
 * Broadcasts a packet to all players within the specified distance
 * who can see this object.
 */


/**
 * @brief Send message to set except receiver
 * @param data Packet to send
 * @param skipped_receiver Player to skip
 *
 * Broadcasts a packet to all players who can see this object
 * except the specified player.
 */


/**
 * @brief Send object despawn animation
 * @param guid GUID of object to despawn
 *
 * Sends a despawn animation packet for the specified object
 * to all nearby players.
 */


/**
 * @brief Set map for object
 * @param map Map to set
 *
 * Sets the map for this object and updates map ID and instance ID.
 */

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
}

/**
 * @brief Reset map
 *
 * Resets the map reference for this object.
 */

/**
 * @brief Resets the world object's map state.
 */
void WorldObject::ResetMap()
{
}

/**
 * @brief Add object to remove list
 *
 * Adds this object to the map's remove list for cleanup.
 */

/**
 * @brief Schedules the object for removal from the map.
 */
void WorldObject::AddObjectToRemoveList()
{
    GetMap()->AddObjectToRemoveList(this);
}

/**
 * @brief Summon creature
 * @param id Creature entry ID
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param ang Orientation
 * @param spwtype Temporary spawn type
 * @param despwtime Despawn time
 * @param asActiveObject If true, set as active object
 * @param setRun If true, set run mode
 * @return Summoned creature pointer or NULL
 *
 * Summons a creature at the specified position.
 */

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
        pos = CreatureCreatePos(this, GetOrientation(), CONTACT_DISTANCE, ang);
    }

    if (!pCreature->Create(GetMap()->GenerateLocalLowGuid(cinfo->GetHighGuid()), pos, cinfo, team))
    {
        delete pCreature;
        return NULL;
    }

    pCreature->SetRespawnCoord(pos);

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
 * @brief Summon game object
 * @param id Game object entry ID
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param angle Orientation
 * @param despwtime Despawn time in milliseconds
 * @return Summoned game object pointer or NULL
 *
 * Summons a game object at the specified position.
 */

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
                : i_object(obj), i_searcher(searcher), i_absAngle(MapManager::NormalizeOrientation(absAngle)), i_selector(selector) {}

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
                    x = c->GetPositionX();
                    y = c->GetPositionY();
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

                x = u->GetPositionX();
                y = u->GetPositionY();

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
                float dx = i_object.GetPositionX() - x;
                float dy = i_object.GetPositionY() - y;
                float dist2d = sqrt((dx * dx) + (dy * dy));

                // It is ok for the objects to require a bit more space
                float delta = u->GetObjectBoundingRadius();
                if (i_selector.m_searchPosFor && i_selector.m_searchPosFor != u)
                {
                    delta += i_selector.m_searchPosFor->GetObjectBoundingRadius();
                }

                delta *= OCCUPY_POS_DEPTH_FACTOR;           // Increase by factor

                // u is too near/far away from i_object. Do not consider it to occupy space
                if (fabs(i_selector.m_searcherDist - dist2d) > delta)
                {
                    return;
                }

                float angle = i_object.GetAngle(u) - i_absAngle;

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

//===================================================================================================

/**
 * @brief Get 2D point near object
 * @param x Output X coordinate
 * @param y Output Y coordinate
 * @param distance2d Distance from object
 * @param absAngle Absolute angle
 *
 * Calculates a 2D point at the specified distance and angle
 * from this object.
 */

/**
 * @brief Computes a 2D point at a given distance and angle from the object.
 *
 * @param x Receives the resulting x coordinate.
 * @param y Receives the resulting y coordinate.
 * @param distance2d The radial distance.
 * @param absAngle The absolute angle.
 */
void WorldObject::GetNearPoint2D(float& x, float& y, float distance2d, float absAngle) const
{
    x = GetPositionX() + distance2d * cos(absAngle);
    y = GetPositionY() + distance2d * sin(absAngle);

    MaNGOS::NormalizeMapCoord(x);
    MaNGOS::NormalizeMapCoord(y);
}

/**
 * @brief Get point near object with collision detection
 * @param searcher Object searching for position
 * @param x Output X coordinate
 * @param y Output Y coordinate
 * @param z Output Z coordinate
 * @param searcher_bounding_radius Bounding radius of searcher
 * @param distance2d Distance from object
 * @param absAngle Absolute angle
 *
 * Calculates a point at the specified distance and angle
 * from this object, accounting for collision detection.
 */

/**
 * @brief Finds a nearby point while accounting for collisions and line of sight.
 *
 * @param searcher The object requesting the position.
 * @param x Receives the resulting x coordinate.
 * @param y Receives the resulting y coordinate.
 * @param z Receives the resulting z coordinate.
 * @param searcher_bounding_radius The requester's bounding radius.
 * @param distance2d The desired distance from this object.
 * @param absAngle The preferred absolute angle.
 */
void WorldObject::GetNearPoint(WorldObject const* searcher, float& x, float& y, float& z, float searcher_bounding_radius, float distance2d, float absAngle) const
{
    GetNearPoint2D(x, y, distance2d, absAngle);
    const float init_z = z = GetPositionZ();

    // if detection disabled, return first point
    if (!sWorld.getConfig(CONFIG_BOOL_DETECT_POS_COLLISION))
    {
        if (searcher)
        {
            searcher->UpdateAllowedPositionZ(x, y, z, GetMap());       // update to LOS height if available
        }
        else
        {
            UpdateGroundPositionZ(x, y, z);
        }
        return;
    }

    // or remember first point
    float first_x = x;
    float first_y = y;
    bool first_los_conflict = false;                        // first point LOS problems

    const float dist = distance2d + searcher_bounding_radius + GetObjectBoundingRadius();

    // prepare selector for work
    ObjectPosSelector selector(GetPositionX(), GetPositionY(), distance2d, searcher_bounding_radius, searcher);

    // adding used positions around object
    {
        MaNGOS::NearUsedPosDo u_do(*this, searcher, absAngle, selector);
        MaNGOS::WorldObjectWorker<MaNGOS::NearUsedPosDo> worker(u_do);

        Cell::VisitAllObjects(this, worker, dist);
    }

    // maybe can just place in primary position
    if (selector.CheckOriginalAngle())
    {
        if (searcher)
        {
            searcher->UpdateAllowedPositionZ(x, y, z, GetMap());       // update to LOS height if available
        }
        else
        {
            UpdateGroundPositionZ(x, y, z);
        }

        if (fabs(init_z - z) < dist && IsWithinLOS(x, y, z))
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
        GetNearPoint2D(x, y, distance2d, absAngle + angle);
        z = GetPositionZ();

        if (searcher)
        {
            searcher->UpdateAllowedPositionZ(x, y, z, GetMap());       // update to LOS height if available
        }
        else
        {
            UpdateGroundPositionZ(x, y, z);
        }

        if (fabs(init_z - z) < dist && IsWithinLOS(x, y, z))
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
            searcher->UpdateAllowedPositionZ(x, y, z, GetMap());       // update to LOS height if available
        }
        else
        {
            UpdateGroundPositionZ(x, y, z);
        }
        return;
    }

    // set first used pos in lists
    selector.InitializeAngle();

    // select in positions after current nodes (selection one by one)
    while (selector.NextUsedAngle(angle))                   // angle for used pos but maybe without LOS problem
    {
        GetNearPoint2D(x, y, distance2d, absAngle + angle);
        z = GetPositionZ();

        if (searcher)
        {
            searcher->UpdateAllowedPositionZ(x, y, z, GetMap());       // update to LOS height if available
        }
        else
        {
            UpdateGroundPositionZ(x, y, z);
        }

        if (fabs(init_z - z) < dist && IsWithinLOS(x, y, z))
        {
            return;
        }
    }

    // BAD BAD NEWS: all found pos (free and used) have LOS problem :(
    x = first_x;
    y = first_y;

    if (searcher)
    {
        searcher->UpdateAllowedPositionZ(x, y, z, GetMap());           // update to LOS height if available
    }
    else
    {
        UpdateGroundPositionZ(x, y, z);
    }
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
    CellPair p = MaNGOS::ComputeCellPair(GetPositionX(), GetPositionY());
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
