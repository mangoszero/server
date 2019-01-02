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

#pragma once

#include "BIH.h"
#include <G3D/Table.h>
#include <G3D/Array.h>
#include <G3D/Set.h>

template<class T, class BoundsFunc = BoundsTrait<T> >
/**
 * @brief
 *
 */
class BIHWrap
{
        template<class RayCallback>
        /**
         * @brief
         *
         */
        struct MDLCallback
        {
            RayCallback& cb; /**< TODO */
            const T* const* objects; /**< TODO */
            uint32 objects_size;

            /**
             * @brief
             *
             * @param callback
             * @param constobjects_array
             */
            MDLCallback(RayCallback& callback, const T* const* obj_array, uint32 obj_size ) : cb(callback), objects(obj_array), objects_size(obj_size) {}

            /**
             * @brief
             *
             * @param r
             * @param Idx
             * @param MaxDist
             * @param bool
             * @return bool operator
             */
            bool operator()(const Ray& r, uint32 Idx, float& MaxDist, bool /*stopAtFirst*/)
            {
                if (Idx >= objects_size) 
                    { return false; }

                if (const T* obj = objects[Idx])
                    { return cb(r, *obj, MaxDist/*, stopAtFirst*/); }
                return false;
            }

            /**
             * @brief
             *
             * @param p
             * @param Idx
             */
            void operator()(const Vector3& p, uint32 Idx)
            {
                if (Idx >= objects_size) 
                    { return; }

                if (const T* obj = objects[Idx])
                    { cb(p, *obj); }
            }
        };

        /**
         * @brief
         *
         */
        typedef G3D::Array<const T*> ObjArray;

        BIH m_tree; /**< TODO */
        ObjArray m_objects; /**< TODO */
        G3D::Table<const T*, uint32> m_obj2Idx; /**< TODO */
        G3D::Set<const T*> m_objects_to_push; /**< TODO */
        int unbalanced_times; /**< TODO */

    public:

        /**
         * @brief
         *
         */
        BIHWrap() : unbalanced_times(0) {}

        /**
         * @brief
         *
         * @param obj
         */
        void insert(const T& obj)
        {
            ++unbalanced_times;
            m_objects_to_push.insert(&obj);
        }

        /**
         * @brief
         *
         * @param obj
         */
        void remove(const T& obj)
        {
            ++unbalanced_times;
            uint32 Idx = 0;
            const T* temp;
            if (m_obj2Idx.getRemove(&obj, temp, Idx))
                { m_objects[Idx] = NULL; }
            else
                { m_objects_to_push.remove(&obj); }
        }

        /**
         * @brief
         *
         */
        void balance()
        {
            if (unbalanced_times == 0)
                { return; }

            unbalanced_times = 0;
            m_objects.fastClear();
            m_obj2Idx.getKeys(m_objects);
            m_objects_to_push.getMembers(m_objects);

            m_tree.build(m_objects, BoundsFunc::getBounds2);
        }

        template<typename RayCallback>
        /**
         * @brief
         *
         * @param r
         * @param intersectCallback
         * @param maxDist
         */
        void intersectRay(const Ray& r, RayCallback& intersectCallback, float& maxDist)
        {
            balance();
            MDLCallback<RayCallback> temp_cb(intersectCallback, m_objects.getCArray(), m_objects.size());
            m_tree.intersectRay(r, temp_cb, maxDist, true);
        }

        template<typename IsectCallback>
        /**
         * @brief
         *
         * @param p
         * @param intersectCallback
         */
        void intersectPoint(const Vector3& p, IsectCallback& intersectCallback)
        {
            balance();
            MDLCallback<IsectCallback> temp_cb(intersectCallback, m_objects.getCArray(), m_objects.size());
            m_tree.intersectPoint(p, temp_cb);
        }
};
