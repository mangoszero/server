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

#ifndef DYNAMICMAP_TREE_H
#define DYNAMICMAP_TREE_H

#include "Platform/Define.h"

namespace G3D
{
    class Vector3;
    class Matrix3;
    class AABox;
    class Ray;
}
class GameObjectModel;

/**
 * @brief
 *
 */
class DynamicMapTree
{
    public:
        /**
         * @brief
         *
         */
        DynamicMapTree();
        /**
         * @brief
         *
         */
        ~DynamicMapTree();

        /**
         * @brief
         *
         * @param x1
         * @param y1
         * @param z1
         * @param x2
         * @param y2
         * @param z2
         * @return bool
         */
        bool isInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2) const;
        /**
         * @brief
         *
         * @param ray
         * @param endPos
         * @param maxDist
         * @return bool
         */
        bool getIntersectionTime(const G3D::Ray& ray, const G3D::Vector3& endPos, float& maxDist) const;
        /**
         * @brief
         *
         * @param pPos1
         * @param pPos2
         * @param pResultHitPos
         * @param pModifyDist
         * @return bool
         */
        bool getObjectHitPos(const G3D::Vector3& pPos1, const G3D::Vector3& pPos2, G3D::Vector3& pResultHitPos, float pModifyDist) const;
        /**
         * @brief
         *
         * @param x1
         * @param y1
         * @param z1
         * @param x2
         * @param y2
         * @param z2
         * @param rx
         * @param ry
         * @param rz
         * @param pModifyDist
         * @return bool
         */
        bool getObjectHitPos(float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float pModifyDist) const;
        /**
         * @brief
         *
         * @param x
         * @param y
         * @param z
         * @param maxSearchDist
         * @return float
         */
        float getHeight(float x, float y, float z, float maxSearchDist) const;

        /**
         * @brief
         *
         * @param
         */
        void insert(const GameObjectModel&);
        /**
         * @brief
         *
         * @param
         */
        void remove(const GameObjectModel&);
        /**
         * @brief
         *
         * @param
         * @return bool
         */
        bool contains(const GameObjectModel&) const;
        /**
         * @brief
         *
         * @return int
         */
        int size() const;

        /**
         * @brief
         *
         */
        void balance();
        /**
         * @brief
         *
         * @param diff
         */
        void update(uint32 diff);
    private:
        struct DynTreeImpl& impl; /**< TODO */
};

#endif
