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
 * @brief Reset map
 *
 * Resets the map reference for this object.
 */


/**
 * @brief Add object to remove list
 *
 * Adds this object to the map's remove list for cleanup.
 */


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













#ifdef ENABLE_ELUNA

#endif /* ENABLE_ELUNA */
