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

#ifndef WMO_H
#define WMO_H

#define TILESIZE (533.33333f)
#define CHUNKSIZE ((TILESIZE) / 16.0f)

#include <string>
#include <set>
#include "vec3d.h"
#include <loadlib.h>

// MOPY flags
#define WMO_MATERIAL_NOCAMCOLLIDE    0x01
#define WMO_MATERIAL_DETAIL          0x02
#define WMO_MATERIAL_NO_COLLISION    0x04
#define WMO_MATERIAL_HINT            0x08
#define WMO_MATERIAL_RENDER          0x10
#define WMO_MATERIAL_COLLIDE_HIT     0x20
#define WMO_MATERIAL_WALL_SURFACE    0x40

class WMOInstance;
class WMOManager;
class MPQFile;

/**
 * @brief for whatever reason a certain company just can't stick to one coordinate system...
 *
 * @param v
 * @return Vec3D
 */
static inline Vec3D fixCoords(const Vec3D& v) { return Vec3D(v.z, v.x, v.y); }

/**
 * @brief
 *
 */
class WMORoot
{
    public:
        uint32 nTextures, nGroups, nP, nLights, nModels, nDoodads, nDoodadSets, RootWMOID, liquidType; /**< TODO */
        unsigned int col; /**< TODO */
        float bbcorn1[3]; /**< TODO */
        float bbcorn2[3]; /**< TODO */

        /**
         * @brief
         *
         * @param filename
         */
        WMORoot(std::string& filename);
        /**
         * @brief
         *
         */
        ~WMORoot();

        /**
         * @brief
         *
         * @return bool
         */
        bool open();
        /**
         * @brief
         *
         * @param output
         * @return bool
         */
        bool ConvertToVMAPRootWmo(FILE* output);
    private:
        std::string filename; /**< TODO */
        char outfilename; /**< TODO */
};

/**
 * @brief
 *
 */
struct WMOLiquidHeader
{
    int xverts, yverts, xtiles, ytiles; /**< TODO */
    float pos_x; /**< TODO */
    float pos_y; /**< TODO */
    float pos_z; /**< TODO */
    short type; /**< TODO */
};

/**
 * @brief
 *
 */
struct WMOLiquidVert
{
    uint16 unk1; /**< TODO */
    uint16 unk2; /**< TODO */
    float height; /**< TODO */
};

/**
 * @brief
 *
 */
class WMOGroup
{
    public:
        // MOGP
        int groupName, descGroupName, mogpFlags; /**< TODO */
        float bbcorn1[3]; /**< TODO */
        float bbcorn2[3]; /**< TODO */
        uint16 moprIdx; /**< TODO */
        uint16 moprNItems; /**< TODO */
        uint16 nBatchA; /**< TODO */
        uint16 nBatchB; /**< TODO */
        uint32 nBatchC, fogIdx, liquidType, groupWMOID; /**< TODO */

        int mopy_size, moba_size; /**< TODO */
        int LiquEx_size; /**< TODO */
        unsigned int nVertices; /**< number when loaded */
        int nTriangles; /**< number when loaded */
        char* MOPY; /**< TODO */
        uint16* MOVI; /**< TODO */
        uint16* MoviEx; /**< TODO */
        float* MOVT; /**< TODO */
        uint16* MOBA; /**< TODO */
        int* MobaEx; /**< TODO */
        WMOLiquidHeader* hlq; /**< TODO */
        WMOLiquidVert* LiquEx; /**< TODO */
        char* LiquBytes; /**< TODO */
        uint32 liquflags; /**< TODO */

        /**
         * @brief
         *
         * @param filename
         */
        WMOGroup(std::string& filename);
        /**
         * @brief
         *
         */
        ~WMOGroup();

        /**
         * @brief
         *
         * @return bool
         */
        bool open();
        /**
         * @brief
         *
         * @param output
         * @param rootWMO
         * @param pPreciseVectorData
         * @return int
         */
        int ConvertToVMAPGroupWmo(FILE* output, WMORoot* rootWMO, bool pPreciseVectorData);

    private:
        std::string filename; /**< TODO */
        char outfilename; /**< TODO */
};

/**
 * @brief
 *
 */
class WMOInstance
{
        static std::set<int> ids; /**< TODO */
    public:
        std::string MapName; /**< TODO */
        int currx; /**< TODO */
        int curry; /**< TODO */
        WMOGroup* wmo; /**< TODO */
        Vec3D pos; /**< TODO */
        Vec3D pos2, pos3, rot; /**< TODO */
        uint32 indx, id, d2, d3; /**< TODO */
        int doodadset; /**< TODO */

        /**
         * @brief
         *
         * @param f
         * @param WmoInstName
         * @param mapID
         * @param tileX
         * @param tileY
         * @param pDirfile
         */
        WMOInstance(MPQFile& f, const char* WmoInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE* pDirfile);

        /**
         * @brief
         *
         */
        static void reset();
};

#endif
