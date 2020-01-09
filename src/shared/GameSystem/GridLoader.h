/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOS_GRIDLOADER_H
#define MANGOS_GRIDLOADER_H

#include "Platform/Define.h"
#include "Grid.h"
#include "TypeContainerVisitor.h"

template
<
class ACTIVE_OBJECT,
      class WORLD_OBJECT_TYPES,
      class GRID_OBJECT_TYPES
      >
/**
 * @brief The GridLoader is working in conjuction with the Grid and responsible
 *        for loading and unloading object-types (one or more) when objects
 *        enters a grid.
 * Unloading is scheduled and might be canceled if an interested object re-enters.
 * GridLoader does not do the actual loading and unloading but implements as a
 * template pattern that delegates its loading and unloading for the actually
 * loader and unloader. GridLoader manages the grid (both local and remote).
 *
 */
class GridLoader
{
    public:

        template<class LOADER>
        /**
         * @brief Loads the grid
         *
         * @param Grid<ACTIVE_OBJECT
         * @param WORLD_OBJECT_TYPES
         * @param grid
         * @param loader
         */
        void Load(Grid<ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES>& grid, LOADER& loader)
        {
            loader.Load(grid);
        }

        template<class STOPER>
        /**
         * @brief Stop the grid
         *
         * @param Grid<ACTIVE_OBJECT
         * @param WORLD_OBJECT_TYPES
         * @param grid
         * @param stoper
         */
        void Stop(Grid<ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES>& grid, STOPER& stoper)
        {
            stoper.Stop(grid);
        }

        template<class UNLOADER>
        /**
         * @brief Unloads the grid
         *
         * @param Grid<ACTIVE_OBJECT
         * @param WORLD_OBJECT_TYPES
         * @param grid
         * @param unloader
         */
        void Unload(Grid<ACTIVE_OBJECT, WORLD_OBJECT_TYPES, GRID_OBJECT_TYPES>& grid, UNLOADER& unloader)
        {
            unloader.Unload(grid);
        }
};

#endif
