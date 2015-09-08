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

#ifndef MODEL_H
#define MODEL_H

#include <ml/loadlib.h>
#include "vec3d.h"
#include "modelheaders.h"
#include <vector>
#include "vmapexport.h"

class WMOInstance;
class MPQFile;

/**
 * @brief
 *
 * @param v
 * @return Vec3D
 */
Vec3D fixCoordSystem(Vec3D v);

/**
 * @brief
 *
 */
class Model
{
    public:
        ModelHeader header; /**< TODO */
        ModelBoundingVertex* boundingVertices; /**< TODO */
        Vec3D* vertices; /**< TODO */
        uint16* indices; /**< TODO */
        size_t nIndices; /**< TODO */

        /**
         * @brief
         *
         * @param failedPaths
         * @return bool
         */
        bool open(StringSet& failedPaths);
        /**
         * @brief
         *
         * @param outfilename
         * @return bool
         */
        bool ConvertToVMAPModel(const char* outfilename);

        bool ok; /**< TODO */

        /**
         * @brief
         *
         * @param filename
         */
        Model(std::string& filename);
        /**
         * @brief
         *
         */
        ~Model() {_unload();}

    private:
        /**
         * @brief
         *
         */
        void _unload()
        {
            delete[] vertices;
            delete[] indices;
            vertices = NULL;
            indices = NULL;
        }
        std::string filename; /**< TODO */
        char outfilename; /**< TODO */
};

/**
 * @brief
 *
 */
class ModelInstance
{
    public:
        Model* model; /**< TODO */

        uint32 id; /**< TODO */
        //unsigned int scale; // This line introduced a regression bug in Mangos Zero, is Fine for other cores.
        Vec3D pos, rot; /**< TODO */
        //float sc; // This line introduced a regression bug in Mangos Zero, is Fine for other cores.
        unsigned int d1, scale;
        float w, sc;

        /**
         * @brief
         *
         */
        ModelInstance() {}
        /**
         * @brief
         *
         * @param f
         * @param ModelInstName
         * @param mapID
         * @param tileX
         * @param tileY
         * @param pDirfile
         */
        ModelInstance(MPQFile& f, const char* ModelInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE* pDirfile);

};

#endif
