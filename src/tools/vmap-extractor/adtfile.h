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

#ifndef ADT_H
#define ADT_H

#include <libmpq/mpq.h>
#include <ml/mpq.h>
#include "wmo.h"
#include "vmapexport.h"
#include "model.h"

#define TILESIZE (533.33333f)
#define CHUNKSIZE ((TILESIZE) / 16.0f)
#define UNITSIZE (CHUNKSIZE / 8.0f)

class Liquid;

/**
 * @brief
 *
 */
typedef struct
{
    float x; /**< TODO */
    float y; /**< TODO */
    float z; /**< TODO */
} svec;

/**
 * @brief
 *
 */
struct vec
{
    double x; /**< TODO */
    double y; /**< TODO */
    double z; /**< TODO */
};

/**
 * @brief
 *
 */
struct triangle
{
    vec v[3]; /**< TODO */
};

/**
 * @brief
 *
 */
typedef struct
{
    float v9[16 * 8 + 1][16 * 8 + 1]; /**< TODO */
    float v8[16 * 8][16 * 8]; /**< TODO */
} Cell;

/**
 * @brief
 *
 */
typedef struct
{
    double v9[9][9]; /**< TODO */
    double v8[8][8]; /**< TODO */
    uint16 area_id; /**< TODO */
    //Liquid *lq;
    float waterlevel[9][9]; /**< TODO */
    uint8 flag; /**< TODO */
} chunk;

/**
 * @brief
 *
 */
typedef struct
{
    chunk ch[16][16]; /**< TODO */
} mcell;

/**
 * @brief
 *
 */
struct MapChunkHeader
{
    uint32 flags; /**< TODO */
    uint32 ix; /**< TODO */
    uint32 iy; /**< TODO */
    uint32 nLayers; /**< TODO */
    uint32 nDoodadRefs; /**< TODO */
    uint32 ofsHeight; /**< TODO */
    uint32 ofsNormal; /**< TODO */
    uint32 ofsLayer; /**< TODO */
    uint32 ofsRefs; /**< TODO */
    uint32 ofsAlpha; /**< TODO */
    uint32 sizeAlpha; /**< TODO */
    uint32 ofsShadow; /**< TODO */
    uint32 sizeShadow; /**< TODO */
    uint32 areaid; /**< TODO */
    uint32 nMapObjRefs; /**< TODO */
    uint32 holes; /**< TODO */
    uint16 s1; /**< TODO */
    uint16 s2; /**< TODO */
    uint32 d1; /**< TODO */
    uint32 d2; /**< TODO */
    uint32 d3; /**< TODO */
    uint32 predTex; /**< TODO */
    uint32 nEffectDoodad; /**< TODO */
    uint32 ofsSndEmitters; /**< TODO */
    uint32 nSndEmitters; /**< TODO */
    uint32 ofsLiquid; /**< TODO */
    uint32 sizeLiquid; /**< TODO */
    float  zpos; /**< TODO */
    float  xpos; /**< TODO */
    float  ypos; /**< TODO */
    uint32 textureId; /**< TODO */
    uint32 props; /**< TODO */
    uint32 effectId; /**< TODO */
};


/**
 * @brief
 *
 */
class ADTFile
{
    public:
        /**
         * @brief
         *
         * @param filename
         */
        ADTFile(char* filename);
        /**
         * @brief
         *
         */
        ~ADTFile();
        int nWMO; /**< TODO */
        int nMDX; /**< TODO */
        string* WmoInstansName; /**< TODO */
        string* ModelInstansName; /**< TODO */
        /**
         * @brief
         *
         * @param map_num
         * @param tileX
         * @param tileY
         * @param failedPaths
         * @return bool
         */
        bool init(uint32 map_num, uint32 tileX, uint32 tileY, StringSet& failedPaths);
        //void LoadMapChunks();

        //uint32 wmo_count;
        /*
            const mcell& Getmcell() const
            {
                return Mcell;
            }
        */
    private:
        //size_t mcnk_offsets[256], mcnk_sizes[256];
        MPQFile ADT; /**< TODO */
        //mcell Mcell;
        string Adtfilename; /**< TODO */
};

/**
 * @brief
 *
 * @param FileName
 * @return const char
 */
const char* GetPlainName(const char* FileName);
/**
 * @brief
 *
 * @param FileName
 * @return char
 */
char* GetPlainName(char* FileName);
/**
 * @brief
 *
 * @param FileName
 * @return const char
 */
char const* GetExtension(char const* FileName);
/**
 * @brief
 *
 * @param name
 * @param len
 */
void fixnamen(char* name, size_t len);
/**
 * @brief
 *
 * @param name
 * @param len
 */
void fixname2(char* name, size_t len);
//void fixMapNamen(char *name, size_t len);

#endif
