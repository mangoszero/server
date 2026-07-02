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



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Sends a mirror timer update to the client.
 *
 * @param Type The mirror timer type.
 * @param MaxValue The maximum timer value.
 * @param CurrentValue The current timer value.
 * @param Regen The regeneration rate.
 */
void Player::SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen)
{
    if (int(MaxValue) == DISABLED_MIRROR_TIMER)
    {
        if (int(CurrentValue) != DISABLED_MIRROR_TIMER)
        {
            StopMirrorTimer(Type);
        }
        return;
    }
    WorldPacket data(SMSG_START_MIRROR_TIMER, (21));
    data << (uint32)Type;
    data << CurrentValue;
    data << MaxValue;
    data << Regen;
    data << (uint8)0;
    data << (uint32)0; // Spell ID
    GetSession()->SendPacket(&data);
}

/**
 * @brief Stops a client mirror timer and clears its tracked value.
 *
 * @param Type The mirror timer type to stop.
 */
void Player::StopMirrorTimer(MirrorTimerType Type)
{
    m_MirrorTimer[Type] = DISABLED_MIRROR_TIMER;
    WorldPacket data(SMSG_STOP_MIRROR_TIMER, 4);
    data << (uint32)Type;
    GetSession()->SendPacket(&data);
}

/**
 * @brief Applies environmental damage to the player.
 *
 * @param type The environmental damage type.
 * @param damage The incoming damage amount.
 * @return The final damage dealt after mitigation.
 */
uint32 Player::EnvironmentalDamage(EnvironmentalDamageType type, uint32 damage)
{
    if (!IsAlive() || isGameMaster())
    {
        return 0;
    }

    // Absorb and resist some environmental damage types
    uint32 absorb = 0;
    uint32 resist = 0;
    if (type == DAMAGE_LAVA)
    {
        if (this->IsImmuneToDamage(SPELL_SCHOOL_MASK_FIRE))
        {
            return 0;
        }

        CalculateDamageAbsorbAndResist(this, SPELL_SCHOOL_MASK_FIRE, DIRECT_DAMAGE, damage, &absorb, &resist);
    }
    else if (type == DAMAGE_SLIME)
    {
        if (this->IsImmuneToDamage(SPELL_SCHOOL_MASK_NATURE))
        {
            return 0;
        }

        CalculateDamageAbsorbAndResist(this, SPELL_SCHOOL_MASK_NATURE, DIRECT_DAMAGE, damage, &absorb, &resist);
    }

    damage -= absorb + resist;

    DealDamageMods(this, damage, &absorb);

    WorldPacket data(SMSG_ENVIRONMENTALDAMAGELOG, (21));
    data << GetObjectGuid();
    data << uint8(type != DAMAGE_FALL_TO_VOID ? type : DAMAGE_FALL);
    data << uint32(damage);
    data << uint32(absorb);
    data << uint32(resist);
    SendMessageToSet(&data, true);

    DamageEffectType damageType = SELF_DAMAGE;
    if (type == DAMAGE_FALL && getClass() == CLASS_ROGUE)
    {
        damageType = SELF_DAMAGE_ROGUE_FALL;
    }

    uint32 final_damage = DealDamage(this, damage, NULL, damageType, SPELL_SCHOOL_MASK_NORMAL, NULL, false);

    if (type == DAMAGE_FALL && !IsAlive()) // DealDamage does not apply item durability loss at self-damage
    {
        DEBUG_LOG("We fell to death, losing 10 percent durability");
        DurabilityLossAll(0.10f, false);
        // Durability lost message
        WorldPacket data2(SMSG_DURABILITY_DAMAGE_DEATH, 0);
        GetSession()->SendPacket(&data2);
    }

    return final_damage;
}

/**
 * @brief Gets the maximum value for a mirror timer type.
 *
 * @param timer The mirror timer type.
 * @return The maximum timer value, or DISABLED_MIRROR_TIMER when inactive.
 */
int32 Player::getMaxTimer(MirrorTimerType timer)
{
    switch (timer)
    {
        case FATIGUE_TIMER:
            if (GetSession()->GetSecurity() >= (AccountTypes)sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_GMLEVEL))
            {
                return DISABLED_MIRROR_TIMER;
            }
            return sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FATIGUE_MAX) * IN_MILLISECONDS;
        case BREATH_TIMER:
        {
            if (!IsAlive() || HasAuraType(SPELL_AURA_WATER_BREATHING) ||
                GetSession()->GetSecurity() >= (AccountTypes)sWorld.getConfig(CONFIG_UINT32_TIMERBAR_BREATH_GMLEVEL))
            {
                return DISABLED_MIRROR_TIMER;
            }
            int32 UnderWaterTime = sWorld.getConfig(CONFIG_UINT32_TIMERBAR_BREATH_MAX) * IN_MILLISECONDS;
            AuraList const& mModWaterBreathing = GetAurasByType(SPELL_AURA_MOD_WATER_BREATHING);
            for (AuraList::const_iterator i = mModWaterBreathing.begin(); i != mModWaterBreathing.end(); ++i)
            {
                UnderWaterTime = uint32(UnderWaterTime * (100.0f + (*i)->GetModifier()->m_amount) / 100.0f);
            }
            return UnderWaterTime;
        }
        case FIRE_TIMER:
        {
            if (!IsAlive() || GetSession()->GetSecurity() >= (AccountTypes)sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FIRE_GMLEVEL))
            {
                return DISABLED_MIRROR_TIMER;
            }
            return sWorld.getConfig(CONFIG_UINT32_TIMERBAR_FIRE_MAX) * IN_MILLISECONDS;
        }
        default:
            return 0;
    }
    return 0;
}

/**
 * @brief Marks mirror timers for refresh during the next drowning update.
 */
void Player::UpdateMirrorTimers()
{
    // Desync flags for update on next HandleDrowning
    if (m_MirrorTimerFlags)
    {
        m_MirrorTimerFlagsLast = ~m_MirrorTimerFlags;
    }
}

/**
 * @brief Updates drowning, fatigue, and fire mirror timers.
 *
 * @param time_diff The elapsed update time in milliseconds.
 */
void Player::HandleDrowning(uint32 time_diff)
{
    // If no mirror timer flags are set, return early
    if (!m_MirrorTimerFlags)
    {
        return;
    }

    // Check if the player is in water
    if (m_MirrorTimerFlags & UNDERWATER_INWATER)
    {
        // If the breath timer is not activated, activate it
        if (m_MirrorTimer[BREATH_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[BREATH_TIMER] = getMaxTimer(BREATH_TIMER);
            SendMirrorTimer(BREATH_TIMER, m_MirrorTimer[BREATH_TIMER], m_MirrorTimer[BREATH_TIMER], -1);
        }
        else
        {
            // Decrease the breath timer
            m_MirrorTimer[BREATH_TIMER] -= time_diff;
            // If the timer reaches the limit, deal damage
            if (m_MirrorTimer[BREATH_TIMER] < 0)
            {
                m_MirrorTimer[BREATH_TIMER] += 2 * IN_MILLISECONDS;
                // Calculate and deal drowning damage
                // TODO: Check this formula
                uint32 damage = GetMaxHealth() / 5 + urand(0, getLevel() - 1);
                EnvironmentalDamage(DAMAGE_DROWNING, damage);
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INWATER)) // Update time in client if needed
            {
                SendMirrorTimer(BREATH_TIMER, getMaxTimer(BREATH_TIMER), m_MirrorTimer[BREATH_TIMER], -1);
            }
        }
    }
    else if (m_MirrorTimer[BREATH_TIMER] != DISABLED_MIRROR_TIMER) // Regenerate breath timer
    {
        int32 UnderWaterTime = getMaxTimer(BREATH_TIMER);
        // Need breath regen
        m_MirrorTimer[BREATH_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[BREATH_TIMER] >= UnderWaterTime || !IsAlive())
        {
            StopMirrorTimer(BREATH_TIMER);
        }
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INWATER)
        {
            SendMirrorTimer(BREATH_TIMER, UnderWaterTime, m_MirrorTimer[BREATH_TIMER], 10);
        }
    }

    // Check if the player is in dark water
    if (m_MirrorTimerFlags & UNDERWATER_INDARKWATER)
    {
        // If the fatigue timer is not activated, activate it
        if (m_MirrorTimer[FATIGUE_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[FATIGUE_TIMER] = getMaxTimer(FATIGUE_TIMER);
            SendMirrorTimer(FATIGUE_TIMER, m_MirrorTimer[FATIGUE_TIMER], m_MirrorTimer[FATIGUE_TIMER], -1);
        }
        else
        {
            // Decrease the fatigue timer
            m_MirrorTimer[FATIGUE_TIMER] -= time_diff;
            // If the timer reaches the limit, deal damage or teleport ghost to graveyard
            if (m_MirrorTimer[FATIGUE_TIMER] < 0)
            {
                m_MirrorTimer[FATIGUE_TIMER] += 2 * IN_MILLISECONDS;
                if (IsAlive()) // Calculate and deal exhaustion damage
                {
                    uint32 damage = GetMaxHealth() / 5 + urand(0, getLevel() - 1);
                    EnvironmentalDamage(DAMAGE_EXHAUSTED, damage);
                }
                else if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST)) // Teleport ghost to graveyard
                {
                    RepopAtGraveyard();
                }
            }
            else if (!(m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER))
            {
                SendMirrorTimer(FATIGUE_TIMER, getMaxTimer(FATIGUE_TIMER), m_MirrorTimer[FATIGUE_TIMER], -1);
            }
        }
    }
    else if (m_MirrorTimer[FATIGUE_TIMER] != DISABLED_MIRROR_TIMER) // Regenerate fatigue timer
    {
        int32 DarkWaterTime = getMaxTimer(FATIGUE_TIMER);
        m_MirrorTimer[FATIGUE_TIMER] += 10 * time_diff;
        if (m_MirrorTimer[FATIGUE_TIMER] >= DarkWaterTime || !IsAlive())
        {
            StopMirrorTimer(FATIGUE_TIMER);
        }
        else if (m_MirrorTimerFlagsLast & UNDERWATER_INDARKWATER)
        {
            SendMirrorTimer(FATIGUE_TIMER, DarkWaterTime, m_MirrorTimer[FATIGUE_TIMER], 10);
        }
    }

    // Check if the player is in lava or slime
    if (m_MirrorTimerFlags & (UNDERWATER_INLAVA /*| UNDERWATER_INSLIME*/) && !(m_lastLiquid && m_lastLiquid->SpellId))
    {
        // If the fire timer is not activated, activate it
        if (m_MirrorTimer[FIRE_TIMER] == DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimer[FIRE_TIMER] = getMaxTimer(FIRE_TIMER);
        }
        else
        {
            // Decrease the fire timer
            m_MirrorTimer[FIRE_TIMER] -= time_diff;
            if (m_MirrorTimer[FIRE_TIMER] < 0)
            {
                m_MirrorTimer[FIRE_TIMER] += 2 * IN_MILLISECONDS;
                // Calculate and deal fire damage
                // TODO: Check this formula
                uint32 damage = urand(600, 700);
                if (m_MirrorTimerFlags & UNDERWATER_INLAVA)
                {
                    EnvironmentalDamage(DAMAGE_LAVA, damage);
                }
                // Skip slime damage in Undercity
                // maybe someone can find better way to handle environmental damage
                //else if (m_zoneUpdateId != 1497)
                //    EnvironmentalDamage(DAMAGE_SLIME, damage);
            }
        }
    }
    else
    {
        m_MirrorTimer[FIRE_TIMER] = DISABLED_MIRROR_TIMER;
    }

    // Recheck timers flag
    m_MirrorTimerFlags &= ~UNDERWATER_EXIST_TIMERS;
    for (int i = 0; i < MAX_TIMERS; ++i)
    {
        if (m_MirrorTimer[i] != DISABLED_MIRROR_TIMER)
        {
            m_MirrorTimerFlags |= UNDERWATER_EXIST_TIMERS;
            break;
        }
    }
    m_MirrorTimerFlagsLast = m_MirrorTimerFlags;
}

/// The player sobers by 256 every 10 seconds
void Player::HandleSobering()
{
    m_drunkTimer = 0;

    // Decrease the drunk value by 256, or set to 0 if less than 256
    uint32 drunk = (m_drunk <= 256) ? 0 : (m_drunk - 256);
    SetDrunkValue(drunk);
}

/**
 * @brief Converts a drunk value to the corresponding drunken state.
 *
 * @param value The raw drunk value.
 * @return The resulting drunken state.
 */
DrunkenState Player::GetDrunkenstateByValue(uint16 value)
{
    // Determine the drunken state based on the drunk value
    if (value >= 23000)
    {
        return DRUNKEN_SMASHED;
    }
    if (value >= 12800)
    {
        return DRUNKEN_DRUNK;
    }
    if (value & 0xFFFE)
    {
        return DRUNKEN_TIPSY;
    }
    return DRUNKEN_SOBER;
}

/**
 * @brief Sets the player's drunk value and updates related visibility effects.
 *
 * @param newDrunkenValue The new drunk value.
 * @param itemId Unused source item identifier.
 */
void Player::SetDrunkValue(uint16 newDrunkenValue, uint32 /*itemId*/)
{
    // Get the old drunken state
    uint32 oldDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    // Set the new drunk value
    m_drunk = newDrunkenValue;
    SetUInt16Value(PLAYER_BYTES_3, 0, uint16(getGender()) | (m_drunk & 0xFFFE));

    // Get the new drunken state
    uint32 newDrunkenState = Player::GetDrunkenstateByValue(m_drunk);

    // Special drunk invisibility detection
    if (newDrunkenState >= DRUNKEN_DRUNK)
    {
        m_detectInvisibilityMask |= (1 << 6);
    }
    else
    {
        m_detectInvisibilityMask &= ~(1 << 6);
    }
}
