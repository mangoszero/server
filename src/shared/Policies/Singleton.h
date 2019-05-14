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
     * @brief
     *
     */
    class Singleton
    {
        public:

            /**
             * @brief
             *
             * @return T
             */
            static T& Instance();

        protected:

            /**
             * @brief
             *
             */
            Singleton()
            {
            }

        private:

            /**
             * @brief Prohibited actions...this does not prevent hijacking.
             *
             * @param
             */
            Singleton(const Singleton&);
            /**
             * @brief
             *
             * @param
             * @return Singleton &operator
             */
            Singleton& operator=(const Singleton&);

            /**
             * @brief Singleton Helpers
             *
             */
            static void DestroySingleton();

            /**
             * @brief data structure
             *
             */
            typedef typename ThreadingModel::Lock Guard;
            static T* si_instance; /**< TODO */
            static bool si_destroyed; /**< TODO */
    };

    template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
    T* Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::si_instance = NULL; /**< TODO */

    template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
    bool Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::si_destroyed = false; /**< TODO */

    template<typename T, class ThreadingModel, class CreatePolicy, class LifeTimePolicy>
    /**
     * @brief
     *
     * @return T &MaNGOS::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>
     */
    T& MaNGOS::Singleton<T, ThreadingModel, CreatePolicy, LifeTimePolicy>::Instance()
    {
        if (!si_instance)
        {
            // double-checked Locking pattern
            Guard();

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
     * @brief
     *
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
