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
 * @file GridStates.cpp
 * @brief Grid state machine implementation for map cell management
 *
 * This file implements the state pattern for map grid lifecycle management.
 * Each grid (64x64 yard cell) transitions through states based on player
 * presence and activity:
 *
 * State transitions:
 * - INVALID_STATE: Initial state, no processing
 * - ACTIVE_STATE: Players nearby, all objects active and updated
 * - IDLE_STATE: No players nearby, objects suspended
 * - REMOVAL_STATE: Ready to unload from memory
 *
 * The state machine ensures efficient memory usage by unloading inactive
 * areas while keeping active areas responsive.
 *
 * @see GridState for the state interface
 * @see NGridType for the grid container
 * @see Map for the grid owner
 */

#include "GridStates.h"
#include "ObjectGridLoader.h"
#include "Log.h"

/**
 * @brief Destroy grid state
 *
 * Virtual destructor for the grid state base class.
 * Logs destruction for debugging purposes.
 */
GridState::~GridState()
{
    DEBUG_LOG("GridState destroyed");
}

/**
 * @brief Update invalid state - no operation
 * @param m Map reference (unused)
 * @param grid Grid container (unused)
 * @param info Grid info (unused)
 * @param x Grid X coordinate (unused)
 * @param y Grid Y coordinate (unused)
 * @param t_diff Time delta (unused)
 *
 * Invalid state is a placeholder/initial state that performs no actions.
 * Used as the default state before proper state assignment.
 */
void
InvalidState::Update(Map&, NGridType&, GridInfo&, const uint32& /*x*/, const uint32& /*y*/, const uint32&) const
{
}

/**
 * @brief Update active grid state
 * @param m Map containing the grid
 * @param grid The grid being updated
 * @param info Grid timing information
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param t_diff Time delta in milliseconds
 *
 * Active state monitors for player/activity absence to transition to idle.
 * Only checks periodically (every grid_expiry/10 ms) to avoid overhead.
 *
 * Transition to IDLE_STATE occurs when:
 * - No active objects in the grid
 * - No active objects in adjacent grids
 *
 * Otherwise, resets the expiry timer to keep the grid active.
 */
void
ActiveState::Update(Map& m, NGridType& grid, GridInfo& info, const uint32& x, const uint32& y, const uint32& t_diff) const
{
    // Only check grid activity every (grid_expiry/10) ms to avoid overhead
    info.UpdateTimeTracker(t_diff);
    if (info.getTimeTracker().Passed())
    {
        if (grid.ActiveObjectsInGrid() == 0 && !m.ActiveObjectsNearGrid(x, y))
        {
            ObjectGridStoper stoper(grid);
            stoper.StopN();
            grid.SetGridState(GRID_STATE_IDLE);
        }
        else
        {
            m.ResetGridExpiry(grid, 0.1f);
        }
    }
}

/**
 * @brief Update idle grid state
 * @param m Map containing the grid
 * @param grid The grid being updated
 * @param info Grid timing information (unused)
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param t_diff Time delta (unused)
 *
 * Idle state prepares the grid for potential removal. Objects are suspended
 * but not yet unloaded. Resets the expiry timer and transitions to
 * REMOVAL_STATE for potential unloading.
 */
void
IdleState::Update(Map& m, NGridType& grid, GridInfo&, const uint32& x, const uint32& y, const uint32&) const
{
    m.ResetGridExpiry(grid);
    grid.SetGridState(GRID_STATE_REMOVAL);
    DEBUG_LOG("Grid[%u,%u] on map %u moved to IDLE state", x, y, m.GetId());
}

/**
 * @brief Update removal state - attempt to unload grid
 * @param m Map containing the grid
 * @param grid The grid being updated
 * @param info Grid timing and lock information
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param t_diff Time delta in milliseconds
 *
 * Removal state attempts to unload the grid from memory to free resources.
 * Only unloads if:
 * - Grid is not locked (getUnloadLock() returns false)
 * - Expiry timer has passed
 * - No players or active objects remain
 *
 * If unloading is deferred (objects nearby), resets expiry for retry later.
 */
void
RemovalState::Update(Map& m, NGridType& grid, GridInfo& info, const uint32& x, const uint32& y, const uint32& t_diff) const
{
    if (!info.getUnloadLock())
    {
        info.UpdateTimeTracker(t_diff);
        if (info.getTimeTracker().Passed())
        {
            if (!m.UnloadGrid(x, y, false))
            {
                DEBUG_LOG("Grid[%u,%u] for map %u differed unloading due to players or active objects nearby", x, y, m.GetId());
                m.ResetGridExpiry(grid);
            }
        }
    }
}
