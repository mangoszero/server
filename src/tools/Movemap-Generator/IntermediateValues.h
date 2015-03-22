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

#ifndef MANGOS_H_INTERMEDIATE_VALUES
#define MANGOS_H_INTERMEDIATE_VALUES

#include <Recast.h>
#include <DetourNavMesh.h>

#include "MMapCommon.h"
#include "TerrainBuilder.h"

namespace MMAP
{
    /**
     * @brief this class gathers all debug info holding and output
     *
     */
    struct IntermediateValues
    {
        rcHeightfield* heightfield; /**< TODO */
        rcCompactHeightfield* compactHeightfield; /**< TODO */
        rcContourSet* contours; /**< TODO */
        rcPolyMesh* polyMesh; /**< TODO */
        rcPolyMeshDetail* polyMeshDetail; /**< TODO */

        /**
         * @brief
         *
         */
        IntermediateValues() :  compactHeightfield(NULL), heightfield(NULL),
            contours(NULL), polyMesh(NULL), polyMeshDetail(NULL) {}
        /**
         * @brief
         *
         */
        ~IntermediateValues();

        /**
         * @brief
         *
         * @param mapID
         * @param tileX
         * @param tileY
         */
        void writeIV(uint32 mapID, uint32 tileX, uint32 tileY);

        /**
         * @brief
         *
         * @param file
         * @param mesh
         */
        void debugWrite(FILE* file, const rcHeightfield* mesh);
        /**
         * @brief
         *
         * @param file
         * @param chf
         */
        void debugWrite(FILE* file, const rcCompactHeightfield* chf);
        /**
         * @brief
         *
         * @param file
         * @param cs
         */
        void debugWrite(FILE* file, const rcContourSet* cs);
        /**
         * @brief
         *
         * @param file
         * @param mesh
         */
        void debugWrite(FILE* file, const rcPolyMesh* mesh);
        /**
         * @brief
         *
         * @param file
         * @param mesh
         */
        void debugWrite(FILE* file, const rcPolyMeshDetail* mesh);

        /**
         * @brief
         *
         * @param mapID
         * @param tileX
         * @param tileY
         * @param meshData
         */
        void generateObjFile(uint32 mapID, uint32 tileX, uint32 tileY, MeshData& meshData);
    };
}
#endif
