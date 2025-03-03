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

#include "MapTree.h"
#include "ModelInstance.h"
#include "VMapManager2.h"
#include "VMapDefinitions.h"

#include <string>
#include <sstream>
#include <iomanip>
#include <limits>

using G3D::Vector3;

namespace VMAP
{
    /**
     * @brief Callback class for ray intersection with models.
     */
    class MapRayCallback
    {
    public:
        MapRayCallback(ModelInstance* val) : prims(val), hit(false) {}

        /**
         * @brief Operator to handle ray intersection.
         *
         * @param ray The ray to intersect.
         * @param entry The entry index.
         * @param distance The distance to intersection.
         * @param pStopAtFirstHit Whether to stop at the first hit.
         * @return true if intersection occurs, false otherwise.
         */
        bool operator()(const G3D::Ray& ray, uint32 entry, float& distance, bool pStopAtFirstHit = true)
        {
            bool result = prims[entry].intersectRay(ray, distance, pStopAtFirstHit);
            if (result)
            {
                hit = true;
            }
            return result;
        }

        /**
         * @brief Checks if an intersection occurred.
         *
         * @return true if an intersection occurred, false otherwise.
         */
        bool didHit() const { return hit; }

    protected:
        ModelInstance* prims; /**< Pointer to model instances. */
        bool hit; /**< Flag indicating if an intersection occurred. */
    };

    /**
     * @brief Callback class for area information.
     */
    class AreaInfoCallback
    {
    public:
        AreaInfoCallback(ModelInstance* val) : prims(val) {}

        /**
         * @brief Operator to handle area information retrieval.
         *
         * @param point The point to check.
         * @param entry The entry index.
         */
        void operator()(const Vector3& point, uint32 entry)
        {
#ifdef VMAP_DEBUG
            DEBUG_LOG("trying to intersect '%s'", prims[entry].name.c_str());
#endif
            prims[entry].GetAreaInfo(point, aInfo);
        }

        ModelInstance* prims; /**< Pointer to model instances. */
        AreaInfo aInfo; /**< Area information. */
    };

    /**
     * @brief Callback class for location information.
     */
    class LocationInfoCallback
    {
    public:
        LocationInfoCallback(ModelInstance* val, LocationInfo& info) : prims(val), locInfo(info), result(false) {}

        /**
         * @brief Operator to handle location information retrieval.
         *
         * @param point The point to check.
         * @param entry The entry index.
         */
        void operator()(const Vector3& point, uint32 entry)
        {
#ifdef VMAP_DEBUG
            DEBUG_LOG("trying to intersect '%s'", prims[entry].name.c_str());
#endif
            if (prims[entry].GetLocationInfo(point, locInfo))
            {
                result = true;
            }
        }

        ModelInstance* prims; /**< Pointer to model instances. */
        LocationInfo& locInfo; /**< Location information. */
        bool result; /**< Flag indicating if location information was found. */
    };

    /**
     * @brief Generates the tile file name based on map ID, tile X, and tile Y.
     *
     * @param mapID The map ID.
     * @param tileX The tile X coordinate.
     * @param tileY The tile Y coordinate.
     * @return std::string The generated tile file name.
     */
    std::string StaticMapTree::getTileFileName(uint32 mapID, uint32 tileX, uint32 tileY)
    {
        std::stringstream tilefilename;
        tilefilename.fill('0');
        tilefilename << std::setw(3) << mapID << "_";
        tilefilename << std::setw(2) << tileY << "_" << std::setw(2) << tileX << ".vmtile";
        return tilefilename.str();
    }

    /**
     * @brief Retrieves area information for a given position.
     *
     * @param pos The position to check.
     * @param flags The area flags.
     * @param adtId The ADT ID.
     * @param rootId The root ID.
     * @param groupId The group ID.
     * @return true if area information was found, false otherwise.
     */
    bool StaticMapTree::getAreaInfo(Vector3& pos, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const
    {
        AreaInfoCallback intersectionCallBack(iTreeValues);
        iTree.intersectPoint(pos, intersectionCallBack);
        if (intersectionCallBack.aInfo.result)
        {
            flags = intersectionCallBack.aInfo.flags;
            adtId = intersectionCallBack.aInfo.adtId;
            rootId = intersectionCallBack.aInfo.rootId;
            groupId = intersectionCallBack.aInfo.groupId;
            pos.z = intersectionCallBack.aInfo.ground_Z;
            return true;
        }
        return false;
    }

    /**
     * @brief Retrieves location information for a given position.
     *
     * @param pos The position to check.
     * @param info The location information.
     * @return true if location information was found, false otherwise.
     */
    bool StaticMapTree::GetLocationInfo(const Vector3& pos, LocationInfo& info) const
    {
        LocationInfoCallback intersectionCallBack(iTreeValues, info);
        iTree.intersectPoint(pos, intersectionCallBack);
        return intersectionCallBack.result;
    }

    /**
     * @brief Constructor for StaticMapTree.
     *
     * @param mapID The map ID.
     * @param basePath The base path for map files.
     */
    StaticMapTree::StaticMapTree(uint32 mapID, const std::string& basePath) :
        iMapID(mapID), iTreeValues(nullptr), iBasePath(basePath), iIsTiled(false), iNTreeValues(0)
    {
        if (iBasePath.length() > 0 && (iBasePath[iBasePath.length() - 1] != '/' || iBasePath[iBasePath.length() - 1] != '\\'))
        {
            iBasePath.append("/");
        }
    }

    /**
     * @brief Destructor for StaticMapTree.
     *
     * Make sure to call unloadMap() to unregister acquired model references before destroying.
     */
    StaticMapTree::~StaticMapTree()
    {
        delete[] iTreeValues;
    }

    /**
     * @brief Checks if there is an intersection within the maximum distance.
     *
     * @param pRay The ray to check.
     * @param pMaxDist The maximum distance to check.
     * @param pStopAtFirstHit Whether to stop at the first hit.
     * @return true if an intersection is found, false otherwise.
     */
    bool StaticMapTree::getIntersectionTime(const G3D::Ray& pRay, float& pMaxDist, bool pStopAtFirstHit) const
    {
        float distance = pMaxDist;
        MapRayCallback intersectionCallBack(iTreeValues);
        iTree.intersectRay(pRay, intersectionCallBack, distance, pStopAtFirstHit);
        if (intersectionCallBack.didHit())
        {
            pMaxDist = distance;
        }
        return intersectionCallBack.didHit();
    }

    /**
     * @brief Checks if there is a line of sight between two positions.
     *
     * @param pos1 The starting position.
     * @param pos2 The ending position.
     * @return true if there is a line of sight, false otherwise.
     */
    bool StaticMapTree::isInLineOfSight(const Vector3& pos1, const Vector3& pos2) const
    {
        float maxDist = (pos2 - pos1).magnitude();
        // Return false if distance is over max float, in case of cheater teleporting to the end of the universe
        if (maxDist == std::numeric_limits<float>::max() ||
            maxDist == std::numeric_limits<float>::infinity())
        {
            return false;
        }

        // Valid map coords should *never ever* produce float overflow, but this would produce NaNs too:
        MANGOS_ASSERT(maxDist < std::numeric_limits<float>::max());
        // Prevent NaN values which can cause BIH intersection to enter infinite loop
        if (maxDist < 1e-10f)
        {
            return true;
        }

        // Direction with length of 1
        G3D::Ray ray = G3D::Ray::fromOriginAndDirection(pos1, (pos2 - pos1) / maxDist);
        if (getIntersectionTime(ray, maxDist, true))
        {
            return false;
        }

        return true;
    }

    /**
     * @brief Checks if an object is hit when moving from pos1 to pos2.
     *
     * @param pPos1 The starting position.
     * @param pPos2 The ending position.
     * @param pResultHitPos The resulting hit position.
     * @param pModifyDist The distance to modify the hit position.
     * @return true if an object is hit, false otherwise.
     */
    bool StaticMapTree::getObjectHitPos(const Vector3& pPos1, const Vector3& pPos2, Vector3& pResultHitPos, float pModifyDist) const
    {
        float maxDist = (pPos2 - pPos1).magnitude();
        // Valid map coords should *never ever* produce float overflow, but this would produce NaNs too:
        MANGOS_ASSERT(maxDist < std::numeric_limits<float>::max());
        // Prevent NaN values which can cause BIH intersection to enter infinite loop
        if (maxDist < 1e-10f)
        {
            pResultHitPos = pPos2;
            return false;
        }

        Vector3 dir = (pPos2 - pPos1) / maxDist; // Direction with length of 1
        G3D::Ray ray(pPos1, dir);
        float dist = maxDist;
        if (getIntersectionTime(ray, dist, false))
        {
            pResultHitPos = pPos1 + dir * dist;
            if (pModifyDist < 0)
            {
                if ((pResultHitPos - pPos1).magnitude() > -pModifyDist)
                {
                    pResultHitPos = pResultHitPos + dir * pModifyDist;
                }
                else
                {
                    pResultHitPos = pPos1;
                }
            }
            else
            {
                pResultHitPos = pResultHitPos + dir * pModifyDist;
            }
            return true;
        }
        else
        {
            pResultHitPos = pPos2;
        }
        return false;
    }

    /**
     * @brief Retrieves the height at a given position.
     *
     * @param pPos The position to check.
     * @param maxSearchDist The maximum search distance.
     * @return float The height at the position.
     */
    float StaticMapTree::getHeight(const Vector3& pPos, float maxSearchDist) const
    {
        float height = G3D::inf();
        Vector3 dir = Vector3(0, 0, -1);
        G3D::Ray ray(pPos, dir); // Direction with length of 1
        float maxDist = maxSearchDist;
        if (getIntersectionTime(ray, maxDist, false))
        {
            height = pPos.z - maxDist;
        }
        return height;
    }

    /**
     * @brief Checks if a map can be loaded.
     *
     * @param vmapPath The path to the VMAP files.
     * @param mapID The map ID.
     * @param tileX The tile X coordinate.
     * @param tileY The tile Y coordinate.
     * @return true if the map can be loaded, false otherwise.
     */
    bool StaticMapTree::CanLoadMap(const std::string& vmapPath, uint32 mapID, uint32 tileX, uint32 tileY)
    {
        std::string basePath = vmapPath;
        if (basePath.length() > 0 && (basePath[basePath.length() - 1] != '/' || basePath[basePath.length() - 1] != '\\'))
        {
            basePath.append("/");
        }
        std::string fullname = basePath + VMapManager2::getMapFileName(mapID);
        bool success = true;
        FILE* rf = fopen(fullname.c_str(), "rb");
        if (!rf)
        {
            return false;
        }
        // TODO: check magic number when implemented...
        char tiled;
        char chunk[8];
        if (!readChunk(rf, chunk, VMAP_MAGIC, 8) || fread(&tiled, sizeof(char), 1, rf) != 1)
        {
            fclose(rf);
            return false;
        }
        if (tiled)
        {
            std::string tilefile = basePath + getTileFileName(mapID, tileX, tileY);
            FILE* tf = fopen(tilefile.c_str(), "rb");
            if (!tf)
            {
                success = false;
            }
            else
            {
                if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
                {
                    success = false;
                }
                fclose(tf);
            }
        }
        fclose(rf);
        return success;
    }

    /**
     * @brief Initializes the map.
     *
     * @param fname The file name of the map.
     * @param vm The VMap manager.
     * @return true if the map was successfully initialized, false otherwise.
     */
    bool StaticMapTree::InitMap(const std::string& fname, VMapManager2* vm)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "Initializing StaticMapTree '%s'", fname.c_str());
        bool success = true;
        std::string fullname = iBasePath + fname;
        FILE* rf = fopen(fullname.c_str(), "rb");
        if (!rf)
        {
            return false;
        }
        else
        {
            char chunk[8];
            // General info
            if (!readChunk(rf, chunk, VMAP_MAGIC, 8))
            {
                success = false;
            }
            char tiled = 0;
            if (success && fread(&tiled, sizeof(char), 1, rf) != 1)
            {
                success = false;
            }
            iIsTiled = bool(tiled);
            // Nodes
            if (success && !readChunk(rf, chunk, "NODE", 4))
            {
                success = false;
            }
            if (success)
            {
                success = iTree.ReadFromFile(rf);
            }
            if (success)
            {
                iNTreeValues = iTree.primCount();
                iTreeValues = new ModelInstance[iNTreeValues];
            }

            if (success && !readChunk(rf, chunk, "GOBJ", 4))
            {
                success = false;
            }
            // Global model spawns
            // Only non-tiled maps have them, and if so exactly one (so far at least...)
            ModelSpawn spawn;
#ifdef VMAP_DEBUG
            DEBUG_LOG("Map isTiled: %u", static_cast<uint32>(iIsTiled));
#endif
            if (!iIsTiled && ModelSpawn::ReadFromFile(rf, spawn))
            {
                WorldModel* model = vm->acquireModelInstance(iBasePath, spawn.name, spawn.flags);
                DEBUG_FILTER_LOG(LOG_FILTER_MAP_LOADING, "StaticMapTree::InitMap(): loading %s", spawn.name.c_str());
                if (model)
                {
                    // Assume that global model always is the first and only tree value (could be improved...)
                    iTreeValues[0] = ModelInstance(spawn, model);
                    iLoadedSpawns[0] = 1;
                }
                else
                {
                    success = false;
                    ERROR_LOG("StaticMapTree::InitMap() could not acquire WorldModel pointer for '%s'!", spawn.name.c_str());
                }
            }

            fclose(rf);
        }
        return success;
    }

    /**
     * @brief Unloads the map.
     *
     * @param vm The VMap manager.
     */
    void StaticMapTree::UnloadMap(VMapManager2* vm)
    {
        for (loadedSpawnMap::iterator i = iLoadedSpawns.begin(); i != iLoadedSpawns.end(); ++i)
        {
            iTreeValues[i->first].setUnloaded();
            for (uint32 refCount = 0; refCount < i->second; ++refCount)
            {
                vm->releaseModelInstance(iTreeValues[i->first].name);
            }
        }
        iLoadedSpawns.clear();
        iLoadedTiles.clear();
    }

    /**
     * @brief Loads a map tile.
     *
     * @param tileX The tile X coordinate.
     * @param tileY The tile Y coordinate.
     * @param vm The VMap manager.
     * @return true if the tile was successfully loaded, false otherwise.
     */
    bool StaticMapTree::LoadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm)
    {
        if (!iIsTiled)
        {
            // Currently, core creates grids for all maps, whether it has terrain tiles or not
            // So we need "fake" tile loads to know when we can unload map geometry
            iLoadedTiles[packTileID(tileX, tileY)] = false;
            return true;
        }
        if (!iTreeValues)
        {
            ERROR_LOG("StaticMapTree::LoadMapTile(): Tree has not been initialized! [%u,%u]", tileX, tileY);
            return false;
        }
        bool result = true;

        std::string tilefile = iBasePath + getTileFileName(iMapID, tileX, tileY);
        FILE* tf = fopen(tilefile.c_str(), "rb");
        if (tf)
        {
            char chunk[8];
            if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
            {
                result = false;
            }
            uint32 numSpawns = 0;
            if (result && fread(&numSpawns, sizeof(uint32), 1, tf) != 1)
            {
                result = false;
            }
            for (uint32 i = 0; i < numSpawns && result; ++i)
            {
                // Read model spawns
                ModelSpawn spawn;
                result = ModelSpawn::ReadFromFile(tf, spawn);
                if (result)
                {
                    // Acquire model instance
                    WorldModel* model = vm->acquireModelInstance(iBasePath, spawn.name, spawn.flags);
                    if (!model)
                    {
                        ERROR_LOG("StaticMapTree::LoadMapTile() could not acquire WorldModel pointer for '%s'!", spawn.name.c_str());
                    }

                    // Update tree
                    uint32 referencedVal;

                    size_t fileRead = fread(&referencedVal, sizeof(uint32), 1, tf);
                    if (!iLoadedSpawns.count(referencedVal) || fileRead <= 0)
                    {
                        if (referencedVal > iNTreeValues)
                        {
                            ERROR_LOG("invalid tree element! (%u/%u)", referencedVal, iNTreeValues);
                            continue;
                        }
                        iTreeValues[referencedVal] = ModelInstance(spawn, model);
                        iLoadedSpawns[referencedVal] = 1;
                    }
                    else
                    {
                        ++iLoadedSpawns[referencedVal];
#ifdef VMAP_DEBUG
                        if (iTreeValues[referencedVal].ID != spawn.ID)
                        {
                            DEBUG_LOG("Error: trying to load wrong spawn in node!");
                        }
                        else if (iTreeValues[referencedVal].name != spawn.name)
                        {
                            DEBUG_LOG("Error: name mismatch on GUID=%u", spawn.ID);
                        }
#endif
                    }
                }
            }
            iLoadedTiles[packTileID(tileX, tileY)] = true;
            fclose(tf);
        }
        else
        {
            iLoadedTiles[packTileID(tileX, tileY)] = false;
        }
        return result;
    }

    //=========================================================

    /**
     * @brief Unloads a map tile.
     *
     * @param tileX The tile X coordinate.
     * @param tileY The tile Y coordinate.
     * @param vm The VMap manager.
     */
    void StaticMapTree::UnloadMapTile(uint32 tileX, uint32 tileY, VMapManager2* vm)
    {
        uint32 tileID = packTileID(tileX, tileY);
        loadedTileMap::iterator tile = iLoadedTiles.find(tileID);
        if (tile == iLoadedTiles.end())
        {
            ERROR_LOG("StaticMapTree::UnloadMapTile(): Trying to unload non-loaded tile. Map:%u X:%u Y:%u", iMapID, tileX, tileY);
            return;
        }
        if (tile->second) // File associated with tile
        {
            std::string tilefile = iBasePath + getTileFileName(iMapID, tileX, tileY);
            FILE* tf = fopen(tilefile.c_str(), "rb");
            if (tf)
            {
                bool result = true;
                char chunk[8];
                if (!readChunk(tf, chunk, VMAP_MAGIC, 8))
                {
                    result = false;
                }
                uint32 numSpawns;
                if (fread(&numSpawns, sizeof(uint32), 1, tf) != 1)
                {
                    result = false;
                }
                for (uint32 i = 0; i < numSpawns && result; ++i)
                {
                    // Read model spawns
                    ModelSpawn spawn;
                    result = ModelSpawn::ReadFromFile(tf, spawn);
                    if (result)
                    {
                        // Release model instance
                        vm->releaseModelInstance(spawn.name);

                        // Update tree
                        uint32 referencedNode;

                        size_t fileRead = fread(&referencedNode, sizeof(uint32), 1, tf);
                        if (!iLoadedSpawns.count(referencedNode) || fileRead <= 0)
                        {
                            ERROR_LOG("Trying to unload non-referenced model '%s' (ID:%u)", spawn.name.c_str(), spawn.ID);
                        }
                        else if (--iLoadedSpawns[referencedNode] == 0)
                        {
                            iTreeValues[referencedNode].setUnloaded();
                            iLoadedSpawns.erase(referencedNode);
                        }
                    }
                }
                fclose(tf);
            }
        }
        iLoadedTiles.erase(tile);
    }
}

