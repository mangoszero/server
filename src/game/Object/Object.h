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

#ifndef MANGOS_H_OBJECT
#define MANGOS_H_OBJECT

#include <unordered_map>
#include "Utilities/Errors.h"
#include "Platform/Define.h"
#include "Utilities/MathDefines.h"
#include <ctime>
#include <vector>
#include <string>
#include <map>
#include "ByteBuffer.h"
#include "UpdateFields.h"
#include "UpdateData.h"
#include "ObjectGuid.h"
#include "Camera.h"
#include "GameTime.h"
#include "Geometry/Placement.h"
#ifdef ENABLE_ELUNA
#include "LuaValue.h"
#endif /* ENABLE_ELUNA */

#include <set>

#define CONTACT_DISTANCE            0.5f
#define INTERACTION_DISTANCE        5.0f
#define ATTACK_DISTANCE             5.0f
#define TRADE_DISTANCE              11.11f                  // max distance for trading
#define MAX_VISIBILITY_DISTANCE     333.0f                  // max distance for visible object show, limited in 333 yards
#define DEFAULT_VISIBILITY_DISTANCE 90.0f                   // default visible distance, 90 yards on continents
#define DEFAULT_VISIBILITY_INSTANCE 120.0f                  // default visible distance in instances, 120 yards
#define DEFAULT_VISIBILITY_BGARENAS 180.0f                  // default visible distance in BG/Arenas, 180 yards

#define DEFAULT_WORLD_OBJECT_SIZE   0.388999998569489f      // currently used (correctly?) for any non Unit world objects. This is actually the bounding_radius, like player/creature from creature_model_data
#define DEFAULT_OBJECT_SCALE        1.0f                    // non-Tauren player/item scale as default, npc/go from database, pets from dbc
#define DEFAULT_TAUREN_MALE_SCALE   1.35f                   // Tauren male player scale by default
#define DEFAULT_TAUREN_FEMALE_SCALE 1.25f                   // Tauren female player scale by default

#define MAX_STEALTH_DETECT_RANGE    45.0f

// How far a deck map extends from its origin. A deck map is the hull, so its bounds are
// the hull's. Used only when the vessel cannot be resolved and its real extent read; the
// job is to reject the absolute continent coordinates a leaving zeppelin sometimes reports.
#define MAX_DECK_EXTENT             250.0f
#define DECK_EDGE_MARGIN            10.0f

/**
 * @brief Temporary spawn type enumeration
 *
 * Defines when and how temporary spawns should despawn.
 */
enum TempSpawnType
{
    TEMPSPAWN_MANUAL_DESPAWN = 0,             ///< Despawns when UnSummon() is called
    TEMPSPAWN_DEAD_DESPAWN = 1,               ///< Despawns when the creature disappears
    TEMPSPAWN_CORPSE_DESPAWN = 2,             ///< Despawns instantly after death
    TEMPSPAWN_CORPSE_TIMED_DESPAWN = 3,       ///< Despawns after a specified time after death (or when the creature disappears)
    TEMPSPAWN_TIMED_DESPAWN = 4,              ///< Despawns after a specified time
    TEMPSPAWN_TIMED_OOC_DESPAWN = 5,          ///< Despawns after a specified time after the creature is out of combat
    TEMPSPAWN_TIMED_OR_DEAD_DESPAWN = 6,      ///< Despawns after a specified time OR when the creature disappears
    TEMPSPAWN_TIMED_OR_CORPSE_DESPAWN = 7,    ///< Despawns after a specified time OR when the creature dies
    TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN = 8,  ///< Despawns after a specified time (OOC) OR when the creature disappears
    TEMPSPAWN_TIMED_OOC_OR_CORPSE_DESPAWN = 9 ///< Despawns after a specified time (OOC) OR when the creature dies
};

class WorldPacket;
class UpdateData;
class WorldSession;
class Creature;
class GameObject;
class Player;
class Unit;
class Group;
class Map;
class UpdateMask;
class InstanceData;
class TerrainInfo;
#ifdef ENABLE_ELUNA
class Eluna;
class ElunaEventProcessor;
class LuaVal;
#endif /* ENABLE_ELUNA */
struct MangosStringLocale;

typedef std::unordered_map<Player*, UpdateData> UpdateDataMapType;

/**
 * @brief Position structure
 *
 * Stores 3D position coordinates and orientation.
 */
struct Position
{

    /**
     * @brief Default constructor
     */
    Position() : x(0.0f), y(0.0f), z(0.0f), o(0.0f) {}
    Position(float _x, float _y, float _z, float _o) : x(_x), y(_y), z(_z), o(_o) {}

    float x; ///< X-coordinate
    float y; ///< Y-coordinate
    float z; ///< Z-coordinate
    float o; ///< Orientation (radians)
};

/**
 * @brief World location structure
 *
 * Stores map ID and position coordinates.
 */
struct WorldLocation
{
    uint32 mapid; ///< Map ID
    float coord_x; ///< X-coordinate
    float coord_y; ///< Y-coordinate
    float coord_z; ///< Z-coordinate
    float orientation; ///< Orientation (radians)

    /**
     * @brief Constructor with parameters
     * @param _mapid Map ID
     * @param _x X-coordinate
     * @param _y Y-coordinate
     * @param _z Z-coordinate
     * @param _o Orientation
     */
    explicit WorldLocation(uint32 _mapid = 0, float _x = 0, float _y = 0, float _z = 0, float _o = 0)
        : mapid(_mapid), coord_x(_x), coord_y(_y), coord_z(_z), orientation(_o) {}

    /**
     * @brief Copy constructor
     * @param loc Source location
     */
    WorldLocation(WorldLocation const& loc)
        : mapid(loc.mapid), coord_x(loc.coord_x), coord_y(loc.coord_y), coord_z(loc.coord_z), orientation(loc.orientation) {}
};

/**
 * @brief World update counter
 *
 * Measures time between world update ticks.
 * Essential for units updating their spells after cells become active.
 */
class WorldUpdateCounter
{
    public:
        /**
         * @brief Constructor
         */
        WorldUpdateCounter() : m_tmStart(0) {}

        /**
         * @brief Get elapsed time since start
         * @return Elapsed time in milliseconds
         */
        time_t timeElapsed()
        {
            if (!m_tmStart)
            {
                m_tmStart = GameTime::GetGameTimeMS();
            }

            return getMSTimeDiff(m_tmStart, GameTime::GetGameTimeMS());
        }

        /**
         * @brief Reset the counter
         */
        void Reset()
        {
            m_tmStart = GameTime::GetGameTimeMS();
        }

    private:
        uint32 m_tmStart; ///< Start time in milliseconds
};

/**
 * @brief Base class for all objects in the MaNGOS world
 *
 * The Object class is the fundamental base class for all entities that exist
 * in the game world, including players, creatures, game objects, items, etc.
 * It provides core functionality for GUID management, update fields, and world state.
 *
 * This class handles:
 * - Object identification and GUID management
 * - Update field system for client synchronization
 * - World state management (in/out of world)
 * - Type casting helpers for safe downcasting
 * - Value accessors for different data types
 *
 * @note This is an abstract base class and should not be instantiated directly
 * @note All derived classes must implement virtual methods appropriately
 */
class Object
{
    public:
        /**
         * @brief Virtual destructor for proper cleanup of derived classes
         */
        virtual ~Object();

        /**
         * @brief Check if object is currently in the game world
         * @return true if object is in world, false otherwise
         */
        const bool& IsInWorld() const { return m_inWorld; }

        /**
         * @brief Add object to the game world
         *
         * This method initializes the object's world state and prepares it for
         * client updates. Should be called when object becomes active in world.
         *
         * @note If object is already in world, this method does nothing
         * @note Clears update mask to prevent sending stale data
         */
        virtual void AddToWorld()
        {
            if (m_inWorld)
            {
                return;
            }

            m_inWorld = true;

            // synchronize values mirror with values array (changes will send in updatecreate opcode any way
            ClearUpdateMask(false);                         // false - we can't have update data in update queue before adding to world
        }

        /**
         * @brief Remove object from the game world
         *
         * This method cleans up the object's world state and prevents further
         * client updates. Should be called when object becomes inactive.
         *
         * @note Clears update mask to prevent sending updates after removal
         */
        virtual void RemoveFromWorld()
        {
            // if we remove from world then sending changes not required
            ClearUpdateMask(true);
            m_inWorld = false;
        }

        /**
         * @brief Get the object's unique GUID
         * @return Reference to the object's GUID
         */
        ObjectGuid const& GetObjectGuid() const { return GetGuidValue(OBJECT_FIELD_GUID); }

        /**
         * @brief Get the low part of the object's GUID
         * @return Low 32 bits of the GUID counter
         */
        uint32 GetGUIDLow() const { return GetObjectGuid().GetCounter(); }

        /**
         * @brief Get the packed GUID representation
         * @return Reference to packed GUID for network transmission
         */
        PackedGuid const& GetPackGUID() const { return m_PackGUID; }

        /**
         * @brief Get the GUID as a string
         * @return String representation of the GUID
         */
        std::string GetGuidStr() const { return GetObjectGuid().GetString(); }

        /**
         * @brief Get the object's entry ID from DBC
         * @return Entry ID from appropriate DBC file
         */
        uint32 GetEntry() const { return GetUInt32Value(OBJECT_FIELD_ENTRY); }

        /**
         * @brief Set the object's entry ID
         * @param entry Entry ID from DBC file
         */
        void SetEntry(uint32 entry) { SetUInt32Value(OBJECT_FIELD_ENTRY, entry); }

        float GetObjectScale() const
        {
            return m_floatValues[OBJECT_FIELD_SCALE_X] ? m_floatValues[OBJECT_FIELD_SCALE_X] : DEFAULT_OBJECT_SCALE;
        }

        void SetObjectScale(float newScale);

        /// Scale feeds the spatial extent of anything that has one. A hook, not a
        /// downcast: Object must not learn that WorldObject exists.
        virtual void OnScaleChanged() {}

        uint8 GetTypeId() const { return m_objectTypeId; }
        bool isType(TypeMask mask) const { return (mask & m_objectType); }

        virtual void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const;
        void SendCreateUpdateToPlayer(Player* player);

        // must be overwrite in appropriate subclasses (WorldObject, Item currently), or will crash
        virtual void AddToClientUpdateList();
        virtual void RemoveFromClientUpdateList();
        virtual void BuildUpdateData(UpdateDataMapType& update_players);
        void MarkForClientUpdate();
        void SendForcedObjectUpdate();

        void BuildValuesUpdateBlockForPlayer(UpdateData* data, Player* target) const;
        void BuildOutOfRangeUpdateBlock(UpdateData* data) const;

        virtual void DestroyForPlayer(Player* target) const;

        const int32& GetInt32Value(uint16 index) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            return m_int32Values[ index ];
        }

        const uint32& GetUInt32Value(uint16 index) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            return m_uint32Values[ index ];
        }

        const uint64& GetUInt64Value(uint16 index) const
        {
            MANGOS_ASSERT(index + 1 < m_valuesCount || PrintIndexError(index , false));
            return *((uint64*) & (m_uint32Values[ index ]));
        }

        const float& GetFloatValue(uint16 index) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            return m_floatValues[ index ];
        }

        uint8 GetByteValue(uint16 index, uint8 offset) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            MANGOS_ASSERT(offset < 4);
            return *(((uint8*)&m_uint32Values[ index ]) + offset);
        }

        uint16 GetUInt16Value(uint16 index, uint8 offset) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            MANGOS_ASSERT(offset < 2);
            return *(((uint16*)&m_uint32Values[ index ]) + offset);
        }

        ObjectGuid const& GetGuidValue(uint16 index) const { return *reinterpret_cast<ObjectGuid const*>(&GetUInt64Value(index)); }

        Player* ToPlayer()
        {
            if (GetTypeId() == TYPEID_PLAYER)
            {
                return reinterpret_cast<Player*>(this);
            }
            else
            {
                return NULL;
            }
        }

        Player const* ToPlayer() const
        {
            if (GetTypeId() == TYPEID_PLAYER)
            {
                return reinterpret_cast<Player const*>(this);
            }
            else
            {
                return NULL;
            }
        }

        Creature* ToCreature()
        {
            if (GetTypeId() == TYPEID_UNIT)
            {
                return reinterpret_cast<Creature*>(this);
            }
            else
            {
                return NULL;
            }
        }

        Creature const* ToCreature() const
        {
            if (GetTypeId() == TYPEID_UNIT)
            {
                return reinterpret_cast<Creature const*>(this);
            }
            else
            {
                return NULL;
            }
        }

        Unit* ToUnit()
        {
            if (isType(TYPEMASK_UNIT))
            {
                return reinterpret_cast<Unit*>(this);
            }
            else
            {
                return NULL;
            }
        }

        Unit const* ToUnit() const
        {
            if (isType(TYPEMASK_UNIT))
            {
                return reinterpret_cast<Unit const*>(this);
            }
            else
            {
                return NULL;
            }
        }

        GameObject* ToGameObject()
        {
            if (GetTypeId() == TYPEID_GAMEOBJECT)
            {
                return reinterpret_cast<GameObject*>(this);
            }
            else
            {
                return NULL;
            }
        }

        GameObject const* ToGameObject() const
        {
            if (GetTypeId() == TYPEID_GAMEOBJECT)
            {
                return reinterpret_cast<GameObject const*>(this);
            }
            else
            {
                return NULL;
            }
        }

        Corpse* ToCorpse()
        {
            if (GetTypeId() == TYPEID_CORPSE)
            {
                return reinterpret_cast<Corpse*>(this);
            }
            else
            {
                return NULL;
            }
        }

        Corpse const* ToCorpse() const
        {
            if (GetTypeId() == TYPEID_CORPSE)
            {
                return reinterpret_cast<Corpse const*>(this);
            }
            else
            {
                return NULL;
            }
        }

        DynamicObject* ToDynObject()
        {
            if (GetTypeId() == TYPEID_DYNAMICOBJECT)
            {
                return reinterpret_cast<DynamicObject*>(this);
            }
            else
            {
                return NULL;
            }
        }

        DynamicObject const* ToDynObject() const
        {
            if (GetTypeId() == TYPEID_DYNAMICOBJECT)
            {
                return reinterpret_cast<DynamicObject const*>(this);
            }
            else
            {
                return NULL;
            }
        }

        void SetInt32Value(uint16 index,        int32  value);
        void SetUInt32Value(uint16 index,       uint32  value);
        void UpdateUInt32Value(uint16 index,    uint32  value);
        void SetUInt64Value(uint16 index, const uint64& value);
        void SetFloatValue(uint16 index,       float   value);
        void SetByteValue(uint16 index, uint8 offset, uint8 value);
        void SetUInt16Value(uint16 index, uint8 offset, uint16 value);
        void SetInt16Value(uint16 index, uint8 offset, int16 value) { SetUInt16Value(index, offset, (uint16)value); }
        void SetGuidValue(uint16 index, ObjectGuid const& value) { SetUInt64Value(index, value.GetRawValue()); }
        void SetStatFloatValue(uint16 index, float value);
        void SetStatInt32Value(uint16 index, int32 value);
        void ForceValuesUpdateAtIndex(uint16 index);
        void ApplyModUInt32Value(uint16 index, int32 val, bool apply);
        void ApplyModInt32Value(uint16 index, int32 val, bool apply);
        void ApplyModPositiveFloatValue(uint16 index, float val, bool apply);
        void ApplyModSignedFloatValue(uint16 index, float val, bool apply);

        void ApplyPercentModFloatValue(uint16 index, float val, bool apply)
        {
            val = val != -100.0f ? val : -99.9f ;
            SetFloatValue(index, GetFloatValue(index) * (apply ? (100.0f + val) / 100.0f : 100.0f / (100.0f + val)));
        }

        /**
         * method to force the update of a given flag to the client. The method is checking the index before indicating the flags need an update.
         *
         * \param index uint16 of the flag to be updated.
         */
        void MarkFlagUpdateForClient(uint16 index);
        void SetFlag(uint16 index, uint32 newFlag);
        void RemoveFlag(uint16 index, uint32 oldFlag);

        void ToggleFlag(uint16 index, uint32 flag)
        {
            if (HasFlag(index, flag))
            {
                RemoveFlag(index, flag);
            }
            else
            {
                SetFlag(index, flag);
            }
        }

        /**
         * Checks if a certain flag is set.
         * @param index The index to check, values may originate from at least \ref EUnitFields
         * @param flag Which flag to check, value may originate from a lot of places, see code
         * for examples of what
         * @return true if the flag is set, false otherwise
         * \todo More info on these flags and where they come from, also, which indexes can be used?
         */
        bool HasFlag(uint16 index, uint32 flag) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            return (m_uint32Values[ index ] & flag) != 0;
        }

        void ApplyModFlag(uint16 index, uint32 flag, bool apply)
        {
            if (apply)
            {
                SetFlag(index, flag);
            }
            else
            {
                RemoveFlag(index, flag);
            }
        }

        void SetByteFlag(uint16 index, uint8 offset, uint8 newFlag);
        void RemoveByteFlag(uint16 index, uint8 offset, uint8 newFlag);

        void ToggleByteFlag(uint16 index, uint8 offset, uint8 flag)
        {
            if (HasByteFlag(index, offset, flag))
            {
                RemoveByteFlag(index, offset, flag);
            }
            else
            {
                SetByteFlag(index, offset, flag);
            }
        }

        bool HasByteFlag(uint16 index, uint8 offset, uint8 flag) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            MANGOS_ASSERT(offset < 4);
            return (((uint8*)&m_uint32Values[index])[offset] & flag) != 0;
        }

        void ApplyModByteFlag(uint16 index, uint8 offset, uint32 flag, bool apply)
        {
            if (apply)
            {
                SetByteFlag(index, offset, flag);
            }
            else
            {
                RemoveByteFlag(index, offset, flag);
            }
        }

        void SetShortFlag(uint16 index, bool highpart, uint16 newFlag);
        void RemoveShortFlag(uint16 index, bool highpart, uint16 oldFlag);

        void ToggleShortFlag(uint16 index, bool highpart, uint8 flag)
        {
            if (HasShortFlag(index, highpart, flag))
            {
                RemoveShortFlag(index, highpart, flag);
            }
            else
            {
                SetShortFlag(index, highpart, flag);
            }
        }

        bool HasShortFlag(uint16 index, bool highpart, uint8 flag) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            return (((uint16*)&m_uint32Values[index])[highpart ? 1 : 0] & flag) != 0;
        }

        void ApplyModShortFlag(uint16 index, bool highpart, uint32 flag, bool apply)
        {
            if (apply)
            {
                SetShortFlag(index, highpart, flag);
            }
            else
            {
                RemoveShortFlag(index, highpart, flag);
            }
        }

        void SetFlag64(uint16 index, uint64 newFlag)
        {
            uint64 oldval = GetUInt64Value(index);
            uint64 newval = oldval | newFlag;
            SetUInt64Value(index, newval);
        }

        void RemoveFlag64(uint16 index, uint64 oldFlag)
        {
            uint64 oldval = GetUInt64Value(index);
            uint64 newval = oldval & ~oldFlag;
            SetUInt64Value(index, newval);
        }

        void ToggleFlag64(uint16 index, uint64 flag)
        {
            if (HasFlag64(index, flag))
            {
                RemoveFlag64(index, flag);
            }
            else
            {
                SetFlag64(index, flag);
            }
        }

        bool HasFlag64(uint16 index, uint64 flag) const
        {
            MANGOS_ASSERT(index < m_valuesCount || PrintIndexError(index , false));
            return (GetUInt64Value(index) & flag) != 0;
        }

        void ApplyModFlag64(uint16 index, uint64 flag, bool apply)
        {
            if (apply)
            {
                SetFlag64(index, flag);
            }
            else
            {
                RemoveFlag64(index, flag);
            }
        }

        void ClearUpdateMask(bool remove);

        bool LoadValues(const char* data);

        uint16 GetValuesCount() const { return m_valuesCount; }

        virtual bool HasQuest(uint32 /* quest_id */) const { return false; }
        virtual bool HasInvolvedQuest(uint32 /* quest_id */) const { return false; }
        void _ReCreate(uint32 entry);
        void SetAsNewObject(bool isNew) { m_isNewObject = isNew; }

    protected:
        Object();

        void _InitValues();
        void _Create(uint32 guidlow, uint32 entry, HighGuid guidhigh);

        virtual void _SetUpdateBits(UpdateMask* updateMask, Player* target) const;

        virtual void _SetCreateBits(UpdateMask* updateMask, Player* target) const;

        void BuildMovementUpdate(ByteBuffer* data, uint8 updateFlags) const;
        void BuildValuesUpdate(uint8 updatetype, ByteBuffer* data, UpdateMask* updateMask, Player* target) const;
        void BuildUpdateDataForPlayer(Player* pl, UpdateDataMapType& update_players);

        uint16 m_objectType;

        uint8 m_objectTypeId;
        uint8 m_updateFlag;

        union
        {
            int32*  m_int32Values;
            uint32* m_uint32Values;
            float*  m_floatValues;
        };

        std::vector<bool> m_changedValues;
        std::map<uint32, uint32> m_plrSpecificFlags;

        uint16 m_valuesCount;

        bool m_objectUpdated;

    private:
        bool m_inWorld;
        bool m_isNewObject;

        PackedGuid m_PackGUID;

        Object(const Object&);                              // prevent generation copy constructor
        Object& operator=(Object const&);                   // prevent generation assigment operator

    public:
        // for output helpfull error messages from ASSERTs
        bool PrintIndexError(uint32 index, bool set) const;
        bool PrintEntryError(char const* descr) const;
};

struct WorldObjectChangeAccumulator;

class WorldObject : public Object
{
    friend struct WorldObjectChangeAccumulator;

    public:

        // class is used to manipulate with WorldUpdateCounter
        // it is needed in order to get time diff between two object's Update() calls
        class UpdateHelper
        {
            public:
                explicit UpdateHelper(WorldObject* obj) : m_obj(obj) {}
                ~UpdateHelper() {}

                void Update(uint32 time_diff)
                {
                    m_obj->Update(m_obj->m_updateTracker.timeElapsed(), time_diff);
                    m_obj->m_updateTracker.Reset();
                }

            private:
                UpdateHelper(const UpdateHelper&);
                UpdateHelper& operator=(const UpdateHelper&);

                WorldObject* const m_obj;
        };

        virtual ~WorldObject();

        virtual void Update(uint32 update_diff, uint32 /*time_diff*/);

        void _Create(uint32 guidlow, HighGuid guidhigh);

        /// WHERE THIS OBJECT IS -- the whole spatial API. An object HAS a placement; it
        /// is not a bag of coordinates with geometry methods bolted on, so there are no
        /// GetPositionX/GetDistance/HasInArc here and there never will be. Ask the
        /// component: obj->Where().DistanceTo(other->Where()).
        Geometry::Placement const& Where() const { return m_placement; }

        /// Mutation of the pose. Movement drives this; nobody else should need it.
        Geometry::Placement& Place() { return m_placement; }

        /// The extent lives in the component; this only pushes a new value in when the
        /// per-class formula's inputs change (a model, a scale -- rarely).
        void RefreshBoundingRadius() { m_placement.Resize(ComputeBoundingRadius()); }

        void OnScaleChanged() override { RefreshBoundingRadius(); }

        uint32 GetMapId() const { return m_mapId; }
        uint32 GetInstanceId() const { return m_InstanceId; }


        InstanceData* GetInstanceData() const;

        const char* GetName() const { return m_name.c_str(); }
        void SetName(const std::string& newname) { m_name = newname; }

        virtual const char* GetNameForLocaleIdx(int32 /*locale_idx*/) const { return GetName(); }

        virtual void CleanupsBeforeDelete();                // used in destructor or explicitly before mass creature delete to remove cross-references to already deleted units

        virtual void SendMessageToSet(WorldPacket* data, bool self) const;
        virtual void SendMessageToSetInRange(WorldPacket* data, float dist, bool self) const;
        void SendMessageToSetExcept(WorldPacket* data, Player const* skipped_receiver) const;

        void MonsterSay(const char* text, uint32 language, Unit const* target = NULL) const;
        void MonsterYell(const char* text, uint32 language, Unit const* target = NULL) const;
        void MonsterTextEmote(const char* text, Unit const* target, bool IsBossEmote = false) const;
        void MonsterWhisper(const char* text, Unit const* target, bool IsBossWhisper = false) const;
        void MonsterText(MangosStringLocale const* textData, Unit const* target) const;

        void PlayDistanceSound(uint32 sound_id, Player const* target = NULL) const;
        void PlayDirectSound(uint32 sound_id, Player const* target = NULL) const;
        void PlayMusic(uint32 sound_id, Player const* target = NULL) const;

        void SendObjectDeSpawnAnim(ObjectGuid guid);

        virtual bool IsHostileTo(Unit const* unit) const = 0;
        virtual bool IsFriendlyTo(Unit const* unit) const = 0;
        virtual bool IsControlledByPlayer() const { return false; }

        virtual void SaveRespawnTime() {}
        void AddObjectToRemoveList();

        void UpdateObjectVisibility();
        virtual void UpdateVisibilityAndView();             // update visibility for object and object for all around

        // main visibility check function in normal case (ignore grey zone distance check)
        bool IsVisibleFor(Player const* u, WorldObject const* viewPoint) const { return IsVisibleForInState(u, viewPoint, false); }

        // low level function for visibility change code, must be define in all main world object subclasses
        virtual bool IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const = 0;

        void SetMap(Map* map);
        Map* GetMap() const { MANGOS_ASSERT(m_currMap); return m_currMap; }
        // used to check all object's GetMap() calls when object is not in world!
        void ResetMap();

        TerrainInfo const* GetTerrain() const;

        void AddToClientUpdateList() override;
        void RemoveFromClientUpdateList() override;
        void BuildUpdateData(UpdateDataMapType&) override;

        Creature* SummonCreature(uint32 id, float x, float y, float z, float ang, TempSpawnType spwtype, uint32 despwtime, bool asActiveObject = false, bool setRun = false);
        GameObject* SummonGameObject(uint32 id, float x, float y, float z, float angle, uint32 despwtime);

        bool IsActiveObject() const { return m_isActiveObject || m_viewPoint.hasViewers(); }
        bool isActiveObject() const { return IsActiveObject(); } // This is for Eluna to build. Should be removed in the future!

        void SetActiveObjectState(bool active);

        // Per-object visibility distance. 0 means use the map default; a positive
        // value overrides it when this object is the viewpoint (e.g. the cinematic
        // flyover body widens the populate radius without touching the map).
        float GetVisibilityDistanceOverride() const { return m_visibilityDistanceOverride; }
        void SetVisibilityDistanceOverride(float dist) { m_visibilityDistanceOverride = dist; }

        ViewPoint& GetViewPoint()
        {
            return m_viewPoint;
        }

        // ASSERT print helper
        bool PrintCoordinatesError(float x, float y, float z, char const* descr) const;

        virtual void StartGroupLoot(Group* /*group*/, uint32 /*timer*/) {}

#ifdef ENABLE_ELUNA
        ElunaEventProcessor* elunaEvents;

        Eluna* GetEluna() const;

        LuaVal lua_data = LuaVal({});
#endif /* ENABLE_ELUNA */

#ifdef MANGOS_SCRIPT_COMPAT
#include "ScriptApiCompat.inl"
#endif

    protected:
        explicit WorldObject();

        /// The per-class spatial extent. Overridden where the object is not a default
        /// blob: a unit reads its model, a gameobject its geometry box.
        virtual float ComputeBoundingRadius() const { return DEFAULT_WORLD_OBJECT_SIZE; }

        // these functions are used mostly for Relocate() and Corpse/Player specific stuff...
        // use them ONLY in LoadFromDB()/Create() funcs and nowhere else!
        // mapId/instanceId should be set in SetMap() function!
        void SetLocationMapId(uint32 _mapId) { m_mapId = _mapId; RefreshFrame(); }
        void SetLocationInstanceId(uint32 _instanceId) { m_InstanceId = _instanceId; RefreshFrame(); }

        /// Re-anchor the component to the frame the object's map identity names. The
        /// pose is untouched: this says where the numbers are measured, not what they are.
        void RefreshFrame()
        {
            m_placement.Rebase(Geometry::Frame::World(m_mapId, m_InstanceId));
        }

        virtual void StopGroupLoot() {}

        std::string m_name;

    private:
        Map* m_currMap;                                     // current object's Map location

        uint32 m_mapId;                                     // object at map with map_id
        uint32 m_InstanceId;                                // in map copy with instance id

        Geometry::Placement m_placement;
        ViewPoint m_viewPoint;
        WorldUpdateCounter m_updateTracker;
        bool m_isActiveObject;
        float m_visibilityDistanceOverride;
};

// Tests that are NOT geometry, so they are not the component's and never the object's:
// world membership is game state, line of sight is a terrain question, and a map's
// coordinate bounds belong to the map. Each asks the placement for the geometry and adds
// only what the placement must not know.
/// Can A reach B -- a common frame is required. Melee, spells, threat, aggro.
bool CanInteract(WorldObject const& a, WorldObject const& b);

/// Can B be shown A -- the wider question, and never the same one as reaching it.
bool CanBeSeen(WorldObject const& seen, WorldObject const& viewer);

/// CanBeSeen plus "near enough to bother".
bool SeenWithin(WorldObject const& seen, WorldObject const& viewer, float dist, bool is3D = true);

bool InReach(WorldObject const& a, WorldObject const& b, float dist, bool is3D = true);
bool InFrontPhased(WorldObject const& a, WorldObject const& b, float dist, float arc);
bool InBackPhased(WorldObject const& a, WorldObject const& b, float dist, float arc);
bool HasLineOfSight(WorldObject const& a, WorldObject const& b);
bool HasLineOfSight(WorldObject const& a, Geometry::Vector3 const& point);
bool IsPlaceable(WorldObject const& obj);

// Terrain and grid answers about a position. The component supplies the geometry; the
// height, the collision sweep and the map's bounds come from the engines that own them.
Geometry::Vector3 PointNear(WorldObject const& anchor, float distance2d, float absAngle);
void DropToGround(WorldObject const& obj, float x, float y, float& z);
void ClampToAllowedZ(WorldObject const& obj, float x, float y, float& z, Map* atMap = NULL);
Geometry::Vector3 RandomGroundPointNear(WorldObject const& obj, Geometry::Vector3 const& centre,
                                        float distance, float minDist = 0.0f, float const* ori = NULL);
void FindFreeSpotNear(WorldObject const& anchor, WorldObject const* searcher, float& x, float& y, float& z,
                      float searcher_bounding_radius, float distance2d, float absAngle);
void ClosePointNear(WorldObject const& anchor, float& x, float& y, float& z, float bounding_radius,
                    float distance2d = 0.0f, float angle = 0.0f, WorldObject const* searcher = NULL);
void ContactPointNear(WorldObject const& anchor, WorldObject const* obj, float& x, float& y, float& z,
                      float distance2d = CONTACT_DISTANCE);

// Helper functions to cast between different Object pointers. Useful when unsure that your object* is valid at all.
inline WorldObject* ToWorldObject(Object* object)
{
    return object && object->isType(TYPEMASK_WORLDOBJECT) ? static_cast<WorldObject*>(object) : nullptr;
}

inline WorldObject const* ToWorldObject(Object const* object)
{
    return object && object->isType(TYPEMASK_WORLDOBJECT) ? static_cast<WorldObject const*>(object) : nullptr;
}

inline GameObject* ToGameObject(Object* object)
{
    return object && object->GetTypeId() == TYPEID_GAMEOBJECT ? reinterpret_cast<GameObject*>(object) : nullptr;
}

inline GameObject const* ToGameObject(Object const* object)
{
    return object && object->GetTypeId() == TYPEID_GAMEOBJECT ? reinterpret_cast<GameObject const*>(object) : nullptr;
}

inline Unit* ToUnit(Object* object)
{
    return object && object->isType(TYPEMASK_UNIT) ? reinterpret_cast<Unit*>(object) : nullptr;
}

inline Unit const* ToUnit(Object const* object)
{
    return object && object->isType(TYPEMASK_UNIT) ? reinterpret_cast<Unit const*>(object) : nullptr;
}

inline Creature* ToCreature(Object* object)
{
    return object && object->GetTypeId() == TYPEID_UNIT ? reinterpret_cast<Creature*>(object) : nullptr;
}

inline Creature const* ToCreature(Object const* object)
{
    return object && object->GetTypeId() == TYPEID_UNIT ? reinterpret_cast<Creature const*>(object) : nullptr;
}

inline Player* ToPlayer(Object* object)
{
    return object && object->GetTypeId() == TYPEID_PLAYER ? reinterpret_cast<Player*>(object) : nullptr;
}

inline Player const* ToPlayer(Object const* object)
{
    return object && object->GetTypeId() == TYPEID_PLAYER ? reinterpret_cast<Player const*>(object) : nullptr;
}

#endif
