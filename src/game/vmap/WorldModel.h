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

#ifndef MANGOS_H_WORLDMODEL
#define MANGOS_H_WORLDMODEL

#include "Platform/Define.h"

#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>
#include "BIH.h"


namespace VMAP
{
    class TreeNode;
    struct AreaInfo;
    struct LocationInfo;

    /**
     * @brief
     *
     */
    class MeshTriangle
    {
        public:
            /**
             * @brief
             *
             */
            MeshTriangle() {};
            /**
             * @brief
             *
             * @param na
             * @param nb
             * @param nc
             */
            MeshTriangle(uint32 na, uint32 nb, uint32 nc): idx0(na), idx1(nb), idx2(nc) {};

            uint32 idx0; /**< TODO */
            uint32 idx1; /**< TODO */
            uint32 idx2; /**< TODO */
    };

    /**
     * @brief
     *
     */
    class WmoLiquid
    {
        public:
            /**
             * @brief
             *
             * @param width
             * @param height
             * @param corner
             * @param type
             */
            WmoLiquid(uint32 width, uint32 height, const Vector3& corner, uint32 type);
            /**
             * @brief
             *
             * @param other
             */
            WmoLiquid(const WmoLiquid& other);
            /**
             * @brief
             *
             */
            ~WmoLiquid();
            /**
             * @brief
             *
             * @param other
             * @return WmoLiquid &operator
             */
            WmoLiquid& operator=(const WmoLiquid& other);
            /**
             * @brief
             *
             * @param pos
             * @param liqHeight
             * @return bool
             */
            bool GetLiquidHeight(const Vector3& pos, float& liqHeight) const;
            /**
             * @brief
             *
             * @return uint32
             */
            uint32 GetType() const { return iType; }
            /**
             * @brief
             *
             * @return float
             */
            float* GetHeightStorage() { return iHeight; }
            /**
             * @brief
             *
             * @return uint8
             */
            uint8* GetFlagsStorage() { return iFlags; }
            /**
             * @brief
             *
             * @return uint32
             */
            uint32 GetFileSize();
            /**
             * @brief
             *
             * @param wf
             * @return bool
             */
            bool WriteToFile(FILE* wf);
            /**
             * @brief
             *
             * @param rf
             * @param liquid
             * @return bool
             */
            static bool ReadFromFile(FILE* rf, WmoLiquid*& liquid);
        private:
            /**
             * @brief
             *
             */
            WmoLiquid(): iHeight(0), iFlags(0) {};

            uint32 iTilesX;  /**< number of tiles in x direction, each */
            uint32 iTilesY;  /**< TODO */
            Vector3 iCorner; /**< the lower corner */
            uint32 iType;    /**< liquid type */
            float* iHeight;  /**< (tilesX + 1)*(tilesY + 1) height values */
            uint8* iFlags;   /**< info if liquid tile is used */
#ifdef MMAP_GENERATOR
        public:
            void getPosInfo(uint32& tilesX, uint32& tilesY, Vector3& corner) const;
#endif
    };

    /**
     * @brief holding additional info for WMO group files
     *
     */
    class GroupModel
    {
        public:
            /**
             * @brief
             *
             */
            GroupModel(): iLiquid(0) {}
            /**
             * @brief
             *
             * @param other
             */
            GroupModel(const GroupModel& other);
            /**
             * @brief
             *
             * @param mogpFlags
             * @param groupWMOID
             * @param bound
             */
            GroupModel(uint32 mogpFlags, uint32 groupWMOID, const AABox& bound):
                iBound(bound), iMogpFlags(mogpFlags), iGroupWMOID(groupWMOID), iLiquid(0) {}
            /**
             * @brief
             *
             */
            ~GroupModel() { delete iLiquid; }

            /**
             * @brief pass mesh data to object and create BIH. Passed vectors get get swapped with old geometry!
             *
             * @param vert
             * @param tri
             */
            void SetMeshData(std::vector<Vector3>& vert, std::vector<MeshTriangle>& tri);
            /**
             * @brief
             *
             * @param liquid
             */
            void SetLiquidData(WmoLiquid*& liquid) { iLiquid = liquid; liquid = NULL; }
            /**
             * @brief
             *
             * @param ray
             * @param distance
             * @param stopAtFirstHit
             * @return bool
             */
            bool IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit) const;
            /**
             * @brief
             *
             * @param pos
             * @param down
             * @param z_dist
             * @return bool
             */
            bool IsInsideObject(const Vector3& pos, const Vector3& down, float& z_dist) const;
            /**
             * @brief
             *
             * @param pos
             * @param liqHeight
             * @return bool
             */
            bool GetLiquidLevel(const Vector3& pos, float& liqHeight) const;
            /**
             * @brief
             *
             * @return uint32
             */
            uint32 GetLiquidType() const;
            /**
             * @brief
             *
             * @param wf
             * @return bool
             */
            bool WriteToFile(FILE* wf);
            /**
             * @brief
             *
             * @param rf
             * @return bool
             */
            bool ReadFromFile(FILE* rf);
            /**
             * @brief
             *
             * @return const G3D::AABox
             */
            const G3D::AABox& GetBound() const { return iBound; }
            /**
             * @brief
             *
             * @return uint32
             */
            uint32 GetMogpFlags() const { return iMogpFlags; }
            /**
             * @brief
             *
             * @return uint32
             */
            uint32 GetWmoID() const { return iGroupWMOID; }
        protected:
            G3D::AABox iBound;  /**< TODO */
            uint32 iMogpFlags;  /**< 0x8 outdor; 0x2000 indoor */
            uint32 iGroupWMOID; /**< TODO */
            std::vector<Vector3> vertices; /**< TODO */
            std::vector<MeshTriangle> triangles; /**< TODO */
            BIH meshTree; /**< TODO */
            WmoLiquid* iLiquid; /**< TODO */

#ifdef MMAP_GENERATOR
        public:
            void getMeshData(std::vector<Vector3>& vertices, std::vector<MeshTriangle>& triangles, WmoLiquid*& liquid);
#endif
    };
    /**
     * @brief Holds a model (converted M2 or WMO) in its original coordinate space
     *
     */
    class WorldModel
    {
        public:
            /**
             * @brief
             *
             */
            WorldModel(): RootWMOID(0) {}

            /**
             * @brief pass group models to WorldModel and create BIH. Passed vector is swapped with old geometry!
             *
             * @param models
             */
            void SetGroupModels(std::vector<GroupModel>& models);
            /**
             * @brief
             *
             * @param id
             */
            void SetRootWmoID(uint32 id) { RootWMOID = id; }
            /**
             * @brief
             *
             * @param ray
             * @param distance
             * @param stopAtFirstHit
             * @return bool
             */
            bool IntersectRay(const G3D::Ray& ray, float& distance, bool stopAtFirstHit) const;
            /**
             * @brief
             *
             * @param point
             * @param dir
             * @param dist
             * @param info
             * @return bool
             */
            bool GetAreaInfo(const G3D::Vector3& point, const G3D::Vector3& dir, float& dist, AreaInfo& info) const;
            /**
             * @brief
             *
             * @param point
             * @param dir
             * @param dist
             * @param info
             * @return bool
             */
            bool GetLocationInfo(const G3D::Vector3& point, const G3D::Vector3& dir, float& dist, LocationInfo& info) const;
            /**
             * @brief
             *
             * @param point
             * @param dir
             * @param dist
             * @return bool
             */
            bool GetContactPoint(const G3D::Vector3& point, const G3D::Vector3& dir, float& dist) const;
            /**
             * @brief
             *
             * @param filename
             * @return bool
             */
            bool WriteFile(const std::string& filename);
            /**
             * @brief
             *
             * @param filename
             * @return bool
             */
            bool ReadFile(const std::string& filename);
            uint32 Flags;
        protected:
            uint32 RootWMOID;
            std::vector<GroupModel> groupModels;
            BIH groupTree;

#ifdef MMAP_GENERATOR
        public:
            void getGroupModels(std::vector<GroupModel>& groupModels);
#endif
    };
}

#endif // _WORLDMODEL_H
