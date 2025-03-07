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

#include "HomeMovementGenerator.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"

/**
 * @brief Initializes the HomeMovementGenerator by setting the target location for the creature.
 * @param owner Reference to the creature.
 */
void HomeMovementGenerator<Creature>::Initialize(Creature& owner)
{
    _setTargetLocation(owner);
}

/**
 * @brief Resets the HomeMovementGenerator.
 * @param owner Reference to the creature.
 */
void HomeMovementGenerator<Creature>::Reset(Creature&)
{
}

/**
 * @brief Sets the target location for the creature to move to its home position.
 * @param owner Reference to the creature.
 */
void HomeMovementGenerator<Creature>::_setTargetLocation(Creature& owner)
{
    if (owner.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    Movement::MoveSplineInit init(owner);
    float x, y, z, o;
    // If the motion master is empty or cannot get the reset position, use the respawn coordinates
    if (owner.GetMotionMaster()->empty() || !owner.GetMotionMaster()->top()->GetResetPosition(owner, x, y, z, o))
    {
        owner.GetRespawnCoord(x, y, z, &o);
    }
    init.SetFacing(o);
    init.MoveTo(x, y, z, true);
    init.SetWalk(false);
    init.Launch();

    arrived = false;
    owner.clearUnitState(UNIT_STAT_ALL_DYN_STATES);
}

/**
 * @brief Updates the HomeMovementGenerator.
 * @param owner Reference to the creature.
 * @param time_diff Time difference.
 * @return True if the update was successful, false otherwise.
 */
bool HomeMovementGenerator<Creature>::Update(Creature& owner, const uint32& /*time_diff*/)
{
    arrived = owner.movespline->Finalized();
    return !arrived;
}

/**
 * @brief Finalizes the HomeMovementGenerator.
 * @param owner Reference to the creature.
 */
void HomeMovementGenerator<Creature>::Finalize(Creature& owner)
{
    if (arrived)
    {
        if (owner.GetTemporaryFactionFlags() & TEMPFACTION_RESTORE_REACH_HOME)
        {
            owner.ClearTemporaryFaction();
        }

        owner.SetWalk(!owner.hasUnitState(UNIT_STAT_RUNNING_STATE) && !owner.IsLevitating(), false);
        owner.LoadCreatureAddon(true);
        owner.AI()->JustReachedHome();
    }
}
