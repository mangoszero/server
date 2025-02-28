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

#ifndef MANGOS_SINGLETON_H
#define MANGOS_SINGLETON_H

/**
 * @brief class Singleton
 */

#include "CreationPolicy.h"
#include "ThreadingModel.h"
#include "ObjectLifeTime.h"

namespace MaNGOS
{
    template
    <
    typename T,
             class ThreadingModel = MaNGOS::SingleThreaded<T>,
             class CreatePolicy = MaNGOS::OperatorNew<T>,
             class LifeTimePolicy = MaNGOS::ObjectLifeTime<T>
             >
    /**
     * @brief Singleton class template
     *
     * This class provides a thread-safe singleton implementation.
     */
    class Singleton
    {
        public:

            /**
             * @brief Get the singleton instance
             *
             * @return T& Reference to the singleton instance
             */
            static T& Instance();

        protected:

            /**
             * @brief Protected constructor to prevent instantiation
             */
            Singleton()
            {
            }

        private:

            /**
             * @brief Prohibited copy constructor
             *
             * @param other The other instance to copy from
             */
            Singleton(const Singleton&);

            /**
             * @brief Prohibited assignment operator
             *
             * @param other The other instance to assign from
             * @return Singleton& Reference to this instance
             */
            Singleton& operator=(const Singleton&);

            /**
             * @brief Destroy the singleton instance
             */
            static void DestroySingleton();

            /**
             * @brief Type alias for the threading model's lock
             */
            typedef typename ThreadingModel::Lock Guard;

            /**
             * @brief Pointer to the singleton instance
             */
            static T* si_instance;

            /**
             * @brief Flag indicating if the singleton has been destroyed
             */
            static bool si_destroyed;
    };

    template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
    T* Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::si_instance = NULL; /**< Initialize singleton instance pointer to NULL */

    template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
    bool Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::si_destroyed = false; /**< Initialize destroyed flag to false */

    template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
    /**
     * @brief Get the singleton instance
     *
     * @return T& Reference to the singleton instance
     */
    T& MaNGOS::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::Instance()
    {
        if (!si_instance)
        {
            // double-checked Locking pattern
            Guard guard; // Named variable

            if (!si_instance)
            {
                if (si_destroyed)
                {
                    si_destroyed = false;
                    LifeTimePolicy::OnDeadReference();
                }

                si_instance = CreatePolicy::Create();
                LifeTimePolicy::ScheduleCall(&DestroySingleton);
            }
        }

        return *si_instance;
    }

    template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
    /**
     * @brief Destroy the singleton instance
     */
    void MaNGOS::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::DestroySingleton()
    {
        CreatePolicy::Destroy(si_instance);
        si_instance = NULL;
        si_destroyed = true;
    }
}

#define INSTANTIATE_SINGLETON_1(TYPE) \
    template class MaNGOS::Singleton<TYPE, MaNGOS::SingleThreaded<TYPE>, MaNGOS::OperatorNew<TYPE>, MaNGOS::ObjectLifeTime<TYPE> >;

#define INSTANTIATE_SINGLETON_2(TYPE, THREADINGMODEL) \
    template class MaNGOS::Singleton<TYPE, THREADINGMODEL, MaNGOS::OperatorNew<TYPE>, MaNGOS::ObjectLifeTime<TYPE> >;

#define INSTANTIATE_SINGLETON_3(TYPE, THREADINGMODEL, CREATIONPOLICY ) \
    template class MaNGOS::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, MaNGOS::ObjectLifeTime<TYPE> >;

#define INSTANTIATE_SINGLETON_4(TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME) \
    template class MaNGOS::Singleton<TYPE, THREADINGMODEL, CREATIONPOLICY, OBJECTLIFETIME >;

#endif
