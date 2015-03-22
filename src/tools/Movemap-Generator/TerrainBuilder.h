/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
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

#ifndef MANGOS_H_MMAP_TERRAIN_BUILDER
#define MANGOS_H_MMAP_TERRAIN_BUILDER

#include "MMapCommon.h"
#include "MangosMap.h"
#include "MoveMapSharedDefines.h"

#include "WorldModel.h"

#include "G3D/Array.h"
#include "G3D/Vector3.h"
#include "G3D/Matrix3.h"

using namespace MaNGOS;

namespace MMAP
{
    /**
     * @brief
     *
     */
    enum Spot
    {
        TOP     = 1,
        RIGHT   = 2,
        LEFT    = 3,
        BOTTOM  = 4,
        ENTIRE  = 5
    };

    /**
     * @brief
     *
     */
    enum Grid
    {
        GRID_V8,
        GRID_V9
    };

    static const int V9_SIZE = 129; /**< TODO */
    static const int V9_SIZE_SQ = V9_SIZE* V9_SIZE; /**< TODO */
    static const int V8_SIZE = 128; /**< TODO */
    static const int V8_SIZE_SQ = V8_SIZE* V8_SIZE; /**< TODO */
    static const float GRID_SIZE = 533.33333f; /**< TODO */
    static const float GRID_PART_SIZE = GRID_SIZE / V8_SIZE; /**< TODO */

    // see contrib/extractor/system.cpp, CONF_use_minHeight
    static const float INVALID_MAP_LIQ_HEIGHT = -500.f; /**< TODO */
    static const float INVALID_MAP_LIQ_HEIGHT_MAX = 5000.0f; /**< TODO */

    // see following files:
    // src/tools/map-extractor/system.cpp
    // src/game/GridMap.cpp
    static char const* MAP_VERSION_MAGIC = "z1.3"; /**< TODO */

    /**
     * @brief
     *
     */
    struct MeshData
    {
        G3D::Array<float> solidVerts; /**< TODO */
        G3D::Array<int> solidTris; /**< TODO */

        G3D::Array<float> liquidVerts; /**< TODO */
        G3D::Array<int> liquidTris; /**< TODO */
        G3D::Array<uint8> liquidType; /**< TODO */

        // offmesh connection data
        G3D::Array<float> offMeshConnections;   // [p0y,p0z,p0x,p1y,p1z,p1x] - per connection /**< TODO */
        G3D::Array<float> offMeshConnectionRads; /**< TODO */
        G3D::Array<unsigned char> offMeshConnectionDirs; /**< TODO */
        G3D::Array<unsigned char> offMeshConnectionsAreas; /**< TODO */
        G3D::Array<unsigned short> offMeshConnectionsFlags; /**< TODO */
    };

    /**
     * @brief
     *
     */
    class TerrainBuilder
    {
        public:
            /**
             * @brief
             *
             * @param skipLiquid
             */
            TerrainBuilder(bool skipLiquid);
            /**
             * @brief
             *
             */
            ~TerrainBuilder();

            /**
             * @brief
             *
             * @param mapID
             * @param tileX
             * @param tileY
             * @param meshData
             */
            void loadMap(int mapID, int tileX, int tileY, MeshData& meshData);
            /**
             * @brief
             *
             * @param mapID
             * @param tileX
             * @param tileY
             * @param meshData
             * @return bool
             */
            bool loadVMap(int mapID, int tileX, int tileY, MeshData& meshData);
            /**
             * @brief
             *
             * @param mapID
             * @param tileX
             * @param tileY
             * @param meshData
             * @param offMeshFilePath
             */
            void loadOffMeshConnections(int mapID, int tileX, int tileY, MeshData& meshData, const char* offMeshFilePath);

            /**
             * @brief
             *
             * @return bool
             */
            bool usesLiquids() { return !m_skipLiquid; }

            /**
             * @brief vert and triangle methods
             *
             * @param original
             * @param transformed
             * @param scale
             * @param rotation
             * @param position
             */
            static void transform(vector<G3D::Vector3>& original, vector<G3D::Vector3>& transformed,
                                  float scale, G3D::Matrix3& rotation, G3D::Vector3& position);
            /**
             * @brief
             *
             * @param source
             * @param dest
             */
            static void copyVertices(vector<G3D::Vector3>& source, G3D::Array<float>& dest);
            /**
             * @brief
             *
             * @param source
             * @param dest
             * @param offest
             * @param flip
             */
            static void copyIndices(vector<VMAP::MeshTriangle>& source, G3D::Array<int>& dest, int offest, bool flip);
            /**
             * @brief
             *
             * @param src
             * @param dest
             * @param offset
             */
            static void copyIndices(G3D::Array<int>& src, G3D::Array<int>& dest, int offset);
            /**
             * @brief
             *
             * @param verts
             * @param tris
             */
            static void cleanVertices(G3D::Array<float>& verts, G3D::Array<int>& tris);
        private:
            /**
             * @brief Loads a portion of a map's terrain
             *
             * @param mapID
             * @param tileX
             * @param tileY
             * @param meshData
             * @param portion
             * @return bool
             */
            bool loadMap(int mapID, int tileX, int tileY, MeshData& meshData, Spot portion);

            /**
             * @brief Sets loop variables for selecting only certain parts of a map's terrain
             *
             * @param portion
             * @param loopStart
             * @param loopEnd
             * @param loopInc
             */
            void getLoopVars(Spot portion, int& loopStart, int& loopEnd, int& loopInc);

            bool m_skipLiquid; /**< Controls whether liquids are loaded */

            /**
             * @brief Load the map terrain from file
             *
             * @param mapID
             * @param tileX
             * @param tileY
             * @param vertices
             * @param triangles
             * @param portion
             * @return bool
             */
            bool loadHeightMap(int mapID, int tileX, int tileY, G3D::Array<float>& vertices, G3D::Array<int>& triangles, Spot portion);

            /**
             * @brief Get the vector coordinate for a specific position
             *
             * @param index
             * @param grid
             * @param xOffset
             * @param yOffset
             * @param coord
             * @param v
             */
            void getHeightCoord(int index, Grid grid, float xOffset, float yOffset, float* coord, float* v);

            /**
             * @brief Get the triangle's vector indices for a specific position
             *
             * @param square
             * @param triangle
             * @param indices
             * @param liquid
             */
            void getHeightTriangle(int square, Spot triangle, int* indices, bool liquid = false);

            /**
             * @brief Determines if the specific position's triangles should be rendered
             *
             * @param square
             * @param holes[][]
             * @return bool
             */
            bool isHole(int square, const uint16 holes[16][16]);

            /**
             * @brief Get the liquid vector coordinate for a specific position
             *
             * @param index
             * @param index2
             * @param xOffset
             * @param yOffset
             * @param coord
             * @param v
             */
            void getLiquidCoord(int index, int index2, float xOffset, float yOffset, float* coord, float* v);

            /**
             * @brief Get the liquid type for a specific position
             *
             * @param square
             * @param liquid_type[][]
             * @return uint8
             */
            uint8 getLiquidType(int square, const uint8 liquid_type[16][16]);

            /**
             * @brief hide parameterless constructor
             *
             */
            TerrainBuilder();
            /**
             * @brief hide copy constructor
             *
             * @param tb
             */
            TerrainBuilder(const TerrainBuilder& tb);
    };
}

#endif
