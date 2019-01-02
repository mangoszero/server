/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "GridStates.h"
#include "ObjectGridLoader.h"
#include "Log.h"

GridState::~GridState()
{
    DEBUG_LOG("GridState destroyed");
}

void
InvalidState::Update(Map&, NGridType&, GridInfo&, const uint32& /*x*/, const uint32& /*y*/, const uint32&) const
{
}

void
ActiveState::Update(Map& m, NGridType& grid, GridInfo& info, const uint32& x, const uint32& y, const uint32& t_diff) const
{
    // Only check grid activity every (grid_expiry/10) ms, because it's really useless to do it every cycle
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

void
IdleState::Update(Map& m, NGridType& grid, GridInfo&, const uint32& x, const uint32& y, const uint32&) const
{
    m.ResetGridExpiry(grid);
    grid.SetGridState(GRID_STATE_REMOVAL);
    DEBUG_LOG("Grid[%u,%u] on map %u moved to IDLE state", x, y, m.GetId());
}

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
