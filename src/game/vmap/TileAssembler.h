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

#ifndef MANGOS_H_TILEASSEMBLER
#define MANGOS_H_TILEASSEMBLER

#include <G3D/Vector3.h>
#include <G3D/Matrix3.h>
#include <map>
#include <set>

#include "ModelInstance.h"
#include "WorldModel.h"

namespace VMAP
{
    /**
     * @brief This Class is used to convert raw vector data into balanced BSP-Trees.
     *
     */
    class ModelPosition
    {
    private:
        G3D::Matrix3 iRotation; /**< Rotation matrix for the model */
    public:
        G3D::Vector3 iPos; /**< Position of the model */
        G3D::Vector3 iDir; /**< Direction of the model */
        float iScale; /**< Scale of the model */

        /**
            * @brief Constructor to initialize member variables.
            *
            * Initializes iPos, iDir, and iScale to default values.
            */
        ModelPosition() : iPos(G3D::Vector3::zero()), iDir(G3D::Vector3::zero()), iScale(1.0f) {}

        /**
            * @brief Initializes the rotation matrix based on the direction
            */
        void init()
        {
            iRotation = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi() * iDir.y / 180.f, G3D::pi() * iDir.x / 180.f, G3D::pi() * iDir.z / 180.f);
        }

        /**
            * @brief Transforms a given vector by the model's position and rotation
            *
            * @param pIn The input vector to transform
            * @return G3D::Vector3 The transformed vector
            */
        G3D::Vector3 transform(const G3D::Vector3& pIn) const;

        /**
            * @brief Moves the model's position to the base position
            *
            * @param pBasePos The base position to move to
            */
        void moveToBasePos(const G3D::Vector3& pBasePos) { iPos -= pBasePos; }
    };

    /**
     * @brief Map of unique model entries
     */
    typedef std::map<uint32, ModelSpawn> UniqueEntryMap;
    /**
     * @brief Multimap of tile entries
     */
    typedef std::multimap<uint32, uint32> TileMap;

    /**
     * @brief Structure to hold map spawns
     */
    struct MapSpawns
    {
        UniqueEntryMap UniqueEntries; /**< Unique model entries */
        TileMap TileEntries; /**< Tile entries */
    };

    /**
     * @brief Map of map data
     */
    typedef std::map<uint32, MapSpawns*> MapData;

    /**
     * @brief Structure to hold raw group model data
     */
    struct GroupModel_Raw
    {
        uint32 mogpflags; /**< Flags for the group model */
        uint32 GroupWMOID; /**< ID of the group WMO */
        G3D::AABox bounds; /**< Bounding box of the group model */
        uint32 liquidflags; /**< Flags for the liquid */
        std::vector<MeshTriangle> triangles; /**< Triangles in the group model */
        std::vector<G3D::Vector3> vertexArray; /**< Vertex array of the group model */
        class WmoLiquid* liquid; /**< Pointer to the WMO liquid */

        /**
         * @brief Constructor to initialize member variables
         */
        GroupModel_Raw() : mogpflags(0), GroupWMOID(0), liquidflags(0), liquid(nullptr) {}
        /**
         * @brief Destructor to clean up resources
         */
        ~GroupModel_Raw();

        /**
         * @brief Reads the group model data from a file
         *
         * @param f The file to read from
         * @return bool True if successful, false otherwise
         */
        bool Read(FILE* f);
    };

    /**
     * @brief Structure to hold raw world model data
     */
    struct WorldModel_Raw
    {
        uint32 RootWMOID; /**< ID of the root WMO */
        std::vector<GroupModel_Raw> groupsArray; /**< Array of group models */

        /**
         * @brief Reads the world model data from a file
         *
         * @param path The path to the file
         * @param RAW_VMAP_MAGIC The validation string to verify the file header
         * @return bool True if successful, false otherwise
         */
        bool Read(const char* path, const char* RAW_VMAP_MAGIC);
    };

    /**
     * @brief Class to assemble tiles from raw data
     */
    class TileAssembler
    {
    private:
        std::string iDestDir; /**< Destination directory */
        std::string iSrcDir; /**< Source directory */
        /**
         * @brief Function pointer for the model name filter method
         *
         * @param pName The name of the model
         * @return bool True if the model name passes the filter, false otherwise
         */
        bool (*iFilterMethod)(char* pName);
        G3D::Table<std::string, unsigned int > iUniqueNameIds; /**< Table of unique name IDs */
        unsigned int iCurrentUniqueNameId; /**< Current unique name ID */
        MapData mapData; /**< Map data */
        std::set<std::string> spawnedModelFiles; /**< Set of spawned model files */

    public:
        /**
         * @brief Constructor to initialize the TileAssembler
         *
         * @param pSrcDirName The source directory name
         * @param pDestDirName The destination directory name
         */
        TileAssembler(const std::string& pSrcDirName, const std::string& pDestDirName);
        /**
         * @brief Destructor to clean up resources
         */
        virtual ~TileAssembler();

        /**
         * @brief Converts the world data to a different format
         *
         * @param RAW_VMAP_MAGIC The validation string to verify the file header
         * @return bool True if successful, false otherwise
         */
        bool convertWorld2(const char* RAW_VMAP_MAGIC);
        /**
         * @brief Reads the map spawns from a file
         *
         * @return bool True if successful, false otherwise
         */
        bool readMapSpawns();
        /**
         * @brief Calculates the transformed bounding box for a model spawn
         *
         * @param spawn The model spawn to calculate the bounding box for
         * @param RAW_VMAP_MAGIC The validation string to verify the file header
         * @return bool True if successful, false otherwise
         */
        bool calculateTransformedBound(ModelSpawn& spawn, const char* RAW_VMAP_MAGIC) const;

        /**
         * @brief Exports the game object models
         *
         * @param RAW_VMAP_MAGIC The validation string to verify the file header
         */
        void exportGameobjectModels(const char* RAW_VMAP_MAGIC);
        /**
         * @brief Converts a raw file to a different format
         *
         * @param pModelFilename The name of the model file
         * @param RAW_VMAP_MAGIC The validation string to verify the file header
         * @return bool True if successful, false otherwise
         */
        bool convertRawFile(const std::string& pModelFilename, const char* RAW_VMAP_MAGIC) const;
        /**
         * @brief Sets the model name filter method
         *
         * @param pFilterMethod The filter method to set
         */
        void setModelNameFilterMethod(bool (*pFilterMethod)(char* pName)) { iFilterMethod = pFilterMethod; }
        /**
         * @brief Gets the directory entry name from the model name
         *
         * @param pMapId The map ID
         * @param pModPosName The model position name
         * @return std::string The directory entry name
         */
        std::string getDirEntryNameFromModName(unsigned int pMapId, const std::string& pModPosName);
        /**
         * @brief Gets the unique name ID for a given name
         *
         * @param pName The name to get the unique ID for
         * @return unsigned int The unique name ID
         */
        unsigned int getUniqueNameId(const std::string pName);
    };
}
#endif
