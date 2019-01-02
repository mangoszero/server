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

#ifndef _VMAPMANAGER2_H
#define _VMAPMANAGER2_H

#include "IVMapManager.h"
#include "Utilities/UnorderedMapSet.h"
#include "Platform/Define.h"
#include <G3D/Vector3.h>

//===========================================================

#define MAP_FILENAME_EXTENSION2 ".vmtree"

#define FILENAMEBUFFER_SIZE 500

/**
This is the main Class to manage loading and unloading of maps, line of sight, height calculation and so on.
For each map or map tile to load it reads a directory file that contains the ModelContainer files used by this map or map tile.
Each global map or instance has its own dynamic BSP-Tree.
The loaded ModelContainers are included in one of these BSP-Trees.
Additionally a table to match map ids and map names is used.
*/

//===========================================================

namespace VMAP
{
    class StaticMapTree;
    class WorldModel;

    /**
     * @brief
     *
     */
    class ManagedModel
    {
        public:
            /**
             * @brief
             *
             */
            ManagedModel(): iModel(0), iRefCount(0) {}
            /**
             * @brief
             *
             * @param model
             */
            void setModel(WorldModel* model) { iModel = model; }
            /**
             * @brief
             *
             * @return WorldModel
             */
            WorldModel* getModel() { return iModel; }
            /**
             * @brief
             *
             */
            void incRefCount() { ++iRefCount; }
            /**
             * @brief
             *
             * @return int
             */
            int decRefCount() { return --iRefCount; }
        protected:
            WorldModel* iModel; /**< TODO */
            int iRefCount; /**< TODO */
    };

    /**
     * @brief
     *
     */
    typedef UNORDERED_MAP<uint32 , StaticMapTree*> InstanceTreeMap;
    /**
     * @brief
     *
     */
    typedef UNORDERED_MAP<std::string, ManagedModel> ModelFileMap;

    enum DisableTypes
    {
        VMAP_DISABLE_AREAFLAG     = 0x1,
        VMAP_DISABLE_HEIGHT       = 0x2,
        VMAP_DISABLE_LOS          = 0x4,
        VMAP_DISABLE_LIQUIDSTATUS = 0x8
    };

    /**
     * @brief
     *
     */
    class VMapManager2 : public IVMapManager
    {
        protected:
            // Tree to check collision
            ModelFileMap iLoadedModelFiles; /**< TODO */
            InstanceTreeMap iInstanceMapTrees; /**< TODO */

            /**
             * @brief
             *
             * @param pMapId
             * @param basePath
             * @param tileX
             * @param tileY
             * @return bool
             */
            bool _loadMap(uint32 pMapId, const std::string& basePath, uint32 tileX, uint32 tileY);
            /* void _unloadMap(uint32 pMapId, uint32 x, uint32 y); */

        public:
            // public for debug
            /**
             * @brief
             *
             * @param x
             * @param y
             * @param z
             * @return G3D::Vector3
             */
            G3D::Vector3 convertPositionToInternalRep(float x, float y, float z) const;
            /**
             * @brief
             *
             * @param pMapId
             * @return std::string
             */
            static std::string getMapFileName(unsigned int pMapId);

            /**
             * @brief
             *
             */
            VMapManager2();
            /**
             * @brief
             *
             */
            ~VMapManager2();

            /**
             * @brief
             *
             * @param pBasePath
             * @param pMapId
             * @param x
             * @param y
             * @return VMAPLoadResult
             */
            VMAPLoadResult loadMap(const char* pBasePath, unsigned int pMapId, int x, int y) override;

            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             */
            void unloadMap(unsigned int pMapId, int x, int y) override;
            /**
             * @brief
             *
             * @param pMapId
             */
            void unloadMap(unsigned int pMapId) override;

            /**
             * @brief
             *
             * @param pMapId
             * @param x1
             * @param y1
             * @param z1
             * @param x2
             * @param y2
             * @param z2
             * @return bool
             */
            bool isInLineOfSight(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2) override;
            /**
            fill the hit pos and return true, if an object was hit
            */
            /**
             * @brief
             *
             * @param pMapId
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
            bool getObjectHitPos(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float pModifyDist) override;
            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             * @param z
             * @param maxSearchDist
             * @return float
             */
            float getHeight(unsigned int pMapId, float x, float y, float z, float maxSearchDist) override;

            /**
             * @brief
             *
             * @param pCommand
             * @return bool
             */
            bool processCommand(char* /*pCommand*/) override { return false; }      // for debug and extensions

            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             * @param z
             * @param flags
             * @param adtId
             * @param rootId
             * @param groupId
             * @return bool
             */
            bool getAreaInfo(unsigned int pMapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const override;
            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             * @param z
             * @param ReqLiquidType
             * @param level
             * @param floor
             * @param type
             * @return bool
             */
            bool GetLiquidLevel(uint32 pMapId, float x, float y, float z, uint8 ReqLiquidType, float& level, float& floor, uint32& type) const override;

            /**
             * @brief
             *
             * @param basepath
             * @param filename
             * @return WorldModel
             */
            WorldModel* acquireModelInstance(const std::string& basepath, const std::string& filename, uint32 flags = 0);
            /**
             * @brief
             *
             * @param filename
             */
            void releaseModelInstance(const std::string& filename);

            /**
             * @brief what's the use of this? o.O
             *
             * @param pMapId
             * @param x
             * @param y
             * @return std::string
             */
            // what's the use of this? o.O
            std::string getDirFileName(unsigned int pMapId, int /*x*/, int /*y*/) const override
            {
                return getMapFileName(pMapId);
            }
            /**
             * @brief
             *
             * @param pBasePath
             * @param pMapId
             * @param x
             * @param y
             * @return bool
             */
            bool existsMap(const char* pBasePath, unsigned int pMapId, int x, int y) override;

            typedef bool(*IsVMAPDisabledForFn)(uint32 entry, uint8 flags);
            IsVMAPDisabledForFn IsVMAPDisabledForPtr;

#ifdef MMAP_GENERATOR
        public:
            void getInstanceMapTree(InstanceTreeMap& instanceMapTree);
#endif
    };
}
#endif
