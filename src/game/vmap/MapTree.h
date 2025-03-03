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
     * @brief Structure to hold location information.
     */
    struct LocationInfo
    {
        /**
         * @brief Default constructor for LocationInfo.
         */
        LocationInfo() : hitInstance(0), hitModel(0), ground_Z(-G3D::inf()) {};
        const ModelInstance* hitInstance; /**< Pointer to the hit model instance. */
        const GroupModel* hitModel; /**< Pointer to the hit group model. */
        float ground_Z; /**< Ground Z coordinate. */
    };

    /**
     * @brief Class representing a static map tree.
     */
    class StaticMapTree
    {
        /**
         * @brief Type definition for loaded tile map.
         */
        typedef UNORDERED_MAP<uint32, bool> loadedTileMap;
        /**
         * @brief Type definition for loaded spawn map.
         */
        typedef UNORDERED_MAP<uint32, uint32> loadedSpawnMap;
    private:
        uint32 iMapID; /**< Map ID. */
        bool iIsTiled; /**< Flag indicating if the map is tiled. */
        BIH iTree; /**< Bounding Interval Hierarchy tree. */
        ModelInstance* iTreeValues; /**< The tree entries. */
        uint32 iNTreeValues; /**< Number of tree values. */

        // Store all the map tile idents that are loaded for that map
        // some maps are not splitted into tiles and we have to make sure, not removing the map before all tiles are removed
        // empty tiles have no tile file, hence map with bool instead of just a set (consistency check)
        loadedTileMap iLoadedTiles; /**< Loaded tiles map. */
        // stores <tree_index, reference_count> to invalidate tree values, unload map, and to be able to report errors
        loadedSpawnMap iLoadedSpawns; /**< Loaded spawns map. */
        std::string iBasePath; /**< Base path for map files. */

    private:
        /**
         * @brief Checks if there is an intersection within the maximum distance.
         *
         * @param pRay The ray to check.
         * @param pMaxDist The maximum distance to check.
         * @param pStopAtFirstHit Whether to stop at the first hit.
         * @return bool True if an intersection is found, false otherwise.
         */
        bool getIntersectionTime(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit) const;
        // bool containsLoadedMapTile(unsigned int pTileIdent) const { return(iLoadedMapTiles.containsKey(pTileIdent)); }
    public:
        /**
         * @brief Generates the tile file name based on map ID, tile X, and tile Y.
         *
         * @param mapID The map ID.
         * @param tileX The tile X coordinate.
         * @param tileY The tile Y coordinate.
         * @return std::string The generated tile file name.
         */
        static std::string getTileFileName(uint32 mapID, uint32 tileX, uint32 tileY);
        /**
         * @brief Packs the tile ID from tile X and tile Y coordinates.
         *
         * @param tileX The tile X coordinate.
         * @param tileY The tile Y coordinate.
         * @return uint32 The packed tile ID.
         */
        static uint32 packTileID(uint32 tileX, uint32 tileY) { return tileX << 16 | tileY; }
        /**
         * @brief Unpacks the tile ID into tile X and tile Y coordinates.
         *
         * @param ID The packed tile ID.
         * @param tileX The tile X coordinate.
         * @param tileY The tile Y coordinate.
         */
        static void unpackTileID(uint32 ID, uint32& tileX, uint32& tileY) { tileX = ID >> 16; tileY = ID & 0xFF; }
        /**
         * @brief Checks if a map can be loaded.
         *
         * @param basePath The base path for map files.
         * @param mapID The map ID.
         * @param tileX The tile X coordinate.
         * @param tileY The tile Y coordinate.
         * @return bool True if the map can be loaded, false otherwise.
         */
        static bool CanLoadMap(const std::string& basePath, uint32 mapID, uint32 tileX, uint32 tileY);

        /**
         * @brief Constructor for StaticMapTree.
         *
         * @param mapID The map ID.
         * @param basePath The base path for map files.
         */
        StaticMapTree(uint32 mapID, const std::string& basePath);
        /**
         * @brief Destructor for StaticMapTree.
         */
        ~StaticMapTree();

        /**
         * @brief Checks if there is a line of sight between two positions.
         *
         * @param pos1 The starting position.
         * @param pos2 The ending position.
         * @return bool True if there is a line of sight, false otherwise.
         */
        bool isInLineOfSight(const G3D::Vector3& pos1, const G3D::Vector3& pos2) const;
        /**
         * @brief Checks if an object is hit when moving from pos1 to pos2.
         *
         * @param pos1 The starting position.
         * @param pos2 The ending position.
         * @param pResultHitPos The resulting hit position.
         * @param pModifyDist The distance to modify the hit position.
         * @return bool True if an object is hit, false otherwise.
         */
        bool getObjectHitPos(const G3D::Vector3& pos1, const G3D::Vector3& pos2, G3D::Vector3& pResultHitPos, float pModifyDist) const;
        /**
         * @brief Retrieves the height at a given position.
         *
         * @param pPos The position to check.
         * @param maxSearchDist The maximum search distance.
         * @return float The height at the position.
         */
        float getHeight(const G3D::Vector3& pPos, float maxSearchDist) const;
        /**
         * @brief Retrieves area information for a given position.
         *
         * @param pos The position to check.
         * @param flags The area flags.
         * @param adtId The ADT ID.
         * @param rootId The root ID.
         * @param groupId The group ID.
         * @return bool True if area information was found, false otherwise.
         */
        bool getAreaInfo(G3D::Vector3& pos, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const;
        /**
         * @brief Retrieves location information for a given position.
         *
         * @param pos The position to check.
         * @param info The location information.
         * @return bool True if location information was found, false otherwise.
         */
        bool GetLocationInfo(const Vector3& pos, LocationInfo& info) const;

        /**
         * @brief Initializes the map.
         *
         * @param fname The file name of the map.
         * @param vm The VMap manager.
         * @return bool True if the map was successfully initialized, false otherwise.
         */
        bool InitMap(const std::string& fname, VMapManager2* vm);
        /**
         * @brief Unloads the map.
         *
         * @param vm The VMap manager.
         */
        void UnloadMap(VMapManager2* vm);
        /**
         * @brief Loads a map tile.
         *
         * @param tileX The tile X coordinate.
         * @param tileY The tile Y coordinate.
         * @param vm The VMap manager.
         * @return bool True if the tile was successfully loaded, false otherwise.
         */
        bool LoadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm);
        /**
         * @brief Unloads a map tile.
         *
         * @param tileX The tile X coordinate.
         * @param tileY The tile Y coordinate.
         * @param vm The VMap manager.
         */
        void UnloadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm);
        /**
         * @brief Checks if the map is tiled.
         *
         * @return bool True if the map is tiled, false otherwise.
         */
        bool isTiled() const { return iIsTiled; }
        /**
         * @brief Returns the number of loaded tiles.
         *
         * @return uint32 The number of loaded tiles.
         */
        uint32 numLoadedTiles() const { return iLoadedTiles.size(); }

#ifdef MMAP_GENERATOR
    public:
        /**
         * @brief Retrieves model instances.
         *
         * @param models Pointer to the model instances.
         * @param count The number of model instances.
         */
        void getModelInstances(ModelInstance*& models, uint32& count);
#endif
    };

    /**
     * @brief Structure to hold area information.
     */
    struct AreaInfo
    {
        /**
         * @brief Default constructor for AreaInfo.
         */
        AreaInfo() : result(false), ground_Z(-G3D::inf()), flags(0), adtId(0), rootId(0), groupId(0) {}
        bool result; /**< Flag indicating if the area information is valid. */
        float ground_Z; /**< Ground Z coordinate. */
        uint32 flags; /**< Area flags. */
        int32 adtId; /**< ADT ID. */
        int32 rootId; /**< Root ID. */
        int32 groupId; /**< Group ID. */
    };
} // namespace VMAP

#endif // MANGOS_H_MAPTREE

