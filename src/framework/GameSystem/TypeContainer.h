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

#ifndef MANGOS_TYPECONTAINER_H
#define MANGOS_TYPECONTAINER_H

/*
 * Here, you'll find a series of containers that allow you to hold multiple
 * types of object at the same time.
 */

#include <cassert>
#include <map>
#include <vector>
#include "Platform/Define.h"
#include "Utilities/TypeList.h"
#include "Utilities/UnorderedMapSet.h"
#include "GameSystem/GridRefManager.h"

template<class OBJECT, class KEY_TYPE>
/**
 * @brief
 *
 */
struct ContainerUnorderedMap
{
    UNORDERED_MAP<KEY_TYPE, OBJECT*> _element; /**< TODO */
};

template<class KEY_TYPE>
/**
 * @brief
 *
 */
struct ContainerUnorderedMap<TypeNull, KEY_TYPE>
{
};

template<class H, class T, class KEY_TYPE>
/**
 * @brief
 *
 */
struct ContainerUnorderedMap< TypeList<H, T>, KEY_TYPE >
{
    ContainerUnorderedMap<H, KEY_TYPE> _elements; /**< TODO */
    ContainerUnorderedMap<T, KEY_TYPE> _TailElements; /**< TODO */
};

template < class OBJECT_TYPES, class KEY_TYPE = OBJECT_HANDLE >
/**
 * @brief
 *
 */
class TypeUnorderedMapContainer
{
    public:

        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param handle
         * @param obj
         * @return bool
         */
        bool insert(KEY_TYPE handle, SPECIFIC_TYPE* obj)
        {
            return TypeUnorderedMapContainer::insert(i_elements, handle, obj);
        }

        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param handle
         * @param
         * @return bool
         */
        bool erase(KEY_TYPE handle, SPECIFIC_TYPE* /*obj*/)
        {
            return TypeUnorderedMapContainer::erase(i_elements, handle, (SPECIFIC_TYPE*)NULL);
        }

        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param hdl
         * @param
         * @return SPECIFIC_TYPE
         */
        SPECIFIC_TYPE* find(KEY_TYPE hdl, SPECIFIC_TYPE* /*obj*/)
        {
            return TypeUnorderedMapContainer::find(i_elements, hdl, (SPECIFIC_TYPE*)NULL);
        }

    private:

        ContainerUnorderedMap<OBJECT_TYPES, KEY_TYPE> i_elements; /**< TODO */

        // Helpers
        template<class SPECIFIC_TYPE>
        /**
         * @brief Insert helpers
         *
         * @param ContainerUnorderedMap<SPECIFIC_TYPE
         * @param elements
         * @param handle
         * @param obj
         * @return bool
         */
        static bool insert(ContainerUnorderedMap<SPECIFIC_TYPE, KEY_TYPE>& elements, KEY_TYPE handle, SPECIFIC_TYPE* obj)
        {
            typename UNORDERED_MAP<KEY_TYPE, SPECIFIC_TYPE*>::iterator i = elements._element.find(handle);
            if (i == elements._element.end())
            {
                elements._element[handle] = obj;
                return true;
            }
            else
            {
                assert(i->second == obj && "Object with certain key already in but objects are different!");
                return false;
            }
        }

        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<TypeNull
         * @param
         * @param KEY_TYPE
         * @param
         * @return bool
         */
        static bool insert(ContainerUnorderedMap<TypeNull, KEY_TYPE>& /*elements*/, KEY_TYPE /*handle*/, SPECIFIC_TYPE* /*obj*/)
        {
            return false;
        }

        template<class SPECIFIC_TYPE, class T>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<T
         * @param
         * @param KEY_TYPE
         * @param
         * @return bool
         */
        static bool insert(ContainerUnorderedMap<T, KEY_TYPE>& /*elements*/, KEY_TYPE /*handle*/, SPECIFIC_TYPE* /*obj*/)
        {
            return false;
        }

        template<class SPECIFIC_TYPE, class H, class T>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<TypeList<H
         * @param T>
         * @param elements
         * @param handle
         * @param obj
         * @return bool
         */
        static bool insert(ContainerUnorderedMap< TypeList<H, T>, KEY_TYPE >& elements, KEY_TYPE handle, SPECIFIC_TYPE* obj)
        {
            bool ret = TypeUnorderedMapContainer::insert(elements._elements, handle, obj);
            return ret ? ret : TypeUnorderedMapContainer::insert(elements._TailElements, handle, obj);
        }

        // Find helpers
        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<SPECIFIC_TYPE
         * @param elements
         * @param hdl
         * @param
         * @return SPECIFIC_TYPE
         */
        static SPECIFIC_TYPE* find(ContainerUnorderedMap<SPECIFIC_TYPE, KEY_TYPE>& elements, KEY_TYPE hdl, SPECIFIC_TYPE* /*obj*/)
        {
            typename UNORDERED_MAP<KEY_TYPE, SPECIFIC_TYPE*>::iterator i = elements._element.find(hdl);
            if (i == elements._element.end())
                { return NULL; }
            else
                { return i->second; }
        }

        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<TypeNull
         * @param
         * @param KEY_TYPE
         * @param
         * @return SPECIFIC_TYPE
         */
        static SPECIFIC_TYPE* find(ContainerUnorderedMap<TypeNull, KEY_TYPE>& /*elements*/, KEY_TYPE /*hdl*/, SPECIFIC_TYPE* /*obj*/)
        {
            return NULL;
        }

        template<class SPECIFIC_TYPE, class T>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<T
         * @param
         * @param KEY_TYPE
         * @param
         * @return SPECIFIC_TYPE
         */
        static SPECIFIC_TYPE* find(ContainerUnorderedMap<T, KEY_TYPE>& /*elements*/, KEY_TYPE /*hdl*/, SPECIFIC_TYPE* /*obj*/)
        {
            return NULL;
        }

        template<class SPECIFIC_TYPE, class H, class T>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<TypeList<H
         * @param T>
         * @param elements
         * @param hdl
         * @param
         * @return SPECIFIC_TYPE
         */
        static SPECIFIC_TYPE* find(ContainerUnorderedMap< TypeList<H, T>, KEY_TYPE >& elements, KEY_TYPE hdl, SPECIFIC_TYPE* /*obj*/)
        {
            SPECIFIC_TYPE* ret = TypeUnorderedMapContainer::find(elements._elements, hdl, (SPECIFIC_TYPE*)NULL);
            return ret ? ret : TypeUnorderedMapContainer::find(elements._TailElements, hdl, (SPECIFIC_TYPE*)NULL);
        }

        // Erase helpers
        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<SPECIFIC_TYPE
         * @param elements
         * @param handle
         * @param
         * @return bool
         */
        static bool erase(ContainerUnorderedMap<SPECIFIC_TYPE, KEY_TYPE>& elements, KEY_TYPE handle, SPECIFIC_TYPE* /*obj*/)
        {
            elements._element.erase(handle);

            return true;
        }

        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<TypeNull
         * @param
         * @param KEY_TYPE
         * @param
         * @return bool
         */
        static bool erase(ContainerUnorderedMap<TypeNull, KEY_TYPE>& /*elements*/, KEY_TYPE /*handle*/, SPECIFIC_TYPE* /*obj*/)
        {
            return false;
        }

        template<class SPECIFIC_TYPE, class T>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<T
         * @param
         * @param KEY_TYPE
         * @param
         * @return bool
         */
        static bool erase(ContainerUnorderedMap<T, KEY_TYPE>& /*elements*/, KEY_TYPE /*handle*/, SPECIFIC_TYPE* /*obj*/)
        {
            return false;
        }

        template<class SPECIFIC_TYPE, class H, class T>
        /**
         * @brief
         *
         * @param ContainerUnorderedMap<TypeList<H
         * @param T>
         * @param elements
         * @param handle
         * @param
         * @return bool
         */
        static bool erase(ContainerUnorderedMap< TypeList<H, T>, KEY_TYPE >& elements, KEY_TYPE handle, SPECIFIC_TYPE* /*obj*/)
        {
            bool ret = TypeUnorderedMapContainer::erase(elements._elements, handle, (SPECIFIC_TYPE*)NULL);
            return ret ? ret : TypeUnorderedMapContainer::erase(elements._TailElements, handle, (SPECIFIC_TYPE*)NULL);
        }
};

template<class OBJECT>
/**
 * @brief ontainerMapList is a multi-type container for map elements
 *
 * By itself its meaningless but collaborate along with TypeContainers,
 * it become the most powerfully container in the whole system.
 *
 */
struct ContainerMapList
{
    GridRefManager<OBJECT> _element; /**< TODO */
};

template<>
/**
 * @brief
 *
 */
struct ContainerMapList<TypeNull>                           /* nothing is in type null */
{
};

template<class H, class T>
/**
 * @brief
 *
 */
struct ContainerMapList<TypeList<H, T> >
{
    ContainerMapList<H> _elements; /**< TODO */
    ContainerMapList<T> _TailElements; /**< TODO */
};

#include "TypeContainerFunctions.h"

template<class OBJECT_TYPES>
/**
 * @brief TypeMapContainer contains a fixed number of types and is determined at compile time.
 *
 * This is probably the most complicated class and do its simplest thing: holds
 * objects of different types.
 *
 */
class TypeMapContainer
{
    public:

        template<class SPECIFIC_TYPE>
        /**
         * @brief
         *
         * @return size_t
         */
        size_t Count() const { return MaNGOS::Count(i_elements, (SPECIFIC_TYPE*)NULL); }

        template<class SPECIFIC_TYPE>
        /**
         * @brief inserts a specific object into the container
         *
         * @param obj
         * @return bool
         */
        bool insert(SPECIFIC_TYPE* obj)
        {
            SPECIFIC_TYPE* t = MaNGOS::Insert(i_elements, obj);
            return (t != NULL);
        }

        template<class SPECIFIC_TYPE>
        /**
         * @brief Removes the object from the container, and returns the removed object
         *
         * @param obj
         * @return bool
         */
        bool remove(SPECIFIC_TYPE* obj)
        {
            SPECIFIC_TYPE* t = MaNGOS::Remove(i_elements, obj);
            return (t != NULL);
        }

        /**
         * @brief
         *
         * @return ContainerMapList<OBJECT_TYPES>
         */
        ContainerMapList<OBJECT_TYPES>& GetElements() { return i_elements; }
        /**
         * @brief
         *
         * @return const ContainerMapList<OBJECT_TYPES>
         */
        const ContainerMapList<OBJECT_TYPES>& GetElements() const { return i_elements;}

    private:

        ContainerMapList<OBJECT_TYPES> i_elements; /**< TODO */
};

#endif
