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

#ifndef MANGOS_H_MAPTREE
#define MANGOS_H_MAPTREE

#include "Platform/Define.h"
#include "Utilities/UnorderedMapSet.h"
#include "BIH.h"

namespace VMAP
{
    class ModelInstance;
    class GroupModel;
    class VMapManager2;

    /**
     * @brief
     *
     */
    struct LocationInfo
    {
        /**
         * @brief
         *
         */
        LocationInfo(): hitInstance(0), hitModel(0), ground_Z(-G3D::inf()) {};
        const ModelInstance* hitInstance; /**< TODO */
        const GroupModel* hitModel; /**< TODO */
        float ground_Z; /**< TODO */
    };

    /**
     * @brief
     *
     */
    class StaticMapTree
    {
            /**
             * @brief
             *
             */
            typedef UNORDERED_MAP<uint32, bool> loadedTileMap;
            /**
             * @brief
             *
             */
            typedef UNORDERED_MAP<uint32, uint32> loadedSpawnMap;
        private:
            uint32 iMapID; /**< TODO */
            bool iIsTiled; /**< TODO */
            BIH iTree; /**< TODO */
            ModelInstance* iTreeValues; // the tree entries /**< TODO */
            uint32 iNTreeValues; /**< TODO */

            // Store all the map tile idents that are loaded for that map
            // some maps are not splitted into tiles and we have to make sure, not removing the map before all tiles are removed
            // empty tiles have no tile file, hence map with bool instead of just a set (consistency check)
            loadedTileMap iLoadedTiles; /**< TODO */
            // stores <tree_index, reference_count> to invalidate tree values, unload map, and to be able to report errors
            loadedSpawnMap iLoadedSpawns; /**< TODO */
            std::string iBasePath; /**< TODO */

        private:
            /**
             * @brief
             *
             * @param pRay
             * @param pMaxDist
             * @param pStopAtFirstHit
             * @return bool
             */
            bool getIntersectionTime(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit) const;
            // bool containsLoadedMapTile(unsigned int pTileIdent) const { return(iLoadedMapTiles.containsKey(pTileIdent)); }
        public:
            /**
             * @brief
             *
             * @param mapID
             * @param tileX
             * @param tileY
             * @return std::string
             */
            static std::string getTileFileName(uint32 mapID, uint32 tileX, uint32 tileY);
            /**
             * @brief
             *
             * @param tileX
             * @param tileY
             * @return uint32
             */
            static uint32 packTileID(uint32 tileX, uint32 tileY) { return tileX << 16 | tileY; }
            /**
             * @brief
             *
             * @param ID
             * @param tileX
             * @param tileY
             */
            static void unpackTileID(uint32 ID, uint32& tileX, uint32& tileY) { tileX = ID >> 16; tileY = ID & 0xFF; }
            /**
             * @brief
             *
             * @param basePath
             * @param mapID
             * @param tileX
             * @param tileY
             * @return bool
             */
            static bool CanLoadMap(const std::string& basePath, uint32 mapID, uint32 tileX, uint32 tileY);

            /**
             * @brief
             *
             * @param mapID
             * @param basePath
             */
            StaticMapTree(uint32 mapID, const std::string& basePath);
            /**
             * @brief
             *
             */
            ~StaticMapTree();

            /**
             * @brief
             *
             * @param pos1
             * @param pos2
             * @return bool
             */
            bool isInLineOfSight(const G3D::Vector3& pos1, const G3D::Vector3& pos2) const;
            /**
             * @brief
             *
             * @param pos1
             * @param pos2
             * @param pResultHitPos
             * @param pModifyDist
             * @return bool
             */
            bool getObjectHitPos(const G3D::Vector3& pos1, const G3D::Vector3& pos2, G3D::Vector3& pResultHitPos, float pModifyDist) const;
            /**
             * @brief
             *
             * @param pPos
             * @param maxSearchDist
             * @return float
             */
            float getHeight(const G3D::Vector3& pPos, float maxSearchDist) const;
            /**
             * @brief
             *
             * @param pos
             * @param flags
             * @param adtId
             * @param rootId
             * @param groupId
             * @return bool
             */
            bool getAreaInfo(G3D::Vector3& pos, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const;
            /**
             * @brief
             *
             * @param pos
             * @param info
             * @return bool
             */
            bool GetLocationInfo(const Vector3& pos, LocationInfo& info) const;

            /**
             * @brief
             *
             * @param fname
             * @param vm
             * @return bool
             */
            bool InitMap(const std::string& fname, VMapManager2* vm);
            /**
             * @brief
             *
             * @param vm
             */
            void UnloadMap(VMapManager2* vm);
            /**
             * @brief
             *
             * @param tileX
             * @param tileY
             * @param vm
             * @return bool
             */
            bool LoadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm);
            /**
             * @brief
             *
             * @param tileX
             * @param tileY
             * @param vm
             */
            void UnloadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm);
            /**
             * @brief
             *
             * @return bool
             */
            bool isTiled() const { return iIsTiled; }
            /**
             * @brief
             *
             * @return uint32
             */
            uint32 numLoadedTiles() const { return iLoadedTiles.size(); }
#ifdef MMAP_GENERATOR
        public:
            void getModelInstances(ModelInstance*& models, uint32& count);
#endif

    };

    /**
     * @brief
     *
     */
    struct AreaInfo
    {
        /**
         * @brief
         *
         */
        AreaInfo(): result(false), ground_Z(-G3D::inf()) {};
        bool result; /**< TODO */
        float ground_Z; /**< TODO */
        uint32 flags; /**< TODO */
        int32 adtId; /**< TODO */
        int32 rootId; /**< TODO */
        int32 groupId; /**< TODO */
    };
}                                                           // VMAP

#endif // _MAPTREE_H
