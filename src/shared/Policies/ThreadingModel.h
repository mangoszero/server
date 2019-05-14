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

#ifndef MANGOS_THREADINGMODEL_H
#define MANGOS_THREADINGMODEL_H

/**
 * @class ThreadingModel<T>
 *
 */

#include "Platform/Define.h"

namespace MaNGOS
{
    template<typename MUTEX>
    /**
     * @brief
     *
     */
    class GeneralLock
    {
        public:

            /**
             * @brief
             *
             * @param m
             */
            GeneralLock(MUTEX& m)
                : i_mutex(m)
            {
                i_mutex.acquire();
            }

            /**
             * @brief
             *
             */
            ~GeneralLock()
            {
                i_mutex.release();
            }

        private:

            /**
             * @brief
             *
             * @param
             */
            GeneralLock(const GeneralLock&);
            /**
             * @brief
             *
             * @param
             * @return GeneralLock &operator
             */
            GeneralLock& operator=(const GeneralLock&);
            MUTEX& i_mutex; /**< TODO */
    };

    template<class T>
    /**
     * @brief
     *
     */
    class SingleThreaded
    {
        public:

            /**
             * @brief empty object
             *
             */
            struct Lock
            {
                /**
                 * @brief
                 *
                 */
                Lock()
                {
                }
                /**
                 * @brief
                 *
                 * @param
                 */
                Lock(const T&)
                {
                }

                /**
                 * @brief for single threaded we ignore this
                 *
                 * @param
                 */
                Lock(const SingleThreaded<T>&)
                {
                }
            };
    };

    template<class T, class MUTEX>
    /**
     * @brief
     *
     */
    class ObjectLevelLockable
    {
        public:

            /**
             * @brief
             *
             */
            ObjectLevelLockable()
                : i_mtx()
            {
            }

            friend class Lock;

            /**
             * @brief
             *
             */
            class Lock
            {
                public:

                    /**
                     * @brief
                     *
                     * @param ObjectLevelLockable<T
                     * @param host
                     */
                    Lock(ObjectLevelLockable<T, MUTEX>& host)
                        : i_lock(host.i_mtx)
                    {
                    }

                private:

                    GeneralLock<MUTEX> i_lock; /**< TODO */
            };

        private:

            /**
             * @brief prevent the compiler creating a copy construct
             *
             * @param ObjectLevelLockable<T
             * @param
             */
            ObjectLevelLockable(const ObjectLevelLockable<T, MUTEX>&);
            /**
             * @brief
             *
             * @param ObjectLevelLockable<T
             * @param
             * @return ObjectLevelLockable<T, MUTEX>
             */
            ObjectLevelLockable<T, MUTEX>& operator=(const ObjectLevelLockable<T, MUTEX>&);

            MUTEX i_mtx; /**< TODO */
    };

    template<class T, class MUTEX>
    /**
     * @brief
     *
     */
    class ClassLevelLockable
    {
        public:

            /**
             * @brief
             *
             */
            ClassLevelLockable()
            {
            }

            friend class Lock;

            /**
             * @brief
             *
             */
            class Lock
            {
                public:

                    /**
                     * @brief
                     *
                     * @param
                     */
                    Lock(const T& /*host*/)
                    {
                        ClassLevelLockable<T, MUTEX>::si_mtx.acquire();
                    }

                    /**
                     * @brief
                     *
                     * @param ClassLevelLockable<T
                     * @param
                     */
                    Lock(const ClassLevelLockable<T, MUTEX>&)
                    {
                        ClassLevelLockable<T, MUTEX>::si_mtx.acquire();
                    }

                    /**
                     * @brief
                     *
                     */
                    Lock()
                    {
                        ClassLevelLockable<T, MUTEX>::si_mtx.acquire();
                    }

                    /**
                     * @brief
                     *
                     */
                    ~Lock()
                    {
                        ClassLevelLockable<T, MUTEX>::si_mtx.release();
                    }
            };

        private:

            static MUTEX si_mtx; /**< TODO */
    };

}

template<class T, class MUTEX> MUTEX MaNGOS::ClassLevelLockable<T, MUTEX>::si_mtx; /**< TODO */

#define INSTANTIATE_CLASS_MUTEX(CTYPE, MUTEX) \
    template class MaNGOS::ClassLevelLockable<CTYPE, MUTEX>

#endif
