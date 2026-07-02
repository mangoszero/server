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



#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "VMapFactory.h"
#include "MovementGenerator.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include <math.h>
#include <stdarg.h>
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
 * @brief Changes the unit's power type and updates dependent state.
 *
 * @param new_powertype The new power type.
 */
void Unit::SetPowerType(Powers new_powertype)
{
    // set power type
    SetByteValue(UNIT_FIELD_BYTES_0, 3, new_powertype);

    // group updates
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_POWER_TYPE);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_POWER_TYPE);
            }
        }
    }

    // special cases for power type switching (druid and pets only)
    if (GetTypeId() == TYPEID_PLAYER || (GetTypeId() == TYPEID_UNIT && ((Creature*)this)->IsPet()))
    {
        uint32 maxValue = GetCreatePowers(new_powertype);
        uint32 curValue = maxValue;

        // special cases with current power = 0
        if (new_powertype == POWER_RAGE)
        {
            curValue = 0;
        }

        // set power (except for mana)
        if (new_powertype != POWER_MANA)
        {
            SetMaxPower(new_powertype, maxValue);
            SetPower(new_powertype, curValue);
        }
    }
}

/**
 * @brief Sets a power value while clamping to its maximum.
 *
 * @param power The power type to set.
 * @param val The requested power value.
 */
void Unit::SetPower(Powers power, uint32 val)
{
    if (GetPower(power) == val)
    {
        return;
    }

    uint32 maxPower = GetMaxPower(power);
    if (maxPower < val)
    {
        val = maxPower;
    }

    SetStatInt32Value(UNIT_FIELD_POWER1 + power, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
            }
        }

        // Update the pet's character sheet with happiness damage bonus
        if (pet->getPetType() == HUNTER_PET && power == POWER_HAPPINESS)
        {
            pet->UpdateDamagePhysical(BASE_ATTACK);
        }
    }
}

/**
 * @brief Sets a power maximum and clamps current power if necessary.
 *
 * @param power The power type to update.
 * @param val The new maximum value.
 */
void Unit::SetMaxPower(Powers power, uint32 val)
{
    uint32 cur_power = GetPower(power);
    SetStatInt32Value(UNIT_FIELD_MAXPOWER1 + power, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
            }
        }
    }

    if (val < cur_power)
    {
        SetPower(power, val);
    }
}

/**
 * @brief Applies or removes a flat modifier to current power.
 *
 * @param power The power type to modify.
 * @param val The amount to apply or remove.
 * @param apply True to apply the modifier; false to remove it.
 */
void Unit::ApplyPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_POWER1 + power, val, apply);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_CUR_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_CUR_POWER);
            }
        }
    }
}

/**
 * @brief Applies or removes a flat modifier to maximum power.
 *
 * @param power The power type to modify.
 * @param val The amount to apply or remove.
 * @param apply True to apply the modifier; false to remove it.
 */
void Unit::ApplyMaxPowerMod(Powers power, uint32 val, bool apply)
{
    ApplyModUInt32Value(UNIT_FIELD_MAXPOWER1 + power, val, apply);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)this)->GetGroup())
        {
            ((Player*)this)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_MAX_POWER);
        }
    }
    else if (((Creature*)this)->IsPet())
    {
        Pet* pet = ((Pet*)this);
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER) && ((Player*)owner)->GetGroup())
            {
                ((Player*)owner)->SetGroupUpdateFlag(GROUP_UPDATE_FLAG_PET_MAX_POWER);
            }
        }
    }
}

/**
 * @brief Registers or unregisters an aura in the proc-trigger-damage list.
 *
 * @param aura The aura to add or remove.
 * @param apply True to add the aura; false to remove it.
 */
void Unit::ApplyAuraProcTriggerDamage(Aura* aura, bool apply)
{
    AuraList& tAuraProcTriggerDamage = m_modAuras[SPELL_AURA_PROC_TRIGGER_DAMAGE];
    if (apply)
    {
        tAuraProcTriggerDamage.push_back(aura);
    }
    else
    {
        tAuraProcTriggerDamage.remove(aura);
    }
}

/**
 * @brief Gets the default base value for a power type.
 *
 * @param power The power type to query.
 * @return The created base power value.
 */
uint32 Unit::GetCreatePowers(Powers power) const
{
    switch (power)
    {
        case POWER_HEALTH:      return 0;                   // is it really should be here?
        case POWER_MANA:        return GetCreateMana();
        case POWER_RAGE:        return POWER_RAGE_DEFAULT;
        case POWER_FOCUS:       return (GetTypeId() == TYPEID_PLAYER || !((Creature const*)this)->IsPet() || ((Pet const*)this)->getPetType() != HUNTER_PET ? 0 : POWER_FOCUS_DEFAULT);
        case POWER_ENERGY:      return POWER_ENERGY_DEFAULT;
        case POWER_HAPPINESS:   return (GetTypeId() == TYPEID_PLAYER || !((Creature const*)this)->IsPet() || ((Pet const*)this)->getPetType() != HUNTER_PET ? 0 : POWER_HAPPINESS_DEFAULT);
        default: break; //MAX_POWERS and POWERS_ALL probably should not belong to the enum Powers to do not require the default: case
    }

    return 0;
}
