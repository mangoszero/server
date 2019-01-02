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

#ifndef MANGOS_H_REGULAR_GRID
#define MANGOS_H_REGULAR_GRID

#include <G3D/Ray.h>
#include <G3D/AABox.h>
#include <G3D/Table.h>
#include <G3D/PositionTrait.h>

#include "Errors.h"

using G3D::Vector2;
using G3D::Vector3;
using G3D::AABox;
using G3D::Ray;

template<class Node>
/**
 * @brief
 *
 */
struct NodeCreator
{
    /**
     * @brief
     *
     * @param int
     * @param int
     * @return Node
     */
    static Node* makeNode(int /*x*/, int /*y*/) { return new Node();}
};

template < class T,
         class Node,
         class NodeCreatorFunc = NodeCreator<Node>,
         /*class BoundsFunc = BoundsTrait<T>,*/
         class PositionFunc = PositionTrait<T>
         >
/**
 * @brief
 *
 */
class RegularGrid2D
{
    public:

        /**
         * @brief
         *
         */
        enum
        {
            CELL_NUMBER = 64
        };

#define HGRID_MAP_SIZE  (533.33333f * 64.f)     // shouldn't be changed
#define CELL_SIZE       float(HGRID_MAP_SIZE/(float)CELL_NUMBER)

        /**
         * @brief
         *
         */
        typedef G3D::Table<const T*, Node*> MemberTable;

        MemberTable memberTable; /**< TODO */
        Node* nodes[CELL_NUMBER][CELL_NUMBER]; /**< TODO */

        /**
         * @brief
         *
         */
        RegularGrid2D()
        {
            memset(nodes, 0, sizeof(nodes));
        }

        /**
         * @brief
         *
         */
        ~RegularGrid2D()
        {
            for (int x = 0; x < CELL_NUMBER; ++x)
                for (int y = 0; y < CELL_NUMBER; ++y)
                    { delete nodes[x][y]; }
        }

        /**
         * @brief
         *
         * @param value
         */
        void insert(const T& value)
        {
            Vector3 pos;
            PositionFunc::getPosition(value, pos);
            Node& node = getGridFor(pos.x, pos.y);
            node.insert(value);
            memberTable.set(&value, &node);
        }

        /**
         * @brief
         *
         * @param value
         */
        void remove(const T& value)
        {
            memberTable[&value]->remove(value);
            // Remove the member
            memberTable.remove(&value);
        }

        /**
         * @brief
         *
         */
        void balance()
        {
            for (int x = 0; x < CELL_NUMBER; ++x)
                for (int y = 0; y < CELL_NUMBER; ++y)
                    if (Node* n = nodes[x][y])
                        { n->balance(); }
        }

        /**
         * @brief
         *
         * @param value
         * @return bool
         */
        bool contains(const T& value) const { return memberTable.containsKey(&value); }
        /**
         * @brief
         *
         * @return int
         */
        int size() const { return memberTable.size(); }

        /**
         * @brief
         *
         */
        struct Cell
        {
            int x, y; /**< TODO */
            /**
             * @brief
             *
             * @param c2
             * @return bool operator
             */
            bool operator == (const Cell& c2) const { return x == c2.x && y == c2.y;}

            /**
             * @brief
             *
             * @param fx
             * @param fy
             * @return Cell
             */
            static Cell ComputeCell(float fx, float fy)
            {
                Cell c = {static_cast<int>(fx* (1.f / CELL_SIZE) + (CELL_NUMBER / 2)), static_cast<int>(fy* (1.f / CELL_SIZE) + (CELL_NUMBER / 2))};
                return c;
            }

            /**
             * @brief
             *
             * @return bool
             */
            bool isValid() const { return x >= 0 && x < CELL_NUMBER && y >= 0 && y < CELL_NUMBER;}
        };

        /**
         * @brief
         *
         * @param fx
         * @param fy
         * @return Node
         */
        Node& getGridFor(float fx, float fy)
        {
            Cell c = Cell::ComputeCell(fx, fy);
            return getGrid(c.x, c.y);
        }

        /**
         * @brief
         *
         * @param x
         * @param y
         * @return Node
         */
        Node& getGrid(int x, int y)
        {
            MANGOS_ASSERT(x < CELL_NUMBER && y < CELL_NUMBER);
            if (!nodes[x][y])
                { nodes[x][y] = NodeCreatorFunc::makeNode(x, y); }
            return *nodes[x][y];
        }

        template<typename RayCallback>
        /**
         * @brief
         *
         * @param ray
         * @param intersectCallback
         * @param max_dist
         */
        void intersectRay(const Ray& ray, RayCallback& intersectCallback, float max_dist)
        {
            intersectRay(ray, intersectCallback, max_dist, ray.origin() + ray.direction() * max_dist);
        }

        template<typename RayCallback>
        /**
         * @brief
         *
         * @param ray
         * @param intersectCallback
         * @param max_dist
         * @param end
         */
        void intersectRay(const Ray& ray, RayCallback& intersectCallback, float& max_dist, const Vector3& end)
        {
            Cell cell = Cell::ComputeCell(ray.origin().x, ray.origin().y);
            if (!cell.isValid())
                { return; }

            Cell last_cell = Cell::ComputeCell(end.x, end.y);

            if (cell == last_cell)
            {
                if (Node* node = nodes[cell.x][cell.y])
                    { node->intersectRay(ray, intersectCallback, max_dist); }
                return;
            }

            float voxel = (float)CELL_SIZE;
            float kx_inv = ray.invDirection().x, bx = ray.origin().x;
            float ky_inv = ray.invDirection().y, by = ray.origin().y;

            int stepX, stepY;
            float tMaxX, tMaxY;
            if (kx_inv >= 0)
            {
                stepX = 1;
                float x_border = (cell.x + 1) * voxel;
                tMaxX = (x_border - bx) * kx_inv;
            }
            else
            {
                stepX = -1;
                float x_border = (cell.x - 1) * voxel;
                tMaxX = (x_border - bx) * kx_inv;
            }

            if (ky_inv >= 0)
            {
                stepY = 1;
                float y_border = (cell.y + 1) * voxel;
                tMaxY = (y_border - by) * ky_inv;
            }
            else
            {
                stepY = -1;
                float y_border = (cell.y - 1) * voxel;
                tMaxY = (y_border - by) * ky_inv;
            }

            //int Cycles = std::max((int)ceilf(max_dist/tMaxX),(int)ceilf(max_dist/tMaxY));
            //int i = 0;

            float tDeltaX = voxel * fabs(kx_inv);
            float tDeltaY = voxel * fabs(ky_inv);
            do
            {
                if (Node* node = nodes[cell.x][cell.y])
                {
                    //float enterdist = max_dist;
                    node->intersectRay(ray, intersectCallback, max_dist);
                }
                if (cell == last_cell)
                    { break; }
                if (tMaxX < tMaxY)
                {
                    tMaxX += tDeltaX;
                    cell.x += stepX;
                }
                else
                {
                    tMaxY += tDeltaY;
                    cell.y += stepY;
                }
                //++i;
            }
            while (cell.isValid());
        }

        template<typename IsectCallback>
        /**
         * @brief
         *
         * @param point
         * @param intersectCallback
         */
        void intersectPoint(const Vector3& point, IsectCallback& intersectCallback)
        {
            Cell cell = Cell::ComputeCell(point.x, point.y);
            if (!cell.isValid())
                { return; }
            if (Node* node = nodes[cell.x][cell.y])
                { node->intersectPoint(point, intersectCallback); }
        }

        template<typename RayCallback>
        /**
         * @brief Optimized verson of intersectRay function for rays with vertical directions
         *
         * @param ray
         * @param intersectCallback
         * @param max_dist
         */
        void intersectZAllignedRay(const Ray& ray, RayCallback& intersectCallback, float& max_dist)
        {
            Cell cell = Cell::ComputeCell(ray.origin().x, ray.origin().y);
            if (!cell.isValid())
                { return; }
            if (Node* node = nodes[cell.x][cell.y])
                { node->intersectRay(ray, intersectCallback, max_dist); }
        }
};

#undef CELL_SIZE
#undef HGRID_MAP_SIZE

#endif
