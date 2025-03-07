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

#include "IdleMovementGenerator.h"
#include "CreatureAI.h"
#include "Creature.h"

// Global instance of IdleMovementGenerator
IdleMovementGenerator si_idleMovement;

/**
 * @brief Resets the IdleMovementGenerator.
 * @param owner Reference to the unit.
 */
void IdleMovementGenerator::Reset(Unit& /*owner*/)
{
}

/**
 * @brief Initializes the DistractMovementGenerator.
 * @param owner Reference to the unit.
 */
void DistractMovementGenerator::Initialize(Unit& owner)
{
    owner.addUnitState(UNIT_STAT_DISTRACTED);
}

/**
 * @brief Finalizes the DistractMovementGenerator.
 * @param owner Reference to the unit.
 */
void DistractMovementGenerator::Finalize(Unit& owner)
{
    owner.clearUnitState(UNIT_STAT_DISTRACTED);
}

/**
 * @brief Resets the DistractMovementGenerator.
 * @param owner Reference to the unit.
 */
void DistractMovementGenerator::Reset(Unit& owner)
{
    Initialize(owner);
}

/**
 * @brief Interrupts the DistractMovementGenerator.
 * @param owner Reference to the unit.
 */
void DistractMovementGenerator::Interrupt(Unit& /*owner*/)
{
}

/**
 * @brief Updates the DistractMovementGenerator.
 * @param owner Reference to the unit.
 * @param time_diff Time difference.
 * @return True if the update was successful, false otherwise.
 */
bool DistractMovementGenerator::Update(Unit& /*owner*/, const uint32& time_diff)
{
    if (time_diff > m_timer)
    {
        return false;
    }

    m_timer -= time_diff;
    return true;
}

/**
 * @brief Finalizes the AssistanceDistractMovementGenerator.
 * @param unit Reference to the unit.
 */
void AssistanceDistractMovementGenerator::Finalize(Unit& unit)
{
    unit.clearUnitState(UNIT_STAT_DISTRACTED);
    if (Unit* victim = unit.getVictim())
    {
        if (unit.IsAlive())
        {
            unit.AttackStop(true);
            ((Creature*)&unit)->AI()->AttackStart(victim);
        }
    }
}
