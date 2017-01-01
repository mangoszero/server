/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2017  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOS_GRID_H
#define MANGOS_GRID_H

#include "Platform/Define.h"
#include "Policies/ThreadingModel.h"
#include "TypeContainer.h"
#include "TypeContainerVisitor.h"

// forward declaration
template<class A, class T, class O> class GridLoader;

template
<
class ACTIVE_OBJECT,
      class WORLD_OBJECT_TYPES,
      class GRID_OBJECT_TYPES
      >
/**
 * @brief Grid is a logical segment of the game world represented inside MaNGOS.
 *
 * Grid is bind at compile time to a particular type of object which
 * we call it the object of interested.  There are many types of loader,
 * specially, dynamic loader, static loader, or on-demand loader.  There's
 * a subtle difference between dynamic loader and on-demand loader but
 * this is implementation specific to the loader class.  From the
 * Grid's perspective, the loader meets its API requirement is suffice.
 */
class Grid
{
        // allows the GridLoader to access its internals
        template<class A, class T, class O> friend class GridLoader;

    public:

        /**
         * @brief destructor to clean up its resources. This includes unloading
         * the grid if it has not been unload.
         *
         */
        ~Grid() {}

        template<class SPECIFIC_OBJECT>
        /**
         * @brief an object of interested enters the grid
         *
         * @param obj
         * @return bool
         */
        bool AddWorldObject(SPECIFIC_OBJECT* obj)
        {
            return i_objects.template insert<SPECIFIC_OBJECT>(obj);
        }

        template<class SPECIFIC_OBJECT>
        /**
         * @brief an object of interested exits the grid
         *
         * @param obj
         * @return bool
         */
        bool RemoveWorldObject(SPECIFIC_OBJECT* obj)
        {
            return i_objects.template remove<SPECIFIC_OBJECT>(obj);
        }

        template<class T>
        /**
         * @brief Grid visitor for grid objects
         *
         * @param TypeContainerVisitor<T
         * @param visitor
         */
        void Visit(TypeContainerVisitor<T, TypeMapContainer<GRID_OBJECT_TYPES> >& visitor)
        {
            visitor.Visit(i_container);
        }

        template<class T>
        /**
         * @brief Grid visitor for world objects
         *
         * @param TypeContainerVisitor<T
         * @param visitor
         */
        void Visit(TypeContainerVisitor<T, TypeMapContainer<WORLD_OBJECT_TYPES> >& visitor)
        {
            visitor.Visit(i_objects);
        }

        /**
         * @brief Returns the number of object within the grid.
         *
         * @return uint32
         */
        uint32 ActiveObjectsInGrid() const
        {
            return m_activeGridObjects.size() + i_objects.template Count<ACTIVE_OBJECT>();
        }

        template<class SPECIFIC_OBJECT>
        /**
         * @brief Inserts a container type object into the grid.
         *
         * @param obj
         * @return bool
         */
        bool AddGridObject(SPECIFIC_OBJECT* obj)
        {
            if (obj->IsActiveObject())
                { m_activeGridObjects.insert(obj); }

            return i_container.template insert<SPECIFIC_OBJECT>(obj);
        }

        template<class SPECIFIC_OBJECT>
        /**
         * @brief Removes a container type object from the grid
         *
         * @param obj
         * @return bool
         */
        bool RemoveGridObject(SPECIFIC_OBJECT* obj)
        {
            if (obj->IsActiveObject())
                { m_activeGridObjects.erase(obj); }

            return i_container.template remove<SPECIFIC_OBJECT>(obj);
        }

    private:

        TypeMapContainer<GRID_OBJECT_TYPES> i_container; /**< TODO */
        TypeMapContainer<WORLD_OBJECT_TYPES> i_objects; /**< TODO */
        /**
         * @brief
         *
         */
        typedef std::set<void*> ActiveGridObjects;
        ActiveGridObjects m_activeGridObjects; /**< TODO */
};

#endif
