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

#include "Common.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "GridNotifiersImpl.h"
#include "SpellMgr.h"
#include "DBCStores.h"

/**
 * @brief Creates an empty dynamic object instance.
 */
DynamicObject::DynamicObject() : WorldObject()
{
    m_objectType |= TYPEMASK_DYNAMICOBJECT;
    m_objectTypeId = TYPEID_DYNAMICOBJECT;
    m_updateFlag = (UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);

    m_valuesCount = DYNAMICOBJECT_END;
}

/**
 * @brief Adds the dynamic object to the world and lookup store.
 */
void DynamicObject::AddToWorld()
{
    ///- Register the dynamicObject for guid lookup
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().insert<DynamicObject>(GetObjectGuid(), (DynamicObject*)this);
    }

    Object::AddToWorld();
}

/**
 * @brief Removes the dynamic object from the world and lookup store.
 */
void DynamicObject::RemoveFromWorld()
{
    ///- Remove the dynamicObject from the accessor
    if (IsInWorld())
    {
        GetMap()->GetObjectsStore().erase<DynamicObject>(GetObjectGuid(), (DynamicObject*)NULL);
        GetViewPoint().Event_RemovedFromWorld();
    }

    Object::RemoveFromWorld();
}

/**
 * @brief Creates a dynamic object for a spell effect.
 *
 * @param guidlow The low part of the dynamic object GUID.
 * @param caster The unit creating the object.
 * @param spellId The spell identifier.
 * @param effIndex The spell effect index.
 * @param x The X position.
 * @param y The Y position.
 * @param z The Z position.
 * @param duration The lifetime in milliseconds.
 * @param radius The effect radius.
 * @param type The dynamic object visual type.
 * @return true if creation succeeded; otherwise, false.
 */
bool DynamicObject::Create(uint32 guidlow, Unit* caster, uint32 spellId, SpellEffectIndex effIndex, float x, float y, float z, int32 duration, float radius, DynamicObjectType type)
{
    WorldObject::_Create(guidlow, HIGHGUID_DYNAMICOBJECT);
    SetMap(caster->GetMap());
    Relocate(x, y, z, 0);

    if (!IsPositionValid())
    {
        sLog.outError("DynamicObject (spell %u eff %u) not created. Suggested coordinates isn't valid (X: %f Y: %f)", spellId, effIndex, GetPositionX(), GetPositionY());
        return false;
    }

    SetEntry(spellId);
    SetObjectScale(DEFAULT_OBJECT_SCALE);

    SetGuidValue(DYNAMICOBJECT_CASTER, caster->GetObjectGuid());

    /** Bytes field, so it's really 4 bit fields. These flags are unknown, but we do know that 0x00000001 is set for most.
     *  Farsight for example, does not have this flag, instead it has 0x80000002.
     *  Flags are set dynamically with some conditions, so one spell may have different flags set, depending on those conditions.
     *  The size of the visual may be controlled to some degree with these flags.
     *
     *  uint32 bytes = 0x00000000;
     *  bytes |= 0x01;
     *  bytes |= 0x00 << 8;
     *  bytes |= 0x00 << 16;
     *  bytes |= 0x00 << 24;
     */
    SetByteValue(DYNAMICOBJECT_BYTES, 0, type);

    SetUInt32Value(DYNAMICOBJECT_SPELLID, spellId);
    SetFloatValue(DYNAMICOBJECT_RADIUS, radius);
    SetFloatValue(DYNAMICOBJECT_POS_X, x);
    SetFloatValue(DYNAMICOBJECT_POS_Y, y);
    SetFloatValue(DYNAMICOBJECT_POS_Z, z);

    SpellEntry const* spellProto = sSpellStore.LookupEntry(spellId);
    if (!spellProto)
    {
        sLog.outError("DynamicObject (spell %u) not created. Spell not exist!", spellId);
        return false;
    }

    m_aliveDuration = duration;
    m_radius = radius;
    m_effIndex = effIndex;
    m_spellId = spellId;
    m_positive = IsPositiveEffect(spellProto, m_effIndex);

    return true;
}

/**
 * @brief Retrieves the caster that owns this dynamic object.
 *
 * @return Pointer to the caster, or NULL if not found.
 */
Unit* DynamicObject::GetCaster() const
{
    // can be not found in some cases
    return sObjectAccessor.GetUnit(*this, GetCasterGuid());
}

/**
 * @brief Updates the dynamic object lifetime and periodic effect processing.
 *
 * @param update_diff The elapsed time since the last update in milliseconds.
 * @param p_time The world update time used for lifetime reduction.
 */
void DynamicObject::Update(uint32 /*update_diff*/, uint32 p_time)
{
    // caster can be not in world at time dynamic object update, but dynamic object not yet deleted in Unit destructor
    Unit* caster = GetCaster();
    if (!caster)
    {
        Delete();
        return;
    }

    bool deleteThis = false;

    if (m_aliveDuration > int32(p_time))
    {
        m_aliveDuration -= p_time;
    }
    else
    {
        deleteThis = true;
    }

    // have radius and work as persistent effect
    if (m_radius)
    {
        // TODO: make a timer and update this in larger intervals
        MaNGOS::DynamicObjectUpdater notifier(*this, caster, m_positive);
        Cell::VisitAllObjects(this, notifier, m_radius);
    }

    if (deleteThis)
    {
        caster->RemoveDynObjectWithGUID(GetObjectGuid());
        Delete();
    }
}

/**
 * @brief Deletes the dynamic object from the world.
 */
void DynamicObject::Delete()
{
    SendObjectDeSpawnAnim(GetObjectGuid());
    AddObjectToRemoveList();
}

/**
 * @brief Delays the dynamic object lifetime and matching aura durations.
 *
 * @param delaytime The delay amount in milliseconds.
 */
void DynamicObject::Delay(int32 delaytime)
{
    m_aliveDuration -= delaytime;
    for (GuidSet::iterator iter = m_affected.begin(); iter != m_affected.end();)
    {
        Unit* target = GetMap()->GetUnit((*iter));
        if (target)
        {
            SpellAuraHolder* holder = target->GetSpellAuraHolder(m_spellId, GetCasterGuid());
            if (!holder)
            {
                ++iter;
                continue;
            }

            bool foundAura = false;
            for (int32 i = m_effIndex + 1; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((holder->GetSpellProto()->Effect[i] == SPELL_EFFECT_PERSISTENT_AREA_AURA || holder->GetSpellProto()->Effect[i] == SPELL_EFFECT_ADD_FARSIGHT) && holder->m_auras[i])
                {
                    foundAura = true;
                    break;
                }
            }

            if (foundAura)
            {
                ++iter;
                continue;
            }

            target->DelaySpellAuraHolder(m_spellId, delaytime, GetCasterGuid());
            ++iter;
        }
        else
        {
            m_affected.erase(iter++);
        }
    }
}

/**
 * @brief Checks whether the dynamic object is visible to a player in the current state.
 *
 * @param u The player evaluating visibility.
 * @param viewPoint The viewpoint used for visibility checks.
 * @param inVisibleList true when the object is already in the visible list.
 * @return true if the object should be visible; otherwise, false.
 */
bool DynamicObject::IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    if (!IsInWorld() || !u->IsInWorld())
    {
        return false;
    }

    // always seen by owner
    if (GetCasterGuid() == u->GetObjectGuid())
    {
        return true;
    }

    // normal case
    return IsWithinDistInMap(viewPoint, GetMap()->GetVisibilityDistance() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f), false);
}

/**
 * @brief Checks whether the caster is hostile to a unit.
 *
 * @param unit The unit to test against.
 * @return true if the caster is hostile to the unit; otherwise, false.
 */
bool DynamicObject::IsHostileTo(Unit const* unit) const
{
    if (Unit* owner = GetCaster())
    {
        return owner->IsHostileTo(unit);
    }
    else
    {
        return false;
    }
}

/**
 * @brief Checks whether the caster is friendly to a unit.
 *
 * @param unit The unit to test against.
 * @return true if the caster is friendly to the unit; otherwise, false.
 */
bool DynamicObject::IsFriendlyTo(Unit const* unit) const
{
    if (Unit* owner = GetCaster())
    {
        return owner->IsFriendlyTo(unit);
    }
    else
    {
        return true;
    }
}
