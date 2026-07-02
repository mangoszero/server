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
 * @brief Recalculates movement speed for a move type from active modifiers.
 *
 * @param mtype The movement type to update.
 * @param forced True to send a forced speed change packet to players.
 * @param ratio Additional multiplier applied after recalculation.
 */
void Unit::UpdateSpeed(UnitMoveType mtype, bool forced, float ratio)
{
    int32 main_speed_mod  = 0;
    float stack_bonus     = 1.0f;
    float non_stack_bonus = 1.0f;

    switch (mtype)
    {
        case MOVE_WALK:
            break;
        case MOVE_RUN:
        {
            if (IsMounted()) // Use on mount auras
            {
                main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED);
                stack_bonus     = GetTotalAuraMultiplier(SPELL_AURA_MOD_MOUNTED_SPEED_ALWAYS);
                non_stack_bonus = (100.0f + GetMaxPositiveAuraModifier(SPELL_AURA_MOD_MOUNTED_SPEED_NOT_STACK)) / 100.0f;
            }
            else
            {
                main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SPEED);
                stack_bonus     = GetTotalAuraMultiplier(SPELL_AURA_MOD_SPEED_ALWAYS);
                non_stack_bonus = (100.0f + GetMaxPositiveAuraModifier(SPELL_AURA_MOD_SPEED_NOT_STACK)) / 100.0f;
            }
            break;
        }
        case MOVE_RUN_BACK:
            return;
        case MOVE_SWIM:
        {
            main_speed_mod  = GetMaxPositiveAuraModifier(SPELL_AURA_MOD_INCREASE_SWIM_SPEED);
            break;
        }
        case MOVE_SWIM_BACK:
            return;
        default:
            sLog.outError("Unit::UpdateSpeed: Unsupported move type (%d)", mtype);
            return;
    }

    float bonus = non_stack_bonus > stack_bonus ? non_stack_bonus : stack_bonus;
    // now we ready for speed calculation
    float speed  = main_speed_mod ? bonus * (100.0f + main_speed_mod) / 100.0f : bonus;

    switch (mtype)
    {
        case MOVE_RUN:
        case MOVE_SWIM:
        {
            // Normalize speed by 191 aura SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED if need
            // TODO: possible affect only on MOVE_RUN
            if (int32 normalization = GetMaxPositiveAuraModifier(SPELL_AURA_USE_NORMAL_MOVEMENT_SPEED))
            {
                // Use speed from aura
                float max_speed = normalization / baseMoveSpeed[mtype];
                if (speed > max_speed)
                {
                    speed = max_speed;
                }
            }
            break;
        }
        default:
            break;
    }

    // for creature case, we check explicit if mob searched for assistance
    if (GetTypeId() == TYPEID_UNIT)
    {
        if (((Creature*)this)->HasSearchedAssistance())
        {
            speed *= 0.66f; // best guessed value, so this will be 33% reduction. Based off initial speed, mob can then "run", "walk fast" or "walk".
        }
    }
    // for player case, we look for some custom rates
    else
    {
        if (GetDeathState() == CORPSE)
        {
            speed *= sWorld.getConfig(((Player*)this)->InBattleGround() ? CONFIG_FLOAT_GHOST_RUN_SPEED_BG : CONFIG_FLOAT_GHOST_RUN_SPEED_WORLD);
        }
    }

    // Apply strongest slow aura mod to speed
    int32 slow = GetMaxNegativeAuraModifier(SPELL_AURA_MOD_DECREASE_SPEED);
    if (slow)
    {
        speed *= (100.0f + slow) / 100.0f;
    }

    if (GetTypeId() == TYPEID_UNIT)
    {
        switch (mtype)
        {
            case MOVE_RUN:
                speed *= ((Creature*)this)->GetCreatureInfo()->SpeedRun;
                break;
            case MOVE_WALK:
                speed *= ((Creature*)this)->GetCreatureInfo()->SpeedWalk;
                break;
            default:
                break;
        }
    }

    SetSpeedRate(mtype, speed * ratio, forced);
}

/**
 * @brief Gets the current movement speed for a move type.
 *
 * @param mtype The movement type.
 * @return The resulting speed value.
 */
float Unit::GetSpeed(UnitMoveType mtype) const
{
    return m_speed_rate[mtype] * baseMoveSpeed[mtype];
}

struct SetSpeedRateHelper
{
    explicit SetSpeedRateHelper(UnitMoveType _mtype, bool _forced) : mtype(_mtype), forced(_forced) {}
    void operator()(Unit* unit) const { unit->UpdateSpeed(mtype, forced); }
    UnitMoveType mtype;
    bool forced;
};

/**
 * @brief Sets the speed rate for a move type and broadcasts the change.
 *
 * @param mtype The movement type.
 * @param rate The new speed rate.
 * @param forced True to use forced speed change handling for players.
 */
void Unit::SetSpeedRate(UnitMoveType mtype, float rate, bool forced)
{
    if (rate < 0)
    {
        rate = 0.0f;
    }

    // Update speed only on change
    if (m_speed_rate[mtype] != rate)
    {
        m_speed_rate[mtype] = rate;

        PropagateSpeedChange();

        typedef const uint16 SpeedOpcodePair[2];
        SpeedOpcodePair SetSpeed2Opc_table[MAX_MOVE_TYPE] =
        {
            {SMSG_FORCE_WALK_SPEED_CHANGE,        SMSG_SPLINE_SET_WALK_SPEED},
            {SMSG_FORCE_RUN_SPEED_CHANGE,         SMSG_SPLINE_SET_RUN_SPEED},
            {SMSG_FORCE_RUN_BACK_SPEED_CHANGE,    SMSG_SPLINE_SET_RUN_BACK_SPEED},
            {SMSG_FORCE_SWIM_SPEED_CHANGE,        SMSG_SPLINE_SET_SWIM_SPEED},
            {SMSG_FORCE_SWIM_BACK_SPEED_CHANGE,   SMSG_SPLINE_SET_SWIM_BACK_SPEED},
            {SMSG_FORCE_TURN_RATE_CHANGE,         SMSG_SPLINE_SET_TURN_RATE},
        };

        const SpeedOpcodePair& speedOpcodes = SetSpeed2Opc_table[mtype];

        if (forced && GetTypeId() == TYPEID_PLAYER)
        {
            // register forced speed changes for WorldSession::HandleForceSpeedChangeAck
            // and do it only for real sent packets and use run for run/mounted as client expected
            ++((Player*)this)->m_forced_speed_changes[mtype];

            WorldPacket data(speedOpcodes[0], 18);
            data << GetPackGUID();
            data << (uint32)0;                              // moveEvent, NUM_PMOVE_EVTS = 0x39
            data << float(GetSpeed(mtype));
            ((Player*)this)->GetSession()->SendPacket(&data);
        }
        WorldPacket data(speedOpcodes[1], 12);
        data << GetPackGUID();
        data << float(GetSpeed(mtype));
        SendMessageToSet(&data, false);
    }

    CallForAllControlledUnits(SetSpeedRateHelper(mtype, forced), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_CHARM | CONTROLLED_MINIPET);
}

/**
 * @brief Applies or removes the feared state.
 *
 * @param apply True to apply fear; false to remove it.
 * @param casterGuid The caster responsible for the effect.
 * @param spellID The spell that caused the effect.
 * @param time The remaining flee duration.
 */
void Unit::SetFeared(bool apply, ObjectGuid casterGuid, uint32 spellID, uint32 time)
{
    SetIncapacitatedState(apply, UNIT_FLAG_FLEEING, casterGuid, spellID, time);
}

/**
 * @brief Applies or removes the confused state.
 *
 * @param apply True to apply confusion; false to remove it.
 * @param casterGuid The caster responsible for the effect.
 * @param spellID The spell that caused the effect.
 */
void Unit::SetConfused(bool apply, ObjectGuid casterGuid, uint32 spellID)
{
    SetIncapacitatedState(apply, UNIT_FLAG_CONFUSED, casterGuid, spellID);
}

/**
 * @brief Applies or removes feign death state handling.
 *
 * @param apply True to enable feign death; false to clear it.
 * @param casterGuid The caster responsible for the effect.
 */
void Unit::SetFeignDeath(bool apply, ObjectGuid casterGuid /*= ObjectGuid()*/)
{
    if (apply)
    {
        if (GetTypeId() != TYPEID_PLAYER)
        {
            StopMoving();
        }
        else
        {
            ((Player*)this)->m_movementInfo.SetMovementFlags(MOVEFLAG_NONE);
        }

        SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
        // blizz like 2.0.x
        // SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH);  [-ZERO] remove/replace ?

        SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);

        addUnitState(UNIT_STAT_DIED);
        CombatStop();
        RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_IMMUNE_OR_LOST_SELECTION);

        // prevent interrupt message
        if (casterGuid == GetObjectGuid())
        {
            FinishSpell(CURRENT_GENERIC_SPELL, false);
        }
        InterruptNonMeleeSpells(true);
        GetHostileRefManager().deleteReferences();
    }
    else
    {
        /* when appropriate! not within this method
        WorldPacket data(SMSG_FEIGN_DEATH_RESISTED, 0);
        SendDirectMessage(&data);
        */

        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_UNK_29);
        // blizz like 2.0.x
        // SetFlag(UNIT_FIELD_FLAGS_2, UNIT_FLAG2_FEIGN_DEATH); [-ZERO] remove/replace ?

        RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);

        clearUnitState(UNIT_STAT_DIED);

        if (GetTypeId() != TYPEID_PLAYER && IsAlive())
        {
            // restore appropriate movement generator
            if (getVictim())
            {
                GetMotionMaster()->MoveChase(getVictim());
            }
            else
            {
                GetMotionMaster()->Initialize();
            }
        }
    }
}
