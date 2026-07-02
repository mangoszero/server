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
 * @brief Force immediate update transmission to all viewers
 *
 * Sends all pending update changes immediately rather than waiting
 * for the next update tick. This is used for urgent updates that
 * must be visible immediately (e.g., combat state changes).
 *
 * The method builds update data for all nearby players and sends
 * it immediately, then removes the object from the pending update list.
 */
void Object::SendForcedObjectUpdate()
{
    if (!m_inWorld || !m_objectUpdated)
    {
        return;
    }

    UpdateDataMapType update_players;

    BuildUpdateData(update_players);
    RemoveFromClientUpdateList();

    WorldPacket packet;                                     // here we allocate a std::vector with a size of 0x10000
    for (UpdateDataMapType::iterator iter = update_players.begin(); iter != update_players.end(); ++iter)
    {
        iter->second.BuildPacket(&packet);
        iter->first->GetSession()->SendPacket(&packet);
        packet.clear();                                     // clean the string
    }
}

/**
 * @brief Build create update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data needed to create this object
 * for the specified player. Includes movement data and
 * all update field values.
 */
void Object::BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    if (!target)
    {
        return;
    }

    uint8  updatetype   = UPDATETYPE_CREATE_OBJECT;
    uint8 updateFlags  = m_updateFlag;

    /** lower flag1 **/
    if (target == this)                                     // building packet for yourself
    {
        updateFlags |= UPDATEFLAG_SELF;
    }

    if (m_isNewObject)
    {
        switch (GetObjectGuid().GetHigh())
        {
            case HighGuid::HIGHGUID_DYNAMICOBJECT:
            case HighGuid::HIGHGUID_CORPSE:
            case HighGuid::HIGHGUID_PLAYER:
            case HighGuid::HIGHGUID_UNIT:
            case HighGuid::HIGHGUID_GAMEOBJECT:
                updatetype = UPDATETYPE_CREATE_OBJECT2;
                break;
            default:
                break;
        }
    }

    // DEBUG_LOG("BuildCreateUpdate: update-type: %u, object-type: %u got updateFlags: %X", updatetype, m_objectTypeId, updateFlags);

    ByteBuffer& buf = data->GetBuffer();
    buf << uint8(updatetype);
    buf << GetPackGUID();
    buf << uint8(m_objectTypeId);

    BuildMovementUpdate(&buf, updateFlags);

    UpdateMask updateMask;
    updateMask.SetCount(m_valuesCount);
    _SetCreateBits(&updateMask, target);
    BuildValuesUpdate(updatetype, &buf, &updateMask, target);
    data->AddUpdateBlock();
}

/**
 * @brief Send create update to player
 * @param player Target player
 *
 * Sends the create update packet to the specified player,
 * causing the object to appear in their game world.
 */
void Object::SendCreateUpdateToPlayer(Player* player)
{
    // send create update to player
    UpdateData upd;
    WorldPacket packet;

    BuildCreateUpdateBlockForPlayer(&upd, player);
    upd.BuildPacket(&packet);
    player->GetSession()->SendPacket(&packet);
}

/**
 * @brief Build values update block for player
 * @param data Update data buffer
 * @param target Target player
 *
 * Builds the update packet data for changed field values
 * to send to the specified player.
 */
void Object::BuildValuesUpdateBlockForPlayer(UpdateData* data, Player* target) const
{
    ByteBuffer& buf = data->GetBuffer();

    buf << uint8(UPDATETYPE_VALUES);
    buf << GetPackGUID();

    UpdateMask updateMask;
    updateMask.SetCount(m_valuesCount);

    _SetUpdateBits(&updateMask, target);
    BuildValuesUpdate(UPDATETYPE_VALUES, &buf, &updateMask, target);

    data->AddUpdateBlock();
}

/**
 * @brief Build out of range update block
 * @param data Update data buffer
 *
 * Adds this object's GUID to the out-of-range list,
 * indicating it should be removed from the client's view.
 */
void Object::BuildOutOfRangeUpdateBlock(UpdateData* data) const
{
    data->AddOutOfRangeGUID(GetObjectGuid());
}

/**
 * @brief Destroy object for player
 * @param target Target player
 *
 * Sends a destroy packet to the specified player,
 * removing this object from their game world.
 */
void Object::DestroyForPlayer(Player* target) const
{
    MANGOS_ASSERT(target);

    WorldPacket data(SMSG_DESTROY_OBJECT, 8);
    data << GetObjectGuid();
    target->GetSession()->SendPacket(&data);
}

/**
 * @brief Build movement update block
 * @param data Byte buffer to write to
 * @param updateFlags Update flags
 *
 * Builds the movement data portion of the update packet.
 * Includes position, orientation, movement flags, and speeds
 * for living objects, or just position for static objects.
 */
void Object::BuildMovementUpdate(ByteBuffer* data, uint8 updateFlags) const
{
    Unit const* unit = NULL;
    uint32 highGuid = 0;
    MovementFlags moveflags = MOVEFLAG_NONE;

    switch (m_objectTypeId)
    {
        case TYPEID_OBJECT:
        case TYPEID_ITEM:
        case TYPEID_CONTAINER:
        case TYPEID_GAMEOBJECT:
        case TYPEID_DYNAMICOBJECT:
        case TYPEID_CORPSE:
            highGuid = uint32(GetObjectGuid().GetHigh());
            break;

        case TYPEID_PLAYER:
            // TODO: this code must not be here
            if (static_cast<Player const*>(this)->GetTransport())
            {
                ((Unit*)this)->m_movementInfo.AddMovementFlag(MOVEFLAG_ONTRANSPORT);
            }
            else
            {
                ((Unit*)this)->m_movementInfo.RemoveMovementFlag(MOVEFLAG_ONTRANSPORT);
            }

        // fall through to TYPEID_UNIT — unit must be set for UPDATEFLAG_LIVING
        case TYPEID_UNIT:
            unit = static_cast<Unit const*>(this);
            moveflags = unit->m_movementInfo.GetMovementFlags();
            break;

        default:
            break;
    }

    *data << uint8(updateFlags);

    if (updateFlags & UPDATEFLAG_LIVING)
    {
        MANGOS_ASSERT(unit);
        if (unit->IsStopped() && unit->m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ENABLED))
        {
            sLog.outError("%s is not moving but have spline movement enabled!", GetGuidStr().c_str());
            std::string victimGuid = "none";
            if (Unit const* victim = unit->getVictim())
            {
                victimGuid = victim->GetGuidStr();
            }

            ObjectGuid const& targetGuid = unit->GetTargetGuid();
            std::string targetGuidString = targetGuid.IsEmpty() ? "none" : targetGuid.GetString();
            GridPair gridPair = MaNGOS::ComputeGridPair(unit->GetPositionX(), unit->GetPositionY());
            CellPair cellPair = MaNGOS::ComputeCellPair(unit->GetPositionX(), unit->GetPositionY());

            sLog.outError("[LivingWorld] spline-stall %s map=%u inst=%u pos=(%.2f,%.2f,%.2f o=%.2f) grid[%u,%u] cell[%u,%u] active-object=%s moveflags=0x%X movegen=%u in-combat=%s combat-timer=%u victim=%s target=%s",
                          unit->GetGuidStr().c_str(), unit->GetMapId(), unit->GetInstanceId(),
                          unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(), unit->GetOrientation(),
                          gridPair.x_coord, gridPair.y_coord, cellPair.x_coord, cellPair.y_coord,
                          unit->IsActiveObject() ? "yes" : "no", uint32(moveflags),
                          uint32(const_cast<Unit*>(unit)->GetMotionMaster()->GetCurrentMovementGeneratorType()),
                          unit->IsInCombat() ? "yes" : "no", unit->GetCombatTimer(),
                          victimGuid.c_str(), targetGuidString.c_str());
            ((Unit*)this)->m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD));
        }

        *data << unit->m_movementInfo;
        // Unit speeds
        *data << float(unit->GetSpeed(MOVE_WALK));
        *data << float(unit->GetSpeed(MOVE_RUN));
        *data << float(unit->GetSpeed(MOVE_RUN_BACK));
        *data << float(unit->GetSpeed(MOVE_SWIM));
        *data << float(unit->GetSpeed(MOVE_SWIM_BACK));
        *data << float(unit->GetSpeed(MOVE_TURN_RATE));

        if (unit->m_movementInfo.HasMovementFlag(MOVEFLAG_SPLINE_ENABLED))
        {
            Movement::PacketBuilder::WriteCreate(*unit->movespline, *data);
        }
    }
    else if (updateFlags & UPDATEFLAG_HAS_POSITION)
    {
        *data << ((WorldObject*)this)->GetPositionX();
        *data << ((WorldObject*)this)->GetPositionY();
        *data << ((WorldObject*)this)->GetPositionZ();
        *data << ((WorldObject*)this)->GetOrientation();
    }

    if (updateFlags & UPDATEFLAG_HIGHGUID)
    {
        *data << highGuid;
    }

    if (updateFlags & UPDATEFLAG_ALL)
    {
        *data << (uint32)0x1;
    }

    if (updateFlags & UPDATEFLAG_FULLGUID)
    {
        if (unit && unit->getVictim())
        {
            *data << unit->getVictim()->GetPackGUID();
        }
        else
        {
            data->appendPackGUID(0);
        }
    }

    // 0x2
    if (updateFlags & UPDATEFLAG_TRANSPORT)
    {
        *data << uint32(GameTime::GetGameTimeMS());           // ms time
    }
}

/**
 * @brief Build values update data
 * @param updatetype Update type (create or values)
 * @param data Byte buffer to write to
 * @param updateMask Update mask indicating which fields changed
 * @param target Target player
 *
 * Builds the actual field value data for the update packet.
 * Handles special cases for gameobjects and units.
 */
void Object::BuildValuesUpdate(uint8 updatetype, ByteBuffer* data, UpdateMask* updateMask, Player* target) const
{
    if (!target)
    {
        return;
    }

    bool IsActivateToQuest = false;
    if (isType(TYPEMASK_GAMEOBJECT) && !((GameObject*)this)->IsTransport())
    {
        IsActivateToQuest = ((GameObject*)this)->ActivateToQuest(target) || target->isGameMaster();

        updateMask->SetBit(GAMEOBJECT_DYN_FLAGS);
        if (updatetype == UPDATETYPE_VALUES)
        {
            updateMask->SetBit(GAMEOBJECT_ANIMPROGRESS);
        }
    }

    MANGOS_ASSERT(updateMask && updateMask->GetCount() == m_valuesCount);

    *data << (uint8)updateMask->GetBlockCount();
    updateMask->AppendToPacket(data);

    // 2 specialized loops for speed optimization in non-unit case
    if (isType(TYPEMASK_UNIT))                              // unit (creature/player) case
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                if (index == UNIT_NPC_FLAGS)
                {
                    uint32 appendValue = m_uint32Values[index];

                    if (GetTypeId() == TYPEID_UNIT)
                    {
                        if (appendValue & UNIT_NPC_FLAG_TRAINER)
                        {
                            if (!((Creature*)this)->IsTrainerOf(target, false))
                            {
                                appendValue &= ~UNIT_NPC_FLAG_TRAINER;
                            }
                        }

                        if (appendValue & UNIT_NPC_FLAG_STABLEMASTER)
                        {
                            if (target->getClass() != CLASS_HUNTER)
                            {
                                appendValue &= ~UNIT_NPC_FLAG_STABLEMASTER;
                            }
                        }
                    }

                    *data << uint32(appendValue);
                }
                // FIXME: Some values at server stored in float format but must be sent to client in uint32 format
                else if (index >= UNIT_FIELD_BASEATTACKTIME && index <= UNIT_FIELD_RANGEDATTACKTIME)
                {
                    // convert from float to uint32 and send
                    *data << uint32(m_floatValues[index] < 0 ? 0 : m_floatValues[index]);
                }

                // there are some float values which may be negative or can't get negative due to other checks
                else if ((index >= PLAYER_FIELD_NEGSTAT0    && index <= PLAYER_FIELD_NEGSTAT4) ||
                    (index >= PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE  && index <= (PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + 6)) ||
                    (index >= PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE  && index <= (PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + 6)) ||
                    (index >= PLAYER_FIELD_POSSTAT0    && index <= PLAYER_FIELD_POSSTAT4))
                {
                    *data << uint32(m_floatValues[index]);
                }

                // Gamemasters should be always able to select units - remove not selectable flag
                else if (index == UNIT_FIELD_FLAGS && target->isGameMaster())
                {
                    *data << (m_uint32Values[index] & ~UNIT_FLAG_NOT_SELECTABLE);
                }
                /* Hide loot animation for players that aren't permitted to loot the corpse */
                else if (index == UNIT_DYNAMIC_FLAGS && GetTypeId() == TYPEID_UNIT)
                {
                    uint32 send_value = m_uint32Values[index];

                    /* Initiate pointer to creature so we can check loot */
                    if (Creature* my_creature = (Creature*)this)
                    {
                        /* If the creature is NOT fully looted */
                        if (!my_creature->loot.isLooted())
                        {
                            /* If the lootable flag is NOT set */
                            if (!(send_value & UNIT_DYNFLAG_LOOTABLE))
                            {
                                /* Update it on the creature */
                                my_creature->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
                                /* Update it in the packet */
                                send_value = send_value | UNIT_DYNFLAG_LOOTABLE;
                            }
                        }
                    }
                    /* If we're not allowed to loot the target, destroy the lootable flag */
                    if (!target->isAllowedToLoot((Creature*)this))
                    {
                        if (send_value & UNIT_DYNFLAG_LOOTABLE)
                        {
                            send_value = send_value & ~UNIT_DYNFLAG_LOOTABLE;
                        }
                    }

                    /* If we are allowed to loot it and mob is tapped by us, destroy the tapped flag */
                    bool is_tapped = target->IsTappedByMeOrMyGroup((Creature*)this);

                    /* If the creature has tapped flag but is tapped by us, remove the flag */
                    if (send_value & UNIT_DYNFLAG_TAPPED && is_tapped)
                    {
                        send_value = send_value & ~UNIT_DYNFLAG_TAPPED;
                    }

                    // Checking SPELL_AURA_EMPATHY and caster
                    if (send_value & UNIT_DYNFLAG_SPECIALINFO && ((Unit*)this)->IsAlive())
                    {
                        bool bIsEmpathy = false;
                        bool bIsCaster = false;
                        Unit::AuraList const& mAuraEmpathy = ((Unit*)this)->GetAurasByType(SPELL_AURA_EMPATHY);
                        for (Unit::AuraList::const_iterator itr = mAuraEmpathy.begin(); !bIsCaster && itr != mAuraEmpathy.end(); ++itr)
                        {
                            bIsEmpathy = true; // Empathy by aura set
                            if ((*itr)->GetCasterGuid() == target->GetObjectGuid())
                            {
                                bIsCaster = true; // target is the caster of an empathy aura
                            }
                        }
                        if (bIsEmpathy && !bIsCaster) // Empathy by aura, but target is not the caster
                        {
                            send_value &= ~UNIT_DYNFLAG_SPECIALINFO;
                        }
                    }

                    *data << send_value;
                }
                else                                        // Unhandled index, just send
                {
                    // send in current format (float as float, uint32 as uint32)
                    *data << m_uint32Values[index];
                }
            }
        }
    }
    else if (isType(TYPEMASK_GAMEOBJECT))                   // gameobject case
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                if (index == GAMEOBJECT_DYN_FLAGS)
                {
                    if (IsActivateToQuest)
                    {
                        switch (((GameObject*)this)->GetGoType())
                        {
                            case GAMEOBJECT_TYPE_QUESTGIVER:
                            case GAMEOBJECT_TYPE_CHEST:
                            case GAMEOBJECT_TYPE_GENERIC:
                            case GAMEOBJECT_TYPE_SPELL_FOCUS:
                            case GAMEOBJECT_TYPE_GOOBER:
                                *data << uint16(GO_DYNFLAG_LO_ACTIVATE);
                                *data << uint16(0);
                                break;
                            default:
                                *data << uint32(0);         // unknown, not happen.
                                break;
                        }
                    }
                    else
                    {
                        // disable quest object
                        *data << uint32(0);
                    }
                }
                else
                {
                    *data << m_uint32Values[index];          // other cases
                }
            }
        }
    }
    else                                                    // other objects case (no special index checks)
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            if (updateMask->GetBit(index))
            {
                // send in current format (float as float, uint32 as uint32)
                *data << m_uint32Values[index];
            }
        }
    }
}

/**
 * @brief Clear update mask
 * @param remove If true, remove from client update list
 *
 * Clears all changed value flags and optionally removes
 * the object from the pending update list.
 */
void Object::ClearUpdateMask(bool remove)
{
    if (m_uint32Values)
    {
        for (uint16 index = 0; index < m_valuesCount; ++index)
        {
            m_changedValues[index] = false;
        }
    }

    if (m_objectUpdated)
    {
        if (remove)
        {
            RemoveFromClientUpdateList();
        }
        m_objectUpdated = false;
    }
}

/**
 * @brief Load values from data string
 * @param data Data string to load from
 * @return True if successful
 *
 * Loads update field values from a character data string.
 * Used when loading objects from database.
 */
bool Object::LoadValues(const char* data)
{
    if (!m_uint32Values)
    {
        _InitValues();
    }

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != m_valuesCount)
    {
        return false;
    }

    Tokens::iterator iter;
    int index;
    for (iter = tokens.begin(), index = 0; index < m_valuesCount; ++iter, ++index)
    {
        m_uint32Values[index] = atol((*iter).c_str());
    }

    return true;
}

/**
 * @brief Set update bits in mask
 * @param updateMask Update mask to modify
 * @param target Target player (unused)
 *
 * Sets bits in the update mask for all fields that have changed.
 */
void Object::_SetUpdateBits(UpdateMask* updateMask, Player* /*target*/) const
{
    for (uint16 index = 0; index < m_valuesCount; ++index)
    {
        if (m_changedValues[index])
        {
            updateMask->SetBit(index);
        }
    }
}

/**
 * @brief Set create bits in mask
 * @param updateMask Update mask to modify
 * @param target Target player (unused)
 *
 * Sets bits in the update mask for all non-zero fields.
 * Used when creating a new object for a player.
 */
void Object::_SetCreateBits(UpdateMask* updateMask, Player* /*target*/) const
{
    for (uint16 index = 0; index < m_valuesCount; ++index)
    {
        if (GetUInt32Value(index) != 0)
        {
            updateMask->SetBit(index);
        }
    }
}
