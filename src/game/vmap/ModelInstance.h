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

#ifndef MANGOS_H_MODELINSTANCE
#define MANGOS_H_MODELINSTANCE

#include "Platform/Define.h"

#include <G3D/Matrix3.h>
#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>


namespace VMAP
{
    class WorldModel;
    struct AreaInfo;
    struct LocationInfo;

    /**
     * @brief
     *
     */
    enum ModelFlags
    {
        MOD_M2 = 1,
        MOD_WORLDSPAWN = 1 << 1,
        MOD_HAS_BOUND = 1 << 2
    };

    /**
     * @brief
     *
     */
    class ModelSpawn
    {
        public:
            // mapID, tileX, tileY, Flags, ID, Pos, Rot, Scale, Bound_lo, Bound_hi, name
            uint32 flags; /**< TODO */
            uint16 adtId; /**< TODO */
            uint32 ID; /**< TODO */
            G3D::Vector3 iPos; /**< TODO */
            G3D::Vector3 iRot; /**< TODO */
            float iScale; /**< TODO */
            G3D::AABox iBound; /**< TODO */
            std::string name; /**< TODO */
            /**
             * @brief
             *
             * @param other
             * @return bool operator
             */
            bool operator==(const ModelSpawn& other) const { return ID == other.ID; }
            // uint32 hashCode() const { return ID; }
            // temp?
            /**
             * @brief
             *
             * @return const G3D::AABox
             */
            const G3D::AABox& getBounds() const { return iBound; }


            /**
             * @brief
             *
             * @param rf
             * @param spawn
             * @return bool
             */
            static bool readFromFile(FILE* rf, ModelSpawn& spawn);
            /**
             * @brief
             *
             * @param rw
             * @param spawn
             * @return bool
             */
            static bool writeToFile(FILE* rw, const ModelSpawn& spawn);
    };

    /**
     * @brief
     *
     */
    class ModelInstance: public ModelSpawn
    {
        public:
            /**
             * @brief
             *
             */
            ModelInstance(): iModel(0) {}
            /**
             * @brief
             *
             * @param spawn
             * @param model
             */
            ModelInstance(const ModelSpawn& spawn, WorldModel* model);
            /**
             * @brief
             *
             */
            void setUnloaded() { iModel = 0; }
            /**
             * @brief
             *
             * @param pRay
             * @param pMaxDist
             * @param pStopAtFirstHit
             * @return bool
             */
            bool intersectRay(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit) const;
            /**
             * @brief
             *
             * @param p
             * @param info
             */
            void GetAreaInfo(const G3D::Vector3& p, AreaInfo& info) const;
            /**
             * @brief
             *
             * @param p
             * @param info
             * @return bool
             */
            bool GetLocationInfo(const G3D::Vector3& p, LocationInfo& info) const;
            /**
             * @brief
             *
             * @param p
             * @param info
             * @param liqHeight
             * @return bool
             */
            bool GetLiquidLevel(const G3D::Vector3& p, LocationInfo& info, float& liqHeight) const;
        protected:
            G3D::Matrix3 iInvRot; /**< TODO */
            float iInvScale; /**< TODO */
            WorldModel* iModel; /**< TODO */

#ifdef MMAP_GENERATOR
        public:
            WorldModel* const getWorldModel();
#endif
    };
} // namespace VMAP

#endif // _MODELINSTANCE
