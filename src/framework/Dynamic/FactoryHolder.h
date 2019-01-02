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

#ifndef MANGOS_FACTORY_HOLDER
#define MANGOS_FACTORY_HOLDER

#include "Platform/Define.h"
#include "Utilities/TypeList.h"
#include "ObjectRegistry.h"
#include "Policies/Singleton.h"

template < class T, class Key = std::string >
/**
 * @brief FactoryHolder holds a factory object of a specific type
 *
 */
class FactoryHolder
{
    public:
        /**
         * @brief
         *
         */
        typedef ObjectRegistry<FactoryHolder<T, Key >, Key > FactoryHolderRegistry;
        /**
         * @brief
         *
         */
        typedef MaNGOS::Singleton<FactoryHolderRegistry > FactoryHolderRepository;

        /**
         * @brief
         *
         * @param k
         */
        FactoryHolder(Key k) : i_key(k) {}
        /**
         * @brief
         *
         */
        virtual ~FactoryHolder() {}
        /**
         * @brief
         *
         * @return Key
         */
        inline Key key() const { return i_key; }

        /**
         * @brief
         *
         */
        void RegisterSelf(void) { FactoryHolderRepository::Instance().InsertItem(this, i_key); }
        /**
         * @brief
         *
         */
        void DeregisterSelf(void) { FactoryHolderRepository::Instance().RemoveItem(this, false); }

        /**
         * @brief Abstract Factory create method
         *
         * @param data
         * @return T
         */
        virtual T* Create(void* data = NULL) const = 0;
    private:
        Key i_key; /**< TODO */
};

template<class T>
/**
 * @brief Permissible is a classic way of letting the object decide whether how good they handle things.
 *
 * This is not retricted to factory selectors.
 */
class Permissible
{
    public:
        /**
         * @brief
         *
         */
        virtual ~Permissible() {}
        /**
         * @brief
         *
         * @param
         * @return int
         */
        virtual int Permit(const T*) const = 0;
};
#endif
