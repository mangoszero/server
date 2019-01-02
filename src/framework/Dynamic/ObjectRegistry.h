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

#ifndef MANGOS_OBJECTREGISTRY_H
#define MANGOS_OBJECTREGISTRY_H

#include "Platform/Define.h"
#include "Utilities/UnorderedMapSet.h"
#include "Policies/Singleton.h"

#include <string>
#include <vector>
#include <map>

template < class T, class Key = std::string >
/**
 * @brief ObjectRegistry holds all registry item of the same type
 *
 */
class ObjectRegistry
{
    public:
        /**
         * @brief
         *
         */
        typedef std::map<Key, T*> RegistryMapType;

        /**
         * @brief Returns a registry item
         *
         * @param key
         * @return const T
         */
        const T* GetRegistryItem(Key key) const
        {
            typename RegistryMapType::const_iterator iter = i_registeredObjects.find(key);
            return(iter == i_registeredObjects.end() ? NULL : iter->second);
        }

        /**
         * @brief Inserts a registry item
         *
         * @param obj
         * @param key
         * @param replace
         * @return bool
         */
        bool InsertItem(T* obj, Key key, bool replace = false)
        {
            typename RegistryMapType::iterator iter = i_registeredObjects.find(key);
            if (iter != i_registeredObjects.end())
            {
                if (!replace)
                    { return false; }
                delete iter->second;
                i_registeredObjects.erase(iter);
            }

            i_registeredObjects[key] = obj;
            return true;
        }

        /**
         * @brief Removes a registry item
         *
         * @param key
         * @param delete_object
         */
        void RemoveItem(Key key, bool delete_object = true)
        {
            typename RegistryMapType::iterator iter = i_registeredObjects.find(key);
            if (iter != i_registeredObjects.end())
            {
                if (delete_object)
                    { delete iter->second; }
                i_registeredObjects.erase(iter);
            }
        }

        /**
         * @brief Returns true if registry contains an item
         *
         * @param key
         * @return bool
         */
        bool HasItem(Key key) const
        {
            return (i_registeredObjects.find(key) != i_registeredObjects.end());
        }

        /**
         * @brief Inefficiently return a vector of registered items
         *
         * @param l
         * @return unsigned int
         */
        unsigned int GetRegisteredItems(std::vector<Key>& l) const
        {
            unsigned int sz = l.size();
            l.resize(sz + i_registeredObjects.size());
            for (typename RegistryMapType::const_iterator iter = i_registeredObjects.begin(); iter != i_registeredObjects.end(); ++iter)
                { l[sz++] = iter->first; }
            return i_registeredObjects.size();
        }

        /**
         * @brief Return the map of registered items
         *
         * @return const RegistryMapType
         */
        RegistryMapType const& GetRegisteredItems() const
        {
            return i_registeredObjects;
        }

    private:
        RegistryMapType i_registeredObjects; /**< TODO */
        friend class MaNGOS::OperatorNew<ObjectRegistry<T, Key> >;

        /**
         * @brief protected for friend use since it should be a singleton
         *
         */
        ObjectRegistry() {}
        /**
         * @brief
         *
         */
        ~ObjectRegistry()
        {
            for (typename RegistryMapType::iterator iter = i_registeredObjects.begin(); iter != i_registeredObjects.end(); ++iter)
                { delete iter->second; }
            i_registeredObjects.clear();
        }
};
#endif
